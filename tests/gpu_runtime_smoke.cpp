#define APP_NAME "MNGL GPU runtime smoke"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#include <MNGL/device.hpp>

#include <exception>
#include <iostream>

int main()
{
	try
	{
		mngl::runtime::ensure();
		const mngl::gpu::device device;
		if (!device.raw())
		{
			std::cerr << "SDL GPU device creation returned an empty handle.\n";
			return 77;
		}
		const char* driver = SDL_GetGPUDeviceDriver(device.raw());
		if (!driver)
		{
			std::cerr << "SDL GPU device did not report its driver.\n";
			return 1;
		}
		std::cout << "SDL GPU driver: " << driver << '\n';
		return 0;
	}
	catch (const mngl::sdl_error& error)
	{
		std::cerr << "No usable SDL GPU backend on this runner: " << error.what() << '\n';
		return 77;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Unexpected GPU runtime smoke failure: " << error.what() << '\n';
		return 1;
	}
}
