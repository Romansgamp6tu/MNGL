#pragma once

#if !defined(MNGL_CUSTOM_AUDIO_BACKEND) && !defined(MNGL_CUSTOM_AUDIO_INTERFACE) &&                          \
    !defined(MNGL_CUSTOM_MIXER_INTERFACE)

#include <expected>
#include <stdexcept>
#include <type_traits>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mngl::audio
{
	enum class error_code
	{
		sdl_runtime,
		mixer_initialization,
		playback_device,
		invalid_argument,
		sound_load,
		sound_playback,
		voice_inactive,
		voice_id_exhausted,
		volume_control,
		music_load,
		music_playback,
		capture_device,
		capture,
		stream_create,
		stream_write,
		spatial_control,
		stream_playback
	};

	struct error
	{
		error_code code;
		std::string message;
	};

	namespace detail
	{
		class sound_resource;
		class music_resource;
		class capture_resource;
		class audio_stream_resource;
		struct recording_device_access;
	} // namespace detail

	class voice_handle;
	class capture_session;
	class audio_stream;
	class recording_device;
	struct play_options;
	struct capture_options;
	struct audio_stream_format;

	class sound
	{
	public:
		// An empty sound is false; copies share the same loaded audio resource.
		sound() noexcept = default;
		sound(const sound&) noexcept = default;
		sound& operator=(const sound&) noexcept = default;
		sound(sound&&) noexcept = default;
		sound& operator=(sound&&) noexcept = default;

		explicit operator bool() const noexcept { return static_cast<bool>(resource_); }

	private:
		explicit sound(std::shared_ptr<detail::sound_resource> resource) noexcept
		    : resource_(std::move(resource))
		{
		}

		std::shared_ptr<detail::sound_resource> resource_;

		friend std::expected<sound, error> try_load_sound(std::string_view path);
		friend std::expected<voice_handle, error> try_play(const sound&, play_options);
	};

	class music
	{
	public:
		// An empty music is false; copies share the same stream source.
		music() noexcept = default;
		music(const music&) noexcept = default;
		music& operator=(const music&) noexcept = default;
		music(music&&) noexcept = default;
		music& operator=(music&&) noexcept = default;

		explicit operator bool() const noexcept { return static_cast<bool>(resource_); }

	private:
		explicit music(std::shared_ptr<detail::music_resource> resource) noexcept
		    : resource_(std::move(resource))
		{
		}

		std::shared_ptr<detail::music_resource> resource_;

		friend std::expected<music, error> try_load_music(std::string_view path);
		friend std::expected<music, error> try_load_music(std::istream& stream);
		friend std::expected<music, error> try_load_music(std::shared_ptr<std::istream> stream);
		friend std::expected<voice_handle, error> try_play(const music&, play_options);
	};

	struct position3d
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	class recording_device
	{
	public:
		recording_device(const recording_device&) = default;
		recording_device& operator=(const recording_device&) = default;
		recording_device(recording_device&&) noexcept = default;
		recording_device& operator=(recording_device&&) noexcept = default;
		[[nodiscard]] std::string_view name() const noexcept { return name_; }
		friend bool operator==(const recording_device&, const recording_device&) = default;

	private:
		recording_device(std::uint32_t id, std::string name) : id_(id), name_(std::move(name)) {}
		std::uint32_t id_ = 0;
		std::string name_;

		friend struct detail::recording_device_access;
	};

	struct capture_options
	{
		int sample_rate = 48000;
		int channels = 1;
	};

	class capture_session
	{
	public:
		capture_session() noexcept = default;
		capture_session(const capture_session&) = delete;
		capture_session& operator=(const capture_session&) = delete;
		capture_session(capture_session&&) noexcept = default;
		capture_session& operator=(capture_session&&) noexcept = default;

		[[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(resource_); }
		[[nodiscard]] int sample_rate() const noexcept { return sample_rate_; }
		[[nodiscard]] int channels() const noexcept { return channels_; }
		[[nodiscard]] std::expected<std::vector<float>, error> try_read();
		[[nodiscard]] std::vector<float> read();
		[[nodiscard]] std::expected<void, error> try_pause();
		[[nodiscard]] std::expected<void, error> try_resume();
		void pause();
		void resume();

	private:
		capture_session(std::shared_ptr<detail::capture_resource> resource, int sample_rate, int channels) noexcept
		    : resource_(std::move(resource)), sample_rate_(sample_rate), channels_(channels)
		{
		}

		std::shared_ptr<detail::capture_resource> resource_;
		int sample_rate_ = 0;
		int channels_ = 0;

		friend std::expected<capture_session, error> try_open_capture(capture_options);
		friend std::expected<capture_session, error> try_open_capture(const recording_device&, capture_options);
	};

	struct audio_stream_format
	{
		int sample_rate = 48000;
		int channels = 2;
	};

	class audio_stream
	{
	public:
		audio_stream() noexcept = default;
		audio_stream(const audio_stream&) = delete;
		audio_stream& operator=(const audio_stream&) = delete;
		audio_stream(audio_stream&&) noexcept = default;
		audio_stream& operator=(audio_stream&&) noexcept = default;

		[[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(resource_); }
		[[nodiscard]] int sample_rate() const noexcept { return sample_rate_; }
		[[nodiscard]] int channels() const noexcept { return channels_; }
		[[nodiscard]] std::expected<std::size_t, error> try_queued_input_frames() const;
		[[nodiscard]] std::size_t queued_input_frames() const;
		[[nodiscard]] std::expected<void, error> try_write(std::span<const float> interleaved_samples);
		void write(std::span<const float> interleaved_samples);

	private:
		audio_stream(std::shared_ptr<detail::audio_stream_resource> resource, int sample_rate,
		             int channels) noexcept
		    : resource_(std::move(resource)), sample_rate_(sample_rate), channels_(channels)
		{
		}

		std::shared_ptr<detail::audio_stream_resource> resource_;
		int sample_rate_ = 0;
		int channels_ = 0;

		friend std::expected<audio_stream, error> try_create_audio_stream(audio_stream_format);
		friend std::expected<voice_handle, error> try_play(audio_stream&, play_options);
	};

	enum class bus
	{
		sfx,
		music
	};

	struct play_options
	{
		// Initial linear gain, clamped to [0, 1].
		float volume = 1.0f;
		// Repeat this voice indefinitely when true.
		bool loop = false;
		mngl::audio::bus bus = mngl::audio::bus::sfx;
		// Optional position relative to the listener fixed at the origin.
		std::optional<position3d> position;
	};

	// Identifies one try_play() call; it is not a mixer channel number.
	class voice_handle
	{
	public:
		voice_handle() noexcept = default;
		voice_handle(const voice_handle&) noexcept = default;
		voice_handle& operator=(const voice_handle&) noexcept = default;
		voice_handle(voice_handle&&) noexcept = default;
		voice_handle& operator=(voice_handle&&) noexcept = default;

		friend bool operator==(const voice_handle&, const voice_handle&) = default;

	private:
		explicit voice_handle(std::uint64_t id) noexcept : id_(id) {}
		std::uint64_t id_ = 0;

		friend std::expected<voice_handle, error> try_play(const sound&, play_options);
		friend std::expected<voice_handle, error> try_play(const music&, play_options);
		friend std::expected<voice_handle, error> try_play(audio_stream&, play_options);
		friend std::expected<bool, error> try_is_playing(voice_handle);
		friend std::expected<bool, error> try_is_paused(voice_handle);
		friend std::expected<void, error> try_pause(voice_handle);
		friend std::expected<void, error> try_resume(voice_handle);
		friend std::expected<void, error> try_stop(voice_handle);
		friend std::expected<void, error> try_set_position(voice_handle, std::optional<position3d>);
	};

	// Loads and pre-decodes an effect from a UTF-8 filesystem path.
	[[nodiscard]] std::expected<sound, error> try_load_sound(std::string_view path);
	// Prepares a stream-backed music source without reading the whole file into memory.
	[[nodiscard]] std::expected<music, error> try_load_music(std::string_view path);
	// Reads from the current position to the end; the source must be seekable and
	// remain alive until music and its active voices are destroyed.
	[[nodiscard]] std::expected<music, error> try_load_music(std::istream& stream);
	// The shared stream overload keeps the source alive for music and active voices.
	[[nodiscard]] std::expected<music, error> try_load_music(std::shared_ptr<std::istream> stream);
	// Device IDs are snapshots and may become stale after a hot-plug change.
	[[nodiscard]] std::expected<std::vector<recording_device>, error> try_recording_devices();
	[[nodiscard]] std::vector<recording_device> recording_devices();
	// Capture opens and starts immediately. The no-device overload follows the system default.
	[[nodiscard]] std::expected<capture_session, error> try_open_capture(capture_options options = {});
	[[nodiscard]] std::expected<capture_session, error> try_open_capture(const recording_device& device,
	                                                                    capture_options options = {});
	[[nodiscard]] capture_session open_capture(capture_options options = {});
	[[nodiscard]] capture_session open_capture(const recording_device& device, capture_options options = {});
	// Creates a live push stream for interleaved float PCM.
	[[nodiscard]] std::expected<audio_stream, error> try_create_audio_stream(audio_stream_format format = {});
	[[nodiscard]] audio_stream create_audio_stream(audio_stream_format format = {});
	// Starts an independent voice. Replaying a sound does not replace earlier voices.
	[[nodiscard]] std::expected<voice_handle, error> try_play(const sound& sound, play_options options = {});
	[[nodiscard]] std::expected<voice_handle, error> try_play(const music& music);
	[[nodiscard]] std::expected<voice_handle, error> try_play(const music& music, play_options options);
	[[nodiscard]] std::expected<voice_handle, error> try_play(audio_stream& stream, play_options options = {});
	// State queries return false when the handle is empty or no longer active.
	[[nodiscard]] std::expected<bool, error> try_is_playing(voice_handle voice);
	[[nodiscard]] std::expected<bool, error> try_is_paused(voice_handle voice);
	// Control calls return error_code::voice_inactive for an empty or expired handle.
	[[nodiscard]] std::expected<void, error> try_pause(voice_handle voice);
	[[nodiscard]] std::expected<void, error> try_resume(voice_handle voice);
	[[nodiscard]] std::expected<void, error> try_stop(voice_handle voice);
	[[nodiscard]] std::expected<void, error> try_set_position(voice_handle voice,
	                                                         std::optional<position3d> position);
	// Finite values are clamped to [0, 1]; NaN and infinities are rejected.
	[[nodiscard]] std::expected<void, error> try_set_master_volume(float volume);
	[[nodiscard]] std::expected<float, error> try_master_volume();
	[[nodiscard]] std::expected<void, error> try_set_bus_volume(bus bus, float volume);
	[[nodiscard]] std::expected<float, error> try_bus_volume(bus bus);
} // namespace mngl::audio

#include "detail/audio/sdl3_mixer_backend.hpp"

namespace mngl::audio
{
	class audio_error : public std::runtime_error
	{
	public:
		explicit audio_error(const error& value) : std::runtime_error(value.message), m_code(value.code) {}
		[[nodiscard]] error_code code() const noexcept { return m_code; }

	private:
		error_code m_code;
	};

	namespace detail
	{
		template <typename T> T value_or_throw(std::expected<T, error> result)
		{
			if (!result)
				throw audio_error(result.error());
			if constexpr (!std::is_void_v<T>)
				return std::move(*result);
		}
	} // namespace detail

	inline std::vector<float> capture_session::read()
	{
		return detail::value_or_throw(try_read());
	}
	inline void capture_session::pause()
	{
		detail::value_or_throw(try_pause());
	}
	inline void capture_session::resume()
	{
		detail::value_or_throw(try_resume());
	}
	inline void audio_stream::write(std::span<const float> interleaved_samples)
	{
		detail::value_or_throw(try_write(interleaved_samples));
	}
	inline std::size_t audio_stream::queued_input_frames() const
	{
		return detail::value_or_throw(try_queued_input_frames());
	}

	[[nodiscard]] inline sound load_sound(std::string_view path)
	{
		return detail::value_or_throw(try_load_sound(path));
	}
	[[nodiscard]] inline music load_music(std::string_view path)
	{
		return detail::value_or_throw(try_load_music(path));
	}
	[[nodiscard]] inline music load_music(std::istream& stream)
	{
		return detail::value_or_throw(try_load_music(stream));
	}
	[[nodiscard]] inline music load_music(std::shared_ptr<std::istream> stream)
	{
		return detail::value_or_throw(try_load_music(std::move(stream)));
	}
	[[nodiscard]] inline std::vector<recording_device> recording_devices()
	{
		return detail::value_or_throw(try_recording_devices());
	}
	[[nodiscard]] inline capture_session open_capture(capture_options options)
	{
		return detail::value_or_throw(try_open_capture(options));
	}
	[[nodiscard]] inline capture_session open_capture(const recording_device& device, capture_options options)
	{
		return detail::value_or_throw(try_open_capture(device, options));
	}
	[[nodiscard]] inline audio_stream create_audio_stream(audio_stream_format format)
	{
		return detail::value_or_throw(try_create_audio_stream(format));
	}
	[[nodiscard]] inline voice_handle play(const sound& source, play_options options = {})
	{
		return detail::value_or_throw(try_play(source, options));
	}
	[[nodiscard]] inline voice_handle play(const music& source)
	{
		return detail::value_or_throw(try_play(source));
	}
	[[nodiscard]] inline voice_handle play(const music& source, play_options options)
	{
		return detail::value_or_throw(try_play(source, options));
	}
	[[nodiscard]] inline voice_handle play(audio_stream& source, play_options options = {})
	{
		return detail::value_or_throw(try_play(source, options));
	}
	[[nodiscard]] inline bool is_playing(voice_handle voice)
	{
		return detail::value_or_throw(try_is_playing(voice));
	}
	[[nodiscard]] inline bool is_paused(voice_handle voice)
	{
		return detail::value_or_throw(try_is_paused(voice));
	}
	inline void pause(voice_handle voice)
	{
		detail::value_or_throw(try_pause(voice));
	}
	inline void resume(voice_handle voice)
	{
		detail::value_or_throw(try_resume(voice));
	}
	inline void stop(voice_handle voice)
	{
		detail::value_or_throw(try_stop(voice));
	}
	inline void set_position(voice_handle voice, std::optional<position3d> position)
	{
		detail::value_or_throw(try_set_position(voice, position));
	}
	inline void set_master_volume(float volume)
	{
		detail::value_or_throw(try_set_master_volume(volume));
	}
	[[nodiscard]] inline float master_volume()
	{
		return detail::value_or_throw(try_master_volume());
	}
	inline void set_bus_volume(bus target, float volume)
	{
		detail::value_or_throw(try_set_bus_volume(target, volume));
	}
	[[nodiscard]] inline float bus_volume(bus target)
	{
		return detail::value_or_throw(try_bus_volume(target));
	}
} // namespace mngl::audio

// Compatibility names; new code uses snake_case.
namespace mngl::audio
{
	using AudioErrorCode = error_code;
	using AudioError = error;
	using Sound = sound;
	using Music = music;
	using VoiceHandle = voice_handle;
	using PlayOptions = play_options;
	using Position3D = position3d;
	using RecordingDevice = recording_device;
	using CaptureOptions = capture_options;
	using CaptureSession = capture_session;
	using AudioStreamFormat = audio_stream_format;
	using AudioStream = audio_stream;
	using Bus = bus;
} // namespace mngl::audio

#endif
