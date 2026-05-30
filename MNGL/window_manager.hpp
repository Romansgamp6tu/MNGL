#pragma once
#include "utilities.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>
namespace mngl
{
	class window;

	class window_manager
	{
	public:
		
		static std::vector<mngl::window*> get_all_windows()
		{
			std::lock_guard lg(mut);
			std::vector<mngl::window*> wins;
			for (const auto& [id, win] : elements)
				wins.push_back(win);
			return wins;
		}

		static mngl::window& get_window(SDL_Window* win)
		{
			std::lock_guard lg(mut);
			auto id = SDL_GetWindowID(win);
			return *elements.at(id);
		}

		static mngl::window& get_window(SDL_WindowID id)
		{
			std::lock_guard lg(mut);
			return *elements.at(id);
		}

		static mngl::window& parent(SDL_Window* win)
		{
			std::lock_guard lg(mut);
			auto prnt = SDL_GetWindowParent(win);
			if (!prnt) throw sdl_error("window does not have a parent!");
			return *elements.at(SDL_GetWindowID(prnt));
		}

		static mngl::window& parent(SDL_WindowID id)
		{
			std::lock_guard lg(mut);
			auto win = SDL_GetWindowFromID(id);
			auto prnt = SDL_GetWindowParent(win);
			if (!prnt) throw sdl_error("window does not have a parent!");
			return *elements.at(SDL_GetWindowID(prnt));
		}

		static window_manager& instance()
		{
			static window_manager w;
			return w;
		}

	private:
		friend class window;
		static std::unordered_map<SDL_WindowID, mngl::window*> elements;
		static std::mutex mut;
		static void add_window(SDL_Window* win, mngl::window* data)
		{
			std::lock_guard lg(mut);
			auto id = SDL_GetWindowID(win);
			elements.insert({ id, data });
		}
		static void remove_window(SDL_Window* win)
		{
			std::lock_guard lg(mut);
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
}