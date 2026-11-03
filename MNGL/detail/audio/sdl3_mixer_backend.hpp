#pragma once

#if !defined(MNGL_CUSTOM_AUDIO_BACKEND) \
	&& !defined(MNGL_CUSTOM_AUDIO_INTERFACE) \
	&& !defined(MNGL_CUSTOM_MIXER_INTERFACE)

#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <functional>
#include <istream>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../utilities.hpp"

namespace mngl::audio::detail
{
	inline std::string sdl_error_message(std::string_view prefix);

	inline AudioError inactive_voice_error()
	{
		return AudioError{
			AudioErrorCode::voice_inactive,
			"Voice handle is no longer active"
		};
	}

	struct Voice
	{
		std::uint64_t id;
		MIX_Track* track;
		Bus bus;
		float base_volume;
	};

	class PropertiesGuard
	{
	public:
		PropertiesGuard() noexcept = default;
		PropertiesGuard(const PropertiesGuard&) = delete;
		PropertiesGuard& operator=(const PropertiesGuard&) = delete;

		~PropertiesGuard()
		{
			if (id)
			{
				SDL_DestroyProperties(id);
			}
		}

		void reset(SDL_PropertiesID properties) noexcept
		{
			if (id)
			{
				SDL_DestroyProperties(id);
			}
			id = properties;
		}

		SDL_PropertiesID get() const noexcept
		{
			return id;
		}

		explicit operator bool() const noexcept
		{
			return id != 0;
		}

	private:
		SDL_PropertiesID id = 0;
	};

	class SoundResource
	{
	public:
		explicit SoundResource(MIX_Audio* audio) noexcept
			: audio(audio)
		{
		}

		SoundResource(const SoundResource&) = delete;
		SoundResource& operator=(const SoundResource&) = delete;

		~SoundResource()
		{
			release();
		}

		MIX_Audio* native_audio() const noexcept
		{
			return audio;
		}

		void release() noexcept
		{
			if (audio)
			{
				MIX_DestroyAudio(std::exchange(audio, nullptr));
			}
		}

	private:
		MIX_Audio* audio = nullptr;
	};

	struct MusicResource
	{
		std::string path;
		std::istream* stream = nullptr;
		std::shared_ptr<std::istream> owned_stream;
		std::mutex stream_mutex;
		std::streamoff stream_origin = 0;
		std::streamoff stream_length = 0;
		std::ios::iostate stream_initial_state = std::ios::goodbit;
	};

	struct MusicIOAdapter
	{
		std::shared_ptr<MusicResource> music;
		Sint64 position = 0;
		std::mutex position_mutex;
	};

	inline void restore_music_stream(MusicResource& music) noexcept
	{
		if (!music.stream)
		{
			return;
		}

		try
		{
			std::lock_guard lock(music.stream_mutex);
			auto& stream = *music.stream;
			stream.clear();
			stream.seekg(music.stream_origin, std::ios::beg);
			stream.clear(music.stream_initial_state);
		}
		catch (...)
		{
		}
	}

	inline Sint64 SDLCALL music_io_size(void* userdata)
	{
		const auto* adapter = static_cast<const MusicIOAdapter*>(userdata);
		return adapter
			? static_cast<Sint64>(adapter->music->stream_length)
			: -1;
	}

	inline Sint64 SDLCALL music_io_seek(
		void* userdata,
		Sint64 offset,
		SDL_IOWhence whence)
	{
		auto* adapter = static_cast<MusicIOAdapter*>(userdata);
		if (!adapter)
		{
			return -1;
		}

		try
		{
			std::lock_guard lock(adapter->position_mutex);
			const Sint64 length = static_cast<Sint64>(adapter->music->stream_length);
			Sint64 base = 0;
			switch (whence)
			{
			case SDL_IO_SEEK_SET:
				break;
			case SDL_IO_SEEK_CUR:
				base = adapter->position;
				break;
			case SDL_IO_SEEK_END:
				base = length;
				break;
			default:
				return -1;
			}

			if (offset < -base || offset > length - base)
			{
				return -1;
			}

			adapter->position = base + offset;
			return adapter->position;
		}
		catch (...)
		{
			return -1;
		}
	}

	inline size_t SDLCALL music_io_read(
		void* userdata,
		void* buffer,
		size_t size,
		SDL_IOStatus* status)
	{
		if (status)
		{
			*status = SDL_IO_STATUS_READY;
		}
		auto* adapter = static_cast<MusicIOAdapter*>(userdata);
		if (!adapter || !buffer || !status)
		{
			if (status)
			{
				*status = SDL_IO_STATUS_ERROR;
			}
			return 0;
		}

		try
		{
			std::lock_guard position_lock(adapter->position_mutex);
			const Sint64 length = static_cast<Sint64>(adapter->music->stream_length);
			const Sint64 remaining = length - adapter->position;
			if (remaining <= 0)
			{
				*status = SDL_IO_STATUS_EOF;
				return 0;
			}

			const auto stream_limit = static_cast<std::uint64_t>(
				std::numeric_limits<std::streamsize>::max());
			const auto requested = std::min<std::uint64_t>(
				static_cast<std::uint64_t>(size),
				stream_limit);
			const auto amount = static_cast<std::streamsize>(std::min<std::uint64_t>(
				requested,
				static_cast<std::uint64_t>(remaining)));

			std::lock_guard stream_lock(adapter->music->stream_mutex);
			auto& stream = *adapter->music->stream;
			stream.clear();
			stream.seekg(
				adapter->music->stream_origin + adapter->position,
				std::ios::beg);
			if (!stream)
			{
				*status = SDL_IO_STATUS_ERROR;
				return 0;
			}

			stream.read(static_cast<char*>(buffer), amount);
			const auto bytes_read = stream.gcount();
			adapter->position += static_cast<Sint64>(bytes_read);
			if (bytes_read != amount)
			{
				*status = stream.eof() ? SDL_IO_STATUS_EOF : SDL_IO_STATUS_ERROR;
			}
			return static_cast<size_t>(bytes_read);
		}
		catch (...)
		{
			*status = SDL_IO_STATUS_ERROR;
			return 0;
		}
	}

	inline bool SDLCALL music_io_close(void* userdata)
	{
		auto* adapter = static_cast<MusicIOAdapter*>(userdata);
		if (adapter && adapter->music)
		{
			restore_music_stream(*adapter->music);
		}
		delete adapter;
		return true;
	}

	inline size_t SDLCALL music_io_write(
		void*,
		const void*,
		size_t,
		SDL_IOStatus* status)
	{
		if (status)
		{
			*status = SDL_IO_STATUS_READONLY;
		}
		return 0;
	}

	inline bool SDLCALL music_io_flush(void*, SDL_IOStatus* status)
	{
		if (status)
		{
			*status = SDL_IO_STATUS_READY;
		}
		return true;
	}

	inline std::expected<SDL_IOStream*, AudioError> open_music_stream(
		const std::shared_ptr<MusicResource>& music,
		AudioErrorCode error_code = AudioErrorCode::music_playback)
	{
		if (!music->stream)
		{
			SDL_IOStream* stream = SDL_IOFromFile(music->path.c_str(), "rb");
			if (!stream)
			{
				return std::unexpected(AudioError{
					error_code,
					sdl_error_message("Failed to open music file '" + music->path + "'")
				});
			}
			return stream;
		}

		std::unique_ptr<MusicIOAdapter> adapter(
			new (std::nothrow) MusicIOAdapter());
		if (!adapter)
		{
			return std::unexpected(AudioError{
				error_code,
				"Failed to allocate C++ music stream adapter"
			});
		}
		adapter->music = music;

		SDL_IOStreamInterface io_interface;
		SDL_INIT_INTERFACE(&io_interface);
		io_interface.size = music_io_size;
		io_interface.seek = music_io_seek;
		io_interface.read = music_io_read;
		io_interface.write = music_io_write;
		io_interface.flush = music_io_flush;
		io_interface.close = music_io_close;

		SDL_IOStream* stream = SDL_OpenIO(&io_interface, adapter.get());
		if (!stream)
		{
			return std::unexpected(AudioError{
				error_code,
				sdl_error_message("Failed to create SDL stream for C++ input stream")
			});
		}
		adapter.release();
		return stream;
	}

	inline std::expected<void, AudioError> inspect_music_stream(MusicResource& music)
	{
		if (!music.stream)
		{
			return {};
		}

		std::lock_guard lock(music.stream_mutex);
		auto& stream = *music.stream;
		const auto previous_state = stream.rdstate();
		music.stream_initial_state = previous_state;
		std::streampos start = std::streampos(-1);
		bool has_start = false;
		auto restore = [&stream, previous_state, &start, &has_start]() noexcept
		{
			try
			{
				stream.clear();
				if (has_start)
				{
					stream.seekg(start);
				}
				stream.clear(previous_state);
			}
			catch (...)
			{
			}
		};

		try
		{
			stream.clear();
			start = stream.tellg();
			if (start == std::streampos(-1))
			{
				restore();
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Music input stream must support seeking"
				});
			}
			has_start = true;

			stream.seekg(0, std::ios::end);
			const auto end = stream.tellg();
			if (!stream || end == std::streampos(-1) || end < start)
			{
				restore();
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Music input stream must be seekable and have a valid length"
				});
			}

			music.stream_origin = static_cast<std::streamoff>(start);
			music.stream_length = static_cast<std::streamoff>(end - start);
			restore();
			if (music.stream_length <= 0)
			{
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Music input stream is empty"
				});
			}
			return {};
		}
		catch (...)
		{
			restore();
			return std::unexpected(AudioError{
				AudioErrorCode::music_load,
				"Failed to inspect music input stream"
			});
		}
	}

	class Engine
	{
	public:
		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;

		Engine(Engine&& other) noexcept
			: mixer(std::exchange(other.mixer, nullptr)),
			  mixer_initialized(std::exchange(other.mixer_initialized, false)),
			  sound_resources(std::move(other.sound_resources)),
			  sound_registration_count(std::exchange(other.sound_registration_count, 0)),
			  voices(std::move(other.voices)),
			  next_voice_id(std::exchange(other.next_voice_id, 0)),
			  master_volume_level(std::exchange(other.master_volume_level, 1.0f)),
			  music_volume_level(std::exchange(other.music_volume_level, 1.0f)),
			  sfx_volume_level(std::exchange(other.sfx_volume_level, 1.0f))
		{
		}

		Engine& operator=(Engine&&) = delete;

		~Engine() noexcept
		{
			shutdown();
		}

		static std::expected<Engine, AudioError> create()
		{
			auto& sdl_runtime = ::__SDL_While_Runtime__::instance();
			(void)sdl_runtime;

			if (::__SDL_While_Runtime__::error())
			{
				return std::unexpected(AudioError{
					AudioErrorCode::sdl_runtime,
					::__SDL_While_Runtime__::content()
				});
			}

			if (!::__SDL_While_Runtime__::running())
			{
				return std::unexpected(AudioError{
					AudioErrorCode::sdl_runtime,
					"SDL runtime is not running"
				});
			}

			if (!MIX_Init())
			{
				return std::unexpected(AudioError{
					AudioErrorCode::mixer_initialization,
					sdl_error_message("SDL3_mixer initialization failed")
				});
			}

			MIX_Mixer* mixer = MIX_CreateMixerDevice(
				SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
				nullptr);
			if (!mixer)
			{
				const std::string message = sdl_error_message(
					"Opening the default playback device failed");
				MIX_Quit();
				return std::unexpected(AudioError{
					AudioErrorCode::playback_device,
					message
				});
			}

			return Engine(mixer);
		}

		MIX_Mixer* native_mixer() const noexcept
		{
			return mixer;
		}

		void register_sound(const std::shared_ptr<SoundResource>& sound)
		{
			if ((++sound_registration_count % 64) == 0)
			{
				std::erase_if(sound_resources, [](const auto& weak_sound)
				{
					return weak_sound.expired();
				});
			}
			sound_resources.emplace_back(sound);
		}

		std::expected<std::uint64_t, AudioError> play_sound(
			MIX_Audio* audio,
			PlayOptions options)
		{
			reap_finished_voices();

			if (!audio)
			{
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Cannot play an empty or released Sound"
				});
			}

			if (next_voice_id == 0)
			{
				return std::unexpected(AudioError{
					AudioErrorCode::voice_id_exhausted,
					"No more unique voice handles are available"
				});
			}

			if (!std::isfinite(options.volume))
			{
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Voice volume must be a finite number"
				});
			}

			const char* bus_tag = nullptr;
			switch (options.bus)
			{
			case Bus::sfx:
				bus_tag = "mngl.audio.bus.sfx";
				break;
			case Bus::music:
				bus_tag = "mngl.audio.bus.music";
				break;
			default:
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Unknown audio bus"
				});
			}

			// Reserve before acquiring native objects so recording the voice cannot
			// throw after playback has already started.
			try
			{
				voices.reserve(voices.size() + 1);
			}
			catch (const std::bad_alloc&)
			{
				return std::unexpected(AudioError{
					AudioErrorCode::sound_playback,
					"Failed to allocate voice tracking state"
				});
			}

			std::unique_ptr<MIX_Track, decltype(&MIX_DestroyTrack)> track(
				MIX_CreateTrack(mixer),
				MIX_DestroyTrack);
			if (!track)
			{
				return std::unexpected(AudioError{
					AudioErrorCode::sound_playback,
					sdl_error_message("Failed to create a playback track")
				});
			}

			auto fail_track = [](AudioErrorCode code, std::string message)
				-> std::expected<std::uint64_t, AudioError>
			{
				return std::unexpected(AudioError{ code, std::move(message) });
			};

			if (!MIX_SetTrackAudio(track.get(), audio))
			{
				return fail_track(AudioErrorCode::sound_playback,
					sdl_error_message("Failed to assign sound to playback track"));
			}

			const float volume = std::clamp(options.volume, 0.0f, 1.0f);
			const float bus_volume = options.bus == Bus::sfx
				? sfx_volume_level
				: music_volume_level;
			if (!MIX_SetTrackGain(track.get(), volume * bus_volume))
			{
				return fail_track(AudioErrorCode::sound_playback,
					sdl_error_message("Failed to set voice volume"));
			}

			if (!MIX_TagTrack(track.get(), bus_tag))
			{
				return fail_track(AudioErrorCode::sound_playback,
					sdl_error_message("Failed to assign voice to audio bus"));
			}

			PropertiesGuard play_properties;
			if (options.loop)
			{
				play_properties.reset(SDL_CreateProperties());
				if (!play_properties)
				{
					return fail_track(AudioErrorCode::sound_playback,
						sdl_error_message("Failed to create playback options"));
				}

				if (!SDL_SetNumberProperty(
					play_properties.get(),
					MIX_PROP_PLAY_LOOPS_NUMBER,
					-1))
				{
					return fail_track(
						AudioErrorCode::sound_playback,
						sdl_error_message("Failed to configure looping playback"));
				}
			}

			const std::uint64_t id = next_voice_id;
			if (!MIX_PlayTrack(track.get(), play_properties.get()))
			{
				return fail_track(
					AudioErrorCode::sound_playback,
					sdl_error_message("Failed to start sound playback"));
			}

			voices.push_back(Voice{ id, track.release(), options.bus, volume });
			next_voice_id = id == std::numeric_limits<std::uint64_t>::max()
				? 0
				: id + 1;
			return id;
		}

		std::expected<std::uint64_t, AudioError> play_music(
			const std::shared_ptr<MusicResource>& music,
			PlayOptions options)
		{
			reap_finished_voices();
			if (!music)
			{
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Cannot play an empty Music resource"
				});
			}

			if (next_voice_id == 0)
			{
				return std::unexpected(AudioError{
					AudioErrorCode::voice_id_exhausted,
					"No more unique voice handles are available"
				});
			}

			if (!std::isfinite(options.volume))
			{
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Voice volume must be a finite number"
				});
			}

			const char* bus_tag = nullptr;
			switch (options.bus)
			{
			case Bus::sfx:
				bus_tag = "mngl.audio.bus.sfx";
				break;
			case Bus::music:
				bus_tag = "mngl.audio.bus.music";
				break;
			default:
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Unknown audio bus"
				});
			}

			try
			{
				voices.reserve(voices.size() + 1);
			}
			catch (const std::bad_alloc&)
			{
				return std::unexpected(AudioError{
					AudioErrorCode::music_playback,
					"Failed to allocate voice tracking state"
				});
			}

			auto music_stream = open_music_stream(music);
			if (!music_stream)
			{
				return std::unexpected(std::move(music_stream.error()));
			}
			std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(
				*music_stream,
				SDL_CloseIO);

			std::unique_ptr<MIX_Track, decltype(&MIX_DestroyTrack)> track(
				MIX_CreateTrack(mixer),
				MIX_DestroyTrack);
			if (!track)
			{
				return std::unexpected(AudioError{
					AudioErrorCode::music_playback,
					sdl_error_message("Failed to create a music track")
				});
			}

			if (!MIX_SetTrackIOStream(track.get(), stream.release(), true))
			{
				return std::unexpected(AudioError{
					AudioErrorCode::music_playback,
					sdl_error_message("Failed to assign music stream to track")
				});
			}

			const float volume = std::clamp(options.volume, 0.0f, 1.0f);
			const float bus_volume = options.bus == Bus::sfx
				? sfx_volume_level
				: music_volume_level;
			if (!MIX_SetTrackGain(track.get(), volume * bus_volume))
			{
				return std::unexpected(AudioError{
					AudioErrorCode::volume_control,
					sdl_error_message("Failed to set music voice volume")
				});
			}

			if (!MIX_TagTrack(track.get(), bus_tag))
			{
				return std::unexpected(AudioError{
					AudioErrorCode::music_playback,
					sdl_error_message("Failed to assign music voice to audio bus")
				});
			}

			PropertiesGuard play_properties;
			if (options.loop)
			{
				play_properties.reset(SDL_CreateProperties());
				if (!play_properties)
				{
					return std::unexpected(AudioError{
						AudioErrorCode::music_playback,
						sdl_error_message("Failed to create music playback options")
					});
				}

				if (!SDL_SetNumberProperty(
					play_properties.get(),
					MIX_PROP_PLAY_LOOPS_NUMBER,
					-1))
				{
					return std::unexpected(AudioError{
						AudioErrorCode::music_playback,
						sdl_error_message("Failed to configure looping music")
					});
				}
			}

			const std::uint64_t id = next_voice_id;
			if (!MIX_PlayTrack(track.get(), play_properties.get()))
			{
				return std::unexpected(AudioError{
					AudioErrorCode::music_playback,
					sdl_error_message("Failed to start music playback")
				});
			}

			voices.push_back(Voice{ id, track.release(), options.bus, volume });
			next_voice_id = id == std::numeric_limits<std::uint64_t>::max()
				? 0
				: id + 1;
			return id;
		}

		bool voice_is_playing(std::uint64_t id)
		{
			reap_finished_voices();
			Voice* voice = find_voice(id);
			return voice && MIX_TrackPlaying(voice->track);
		}

		bool voice_is_paused(std::uint64_t id)
		{
			reap_finished_voices();
			Voice* voice = find_voice(id);
			return voice && MIX_TrackPaused(voice->track);
		}

		std::expected<void, AudioError> set_master_volume(float volume)
		{
			if (!std::isfinite(volume))
			{
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Master volume must be a finite number"
				});
			}

			const float clamped_volume = std::clamp(volume, 0.0f, 1.0f);
			if (!MIX_SetMixerGain(mixer, clamped_volume))
			{
				return std::unexpected(AudioError{
					AudioErrorCode::volume_control,
					sdl_error_message("Failed to set master volume")
				});
			}

			master_volume_level = clamped_volume;
			return {};
		}

		float master_volume() const noexcept
		{
			return master_volume_level;
		}

		std::expected<void, AudioError> set_bus_volume(Bus bus, float volume)
		{
			if (!std::isfinite(volume))
			{
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Bus volume must be a finite number"
				});
			}

			float* current_volume = nullptr;
			switch (bus)
			{
			case Bus::sfx:
				current_volume = &sfx_volume_level;
				break;
			case Bus::music:
				current_volume = &music_volume_level;
				break;
			default:
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Unknown audio bus"
				});
			}

			const float previous_volume = *current_volume;
			const float clamped_volume = std::clamp(volume, 0.0f, 1.0f);
			if (previous_volume == clamped_volume)
			{
				return {};
			}

			reap_finished_voices();
			for (auto& voice : voices)
			{
				if (voice.bus == bus && !MIX_SetTrackGain(
					voice.track,
					voice.base_volume * clamped_volume))
				{
					const std::string message = sdl_error_message(
						"Failed to set bus volume");
					for (auto& rollback_voice : voices)
					{
						if (rollback_voice.bus == bus)
						{
							MIX_SetTrackGain(
								rollback_voice.track,
								rollback_voice.base_volume * previous_volume);
						}
					}
					return std::unexpected(AudioError{
						AudioErrorCode::volume_control,
						message
					});
				}
			}

			*current_volume = clamped_volume;
			return {};
		}

		std::expected<float, AudioError> bus_volume(Bus bus) const
		{
			switch (bus)
			{
			case Bus::sfx:
				return sfx_volume_level;
			case Bus::music:
				return music_volume_level;
			default:
				return std::unexpected(AudioError{
					AudioErrorCode::invalid_argument,
					"Unknown audio bus"
				});
			}
		}

		std::expected<void, AudioError> pause_voice(std::uint64_t id)
		{
			reap_finished_voices();
			Voice* voice = find_voice(id);
			if (!voice)
			{
				return std::unexpected(detail::inactive_voice_error());
			}

			if (!MIX_PauseTrack(voice->track))
			{
				return std::unexpected(AudioError{
					AudioErrorCode::sound_playback,
					sdl_error_message("Failed to pause voice")
				});
			}
			return {};
		}

		std::expected<void, AudioError> resume_voice(std::uint64_t id)
		{
			reap_finished_voices();
			Voice* voice = find_voice(id);
			if (!voice)
			{
				return std::unexpected(detail::inactive_voice_error());
			}

			if (!MIX_ResumeTrack(voice->track))
			{
				return std::unexpected(AudioError{
					AudioErrorCode::sound_playback,
					sdl_error_message("Failed to resume voice")
				});
			}
			return {};
		}

		std::expected<void, AudioError> stop_voice(std::uint64_t id)
		{
			reap_finished_voices();
			auto voice = find_voice_iterator(id);
			if (voice == voices.end())
			{
				return std::unexpected(detail::inactive_voice_error());
			}

			if (!MIX_StopTrack(voice->track, 0))
			{
				const std::string message = sdl_error_message("Failed to stop voice");
				MIX_DestroyTrack(voice->track);
				voices.erase(voice);
				return std::unexpected(AudioError{
					AudioErrorCode::sound_playback,
					message
				});
			}

			MIX_DestroyTrack(voice->track);
			voices.erase(voice);
			return {};
		}

	private:
		explicit Engine(MIX_Mixer* mixer) noexcept
			: mixer(mixer), mixer_initialized(true)
		{
		}

		void shutdown() noexcept
		{
			if (mixer)
			{
				MIX_DestroyMixer(std::exchange(mixer, nullptr));
			}
			voices.clear();

			// Stop active tracks first, then drop MNGL's references to every Sound.
			// This leaves wrappers harmless if static objects outlive the audio slot.
			for (auto& weak_sound : sound_resources)
			{
				if (auto sound = weak_sound.lock())
				{
					sound->release();
				}
			}
			sound_resources.clear();

			if (mixer_initialized)
			{
				MIX_Quit();
				mixer_initialized = false;
			}
		}

		static std::string sdl_error_message(const char* prefix)
		{
			const char* error = SDL_GetError();
			if (!error || !*error)
			{
				return prefix;
			}
			return std::string(prefix) + ": " + error;
		}

		Voice* find_voice(std::uint64_t id)
		{
			for (auto& voice : voices)
			{
				if (voice.id == id)
				{
					return &voice;
				}
			}
			return nullptr;
		}

		auto find_voice_iterator(std::uint64_t id)
		{
			return std::find_if(voices.begin(), voices.end(), [id](const Voice& voice)
			{
				return voice.id == id;
			});
		}

		void reap_finished_voices()
		{
			for (auto voice = voices.begin(); voice != voices.end();)
			{
				if (!MIX_TrackPlaying(voice->track) && !MIX_TrackPaused(voice->track))
				{
					MIX_DestroyTrack(voice->track);
					voice = voices.erase(voice);
				}
				else
				{
					++voice;
				}
			}
		}

		MIX_Mixer* mixer = nullptr;
		bool mixer_initialized = false;
		std::vector<std::weak_ptr<SoundResource>> sound_resources;
		std::size_t sound_registration_count = 0;
		std::vector<Voice> voices;
		std::uint64_t next_voice_id = 1;
		float master_volume_level = 1.0f;
		float music_volume_level = 1.0f;
		float sfx_volume_level = 1.0f;
	};

	struct EngineSlot
	{
		std::optional<Engine> engine;
	};

	inline EngineSlot& engine_slot()
	{
		static EngineSlot slot;
		return slot;
	}

	// Force construction of SDL's runtime singleton before the audio slot. This
	// registers the audio slot for destruction first, so its Engine is destroyed
	// before SDL_Quit() runs at process shutdown.
	inline std::expected<std::reference_wrapper<Engine>, AudioError> ensure_engine()
	{
		auto& sdl_runtime = ::__SDL_While_Runtime__::instance();
		(void)sdl_runtime;

		if (::__SDL_While_Runtime__::error())
		{
			return std::unexpected(AudioError{
				AudioErrorCode::sdl_runtime,
				::__SDL_While_Runtime__::content()
			});
		}

		auto& slot = engine_slot();
		if (!slot.engine)
		{
			auto created = Engine::create();
			if (!created)
			{
				return std::unexpected(std::move(created.error()));
			}
			slot.engine.emplace(std::move(*created));
		}

		return std::ref(*slot.engine);
	}

	inline std::string sdl_error_message(std::string_view prefix)
	{
		const char* error = SDL_GetError();
		if (!error || !*error)
		{
			return std::string(prefix);
		}
		return std::string(prefix) + ": " + error;
	}

	inline std::expected<void, AudioError> validate_music_resource(
		const std::shared_ptr<MusicResource>& music)
	{
		auto io = open_music_stream(music, AudioErrorCode::music_load);
		if (!io)
		{
			return std::unexpected(std::move(io.error()));
		}
		std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(
			*io,
			SDL_CloseIO);

		// Keep ownership here so the stream is closed on every decoder failure
		// path as well as after successful validation.
		std::unique_ptr<MIX_AudioDecoder, decltype(&MIX_DestroyAudioDecoder)> decoder(
			MIX_CreateAudioDecoder_IO(stream.get(), false, 0),
			MIX_DestroyAudioDecoder);
		if (!decoder)
		{
			return std::unexpected(AudioError{
				AudioErrorCode::music_load,
				sdl_error_message("Unsupported or unreadable music source")
			});
		}
		return {};
	}
}

namespace mngl::audio
{
	inline std::expected<Music, AudioError> load_music(std::string_view path)
	{
		if (path.empty() || path.find('\0') != std::string_view::npos)
		{
			return std::unexpected(AudioError{
				AudioErrorCode::invalid_argument,
				"Music path must be non-empty and cannot contain a null byte"
			});
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}

		try
		{
			auto resource = std::make_shared<detail::MusicResource>();
			resource->path.assign(path);
			auto validation = detail::validate_music_resource(resource);
			if (!validation)
			{
				return std::unexpected(std::move(validation.error()));
			}
			return Music(std::move(resource));
		}
		catch (const std::bad_alloc&)
		{
			return std::unexpected(AudioError{
				AudioErrorCode::music_load,
				"Failed to allocate music resource state"
			});
		}
	}

	inline std::expected<Music, AudioError> load_music(std::istream& stream)
	{
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}

		try
		{
			auto resource = std::make_shared<detail::MusicResource>();
			resource->stream = &stream;
			auto inspected = detail::inspect_music_stream(*resource);
			if (!inspected)
			{
				return std::unexpected(std::move(inspected.error()));
			}

			auto validation = detail::validate_music_resource(resource);
			if (!validation)
			{
				return std::unexpected(std::move(validation.error()));
			}
			return Music(std::move(resource));
		}
		catch (const std::bad_alloc&)
		{
			return std::unexpected(AudioError{
				AudioErrorCode::music_load,
				"Failed to allocate music resource state"
			});
		}
	}

	inline std::expected<Music, AudioError> load_music(
		std::shared_ptr<std::istream> stream)
	{
		if (!stream)
		{
			return std::unexpected(AudioError{
				AudioErrorCode::invalid_argument,
				"Music input stream cannot be null"
			});
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}

		try
		{
			auto resource = std::make_shared<detail::MusicResource>();
			resource->stream = stream.get();
			resource->owned_stream = std::move(stream);
			auto inspected = detail::inspect_music_stream(*resource);
			if (!inspected)
			{
				return std::unexpected(std::move(inspected.error()));
			}

			auto validation = detail::validate_music_resource(resource);
			if (!validation)
			{
				return std::unexpected(std::move(validation.error()));
			}
			return Music(std::move(resource));
		}
		catch (const std::bad_alloc&)
		{
			return std::unexpected(AudioError{
				AudioErrorCode::music_load,
				"Failed to allocate music resource state"
			});
		}
	}

	inline std::expected<Sound, AudioError> load_sound(std::string_view path)
	{
		if (path.empty() || path.find('\0') != std::string_view::npos)
		{
			return std::unexpected(AudioError{
				AudioErrorCode::invalid_argument,
				"Sound path must be non-empty and cannot contain a null byte"
			});
		}

		const std::string terminated_path(path);
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}

		MIX_Mixer* mixer = engine->get().native_mixer();
		MIX_Audio* audio = MIX_LoadAudio(mixer, terminated_path.c_str(), true);
		if (!audio)
		{
			return std::unexpected(AudioError{
				AudioErrorCode::sound_load,
				detail::sdl_error_message("Failed to load sound '" + terminated_path + "'")
			});
		}

		std::unique_ptr<MIX_Audio, decltype(&MIX_DestroyAudio)> audio_guard(
			audio,
			MIX_DestroyAudio);
		try
		{
			auto resource = std::make_shared<detail::SoundResource>(audio);
			audio_guard.release();
			engine->get().register_sound(resource);
			return Sound(std::move(resource));
		}
		catch (const std::bad_alloc&)
		{
			return std::unexpected(AudioError{
				AudioErrorCode::sound_load,
				"Failed to allocate sound resource state"
			});
		}
	}

	inline std::expected<VoiceHandle, AudioError> play(
		const Sound& sound,
		PlayOptions options)
	{
		if (!sound.resource_ || !sound.resource_->native_audio())
		{
			return std::unexpected(AudioError{
				AudioErrorCode::invalid_argument,
				"Cannot play an empty or released Sound"
			});
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}

		auto voice_id = engine->get().play_sound(
			sound.resource_->native_audio(),
			options);
		if (!voice_id)
		{
			return std::unexpected(std::move(voice_id.error()));
		}

		return VoiceHandle(*voice_id);
	}

	inline std::expected<VoiceHandle, AudioError> play(const Music& music)
	{
		PlayOptions options;
		options.bus = Bus::music;
		return play(music, options);
	}

	inline std::expected<VoiceHandle, AudioError> play(
		const Music& music,
		PlayOptions options)
	{
		if (!music.resource_)
		{
			return std::unexpected(AudioError{
				AudioErrorCode::invalid_argument,
				"Cannot play an empty Music resource"
			});
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}

		auto voice_id = engine->get().play_music(music.resource_, options);
		if (!voice_id)
		{
			return std::unexpected(std::move(voice_id.error()));
		}
		return VoiceHandle(*voice_id);
	}

	inline std::expected<bool, AudioError> is_playing(VoiceHandle voice)
	{
		if (voice.id_ == 0)
		{
			return false;
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().voice_is_playing(voice.id_);
	}

	inline std::expected<bool, AudioError> is_paused(VoiceHandle voice)
	{
		if (voice.id_ == 0)
		{
			return false;
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().voice_is_paused(voice.id_);
	}

	inline std::expected<void, AudioError> pause(VoiceHandle voice)
	{
		if (voice.id_ == 0)
		{
			return std::unexpected(detail::inactive_voice_error());
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().pause_voice(voice.id_);
	}

	inline std::expected<void, AudioError> resume(VoiceHandle voice)
	{
		if (voice.id_ == 0)
		{
			return std::unexpected(detail::inactive_voice_error());
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().resume_voice(voice.id_);
	}

	inline std::expected<void, AudioError> stop(VoiceHandle voice)
	{
		if (voice.id_ == 0)
		{
			return std::unexpected(detail::inactive_voice_error());
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().stop_voice(voice.id_);
	}

	inline std::expected<void, AudioError> set_master_volume(float volume)
	{
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().set_master_volume(volume);
	}

	inline std::expected<float, AudioError> master_volume()
	{
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().master_volume();
	}

	inline std::expected<void, AudioError> set_bus_volume(Bus bus, float volume)
	{
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().set_bus_volume(bus, volume);
	}

	inline std::expected<float, AudioError> bus_volume(Bus bus)
	{
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().bus_volume(bus);
	}
}

#endif
