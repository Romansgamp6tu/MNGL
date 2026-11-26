#define APP_NAME "MNGL audio runtime smoke"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_AUDIO | SDL_INIT_EVENTS)
#define MNGL_AUDIO_TESTING 1

#include <MNGL/audio.hpp>
#include <MNGL/paths.hpp>

#include <array>
#include <chrono>
#include <exception>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	void test_finished_voice_cleanup()
	{
		std::vector<mngl::audio::audio_stream> streams;
		std::vector<mngl::audio::voice_handle> voices;
		const std::array<float, 256> samples{};
		for (int index = 0; index < 32; ++index)
		{
			streams.push_back(mngl::audio::create_audio_stream());
			streams.back().write(samples);
			voices.push_back(mngl::audio::play(streams.back()));
		}
		auto paused_stream = mngl::audio::create_audio_stream();
		paused_stream.write(samples);
		const auto paused_voice = mngl::audio::play(paused_stream, {.bus = mngl::audio::bus::music});
		mngl::audio::pause(paused_voice);

		// End a batch through the native mixer, so the next MNGL query must reap all of it.
		auto engine = mngl::audio::detail::ensure_engine();
		require(engine.has_value(), "Audio engine was unavailable after stream playback");
		int count = 0;
		std::unique_ptr<MIX_Track*, decltype(&SDL_free)> tracks(
		    MIX_GetTaggedTracks(engine->get().native_mixer(), "mngl.audio.bus.sfx", &count), SDL_free);
		require(tracks && count == static_cast<int>(voices.size()), "Mixer did not retain all live voices");
		for (int index = 0; index < count; ++index)
			mngl::check_sdl(MIX_StopTrack(tracks.get()[index], 0));
		for (const auto voice : voices)
			require(!mngl::audio::is_playing(voice), "Finished voice remained active after cleanup");
		require(mngl::audio::is_paused(paused_voice), "Finished-voice cleanup destroyed a paused voice");
		for (auto& stream : streams)
		{
			const auto replay = mngl::audio::play(stream);
			mngl::audio::stop(replay);
		}
		mngl::audio::stop(paused_voice);
	}
} // namespace

int main()
{
	try
	{
#if defined(MNGL_SHADER_INSTALL_DIRECTORY)
		const auto shader_directory = mngl::paths::shader_directory("batcher2d.vert.hlsl");
		if (!std::filesystem::is_regular_file(shader_directory / "batcher2d.vert.hlsl"))
		{
			std::cerr << "MNGL package did not resolve its installed runtime shader directory.\n";
			return 1;
		}
#endif
		mngl::runtime::ensure();
		// SDL reports enumeration failures with both a null list and count == 0.
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		const auto unavailable = mngl::audio::try_recording_devices();
		mngl::check_sdl(SDL_InitSubSystem(SDL_INIT_AUDIO));
		if (unavailable || unavailable.error().code != mngl::audio::error_code::capture_device)
		{
			std::cerr << "Audio enumeration failure was reported as an empty device list.\n";
			return 1;
		}
		const auto volume = mngl::audio::try_master_volume();
		if (!volume)
		{
			std::cerr << volume.error().message << '\n';
			return 1;
		}
		if (*volume < 0.0f || *volume > 1.0f)
		{
			std::cerr << "Mixer returned an invalid master volume.\n";
			return 1;
		}

		test_finished_voice_cleanup();

		const auto wav_path =
		    std::filesystem::temp_directory_path() /
		    ("mngl-audio-voices-" +
			 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".wav");
		{
			std::ofstream wav(wav_path, std::ios::binary | std::ios::trunc);
			const auto write_u16 = [&wav](std::uint16_t value)
			{
				const char bytes[]{static_cast<char>(value & 0xff), static_cast<char>((value >> 8) & 0xff)};
				wav.write(bytes, sizeof(bytes));
			};
			const auto write_u32 = [&wav](std::uint32_t value)
			{
				const char bytes[]{static_cast<char>(value & 0xff), static_cast<char>((value >> 8) & 0xff),
				                   static_cast<char>((value >> 16) & 0xff),
				                   static_cast<char>((value >> 24) & 0xff)};
				wav.write(bytes, sizeof(bytes));
			};
			constexpr std::uint32_t pcm_bytes = 8000 * 2;
			wav.write("RIFF", 4);
			write_u32(36 + pcm_bytes);
			wav.write("WAVEfmt ", 8);
			write_u32(16);
			write_u16(1);
			write_u16(1);
			write_u32(8000);
			write_u32(16000);
			write_u16(2);
			write_u16(16);
			wav.write("data", 4);
			write_u32(pcm_bytes);
			std::vector<char> silence(pcm_bytes, 0);
			wav.write(silence.data(), static_cast<std::streamsize>(silence.size()));
			if (!wav)
			{
				std::cerr << "Could not create the audio voice regression fixture.\n";
				return 1;
			}
		}

		const auto sound = mngl::audio::try_load_sound(wav_path.string());
		if (!sound)
		{
			std::cerr << sound.error().message << '\n';
			return 1;
		}
		mngl::audio::play_options options;
		options.loop = true;
		options.volume = 0.0f;
		std::vector<mngl::audio::voice_handle> voices;
		voices.reserve(512);
		for (std::size_t i = 0; i < 512; ++i)
		{
			const auto voice = mngl::audio::try_play(*sound, options);
			if (!voice)
			{
				std::cerr << voice.error().message << '\n';
				return 1;
			}
			voices.push_back(*voice);
		}

		for (const std::size_t count : {32u, 128u, 512u})
		{
			mngl::audio::detail::voice_status_query_count = 0;
			const auto started = std::chrono::steady_clock::now();
			for (std::size_t i = 0; i < count; ++i)
			{
				const auto playing = mngl::audio::try_is_playing(voices[i]);
				const auto paused = mngl::audio::try_is_paused(voices[i]);
				if (!playing || !*playing || !paused || *paused)
				{
					std::cerr << "Looping audio voice returned an invalid state.\n";
					return 1;
				}
			}
			const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
			    std::chrono::steady_clock::now() - started);
			if (mngl::audio::detail::voice_status_query_count != count * 3)
			{
				std::cerr << "Voice polling inspected unrelated tracks: expected " << count * 3
				          << " mixer status calls, observed " << mngl::audio::detail::voice_status_query_count
				          << ".\n";
				return 1;
			}
			std::cout << "Polled " << count << " voices in " << elapsed.count() << " us\n";
		}

		if (!mngl::audio::try_pause(voices.front()) || mngl::audio::is_playing(voices.front()) ||
		    !mngl::audio::is_paused(voices.front()) || !mngl::audio::try_resume(voices.front()))
		{
			std::cerr << "Paused voice state was not preserved by individual queries.\n";
			return 1;
		}
		if (!mngl::audio::try_stop(voices.back()) || mngl::audio::is_playing(voices.back()) ||
		    mngl::audio::is_paused(voices.back()))
		{
			std::cerr << "Stopped voice handle did not retain its stale-handle behavior.\n";
			return 1;
		}
		std::error_code remove_error;
		std::filesystem::remove(wav_path, remove_error);
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
