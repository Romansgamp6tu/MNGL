#pragma once

#include "utilities.hpp"
#include "pixel_format.hpp"
#include "display.hpp"

namespace mngl
{
	struct display_mode
	{
		display_mode(SDL_DisplayMode mode) :
			disp(mode.displayID),
			format(static_cast<mngl::pixel_format>(mode.format)),
			size({mode.w, mode.h}),
			pixel_density(mode.pixel_density),
			fps(mode.refresh_rate),
			fps_rational({mode.refresh_rate_numerator, mode.refresh_rate_denominator})
		{ }
		~display_mode() { }
		mngl::display disp;
		mngl::pixel_format format;
		glm::ivec2 size;
		float pixel_density;
		float fps;
		glm::ivec2 fps_rational;

		operator SDL_DisplayMode()
		{
			return SDL_DisplayMode{
				disp.raw(),
				static_cast<SDL_PixelFormat>(format),
				size.x,
				size.y,
				pixel_density,
				fps,
				fps_rational.x,
				fps_rational.y,
				sdl_internal
			};
		}

	private:
		SDL_DisplayModeData* sdl_internal = nullptr; // не трогать, я не ебу что оно делает
		display_mode() { } // не разрешено делать долбаёбам
		friend class display;
		
	};

	mngl::display_mode display::current_mode() const
	{
		const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(disp);
		if (!mode) throw sdl_error("failed to get display mode: ");
		return *mode;
	}

	std::vector<mngl::display_mode> display::fullscreen_modes() const
	{
		std::vector<mngl::display_mode> data;
		int cnt = 0;
		std::unique_ptr<std::unique_ptr<SDL_DisplayMode, SDL_free>> modes(SDL_GetFullscreenDisplayModes(disp, &cnt));

	}

	mngl::display_mode display::closest_mode(glm::ivec2 size, float fps, bool include_hdpi = true) const
	{

	}
}