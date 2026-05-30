#pragma once

#include "utilities.hpp"
#include <string>

#include "window_manager.hpp"


namespace mngl
{
	class window
	{
	public:

		struct flags
		{
			bool fullscreen			: 1 = false;
			bool hidden				: 1 = false;
			bool borderless			: 1 = false;
			bool resizeable			: 1 = false;
			bool minimized			: 1 = false;
			bool maximized			: 1 = false;
			bool mouse_grabbed		: 1 = false;
			bool modal				: 1 = false;
			bool high_pixel_density	: 1 = false;
			bool always_on_top		: 1 = false;
			bool utility			: 1 = false;
			bool tooltip			: 1 = false;
			bool popup_menu			: 1 = false;
			bool keyboard_grabbed	: 1 = false;
			bool transparent		: 1 = false;
			bool not_focusable		: 1 = false;
			enum class backends : uint8_t
			{
				none,
				opengl,
				vulkan,
				metal,
			} backend = backends::none;
			operator SDL_WindowFlags() const noexcept
			{
				SDL_WindowFlags f = 0;

				if (fullscreen)         f |= SDL_WINDOW_FULLSCREEN;
				if (hidden)             f |= SDL_WINDOW_HIDDEN;
				if (borderless)         f |= SDL_WINDOW_BORDERLESS;
				if (resizeable)         f |= SDL_WINDOW_RESIZABLE;
				if (minimized)          f |= SDL_WINDOW_MINIMIZED;
				if (maximized)          f |= SDL_WINDOW_MAXIMIZED;
				if (mouse_grabbed)      f |= SDL_WINDOW_MOUSE_GRABBED;
				if (modal)              f |= SDL_WINDOW_MODAL;
				if (high_pixel_density) f |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
				if (always_on_top)      f |= SDL_WINDOW_ALWAYS_ON_TOP;
				if (utility)            f |= SDL_WINDOW_UTILITY;
				if (tooltip)            f |= SDL_WINDOW_TOOLTIP;
				if (popup_menu)         f |= SDL_WINDOW_POPUP_MENU;
				if (keyboard_grabbed)   f |= SDL_WINDOW_KEYBOARD_GRABBED;
				if (transparent)        f |= SDL_WINDOW_TRANSPARENT;
				if (not_focusable)      f |= SDL_WINDOW_NOT_FOCUSABLE;

				// Графический бэкенд
				switch (backend)
				{
				case backends::opengl: f |= SDL_WINDOW_OPENGL; break;
				case backends::vulkan: f |= SDL_WINDOW_VULKAN; break;
				case backends::metal:  f |= SDL_WINDOW_METAL;  break;
				case backends::none:   break;
				}

				return f;
			}
		};

		window(std::string_view name, glm::uvec2 size, flags f = flags())
		{
			win = SDL_CreateWindow(name.data(), size.x, size.y, f);
			if (!win) throw sdl_error("failed to create window!");
			logger.debug("created window with parametrs: title = ", name, "; size = {", size.x, ",", size.y, "}; flags = ", SDL_WindowFlags(f));
			logger.debug("window located in ", win);
			window_manager::add_window(win, this);
		}

		window(const window&) = delete;
		window& operator=(const window&) = delete;

		window(window&& other)
		{
			win = other.win;
			other.win = nullptr;
			logger.debug("moved window ", win);
		}
		window& operator=(window&& other)
		{
			void* windump = win;
			if (win)
			{
				SDL_DestroyWindow(win);
				window_manager::remove_window(win);
				logger.debug("moved window ", win);
			}
			else
			{
				logger.debug("moved window ", win, " to ", windump);
			}
			win = other.win;
			other.win = nullptr;
			return *this;
		}

		~window()
		{
			void* windump = win;
			if (win)
			{
				SDL_DestroyWindow(win);
				window_manager::remove_window(win);
				logger.debug("destroyed window ", win);
			}
		}

		SDL_WindowID id() const
		{
			return SDL_GetWindowID(win);
		}



	private:
		SDL_Window* win = nullptr;
	};
}