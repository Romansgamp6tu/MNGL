#pragma once

#if !defined(MNGL_CUSTOM_AUDIO_BACKEND) \
	&& !defined(MNGL_CUSTOM_AUDIO_INTERFACE) \
	&& !defined(MNGL_CUSTOM_MIXER_INTERFACE)

#include <expected>
#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace mngl::audio
{
	enum class AudioErrorCode
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
		music_playback
	};

	struct AudioError
	{
		AudioErrorCode code;
		std::string message;
	};

	namespace detail
	{
		class SoundResource;
		class MusicResource;
	}

	class Sound
	{
	public:
		// An empty Sound is false; copies share the same loaded audio resource.
		Sound() noexcept = default;
		Sound(const Sound&) noexcept = default;
		Sound& operator=(const Sound&) noexcept = default;
		Sound(Sound&&) noexcept = default;
		Sound& operator=(Sound&&) noexcept = default;

		explicit operator bool() const noexcept
		{
			return static_cast<bool>(resource_);
		}

	private:
		explicit Sound(std::shared_ptr<detail::SoundResource> resource) noexcept
			: resource_(std::move(resource))
		{
		}

		std::shared_ptr<detail::SoundResource> resource_;

		friend std::expected<Sound, AudioError> load_sound(std::string_view path);
	};

	class Music
	{
	public:
		// An empty Music is false; copies share the same stream source.
		Music() noexcept = default;
		Music(const Music&) noexcept = default;
		Music& operator=(const Music&) noexcept = default;
		Music(Music&&) noexcept = default;
		Music& operator=(Music&&) noexcept = default;

		explicit operator bool() const noexcept
		{
			return static_cast<bool>(resource_);
		}

	private:
		explicit Music(std::shared_ptr<detail::MusicResource> resource) noexcept
			: resource_(std::move(resource))
		{
		}

		std::shared_ptr<detail::MusicResource> resource_;

		friend std::expected<Music, AudioError> load_music(std::string_view path);
		friend std::expected<Music, AudioError> load_music(std::istream& stream);
		friend std::expected<Music, AudioError> load_music(
			std::shared_ptr<std::istream> stream);
	};

	enum class Bus
	{
		sfx,
		music
	};

	struct PlayOptions
	{
		// Initial linear gain, clamped to [0, 1].
		float volume = 1.0f;
		// Repeat this voice indefinitely when true.
		bool loop = false;
		Bus bus = Bus::sfx;
	};

	// Identifies one play() call; it is not a mixer channel number.
	class VoiceHandle
	{
	public:
		VoiceHandle() noexcept = default;
		VoiceHandle(const VoiceHandle&) noexcept = default;
		VoiceHandle& operator=(const VoiceHandle&) noexcept = default;
		VoiceHandle(VoiceHandle&&) noexcept = default;
		VoiceHandle& operator=(VoiceHandle&&) noexcept = default;

		friend bool operator==(const VoiceHandle&, const VoiceHandle&) = default;

	private:
		explicit VoiceHandle(std::uint64_t id) noexcept : id_(id) {}
		std::uint64_t id_ = 0;

		friend std::expected<VoiceHandle, AudioError> play(const Sound&, PlayOptions);
		friend std::expected<VoiceHandle, AudioError> play(const Music&, PlayOptions);
		friend std::expected<bool, AudioError> is_playing(VoiceHandle);
		friend std::expected<bool, AudioError> is_paused(VoiceHandle);
		friend std::expected<void, AudioError> pause(VoiceHandle);
		friend std::expected<void, AudioError> resume(VoiceHandle);
		friend std::expected<void, AudioError> stop(VoiceHandle);
	};

	// Loads and pre-decodes an effect from a UTF-8 filesystem path.
	[[nodiscard]] std::expected<Sound, AudioError> load_sound(std::string_view path);
	// Prepares a stream-backed music source without reading the whole file into memory.
	[[nodiscard]] std::expected<Music, AudioError> load_music(std::string_view path);
	// Reads from the current position to the end; the source must be seekable and
	// remain alive until Music and its active voices are destroyed.
	[[nodiscard]] std::expected<Music, AudioError> load_music(std::istream& stream);
	// The shared stream overload keeps the source alive for Music and active voices.
	[[nodiscard]] std::expected<Music, AudioError> load_music(
		std::shared_ptr<std::istream> stream);
	// Starts an independent voice. Replaying a Sound does not replace earlier voices.
	[[nodiscard]] std::expected<VoiceHandle, AudioError> play(
		const Sound& sound,
		PlayOptions options = {});
	[[nodiscard]] std::expected<VoiceHandle, AudioError> play(const Music& music);
	[[nodiscard]] std::expected<VoiceHandle, AudioError> play(
		const Music& music,
		PlayOptions options);
	// State queries return false when the handle is empty or no longer active.
	[[nodiscard]] std::expected<bool, AudioError> is_playing(VoiceHandle voice);
	[[nodiscard]] std::expected<bool, AudioError> is_paused(VoiceHandle voice);
	// Control calls return AudioErrorCode::voice_inactive for an empty or expired handle.
	[[nodiscard]] std::expected<void, AudioError> pause(VoiceHandle voice);
	[[nodiscard]] std::expected<void, AudioError> resume(VoiceHandle voice);
	[[nodiscard]] std::expected<void, AudioError> stop(VoiceHandle voice);
	// Finite values are clamped to [0, 1]; NaN and infinities are rejected.
	[[nodiscard]] std::expected<void, AudioError> set_master_volume(float volume);
	[[nodiscard]] std::expected<float, AudioError> master_volume();
	[[nodiscard]] std::expected<void, AudioError> set_bus_volume(Bus bus, float volume);
	[[nodiscard]] std::expected<float, AudioError> bus_volume(Bus bus);
}

#include "detail/audio/sdl3_mixer_backend.hpp"

#endif
