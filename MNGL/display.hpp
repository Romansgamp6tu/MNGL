#pragma once

#include "utilities.hpp"
#include "rectangle.hpp"
#include "display_orientation.hpp"
#include <mutex>
#include <string>

namespace mngl
{
	class display_mode;

	class display
	{
	public:
		~display() = default;
		display(const display& other) : disp(other.disp) { }
		display(display&& other)
		{
			std::swap(other.disp, disp);
		}
		display& operator=(display other)
		{
			std::swap(other.disp, disp);
			return *this;
		}

		SDL_DisplayID raw() const
		{
			return disp;
		}

		static std::vector<display> displays()
		{
			std::lock_guard lg(mut);
			int cnt = 0;
			std::unique_ptr<SDL_DisplayID[], decltype(&SDL_free)> guard(SDL_GetDisplays(&cnt), &SDL_free);
			auto* d = guard.get();
			if (d == NULL) throw sdl_error("error getting displays: ");
			std::vector<display> res;
			res.reserve(cnt);
			for (; cnt > 0; cnt--, d++)
			{
				res.push_back(*d);
			}
			return std::move(res);
		}

		static display primary()
		{
			std::lock_guard lg(mut);
			auto d = SDL_GetPrimaryDisplay();
			if (d == 0) throw sdl_error("failed to get priary display: ");
			return d;
		}

		std::string_view name() const
		{
			auto* n = SDL_GetDisplayName(disp);
			if (!n) throw sdl_error("failed to get display name: ");
			return n;
		}

		mngl::rectangle bounds() const
		{
			mngl::rectangle rect;
			if(!SDL_GetDisplayBounds(disp, mngl::rectangleProxy(rect))) throw sdl_error("failed to get disply bounds: ");
			return rect;
		}

		mngl::rectangle usable_bounds() const
		{
			mngl::rectangle rect;
			if (!SDL_GetDisplayUsableBounds(disp, mngl::rectangleProxy(rect))) throw sdl_error("failed to get disply bounds: ");
			return rect;
		}

		float scale() const
		{
			float s = SDL_GetDisplayContentScale(disp);
			if (s == 0) throw sdl_error("failed to get content scale of display: ");
			return s;
		}

		mngl::display_orientation orientation() const
		{
			mngl::display_orientation orient = 
				static_cast<mngl::display_orientation>
				(SDL_GetNaturalDisplayOrientation(disp));
#ifdef MNGL_DISPLAYORIENTATION_UNKNOWS_IS_ERROR
			if (orient == mngl::display_orientation::unknown) throw std::runtime_error("display orientation is unavaible!");
#endif
			return orient;
		}

		//declarated in display_mode.hpp
		mngl::display_mode current_mode() const;
		std::vector<mngl::display_mode> fullscreen_modes() const;
		mngl::display_mode closest_mode(glm::ivec2 size, float fps, bool include_hdpi = true) const;

		static display for_point(glm::ivec2 pnt)
		{
			std::lock_guard lg(mut);
			auto d = SDL_GetDisplayForPoint(reinterpret_cast<SDL_Point*>(glm::value_ptr(pnt)));
			if (!d) throw sdl_error("failed to find such display for point " + to_string(pnt) + ": ");
			return d;
		}

		static display for_rect(mngl::rectangle rect)
		{
			std::lock_guard lg(mut);
			auto d = SDL_GetDisplayForRect(mngl::rectangleProxy(rect));
			if (!d) throw sdl_error("failed to find such display for rectangle at position " + to_string(rect.pos) + " and size " + to_string(rect.size) + ": ");
			return d;
		}

		//declarated in window.hpp
		static display for_window(const mngl::window& win);

	private:
		// запрет создания
		display() = default;
		display(SDL_DisplayID id) : disp(id) { }
		SDL_DisplayID disp = NULL;
		// так же мьютексы для многопоточки
		static std::mutex mut;

		friend class display_mode;
	};
}