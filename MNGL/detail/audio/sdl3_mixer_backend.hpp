#pragma once

#if !defined(MNGL_CUSTOM_AUDIO_BACKEND) && !defined(MNGL_CUSTOM_AUDIO_INTERFACE) &&                          \
    !defined(MNGL_CUSTOM_MIXER_INTERFACE)

#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <atomic>
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
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../runtime.hpp"

namespace mngl::audio::detail
{
	inline std::string sdl_error_message(std::string_view prefix);

	class capture_resource
	{
	public:
		explicit capture_resource(SDL_AudioStream* stream) noexcept : stream_(stream) {}
		capture_resource(const capture_resource&) = delete;
		capture_resource& operator=(const capture_resource&) = delete;
		~capture_resource() { release(); }

		SDL_AudioStream* native_stream() const noexcept { return stream_; }
		void release() noexcept
		{
			if (stream_)
			{
				SDL_DestroyAudioStream(std::exchange(stream_, nullptr));
			}
		}

	private:
		SDL_AudioStream* stream_ = nullptr;
	};

	class audio_stream_resource
	{
	public:
		explicit audio_stream_resource(SDL_AudioStream* stream) noexcept : stream_(stream) {}
		audio_stream_resource(const audio_stream_resource&) = delete;
		audio_stream_resource& operator=(const audio_stream_resource&) = delete;
		~audio_stream_resource() { release(); }

		SDL_AudioStream* native_stream() const noexcept { return stream_; }
		void release() noexcept
		{
			if (stream_)
			{
				SDL_DestroyAudioStream(std::exchange(stream_, nullptr));
			}
			attached.store(false, std::memory_order_release);
		}

		std::atomic_bool attached = false;

	private:
		SDL_AudioStream* stream_ = nullptr;
	};

	struct recording_device_access
	{
		static recording_device make(SDL_AudioDeviceID id, std::string name)
		{
			return recording_device(id, std::move(name));
		}
		static SDL_AudioDeviceID id(const recording_device& device) noexcept
		{
			return static_cast<SDL_AudioDeviceID>(device.id_);
		}
	};

	inline error inactive_voice_error()
	{
		return error{error_code::voice_inactive, "voice handle is no longer active"};
	}

	struct voice
	{
		std::uint64_t id;
		MIX_Track* track;
		mngl::audio::bus bus;
		float base_volume;
		std::shared_ptr<audio_stream_resource> stream;
	};

	class properties_guard
	{
	public:
		properties_guard() noexcept = default;
		properties_guard(const properties_guard&) = delete;
		properties_guard& operator=(const properties_guard&) = delete;

		~properties_guard()
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

		SDL_PropertiesID get() const noexcept { return id; }

		explicit operator bool() const noexcept { return id != 0; }

	private:
		SDL_PropertiesID id = 0;
	};

	class sound_resource
	{
	public:
		explicit sound_resource(MIX_Audio* audio) noexcept : audio(audio) {}

		sound_resource(const sound_resource&) = delete;
		sound_resource& operator=(const sound_resource&) = delete;

		~sound_resource() { release(); }

		MIX_Audio* native_audio() const noexcept { return audio; }

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

	struct music_resource
	{
		std::string path;
		std::istream* stream = nullptr;
		std::shared_ptr<std::istream> owned_stream;
		std::mutex stream_mutex;
		std::streamoff stream_origin = 0;
		std::streamoff stream_length = 0;
		std::ios::iostate stream_initial_state = std::ios::goodbit;
	};

	struct music_io_adapter
	{
		std::shared_ptr<music_resource> music;
		Sint64 position = 0;
		std::mutex position_mutex;
	};

	inline void restore_music_stream(music_resource& music) noexcept
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
		const auto* adapter = static_cast<const music_io_adapter*>(userdata);
		return adapter ? static_cast<Sint64>(adapter->music->stream_length) : -1;
	}

	inline Sint64 SDLCALL music_io_seek(void* userdata, Sint64 offset, SDL_IOWhence whence)
	{
		auto* adapter = static_cast<music_io_adapter*>(userdata);
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

	inline std::size_t SDLCALL music_io_read(void* userdata, void* buffer, std::size_t size,
	                                         SDL_IOStatus* status)
	{
		if (status)
		{
			*status = SDL_IO_STATUS_READY;
		}
		auto* adapter = static_cast<music_io_adapter*>(userdata);
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

			const auto stream_limit = static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max());
			const auto requested = std::min<std::uint64_t>(static_cast<std::uint64_t>(size), stream_limit);
			const auto amount = static_cast<std::streamsize>(
			    std::min<std::uint64_t>(requested, static_cast<std::uint64_t>(remaining)));

			std::lock_guard stream_lock(adapter->music->stream_mutex);
			auto& stream = *adapter->music->stream;
			stream.clear();
			stream.seekg(adapter->music->stream_origin + adapter->position, std::ios::beg);
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
			return static_cast<std::size_t>(bytes_read);
		}
		catch (...)
		{
			*status = SDL_IO_STATUS_ERROR;
			return 0;
		}
	}

	inline bool SDLCALL music_io_close(void* userdata)
	{
		auto* adapter = static_cast<music_io_adapter*>(userdata);
		if (adapter && adapter->music)
		{
			restore_music_stream(*adapter->music);
		}
		delete adapter;
		return true;
	}

	inline std::size_t SDLCALL music_io_write(void*, const void*, std::size_t, SDL_IOStatus* status)
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

	inline std::expected<SDL_IOStream*, error> open_music_stream(
	    const std::shared_ptr<music_resource>& music, error_code error_code = error_code::music_playback)
	{
		if (!music->stream)
		{
			SDL_IOStream* stream = SDL_IOFromFile(music->path.c_str(), "rb");
			if (!stream)
			{
				return std::unexpected(
				    error{error_code, sdl_error_message("Failed to open music file '" + music->path + "'")});
			}
			return stream;
		}

		std::unique_ptr<music_io_adapter> adapter(new (std::nothrow) music_io_adapter());
		if (!adapter)
		{
			return std::unexpected(error{error_code, "Failed to allocate C++ music stream adapter"});
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
			return std::unexpected(
			    error{error_code, sdl_error_message("Failed to create SDL stream for C++ input stream")});
		}
		adapter.release();
		return stream;
	}

	inline std::expected<void, error> inspect_music_stream(music_resource& music)
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
				return std::unexpected(
				    error{error_code::invalid_argument, "music input stream must support seeking"});
			}
			has_start = true;

			stream.seekg(0, std::ios::end);
			const auto end = stream.tellg();
			if (!stream || end == std::streampos(-1) || end < start)
			{
				restore();
				return std::unexpected(error{error_code::invalid_argument,
				                             "music input stream must be seekable and have a valid length"});
			}

			music.stream_origin = static_cast<std::streamoff>(start);
			music.stream_length = static_cast<std::streamoff>(end - start);
			restore();
			if (music.stream_length <= 0)
			{
				return std::unexpected(error{error_code::invalid_argument, "music input stream is empty"});
			}
			return {};
		}
		catch (...)
		{
			restore();
			return std::unexpected(error{error_code::music_load, "Failed to inspect music input stream"});
		}
	}

	class engine
	{
	public:
		engine(const engine&) = delete;
		engine& operator=(const engine&) = delete;

		engine(engine&& other) noexcept
		    : mixer(std::exchange(other.mixer, nullptr)),
		      mixer_initialized(std::exchange(other.mixer_initialized, false)),
		      sound_resources(std::move(other.sound_resources)),
		      sound_registration_count(std::exchange(other.sound_registration_count, 0)),
		      audio_stream_resources(std::move(other.audio_stream_resources)),
		      audio_stream_registration_count(std::exchange(other.audio_stream_registration_count, 0)),
		      voices(std::move(other.voices)), next_voice_id(std::exchange(other.next_voice_id, 0)),
		      master_volume_level(std::exchange(other.master_volume_level, 1.0f)),
		      music_volume_level(std::exchange(other.music_volume_level, 1.0f)),
		      sfx_volume_level(std::exchange(other.sfx_volume_level, 1.0f))
		{
		}

		engine& operator=(engine&&) = delete;

		~engine() noexcept { shutdown(); }

		static std::expected<engine, error> create()
		{
			auto& sdl_runtime = ::mngl::runtime::instance();
			(void)sdl_runtime;

			if (::mngl::runtime::error())
			{
				return std::unexpected(error{error_code::sdl_runtime, ::mngl::runtime::content()});
			}

			if (!::mngl::runtime::running())
			{
				return std::unexpected(error{error_code::sdl_runtime, "SDL runtime is not running"});
			}

			if (!MIX_Init())
			{
				return std::unexpected(error{error_code::mixer_initialization,
				                             sdl_error_message("SDL3_mixer initialization failed")});
			}

			MIX_Mixer* mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
			if (!mixer)
			{
				const std::string message = sdl_error_message("Opening the default playback device failed");
				MIX_Quit();
				return std::unexpected(error{error_code::playback_device, message});
			}

			return engine(mixer);
		}

		MIX_Mixer* native_mixer() const noexcept { return mixer; }

		void register_sound(const std::shared_ptr<sound_resource>& sound)
		{
			if ((++sound_registration_count % 64) == 0)
			{
				std::erase_if(sound_resources, [](const auto& weak_sound) { return weak_sound.expired(); });
			}
			sound_resources.emplace_back(sound);
		}

		void register_audio_stream(const std::shared_ptr<audio_stream_resource>& stream)
		{
			if ((++audio_stream_registration_count % 64) == 0)
			{
				std::erase_if(audio_stream_resources, [](const auto& weak_stream) { return weak_stream.expired(); });
			}
			audio_stream_resources.emplace_back(stream);
		}

		std::expected<std::uint64_t, error> play_sound(MIX_Audio* audio, play_options options)
		{
			reap_finished_voices();

			if (!audio)
			{
				return std::unexpected(
				    error{error_code::invalid_argument, "Cannot play an empty or released sound"});
			}

			if (next_voice_id == 0)
			{
				return std::unexpected(
				    error{error_code::voice_id_exhausted, "No more unique voice handles are available"});
			}

			if (!std::isfinite(options.volume))
			{
				return std::unexpected(
				    error{error_code::invalid_argument, "voice volume must be a finite number"});
			}
			if (options.position &&
			    (!std::isfinite(options.position->x) || !std::isfinite(options.position->y) ||
			     !std::isfinite(options.position->z)))
			{
				return std::unexpected(error{error_code::invalid_argument, "3D position must contain finite values"});
			}

			const char* bus_tag = nullptr;
			switch (options.bus)
			{
			case bus::sfx:
				bus_tag = "mngl.audio.bus.sfx";
				break;
			case bus::music:
				bus_tag = "mngl.audio.bus.music";
				break;
			default:
				return std::unexpected(error{error_code::invalid_argument, "Unknown audio bus"});
			}

			// Reserve before acquiring native objects so recording the voice cannot
			// throw after playback has already started.
			try
			{
				voices.reserve(voices.size() + 1);
			}
			catch (const std::bad_alloc&)
			{
				return std::unexpected(
				    error{error_code::sound_playback, "Failed to allocate voice tracking state"});
			}

			std::unique_ptr<MIX_Track, decltype(&MIX_DestroyTrack)> track(MIX_CreateTrack(mixer),
			                                                              MIX_DestroyTrack);
			if (!track)
			{
				return std::unexpected(error{error_code::sound_playback,
				                             sdl_error_message("Failed to create a playback track")});
			}

			auto fail_track = [](error_code code, std::string message) -> std::expected<std::uint64_t, error>
			{ return std::unexpected(error{code, std::move(message)}); };

			if (!MIX_SetTrackAudio(track.get(), audio))
			{
				return fail_track(error_code::sound_playback,
				                  sdl_error_message("Failed to assign sound to playback track"));
			}

			const float volume = std::clamp(options.volume, 0.0f, 1.0f);
			const float bus_volume = options.bus == bus::sfx ? sfx_volume_level : music_volume_level;
			if (!MIX_SetTrackGain(track.get(), volume * bus_volume))
			{
				return fail_track(error_code::sound_playback,
				                  sdl_error_message("Failed to set voice volume"));
			}

			if (!MIX_TagTrack(track.get(), bus_tag))
			{
				return fail_track(error_code::sound_playback,
				                  sdl_error_message("Failed to assign voice to audio bus"));
			}
			if (options.position)
			{
				const MIX_Point3D position{options.position->x, options.position->y, options.position->z};
				if (!MIX_SetTrack3DPosition(track.get(), &position))
				{
					return fail_track(error_code::spatial_control,
					                  sdl_error_message("Failed to set sound position"));
				}
			}

			properties_guard play_properties;
			if (options.loop)
			{
				play_properties.reset(SDL_CreateProperties());
				if (!play_properties)
				{
					return fail_track(error_code::sound_playback,
					                  sdl_error_message("Failed to create playback options"));
				}

				if (!SDL_SetNumberProperty(play_properties.get(), MIX_PROP_PLAY_LOOPS_NUMBER, -1))
				{
					return fail_track(error_code::sound_playback,
					                  sdl_error_message("Failed to configure looping playback"));
				}
			}

			const std::uint64_t id = next_voice_id;
			if (!MIX_PlayTrack(track.get(), play_properties.get()))
			{
				return fail_track(error_code::sound_playback,
				                  sdl_error_message("Failed to start sound playback"));
			}

			voices.push_back(voice{id, track.release(), options.bus, volume, {}});
			next_voice_id = id == std::numeric_limits<std::uint64_t>::max() ? 0 : id + 1;
			return id;
		}

		std::expected<std::uint64_t, error> play_music(const std::shared_ptr<music_resource>& music,
		                                               play_options options)
		{
			reap_finished_voices();
			if (!music)
			{
				return std::unexpected(
				    error{error_code::invalid_argument, "Cannot play an empty music resource"});
			}

			if (next_voice_id == 0)
			{
				return std::unexpected(
				    error{error_code::voice_id_exhausted, "No more unique voice handles are available"});
			}

			if (!std::isfinite(options.volume))
			{
				return std::unexpected(
				    error{error_code::invalid_argument, "voice volume must be a finite number"});
			}
			if (options.position &&
			    (!std::isfinite(options.position->x) || !std::isfinite(options.position->y) ||
			     !std::isfinite(options.position->z)))
			{
				return std::unexpected(error{error_code::invalid_argument, "3D position must contain finite values"});
			}

			const char* bus_tag = nullptr;
			switch (options.bus)
			{
			case bus::sfx:
				bus_tag = "mngl.audio.bus.sfx";
				break;
			case bus::music:
				bus_tag = "mngl.audio.bus.music";
				break;
			default:
				return std::unexpected(error{error_code::invalid_argument, "Unknown audio bus"});
			}

			try
			{
				voices.reserve(voices.size() + 1);
			}
			catch (const std::bad_alloc&)
			{
				return std::unexpected(
				    error{error_code::music_playback, "Failed to allocate voice tracking state"});
			}

			auto music_stream = open_music_stream(music);
			if (!music_stream)
			{
				return std::unexpected(std::move(music_stream.error()));
			}
			std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(*music_stream, SDL_CloseIO);

			std::unique_ptr<MIX_Track, decltype(&MIX_DestroyTrack)> track(MIX_CreateTrack(mixer),
			                                                              MIX_DestroyTrack);
			if (!track)
			{
				return std::unexpected(
				    error{error_code::music_playback, sdl_error_message("Failed to create a music track")});
			}

			if (!MIX_SetTrackIOStream(track.get(), stream.release(), true))
			{
				return std::unexpected(error{error_code::music_playback,
				                             sdl_error_message("Failed to assign music stream to track")});
			}

			const float volume = std::clamp(options.volume, 0.0f, 1.0f);
			const float bus_volume = options.bus == bus::sfx ? sfx_volume_level : music_volume_level;
			if (!MIX_SetTrackGain(track.get(), volume * bus_volume))
			{
				return std::unexpected(
				    error{error_code::volume_control, sdl_error_message("Failed to set music voice volume")});
			}

			if (!MIX_TagTrack(track.get(), bus_tag))
			{
				return std::unexpected(error{error_code::music_playback,
				                             sdl_error_message("Failed to assign music voice to audio bus")});
			}
			if (options.position)
			{
				const MIX_Point3D position{options.position->x, options.position->y, options.position->z};
				if (!MIX_SetTrack3DPosition(track.get(), &position))
				{
					return std::unexpected(error{error_code::spatial_control,
					                             sdl_error_message("Failed to set music position")});
				}
			}

			properties_guard play_properties;
			if (options.loop)
			{
				play_properties.reset(SDL_CreateProperties());
				if (!play_properties)
				{
					return std::unexpected(
					    error{error_code::music_playback,
						      sdl_error_message("Failed to create music playback options")});
				}

				if (!SDL_SetNumberProperty(play_properties.get(), MIX_PROP_PLAY_LOOPS_NUMBER, -1))
				{
					return std::unexpected(error{error_code::music_playback,
					                             sdl_error_message("Failed to configure looping music")});
				}
			}

			const std::uint64_t id = next_voice_id;
			if (!MIX_PlayTrack(track.get(), play_properties.get()))
			{
				return std::unexpected(
				    error{error_code::music_playback, sdl_error_message("Failed to start music playback")});
			}

			voices.push_back(voice{id, track.release(), options.bus, volume, {}});
			next_voice_id = id == std::numeric_limits<std::uint64_t>::max() ? 0 : id + 1;
			return id;
		}

		std::expected<std::uint64_t, error> play_stream(const std::shared_ptr<audio_stream_resource>& stream,
		                                               play_options options)
		{
			reap_finished_voices();
			if (!stream || !stream->native_stream())
			{
				return std::unexpected(error{error_code::invalid_argument, "Cannot play an empty audio stream"});
			}
			if (options.loop)
			{
				return std::unexpected(error{error_code::invalid_argument,
				                             "Live audio streams cannot use playback looping"});
			}
			if (next_voice_id == 0)
			{
				return std::unexpected(
				    error{error_code::voice_id_exhausted, "No more unique voice handles are available"});
			}
			if (!std::isfinite(options.volume))
			{
				return std::unexpected(error{error_code::invalid_argument, "voice volume must be a finite number"});
			}
			if (options.position &&
			    (!std::isfinite(options.position->x) || !std::isfinite(options.position->y) ||
			     !std::isfinite(options.position->z)))
			{
				return std::unexpected(error{error_code::invalid_argument, "3D position must contain finite values"});
			}

			const char* bus_tag = nullptr;
			switch (options.bus)
			{
			case bus::sfx:
				bus_tag = "mngl.audio.bus.sfx";
				break;
			case bus::music:
				bus_tag = "mngl.audio.bus.music";
				break;
			default:
				return std::unexpected(error{error_code::invalid_argument, "Unknown audio bus"});
			}

			try
			{
				voices.reserve(voices.size() + 1);
			}
			catch (const std::bad_alloc&)
			{
				return std::unexpected(error{error_code::stream_playback, "Failed to allocate voice tracking state"});
			}

			bool expected_detached = false;
			if (!stream->attached.compare_exchange_strong(expected_detached, true, std::memory_order_acq_rel))
			{
				return std::unexpected(error{error_code::invalid_argument,
				                             "An audio stream can be attached to only one active voice"});
			}
			struct attachment_guard
			{
				std::shared_ptr<audio_stream_resource> resource;
				bool active = true;
				~attachment_guard()
				{
					if (active)
						resource->attached.store(false, std::memory_order_release);
				}
			} attachment{stream};

			std::unique_ptr<MIX_Track, decltype(&MIX_DestroyTrack)> track(MIX_CreateTrack(mixer), MIX_DestroyTrack);
			if (!track)
			{
				return std::unexpected(
				    error{error_code::stream_playback, sdl_error_message("Failed to create a streaming track")});
			}
			if (!MIX_SetTrackAudioStream(track.get(), stream->native_stream()))
			{
				return std::unexpected(
				    error{error_code::stream_playback, sdl_error_message("Failed to attach audio stream to track")});
			}

			const float volume = std::clamp(options.volume, 0.0f, 1.0f);
			const float bus_volume = options.bus == bus::sfx ? sfx_volume_level : music_volume_level;
			if (!MIX_SetTrackGain(track.get(), volume * bus_volume))
			{
				return std::unexpected(
				    error{error_code::volume_control, sdl_error_message("Failed to set stream voice volume")});
			}
			if (!MIX_TagTrack(track.get(), bus_tag))
			{
				return std::unexpected(error{error_code::stream_playback,
				                             sdl_error_message("Failed to assign stream voice to audio bus")});
			}
			if (options.position)
			{
				const MIX_Point3D position{options.position->x, options.position->y, options.position->z};
				if (!MIX_SetTrack3DPosition(track.get(), &position))
				{
					return std::unexpected(
					    error{error_code::spatial_control, sdl_error_message("Failed to set stream position")});
				}
			}

			properties_guard play_properties;
			play_properties.reset(SDL_CreateProperties());
			if (!play_properties ||
			    !SDL_SetBooleanProperty(play_properties.get(), MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN, false))
			{
				return std::unexpected(error{error_code::stream_playback,
				                             sdl_error_message("Failed to configure a live audio stream")});
			}

			const std::uint64_t id = next_voice_id;
			if (!MIX_PlayTrack(track.get(), play_properties.get()))
			{
				return std::unexpected(
				    error{error_code::stream_playback, sdl_error_message("Failed to start audio stream playback")});
			}

			voices.push_back(voice{id, track.release(), options.bus, volume, stream});
			attachment.active = false;
			next_voice_id = id == std::numeric_limits<std::uint64_t>::max() ? 0 : id + 1;
			return id;
		}

		bool voice_is_playing(std::uint64_t id)
		{
			reap_finished_voices();
			voice* voice = find_voice(id);
			return voice && MIX_TrackPlaying(voice->track);
		}

		bool voice_is_paused(std::uint64_t id)
		{
			reap_finished_voices();
			voice* voice = find_voice(id);
			return voice && MIX_TrackPaused(voice->track);
		}

		std::expected<void, error> try_set_master_volume(float volume)
		{
			if (!std::isfinite(volume))
			{
				return std::unexpected(
				    error{error_code::invalid_argument, "Master volume must be a finite number"});
			}

			const float clamped_volume = std::clamp(volume, 0.0f, 1.0f);
			if (!MIX_SetMixerGain(mixer, clamped_volume))
			{
				return std::unexpected(
				    error{error_code::volume_control, sdl_error_message("Failed to set master volume")});
			}

			master_volume_level = clamped_volume;
			return {};
		}

		float try_master_volume() const noexcept { return master_volume_level; }

		std::expected<void, error> try_set_bus_volume(bus bus, float volume)
		{
			if (!std::isfinite(volume))
			{
				return std::unexpected(
				    error{error_code::invalid_argument, "bus volume must be a finite number"});
			}

			float* current_volume = nullptr;
			switch (bus)
			{
			case bus::sfx:
				current_volume = &sfx_volume_level;
				break;
			case bus::music:
				current_volume = &music_volume_level;
				break;
			default:
				return std::unexpected(error{error_code::invalid_argument, "Unknown audio bus"});
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
				if (voice.bus == bus && !MIX_SetTrackGain(voice.track, voice.base_volume * clamped_volume))
				{
					const std::string message = sdl_error_message("Failed to set bus volume");
					for (auto& rollback_voice : voices)
					{
						if (rollback_voice.bus == bus)
						{
							MIX_SetTrackGain(rollback_voice.track,
							                 rollback_voice.base_volume * previous_volume);
						}
					}
					return std::unexpected(error{error_code::volume_control, message});
				}
			}

			*current_volume = clamped_volume;
			return {};
		}

		std::expected<float, error> try_bus_volume(bus bus) const
		{
			switch (bus)
			{
			case bus::sfx:
				return sfx_volume_level;
			case bus::music:
				return music_volume_level;
			default:
				return std::unexpected(error{error_code::invalid_argument, "Unknown audio bus"});
			}
		}

		std::expected<void, error> pause_voice(std::uint64_t id)
		{
			reap_finished_voices();
			voice* voice = find_voice(id);
			if (!voice)
			{
				return std::unexpected(detail::inactive_voice_error());
			}

			if (!MIX_PauseTrack(voice->track))
			{
				return std::unexpected(
				    error{error_code::sound_playback, sdl_error_message("Failed to pause voice")});
			}
			return {};
		}

		std::expected<void, error> resume_voice(std::uint64_t id)
		{
			reap_finished_voices();
			voice* voice = find_voice(id);
			if (!voice)
			{
				return std::unexpected(detail::inactive_voice_error());
			}

			if (!MIX_ResumeTrack(voice->track))
			{
				return std::unexpected(
				    error{error_code::sound_playback, sdl_error_message("Failed to resume voice")});
			}
			return {};
		}

		std::expected<void, error> stop_voice(std::uint64_t id)
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
				if (voice->stream)
					voice->stream->attached.store(false, std::memory_order_release);
				voices.erase(voice);
				return std::unexpected(error{error_code::sound_playback, message});
			}

			MIX_DestroyTrack(voice->track);
			if (voice->stream)
				voice->stream->attached.store(false, std::memory_order_release);
			voices.erase(voice);
			return {};
		}

		std::expected<void, error> set_voice_position(std::uint64_t id, std::optional<position3d> position)
		{
			if (position && (!std::isfinite(position->x) || !std::isfinite(position->y) ||
			                 !std::isfinite(position->z)))
			{
				return std::unexpected(error{error_code::invalid_argument, "3D position must contain finite values"});
			}

			reap_finished_voices();
			voice* voice = find_voice(id);
			if (!voice)
			{
				return std::unexpected(detail::inactive_voice_error());
			}

			const MIX_Point3D native_position{position ? position->x : 0.0f, position ? position->y : 0.0f,
			                                  position ? position->z : 0.0f};
			if (!MIX_SetTrack3DPosition(voice->track, position ? &native_position : nullptr))
			{
				return std::unexpected(
				    error{error_code::spatial_control, sdl_error_message("Failed to update voice position")});
			}
			return {};
		}

	private:
		explicit engine(MIX_Mixer* mixer) noexcept : mixer(mixer), mixer_initialized(true) {}

		void shutdown() noexcept
		{
			if (mixer)
			{
				MIX_DestroyMixer(std::exchange(mixer, nullptr));
			}
			voices.clear();

			// Stop active tracks first, then drop MNGL's references to every sound.
			// This leaves wrappers harmless if static objects outlive the audio slot.
			for (auto& weak_sound : sound_resources)
			{
				if (auto sound = weak_sound.lock())
				{
					sound->release();
				}
			}
			sound_resources.clear();
			for (auto& weak_stream : audio_stream_resources)
			{
				if (auto stream = weak_stream.lock())
				{
					stream->release();
				}
			}
			audio_stream_resources.clear();

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

		voice* find_voice(std::uint64_t id)
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
			return std::find_if(voices.begin(), voices.end(),
			                    [id](const voice& voice) { return voice.id == id; });
		}

		void reap_finished_voices()
		{
			for (auto voice = voices.begin(); voice != voices.end();)
			{
				if (!MIX_TrackPlaying(voice->track) && !MIX_TrackPaused(voice->track))
				{
					MIX_DestroyTrack(voice->track);
					if (voice->stream)
						voice->stream->attached.store(false, std::memory_order_release);
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
		std::vector<std::weak_ptr<sound_resource>> sound_resources;
		std::size_t sound_registration_count = 0;
		std::vector<std::weak_ptr<audio_stream_resource>> audio_stream_resources;
		std::size_t audio_stream_registration_count = 0;
		std::vector<voice> voices;
		std::uint64_t next_voice_id = 1;
		float master_volume_level = 1.0f;
		float music_volume_level = 1.0f;
		float sfx_volume_level = 1.0f;
	};

	struct engine_state
	{
		std::optional<engine> active_engine;
	};

	struct capture_state
	{
		std::vector<std::weak_ptr<capture_resource>> resources;

		~capture_state()
		{
			for (auto& weak_resource : resources)
			{
				if (auto resource = weak_resource.lock())
				{
					resource->release();
				}
			}
		}
	};

	inline engine_state& engine_slot()
	{
		static engine_state slot;
		return slot;
	}

	inline capture_state& capture_slot()
	{
		static capture_state slot;
		return slot;
	}

	inline std::expected<void, error> ensure_sdl_runtime()
	{
		auto& sdl_runtime = ::mngl::runtime::instance();
		(void)sdl_runtime;
		if (::mngl::runtime::error())
		{
			return std::unexpected(error{error_code::sdl_runtime, ::mngl::runtime::content()});
		}
		if (!::mngl::runtime::running())
		{
			return std::unexpected(error{error_code::sdl_runtime, "SDL runtime is not running"});
		}
		return {};
	}

	// Force construction of SDL's runtime singleton before the audio slot. This
	// registers the audio slot for destruction first, so its engine is destroyed
	// before SDL_Quit() runs at process shutdown.
	inline std::expected<std::reference_wrapper<engine>, error> ensure_engine()
	{
		auto& sdl_runtime = ::mngl::runtime::instance();
		(void)sdl_runtime;

		if (::mngl::runtime::error())
		{
			return std::unexpected(error{error_code::sdl_runtime, ::mngl::runtime::content()});
		}

		auto& slot = engine_slot();
		if (!slot.active_engine)
		{
			auto created = engine::create();
			if (!created)
			{
				return std::unexpected(std::move(created.error()));
			}
			slot.active_engine.emplace(std::move(*created));
		}

		return std::ref(*slot.active_engine);
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

	inline std::expected<std::shared_ptr<capture_resource>, error> create_capture_resource(
	    SDL_AudioDeviceID device, capture_options options)
	{
		if (options.sample_rate <= 0 || (options.channels != 1 && options.channels != 2))
		{
			return std::unexpected(error{error_code::invalid_argument,
			                             "capture format requires a positive sample rate and 1 or 2 channels"});
		}

		SDL_AudioSpec format{SDL_AUDIO_F32, options.channels, options.sample_rate};
		SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(device, &format, nullptr, nullptr);
		if (!stream)
		{
			return std::unexpected(error{error_code::capture_device,
			                             sdl_error_message("Failed to open audio recording device")});
		}
		std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> stream_guard(
		    stream, SDL_DestroyAudioStream);

		std::shared_ptr<capture_resource> resource;
		try
		{
			resource = std::make_shared<capture_resource>(stream);
			stream_guard.release();
		}
		catch (const std::bad_alloc&)
		{
			return std::unexpected(error{error_code::capture, "Failed to allocate audio capture state"});
		}

		if (!SDL_ResumeAudioStreamDevice(resource->native_stream()))
		{
			return std::unexpected(
			    error{error_code::capture, sdl_error_message("Failed to start audio capture")});
		}

		capture_slot().resources.emplace_back(resource);
		return resource;
	}

	inline std::expected<void, error> validate_music_resource(const std::shared_ptr<music_resource>& music)
	{
		auto io = open_music_stream(music, error_code::music_load);
		if (!io)
		{
			return std::unexpected(std::move(io.error()));
		}
		std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(*io, SDL_CloseIO);

		// Keep ownership here so the stream is closed on every decoder failure
		// path as well as after successful validation.
		std::unique_ptr<MIX_AudioDecoder, decltype(&MIX_DestroyAudioDecoder)> decoder(
		    MIX_CreateAudioDecoder_IO(stream.get(), false, 0), MIX_DestroyAudioDecoder);
		if (!decoder)
		{
			return std::unexpected(
			    error{error_code::music_load, sdl_error_message("Unsupported or unreadable music source")});
		}
		return {};
	}
} // namespace mngl::audio::detail

namespace mngl::audio
{
	inline std::expected<std::vector<recording_device>, error> try_recording_devices()
	{
		auto runtime = detail::ensure_sdl_runtime();
		if (!runtime)
		{
			return std::unexpected(std::move(runtime.error()));
		}

		int count = 0;
		std::unique_ptr<SDL_AudioDeviceID, decltype(&SDL_free)> ids(SDL_GetAudioRecordingDevices(&count), SDL_free);
		if (count < 0 || (!ids && count > 0))
		{
			return std::unexpected(
			    error{error_code::capture_device, detail::sdl_error_message("Failed to list recording devices")});
		}

		std::vector<recording_device> devices;
		devices.reserve(static_cast<std::size_t>(count));
		for (int index = 0; index < count; ++index)
		{
			const SDL_AudioDeviceID id = ids.get()[index];
			const char* name = SDL_GetAudioDeviceName(id);
			if (!name)
			{
				return std::unexpected(error{error_code::capture_device,
				                             detail::sdl_error_message("Failed to get a recording device name")});
			}
			devices.push_back(detail::recording_device_access::make(id, name));
		}
		return devices;
	}

	inline std::expected<capture_session, error> try_open_capture(capture_options options)
	{
		auto runtime = detail::ensure_sdl_runtime();
		if (!runtime)
		{
			return std::unexpected(std::move(runtime.error()));
		}
		auto resource = detail::create_capture_resource(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, options);
		if (!resource)
		{
			return std::unexpected(std::move(resource.error()));
		}
		return capture_session(std::move(*resource), options.sample_rate, options.channels);
	}

	inline std::expected<capture_session, error> try_open_capture(const recording_device& device,
	                                                             capture_options options)
	{
		auto runtime = detail::ensure_sdl_runtime();
		if (!runtime)
		{
			return std::unexpected(std::move(runtime.error()));
		}
		const SDL_AudioDeviceID id = detail::recording_device_access::id(device);
		if (id == 0)
		{
			return std::unexpected(error{error_code::invalid_argument, "Recording device handle is empty"});
		}
		auto resource = detail::create_capture_resource(id, options);
		if (!resource)
		{
			return std::unexpected(std::move(resource.error()));
		}
		return capture_session(std::move(*resource), options.sample_rate, options.channels);
	}

	inline std::expected<audio_stream, error> try_create_audio_stream(audio_stream_format format)
	{
		if (format.sample_rate <= 0 || (format.channels != 1 && format.channels != 2))
		{
			return std::unexpected(error{error_code::invalid_argument,
			                             "audio stream format requires a positive sample rate and 1 or 2 channels"});
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}

		SDL_AudioSpec source_format{SDL_AUDIO_F32, format.channels, format.sample_rate};
		SDL_AudioSpec mixer_format{};
		if (!MIX_GetMixerFormat(engine->get().native_mixer(), &mixer_format))
		{
			return std::unexpected(
			    error{error_code::stream_create, detail::sdl_error_message("Failed to query mixer format")});
		}
		const SDL_AudioSpec mixer_input_format{SDL_AUDIO_F32, mixer_format.channels, mixer_format.freq};
		SDL_AudioStream* native_stream = SDL_CreateAudioStream(&source_format, &mixer_input_format);
		if (!native_stream)
		{
			return std::unexpected(
			    error{error_code::stream_create, detail::sdl_error_message("Failed to create audio stream")});
		}
		std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> stream_guard(native_stream,
		                                                                                   SDL_DestroyAudioStream);
		std::shared_ptr<detail::audio_stream_resource> resource;
		try
		{
			resource = std::make_shared<detail::audio_stream_resource>(native_stream);
			stream_guard.release();
			engine->get().register_audio_stream(resource);
		}
		catch (const std::bad_alloc&)
		{
			return std::unexpected(error{error_code::stream_create, "Failed to allocate audio stream state"});
		}
		return audio_stream(std::move(resource), format.sample_rate, format.channels);
	}

	inline std::expected<music, error> try_load_music(std::string_view path)
	{
		if (path.empty() || path.find('\0') != std::string_view::npos)
		{
			return std::unexpected(error{error_code::invalid_argument,
			                             "music path must be non-empty and cannot contain a null byte"});
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}

		try
		{
			auto resource = std::make_shared<detail::music_resource>();
			resource->path.assign(path);
			auto validation = detail::validate_music_resource(resource);
			if (!validation)
			{
				return std::unexpected(std::move(validation.error()));
			}
			return music(std::move(resource));
		}
		catch (const std::bad_alloc&)
		{
			return std::unexpected(error{error_code::music_load, "Failed to allocate music resource state"});
		}
	}

	inline std::expected<music, error> try_load_music(std::istream& stream)
	{
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}

		try
		{
			auto resource = std::make_shared<detail::music_resource>();
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
			return music(std::move(resource));
		}
		catch (const std::bad_alloc&)
		{
			return std::unexpected(error{error_code::music_load, "Failed to allocate music resource state"});
		}
	}

	inline std::expected<music, error> try_load_music(std::shared_ptr<std::istream> stream)
	{
		if (!stream)
		{
			return std::unexpected(error{error_code::invalid_argument, "music input stream cannot be null"});
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}

		try
		{
			auto resource = std::make_shared<detail::music_resource>();
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
			return music(std::move(resource));
		}
		catch (const std::bad_alloc&)
		{
			return std::unexpected(error{error_code::music_load, "Failed to allocate music resource state"});
		}
	}

	inline std::expected<sound, error> try_load_sound(std::string_view path)
	{
		if (path.empty() || path.find('\0') != std::string_view::npos)
		{
			return std::unexpected(error{error_code::invalid_argument,
			                             "sound path must be non-empty and cannot contain a null byte"});
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
			return std::unexpected(
			    error{error_code::sound_load,
				      detail::sdl_error_message("Failed to load sound '" + terminated_path + "'")});
		}

		std::unique_ptr<MIX_Audio, decltype(&MIX_DestroyAudio)> audio_guard(audio, MIX_DestroyAudio);
		try
		{
			auto resource = std::make_shared<detail::sound_resource>(audio);
			audio_guard.release();
			engine->get().register_sound(resource);
			return sound(std::move(resource));
		}
		catch (const std::bad_alloc&)
		{
			return std::unexpected(error{error_code::sound_load, "Failed to allocate sound resource state"});
		}
	}

	inline std::expected<voice_handle, error> try_play(const sound& sound, play_options options)
	{
		if (!sound.resource_ || !sound.resource_->native_audio())
		{
			return std::unexpected(
			    error{error_code::invalid_argument, "Cannot play an empty or released sound"});
		}

		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}

		auto voice_id = engine->get().play_sound(sound.resource_->native_audio(), options);
		if (!voice_id)
		{
			return std::unexpected(std::move(voice_id.error()));
		}

		return voice_handle(*voice_id);
	}

	inline std::expected<voice_handle, error> try_play(const music& music)
	{
		play_options options;
		options.bus = bus::music;
		return try_play(music, options);
	}

	inline std::expected<voice_handle, error> try_play(const music& music, play_options options)
	{
		if (!music.resource_)
		{
			return std::unexpected(
			    error{error_code::invalid_argument, "Cannot play an empty music resource"});
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
		return voice_handle(*voice_id);
	}

	inline std::expected<voice_handle, error> try_play(audio_stream& stream, play_options options)
	{
		if (!stream.resource_ || !stream.resource_->native_stream())
		{
			return std::unexpected(error{error_code::invalid_argument, "Cannot play an empty audio stream"});
		}
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		auto voice_id = engine->get().play_stream(stream.resource_, options);
		if (!voice_id)
		{
			return std::unexpected(std::move(voice_id.error()));
		}
		return voice_handle(*voice_id);
	}

	inline std::expected<std::vector<float>, error> capture_session::try_read()
	{
		if (!resource_ || !resource_->native_stream())
		{
			return std::unexpected(error{error_code::capture, "Audio capture session is empty or closed"});
		}

		const int available_bytes = SDL_GetAudioStreamAvailable(resource_->native_stream());
		if (available_bytes < 0)
		{
			return std::unexpected(error{error_code::capture,
			                             detail::sdl_error_message("Failed to query captured audio")});
		}
		const int frame_bytes = channels_ * static_cast<int>(sizeof(float));
		const int readable_bytes = available_bytes - (available_bytes % frame_bytes);
		if (readable_bytes == 0)
		{
			return std::vector<float>{};
		}

		std::vector<float> samples(static_cast<std::size_t>(readable_bytes) / sizeof(float));
		const int read_bytes = SDL_GetAudioStreamData(resource_->native_stream(), samples.data(), readable_bytes);
		if (read_bytes < 0)
		{
			return std::unexpected(
			    error{error_code::capture, detail::sdl_error_message("Failed to read captured audio")});
		}
		samples.resize(static_cast<std::size_t>(read_bytes) / sizeof(float));
		return samples;
	}

	inline std::expected<void, error> capture_session::try_pause()
	{
		if (!resource_ || !resource_->native_stream())
		{
			return std::unexpected(error{error_code::capture, "Audio capture session is empty or closed"});
		}
		if (!SDL_PauseAudioStreamDevice(resource_->native_stream()))
		{
			return std::unexpected(
			    error{error_code::capture, detail::sdl_error_message("Failed to pause audio capture")});
		}
		return {};
	}

	inline std::expected<void, error> capture_session::try_resume()
	{
		if (!resource_ || !resource_->native_stream())
		{
			return std::unexpected(error{error_code::capture, "Audio capture session is empty or closed"});
		}
		if (!SDL_ResumeAudioStreamDevice(resource_->native_stream()))
		{
			return std::unexpected(
			    error{error_code::capture, detail::sdl_error_message("Failed to resume audio capture")});
		}
		return {};
	}

	inline std::expected<void, error> audio_stream::try_write(std::span<const float> interleaved_samples)
	{
		if (!resource_ || !resource_->native_stream())
		{
			return std::unexpected(error{error_code::stream_write, "Audio stream is empty or closed"});
		}
		if (channels_ <= 0 || interleaved_samples.size() % static_cast<std::size_t>(channels_) != 0)
		{
			return std::unexpected(error{error_code::invalid_argument,
			                             "interleaved sample count must be divisible by the stream channel count"});
		}
		if (interleaved_samples.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) / sizeof(float))
		{
			return std::unexpected(error{error_code::invalid_argument, "audio write exceeds SDL's size limit"});
		}
		if (interleaved_samples.empty())
		{
			return {};
		}
		const int byte_count = static_cast<int>(interleaved_samples.size() * sizeof(float));
		if (!SDL_PutAudioStreamData(resource_->native_stream(), interleaved_samples.data(), byte_count))
		{
			return std::unexpected(error{error_code::stream_write,
			                             detail::sdl_error_message("Failed to write audio stream data")});
		}
		return {};
	}

	inline std::expected<std::size_t, error> audio_stream::try_queued_input_frames() const
	{
		if (!resource_ || !resource_->native_stream() || channels_ <= 0)
		{
			return std::unexpected(error{error_code::stream_write, "Audio stream is empty or closed"});
		}
		const int queued_bytes = SDL_GetAudioStreamQueued(resource_->native_stream());
		if (queued_bytes < 0)
		{
			return std::unexpected(error{error_code::stream_write,
			                             detail::sdl_error_message("Failed to query queued audio")});
		}
		const std::size_t frame_bytes = static_cast<std::size_t>(channels_) * sizeof(float);
		return static_cast<std::size_t>(queued_bytes) / frame_bytes;
	}

	inline std::expected<bool, error> try_is_playing(voice_handle voice)
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

	inline std::expected<bool, error> try_is_paused(voice_handle voice)
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

	inline std::expected<void, error> try_pause(voice_handle voice)
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

	inline std::expected<void, error> try_resume(voice_handle voice)
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

	inline std::expected<void, error> try_stop(voice_handle voice)
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

	inline std::expected<void, error> try_set_position(voice_handle voice, std::optional<position3d> position)
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
		return engine->get().set_voice_position(voice.id_, position);
	}

	inline std::expected<void, error> try_set_master_volume(float volume)
	{
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().try_set_master_volume(volume);
	}

	inline std::expected<float, error> try_master_volume()
	{
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().try_master_volume();
	}

	inline std::expected<void, error> try_set_bus_volume(bus bus, float volume)
	{
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().try_set_bus_volume(bus, volume);
	}

	inline std::expected<float, error> try_bus_volume(bus bus)
	{
		auto engine = detail::ensure_engine();
		if (!engine)
		{
			return std::unexpected(std::move(engine.error()));
		}
		return engine->get().try_bus_volume(bus);
	}
} // namespace mngl::audio

#endif
