#define APP_NAME "MNGL audio runtime smoke"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_AUDIO | SDL_INIT_EVENTS)

#include <MNGL/audio.hpp>

#include <exception>
#include <iostream>
#include <array>
#include <memory>
#include <stdexcept>
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
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
