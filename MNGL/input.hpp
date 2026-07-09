#pragma once

#include "utilities.hpp"
#include <bitset>

#include "event.hpp"
#include "iupdate.h"
#include <set>

namespace mngl
{
	enum class mouse_button : uint8_t
	{
		left = 1, // SDL_BUTTON_LEFT
		middle = 2, // SDL_BUTTON_MIDDLE
		right = 3, // SDL_BUTTON_RIGHT
		x1 = 4, // SDL_BUTTON_X1
		x2 = 5  // SDL_BUTTON_X2
	};

	class Input
	{
	private:
		static inline double dt = 0.1;
		static void update_dt()
		{
			static uint64_t last_time = 0;
			if (!last_time) [[unlikely]]
			{
				last_time = performance_counter();
				return;
			}
			auto current_time = performance_counter();
			dt = std::min((current_time - last_time) / static_cast<double>(performance_frequency()), 0.1);
			last_time = current_time;
		}
		static void init_all()
		{
			static [[maybe_unused]] bool app_quit_inited = [&]() -> bool
			{
				event_manager::on<appQuit>([](const appQuit& e) { should_work = false; });
				event_manager::on<appTerminating>([](const appTerminating& e) { should_work = false; });
				event_manager::on<windowCloseRequest>([](const windowCloseRequest& e) { should_work = false; });
				return true;
			}();

			static [[maybe_unused]] bool kboard_inited = [&]() -> bool
			{
				event_manager::on<keyDown>([](const keyDown& e) { if(!e.repeat) [[likely]] pressed_keys.insert(e.code); });
				event_manager::on<keyUp>([](const keyUp& e) { released_keys.insert(e.code); holded_keys.erase(e.code); });
				return true;
			}();

			static [[maybe_unused]] bool mouse_inited = [&]() -> bool
			{
				event_manager::on<mouseMotion>([](const mouseMotion& e) { mdelta += e.position - mpos; mpos = e.position; });
				event_manager::on<mouseButtonDown>([](const mouseButtonDown& e) { mpressed |= 1 << (e.button - 1); });
				event_manager::on<mouseButtonUp>([](const mouseButtonUp& e) { mreleased |= 1 << (e.button - 1); mholded &= ~(1 << (e.button - 1)); });
				event_manager::on<mouseWheel>([](const mouseWheel& e) { mscrolldelta += e.scroll; });
				return true;
			}();
		}

		static inline std::set<scancode> pressed_keys;
		static inline std::set<scancode> holded_keys;
		static inline std::set<scancode> released_keys;

		static inline glm::vec2 mpos;
		static inline glm::vec2 mdelta;
		static inline glm::vec2 mscrolldelta;

		static inline uint32_t mpressed;
		static inline uint32_t mholded;
		static inline uint32_t mreleased;

		static void update_info()
		{
			holded_keys.insert(pressed_keys.begin(), pressed_keys.end());
			pressed_keys.clear();
			released_keys.clear();
			mholded |= mpressed;
			mreleased = 0;
			mpressed = 0;
			mdelta = {0, 0};
			mscrolldelta = {0, 0};
		}

		static void pool_events()
		{
			update_info();
			SDL_PumpEvents();
			SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
		}
	public:
		static uint64_t performance_counter()
		{
			return SDL_GetPerformanceCounter();
		}
		static uint64_t performance_frequency()
		{
			return SDL_GetPerformanceFrequency();
		}
		static double get_dt()
		{
			return dt;
		}
		static bool continue_work()
		{
			init_all();
			pool_events();
			if (!should_work) [[unlikely]]
				return false;
			//перерасчёт всех дельт, и тому подобного
			update_dt();
			update_manager::tick(dt);
			return true;
		}

		static bool is_key_pressed(scancode code)
		{
			return pressed_keys.find(code) != pressed_keys.end();
		}
		static bool is_key_holded(scancode code)
		{
			return holded_keys.find(code) != holded_keys.end();
		}
		static bool is_key_released(scancode code)
		{
			return released_keys.find(code) != released_keys.end();
		}

		static glm::vec2 get_mouse_position()
		{
			return mpos;
		}
		static glm::vec2 get_mouse_delta()
		{
			return mdelta;
		}
		static glm::vec2 get_mouse_scroll_delta()
		{
			return mscrolldelta;
		}

		static bool is_mouse_button_pressed(mouse_button button)
		{
			return mpressed & (1 << (static_cast<uint8_t>(button) - 1));
		}
		static bool is_mouse_button_holded(mouse_button button)
		{
			return mholded & (1 << (static_cast<uint8_t>(button) - 1));
		}
		static bool is_mouse_button_released(mouse_button button)
		{
			return mreleased & (1 << (static_cast<uint8_t>(button) - 1));
		}



		inline static bool should_work = true;
	};
}