#pragma once

#include "utilities.hpp"
#include <string>

#include "window_manager.hpp"
#include "display_mode.hpp"


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
			flags() = default;
			flags(SDL_WindowFlags fl)
			{
				fullscreen = (fl & SDL_WINDOW_FULLSCREEN) != 0;
				hidden = (fl & SDL_WINDOW_HIDDEN) != 0;
				borderless = (fl & SDL_WINDOW_BORDERLESS) != 0;
				resizeable = (fl & SDL_WINDOW_RESIZABLE) != 0;
				minimized = (fl & SDL_WINDOW_MINIMIZED) != 0;
				maximized = (fl & SDL_WINDOW_MAXIMIZED) != 0;
				mouse_grabbed = (fl & SDL_WINDOW_MOUSE_GRABBED) != 0;
				modal = (fl & SDL_WINDOW_MODAL) != 0;
				high_pixel_density = (fl & SDL_WINDOW_HIGH_PIXEL_DENSITY) != 0;
				always_on_top = (fl & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
				utility = (fl & SDL_WINDOW_UTILITY) != 0;
				tooltip = (fl & SDL_WINDOW_TOOLTIP) != 0;
				popup_menu = (fl & SDL_WINDOW_POPUP_MENU) != 0;
				keyboard_grabbed = (fl & SDL_WINDOW_KEYBOARD_GRABBED) != 0;
				transparent = (fl & SDL_WINDOW_TRANSPARENT) != 0;
				not_focusable = (fl & SDL_WINDOW_NOT_FOCUSABLE) != 0;
				if ((fl & SDL_WINDOW_OPENGL) != 0)
					backend = backends::opengl;
				else if ((fl & SDL_WINDOW_VULKAN) != 0)
					backend = backends::vulkan;
				else if ((fl & SDL_WINDOW_METAL) != 0)
					backend = backends::metal;
				else 
					backend = backends::none;
			}
		};

		window(std::string_view name, glm::uvec2 size, flags f = flags())
		{
			win = SDL_CreateWindow(name.data(), size.x, size.y, f);
			if (!win) throw sdl_error("failed to create window: ");
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

		operator SDL_WindowID() const
		{
			return id();
		}

		SDL_Window* raw() const
		{
			return win;
		}

		explicit operator SDL_Window* () const
		{
			return raw();
		}

		
		// геттеры и сеттеры

		// загаловок окна
		std::string_view title() const
		{
			auto* title = SDL_GetWindowTitle(win);
			if (!title) throw sdl_error("failed to get window title: ");
			return std::string_view(title);
		}
		void title(std::string_view new_title)
		{
			if (SDL_SetWindowTitle(win, new_title.data()) != 0) throw sdl_error("failed to set window title: ");
			logger.debug("changed window ", win, " title to ", new_title);
		}

		// размер окна
		glm::uvec2 size() const
		{
			int w, h;
			if (!SDL_GetWindowSize(win, &w, &h)) throw sdl_error("failed to get window size: ");
			return {static_cast<unsigned int>(w), static_cast<unsigned int>(h)};
		}
		void size(glm::uvec2 new_size)
		{
			SDL_SetWindowSize(win, new_size.x, new_size.y);
			logger.debug("changed window ", win, " size to {", new_size.x, ",", new_size.y, "}");
		}

		// позиция окна
		glm::ivec2 position() const
		{
			int x, y;
			if (!SDL_GetWindowPosition(win, &x, &y)) throw sdl_error("failed to get window position: ");
			return {x, y};
		}
		void position(glm::ivec2 new_pos)
		{
			if (!SDL_SetWindowPosition(win, new_pos.x, new_pos.y)) throw sdl_error("failed to set window position: ");
			logger.debug("changed window ", win, " position to {", new_pos.x, ",", new_pos.y, "}");
		}
		void move(glm::ivec2 delta)
		{
			auto pos = position();
			position(pos + delta);
		}
		
		//минимальный / максимальный размер окна
		glm::uvec2 min_size() const
		{
			int w, h;
			if (!SDL_GetWindowMinimumSize(win, &w, &h)) throw sdl_error("failed to get window minimum size: ");
			return {static_cast<unsigned int>(w), static_cast<unsigned int>(h)};
		}
		void min_size(glm::uvec2 new_min_size)
		{
			if (!SDL_SetWindowMinimumSize(win, new_min_size.x, new_min_size.y)) throw sdl_error("failed to set window minimum size: ");
			logger.debug("changed window ", win, " minimum size to {", new_min_size.x, ",", new_min_size.y, "}");
		}

		glm::uvec2 max_size() const
		{
			int w, h;
			if (!SDL_GetWindowMaximumSize(win, &w, &h)) throw sdl_error("failed to get window maximum size: ");
			return {static_cast<unsigned int>(w), static_cast<unsigned int>(h)};
		}
		void max_size(glm::uvec2 new_max_size)
		{
			if (!SDL_SetWindowMaximumSize(win, new_max_size.x, new_max_size.y)) throw sdl_error("failed to set window maximum size: ");
			logger.debug("changed window ", win, " maximum size to {", new_max_size.x, ",", new_max_size.y, "}");
		}

		// границы положения окна
		glm::imat2x2 borders()
		{
			glm::imat2x2 res;
			if (!SDL_GetWindowBordersSize(win, &res[0].x, &res[0].y, &res[1].x, &res[1].y)) throw sdl_error("failed to get window borders size: ");
			return res;
		}

		// флаги
		flags get_flags() const
		{
			return SDL_GetWindowFlags(win);
		}

		void show()
		{
			if(!SDL_ShowWindow(win)) throw sdl_error("failed to show window: ");
			logger.debug("shown window ", win);
		}

		void hide()
		{
			if(!SDL_HideWindow(win)) throw sdl_error("failed to hide windows: ");
			logger.debug("hidden window ", win);
		}

		void raise()
		{
			if(!SDL_RaiseWindow(win)) throw sdl_error("failed to raise window: ");
			logger.debug("raised window ", win);
		}

		void maximize()
		{
			if(!SDL_MaximizeWindow(win)) throw sdl_error("failed to maximize window: ");
			logger.debug("maximized window ", win);
		}

		void minimize()
		{
			if(!SDL_MinimizeWindow(win)) throw sdl_error("failed to minimize window: ");
			logger.debug("minimized window ", win);
		}

		void restore()
		{
			if(!SDL_RestoreWindow(win)) throw sdl_error("failed to restore window: ");
			logger.debug("restored window ", win);
		}

		//полный экран

		void fullscreen(bool to = true)
		{
			if (!SDL_SetWindowFullscreen(win, to ? SDL_WINDOW_FULLSCREEN : 0)) throw sdl_error("failed to set window fullscreen state: ");
			logger.debug((to ? "enabled" : "disabled"), " fullscreen for window ", win);
		}

		void unfullscreen()
		{
			fullscreen(false);
		}

		bool fullscreened() const
		{
			return (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
		}

	private:
		SDL_Window* win = nullptr;
	};
}