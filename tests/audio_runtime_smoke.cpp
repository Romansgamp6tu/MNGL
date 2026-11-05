#define APP_NAME "MNGL audio runtime smoke"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_AUDIO | SDL_INIT_EVENTS)

#include <MNGL/audio.hpp>

#include <exception>
#include <iostream>

int main()
{
	try
	{
		mngl::runtime::ensure();
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
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
