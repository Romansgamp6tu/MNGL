#pragma once
#include "utilities.hpp"
#include "pixel_format.hpp"
#include "rectangle.hpp"
#include "surface.hpp"
#include "event.hpp"

#include <cmath>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include <string>
#include <string_view>
#include <memory>

namespace mngl
{
	// Предварительные объявления (Forward declarations)
	class window;
	class display;
	struct display_mode;

	class window_manager
	{
	public:

		static std::vector<mngl::window*> get_all_windows()
		{
			std::shared_lock lg(mut);
			std::vector<mngl::window*> wins;
			wins.reserve(elements.size()); // Избегаем лишних реаллокаций памяти
			for (const auto& [id, win] : elements)
				wins.push_back(win);
			return wins;
		}

		static mngl::window& get_window(SDL_Window* win)
		{
			std::shared_lock lg(mut);
			auto id = SDL_GetWindowID(win);
			return *elements.at(id);
		}

		static mngl::window& get_window(SDL_WindowID id)
		{
			std::shared_lock lg(mut);
			return *elements.at(id);
		}

		static mngl::window& parent(SDL_Window* win)
		{
			std::shared_lock lg(mut);
			auto prnt = SDL_GetWindowParent(win);
			if (!prnt) throw sdl_error("window does not have a parent!");
			return *elements.at(SDL_GetWindowID(prnt));
		}

		static mngl::window& parent(SDL_WindowID id)
		{
			std::shared_lock lg(mut);
			auto win = SDL_GetWindowFromID(id);
			auto prnt = SDL_GetWindowParent(win);
			if (!prnt) throw sdl_error("window does not have a parent!");
			return *elements.at(SDL_GetWindowID(prnt));
		}

	private:
		friend class window;
		static inline std::unordered_map<SDL_WindowID, mngl::window*> elements;
		static inline std::shared_mutex mut;

		static void add_window(SDL_Window* win, mngl::window* data)
		{
			std::unique_lock lg(mut);
			auto id = SDL_GetWindowID(win);
			elements.insert_or_assign(id, data);
		}

		static void remove_window(SDL_Window* win)
		{
			if (!win) return;
			std::unique_lock lg(mut);
			auto id = SDL_GetWindowID(win);
			elements.erase(id);
		}

		window_manager(const window_manager&) = delete;
		window_manager& operator=(const window_manager&) = delete;
		window_manager(window_manager&&) = delete;
		window_manager& operator=(window_manager&&) = delete;
		window_manager() = default;
		~window_manager() = default;
	};

	class display
	{
	public:
		~display() = default;
		display(const display& other) : disp(other.disp) {}
		display(display&& other) noexcept
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
			if (d == nullptr) throw sdl_error("error getting displays: ");
			std::vector<display> res;
			res.reserve(cnt);
			for (; cnt > 0; cnt--, d++)
			{
				res.push_back(*d);
			}
			return res; // RVO отлично сработает и без std::move
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
			if (!SDL_GetDisplayBounds(disp, mngl::rectangleProxy(rect, mngl::proxy_mode::output))) throw sdl_error("failed to get disply bounds: ");
			return rect;
		}

		mngl::rectangle usable_bounds() const
		{
			mngl::rectangle rect;
			if (!SDL_GetDisplayUsableBounds(disp, mngl::rectangleProxy(rect, mngl::proxy_mode::output))) throw sdl_error("failed to get disply bounds: ");
			return rect;
		}

		float scale() const
		{
			float s = SDL_GetDisplayContentScale(disp);
			if (s == 0.0f) throw sdl_error("failed to get content scale of display: ");
			return s;
		}

		mngl::display_orientation orientation() const
		{
			mngl::display_orientation orient =
				static_cast<mngl::display_orientation>(SDL_GetNaturalDisplayOrientation(disp));
#ifdef MNGL_DISPLAYORIENTATION_UNKNOWS_IS_ERROR
			if (orient == mngl::display_orientation::unknown) throw std::runtime_error("display orientation is unavaible!");
#endif
			return orient;
		}

		mngl::display_mode current_mode() const;
		std::vector<mngl::display_mode> fullscreen_modes() const;
		mngl::display_mode closest_mode(glm::ivec2 size, float fps, bool include_hdpi = true) const;

		static display for_point(glm::ivec2 pnt)
		{
			std::lock_guard lg(mut);
			SDL_Point point{ pnt.x, pnt.y }; // Безопасное создание структуры вместо reinterpret_cast
			auto d = SDL_GetDisplayForPoint(&point);
			if (!d) throw sdl_error("failed to find such display for point " + to_string(pnt) + ": ");
			return d;
		}

		static display for_rect(mngl::rectangle rect)
		{
			std::lock_guard lg(mut);
			auto d = SDL_GetDisplayForRect(mngl::rectangleProxy(rect, mngl::proxy_mode::input));
			if (!d) throw sdl_error("failed to find such display for rectangle at position " + to_string(rect.pos) + " and size " + to_string(rect.size) + ": ");
			return d;
		}

		static display for_window(const mngl::window& win);

	private:
		display() = default;
		display(SDL_DisplayID id) : disp(id) {}
		SDL_DisplayID disp = 0;
		static inline std::mutex mut;

		friend struct display_mode;
	};

	struct display_mode
	{
		display_mode(SDL_DisplayMode mode) :
			disp(mode.displayID),
			format(static_cast<mngl::pixel_format>(mode.format)),
			size({ mode.w, mode.h }),
			pixel_density(mode.pixel_density),
			fps(mode.refresh_rate),
			fps_rational({ mode.refresh_rate_numerator, mode.refresh_rate_denominator })
		{}
		~display_mode() = default;

		mngl::display disp;
		mngl::pixel_format format;
		glm::ivec2 size;
		float pixel_density;
		float fps;
		glm::ivec2 fps_rational;

		operator SDL_DisplayMode() const
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
		display_mode() = default; // не разрешено делать долбаёбам
		friend class display;
	};

	// Реализация отложенных методов display (после объявления display_mode)
	inline mngl::display_mode display::current_mode() const
	{
		const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(disp);
		if (!mode) throw sdl_error("failed to get display mode: ");
		return *mode;
	}

	inline std::vector<mngl::display_mode> display::fullscreen_modes() const
	{
		int cnt = 0;
		auto raw = SDL_GetFullscreenDisplayModes(disp, &cnt);
		auto deleter = [](SDL_DisplayMode** p) { if (p) SDL_free(p); };
		std::unique_ptr<SDL_DisplayMode*, decltype(deleter)> modes(raw, deleter);
		std::vector<mngl::display_mode> view;
		view.reserve(cnt);
		if (!modes && cnt > 0) throw sdl_error("error getting fullscreen modes: ");
		for (auto i = modes.get(); i < modes.get() + cnt; i++)
		{
			if (*i) view.push_back(**i);
		}
		return view;
	}

	inline mngl::display_mode display::closest_mode(glm::ivec2 size, float fps, bool include_hdpi) const
	{
		SDL_DisplayMode out;
		if (!SDL_GetClosestFullscreenDisplayMode(disp, size.x, size.y, fps, include_hdpi, &out)) throw sdl_error("error finding closest display mode: ");
		return out;
	}

	class window
	{
	public:

		struct flags
		{
			bool fullscreen : 1 = false;
			bool hidden : 1 = false;
			bool borderless : 1 = false;
			bool resizeable : 1 = false;
			bool minimized : 1 = false;
			bool maximized : 1 = false;
			bool mouse_grabbed : 1 = false;
			bool modal : 1 = false;
			bool high_pixel_density : 1 = false;
			bool always_on_top : 1 = false;
			bool utility : 1 = false;
			bool tooltip : 1 = false;
			bool popup_menu : 1 = false;
			bool keyboard_grabbed : 1 = false;
			bool transparent : 1 = false;
			bool not_focusable : 1 = false;

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

		window(const std::string& name, glm::uvec2 size, flags f = flags())
		{
			win = SDL_CreateWindow(name.data(), size.x, size.y, f);
			if (!win) throw sdl_error("failed to create window: ");
			logger.debug("created window with parametrs: title = ", name, "; size = {", size.x, ",", size.y, "}; flags = ", SDL_WindowFlags(f));
			logger.debug("window located in ", win);
			window_manager::add_window(win, this);
		}

		window(const window&) = delete;
		window& operator=(const window&) = delete;

		window(window&& other) noexcept : win(other.win)
		{
			if (win)
			{
				window_manager::add_window(win, this);
				logger.debug("moved window ", win);
			}
			other.win = nullptr;
		}

		window& operator=(window&& other) noexcept
		{
			if (this == &other) return *this;
			if (win)
			{
				SDL_Window* old_win = win;
				window_manager::remove_window(old_win);
				win = nullptr;
				SDL_DestroyWindow(old_win);
				logger.debug("destroyed old window ", old_win, " due to move assignment");
			}
			win = other.win;
			if (win)
			{
				window_manager::add_window(win, this);
				other.win = nullptr;
				logger.debug("moved window ", win);
			}
			return *this;
		}

		virtual ~window()
		{
			if (win)
			{
				SDL_Window* old_win = win;
				window_manager::remove_window(old_win);
				win = nullptr;
				SDL_DestroyWindow(old_win);
				logger.debug("destroyed window ", old_win);
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

		// заголовок окна
		std::string title() const
		{
			auto* title = SDL_GetWindowTitle(win);
			if (!title) throw sdl_error("failed to get window title: ");
			return title;
		}

		void title(const std::string& new_title)
		{
			// ФИКС БАГА SDL3: проверка через оператор отрицания (!)
			if (!SDL_SetWindowTitle(win, new_title.data())) throw sdl_error("failed to set window title: ");
			logger.debug("changed window ", win, " title to ", new_title);
		}

		// размер окна
		glm::uvec2 size() const
		{
			int w, h;
			if (!SDL_GetWindowSize(win, &w, &h)) throw sdl_error("failed to get window size: ");
			return { static_cast<unsigned int>(w), static_cast<unsigned int>(h) };
		}

		void size(glm::uvec2 new_size)
		{
			if (!SDL_SetWindowSize(win, new_size.x, new_size.y)) throw sdl_error("failed to set window size: ");
			logger.debug("changed window ", win, " size to {", new_size.x, ",", new_size.y, "}");
		}

		// позиция окна
		glm::ivec2 position() const
		{
			int x, y;
			if (!SDL_GetWindowPosition(win, &x, &y)) throw sdl_error("failed to get window position: ");
			return { x, y };
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

		// минимальный / максимальный размер окна
		glm::uvec2 min_size() const
		{
			int w, h;
			if (!SDL_GetWindowMinimumSize(win, &w, &h)) throw sdl_error("failed to get window minimum size: ");
			return { static_cast<unsigned int>(w), static_cast<unsigned int>(h) };
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
			return { static_cast<unsigned int>(w), static_cast<unsigned int>(h) };
		}

		void max_size(glm::uvec2 new_max_size)
		{
			if (!SDL_SetWindowMaximumSize(win, new_max_size.x, new_max_size.y)) throw sdl_error("failed to set window maximum size: ");
			logger.debug("changed window ", win, " maximum size to {", new_max_size.x, ",", new_max_size.y, "}");
		}

		struct window_borders
		{
			int top = 0;
			int left = 0;
			int bottom = 0;
			int right = 0;
		};

		window_borders borders() const
		{
			window_borders res;
			if (!SDL_GetWindowBordersSize(win, &res.top, &res.left, &res.bottom, &res.right))
				throw sdl_error("failed to get window borders size: ");
			return res;
		}

		// флаги
		flags get_flags() const
		{
			return SDL_GetWindowFlags(win);
		}

		void show()
		{
			if (!SDL_ShowWindow(win)) throw sdl_error("failed to show window: ");
			logger.debug("shown window ", win);
		}

		void hide()
		{
			if (!SDL_HideWindow(win)) throw sdl_error("failed to hide windows: ");
			logger.debug("hidden window ", win);
		}

		void raise()
		{
			if (!SDL_RaiseWindow(win)) throw sdl_error("failed to raise window: ");
			logger.debug("raised window ", win);
		}

		void maximize()
		{
			if (!SDL_MaximizeWindow(win)) throw sdl_error("failed to maximize window: ");
			logger.debug("maximized window ", win);
		}

		void minimize()
		{
			if (!SDL_MinimizeWindow(win)) throw sdl_error("failed to minimize window: ");
			logger.debug("minimized window ", win);
		}

		void restore()
		{
			if (!SDL_RestoreWindow(win)) throw sdl_error("failed to restore window: ");
			logger.debug("restored window ", win);
		}

		// полный экран
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

		mngl::display_mode mode() const
		{
			return mngl::display::for_window(*this).current_mode();
		}

		mngl::display attached_display() const
		{
			return mngl::display::for_window(*this);
		}

		mngl::rectangle safe_area() const
		{
			mngl::rectangle res;
			if (!SDL_GetWindowSafeArea(win, mngl::rectangleProxy(res, mngl::proxy_mode::output))) throw sdl_error("failed to get safe area for window: ");
			return res;
		}

		void parent(const window& other)
		{
			if (!SDL_SetWindowParent(win, other.win)) throw sdl_error("failed to set parent for window: ");
		}

		window& parent()
		{
			return mngl::window_manager::parent(win);
		}

		const window& parent() const
		{
			return mngl::window_manager::parent(win);
		}

		void modal(bool to)
		{
			if (!SDL_SetWindowModal(win, to)) throw sdl_error("failed to set window modality to " + std::to_string(to) + ": ");
		}

		// другая поебень
		glm::uvec2 pixel_size() const
		{
			int w, h;
			if (!SDL_GetWindowSizeInPixels(win, &w, &h))
				throw sdl_error("failed to get window pixel size: ");
			return { static_cast<unsigned int>(w), static_cast<unsigned int>(h) };
		}

		float display_scale() const
		{
			float s = SDL_GetWindowDisplayScale(win);
			if (s <= 0.0f || !std::isfinite(s))
			{
				s = attached_display().scale();
			}
			return (s > 0.0f && std::isfinite(s)) ? s : 1.0f;
		}

		void aspect_ratio(float min_aspect, float max_aspect)
		{
			if (!SDL_SetWindowAspectRatio(win, min_aspect, max_aspect))
				throw sdl_error("failed to set window aspect ratio: ");
		}

		void mouse_grab(bool enable)
		{
			if (!SDL_SetWindowMouseGrab(win, enable))
				throw sdl_error("failed to set mouse grab: ");
		}

		bool mouse_grab() const
		{
			return SDL_GetWindowMouseGrab(win);
		}

		bool has_keyboard_focus() const
		{
			return (SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS) != 0;
		}

		void clipboard_text(const char* text)
		{
			if (!SDL_SetClipboardText(text)) throw sdl_error("failed to set clipboard text: ");
		}

		std::string clipboard_text() const
		{
			auto* text = SDL_GetClipboardText();
			if (!text) return "";
			std::string str(text);
			SDL_free(text);
			return str;
		}

		void flash(bool enable = true)
		{
			if (!SDL_FlashWindow(win, enable ? SDL_FLASH_UNTIL_FOCUSED : SDL_FLASH_BRIEFLY)) throw sdl_error("failed to flash window: ");
		}

		float opacity() const
		{
			// ФИКС БАГА С ПРИОРИТЕТОМ: разделили вызов функции и проверку на ошибку
			float val = SDL_GetWindowOpacity(win);
			if (val < 0.0f) throw sdl_error("failed to get window opacity: ");
			return val;
		}

		void opacity(float factor)
		{
			if (!SDL_SetWindowOpacity(win, factor)) throw sdl_error("failed to set window opacity: ");
			logger.debug("changed window ", win, " opacity to ", factor);
		}

		void icon(mngl::surface& icon)
		{
			if (!SDL_SetWindowIcon(win, icon.raw())) throw sdl_error("failed to set icon!");

			if (icon.animated())
			{
				SDL_Window* raw_win = this->win;
				icon.on_frame_changed = [raw_win](SDL_Surface* new_frame)
				{
					if (!SDL_SetWindowIcon(raw_win, new_frame)) throw sdl_error("failed to set icon frame!");
				};
			}

			logger.debug("changed window ", win, " icon");
		}

		void borderless(bool enable)
		{
			// В SDL3 передаем !enable, так как функция ожидает параметр "bordered" (наличие рамки)
			if (!SDL_SetWindowBordered(win, !enable))
				throw sdl_error("failed to set window borderless state: ");
			logger.debug((enable ? "enabled" : "disabled"), " borderless mode for window ", win);
		}

		bool borderless() const
		{
			return (SDL_GetWindowFlags(win) & SDL_WINDOW_BORDERLESS) != 0;
		}

		bool transparent() const
		{
			return (SDL_GetWindowFlags(win) & SDL_WINDOW_TRANSPARENT) != 0;
		}

	private:
		SDL_Window* win = nullptr;
	};

	// Реализация display::for_window (после того как класс window полностью объявлен)
	inline display display::for_window(const mngl::window& win)
	{
		std::lock_guard lg(mut);
		auto d = SDL_GetDisplayForWindow(win.raw());
		if (!d) throw sdl_error("failed to find display for window " + std::to_string(reinterpret_cast<uintptr_t>(win.raw())) + ": ");
		return d;
	}
}
