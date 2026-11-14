#include <MNGL/render_window.hpp>
#include <MNGL/runtime.hpp>

#include <SDL3/SDL.h>

#include <exception>
#include <iostream>
#include <memory>
#include <string_view>

int main()
{
	SDL_SetHint(SDL_HINT_GPU_DRIVER, "vulkan");
	try
	{
		mngl::runtime::ensure();
	}
	catch (const std::exception& error)
	{
		const std::string_view message(error.what());
		if (message.find("No available video device") != std::string_view::npos)
		{
			std::cerr << "Skipping Vulkan shader smoke test because no SDL video device is available\n";
			return 77;
		}
		if (message.find("Failed to start shader compiler 'shadercross'") != std::string_view::npos)
		{
			std::cerr << "Skipping Vulkan shader smoke test because shadercross is unavailable\n";
			return 77;
		}
		std::cerr << error.what() << '\n';
		return 1;
	}

	std::unique_ptr<mngl::render_window> window;
	try
	{
		mngl::window::flags flags{};
		flags.hidden = true;
		window =
		    std::make_unique<mngl::render_window>("MNGL Vulkan shader smoke", glm::uvec2{320, 240}, flags);
	}
	catch (const mngl::sdl_error&)
	{
		// CTest treats 77 as skipped when SDL or the Vulkan driver is unavailable.
		return 77;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}

	const char* driver = SDL_GetGPUDeviceDriver(window->gpu_device().raw());
	if (!driver || std::string_view(driver) != "vulkan")
		return 77;

	try
	{
		auto frame = window->frame();
		frame.display();
	}
	catch (const std::exception& error)
	{
		if (std::string_view(error.what()).find("Failed to start shader compiler 'shadercross'") !=
		    std::string_view::npos)
		{
			std::cerr << "Skipping Vulkan shader smoke test because shadercross is unavailable\n";
			return 77;
		}
		std::cerr << error.what() << '\n';
		return 1;
	}
	return 0;
}
