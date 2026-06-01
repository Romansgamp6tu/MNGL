#pragma once
#include "window_platform_sync.hpp"
#include "window_manager.hpp"
#include "window_core.hpp"

namespace mngl
{
	display display::for_window(const mngl::window& win)
	{
		std::lock_guard lg(mut);
		auto d = SDL_GetDisplayForWindow(win.raw());
		if (!d) throw sdl_error("failed to find display for window " + std::to_string(reinterpret_cast<uintptr_t>(win.raw())) + ": ");
		return d;
	}
}