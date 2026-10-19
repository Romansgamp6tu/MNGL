#pragma once

#include "utilities.hpp"
#include <algorithm>
#include <atomic>
#include <bitset>
#include <mutex>

#include "event.hpp"
#include "iupdate.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/imgui.h"
#include <set>

namespace mngl
{
	class imgui_context;

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
		static inline std::atomic<double> dt = 0.1;
		static inline std::atomic<uint32_t> max_fps_limit = 60;
		static inline uint64_t next_frame_ns = 0;
		static inline std::atomic_bool frame_clock_initialized = false;

		static void wait_for_next_frame()
		{
			const uint32_t fps = max_fps_limit.load(std::memory_order_relaxed);
			if (fps == 0) return;

			const uint64_t frame_interval_ns =
				std::max<uint64_t>(SDL_NS_PER_SECOND / fps, 1);
			uint64_t now = SDL_GetTicksNS();
			if (!frame_clock_initialized.exchange(true, std::memory_order_relaxed))
			{
				next_frame_ns = now;
			}

			while (now < next_frame_ns)
			{
				SDL_DelayNS(next_frame_ns - now);
				now = SDL_GetTicksNS();
			}

			next_frame_ns += ((now - next_frame_ns) / frame_interval_ns + 1) * frame_interval_ns;
		}

		static void update_dt()
		{
			static uint64_t last_time = 0;
			if (!last_time) [[unlikely]]
			{
				last_time = performance_counter();
				return;
			}
			auto current_time = performance_counter();
			dt.store(
				std::min((current_time - last_time) / static_cast<double>(performance_frequency()), 0.1),
				std::memory_order_relaxed);
			last_time = current_time;
		}
		static void init_all()
		{
			[[maybe_unused]] static auto app_quit_subscription =
				event_manager::on<appQuit>([](const appQuit&)
				{
					should_work.store(false, std::memory_order_release);
				});
			[[maybe_unused]] static auto app_terminating_subscription =
				event_manager::on<appTerminating>([](const appTerminating&)
				{
					should_work.store(false, std::memory_order_release);
				});
			[[maybe_unused]] static auto window_close_subscription =
				event_manager::on<windowCloseRequest>([](const windowCloseRequest&)
				{
					should_work.store(false, std::memory_order_release);
				});

			[[maybe_unused]] static auto key_down_subscription =
				event_manager::on<keyDown>([](const keyDown& e)
				{
					if (!e.repeat) [[likely]]
					{
						std::lock_guard lock(state_mutex);
						if (!gui_wants_keyboard.load(std::memory_order_relaxed))
						{
							pressed_keys.insert(e.code);
						}
					}
				});
			[[maybe_unused]] static auto key_up_subscription =
				event_manager::on<keyUp>([](const keyUp& e)
				{
					std::lock_guard lock(state_mutex);
					if (!gui_wants_keyboard.load(std::memory_order_relaxed))
					{
						released_keys.insert(e.code);
					}
					else
					{
						released_keys.erase(e.code);
						pressed_keys.erase(e.code);
					}
					holded_keys.erase(e.code);
				});

			[[maybe_unused]] static auto mouse_motion_subscription =
				event_manager::on<mouseMotion>([](const mouseMotion& e)
				{
					std::lock_guard lock(state_mutex);
					update_mouse_position(
						e.position,
						!gui_wants_mouse.load(std::memory_order_relaxed));
				});
			[[maybe_unused]] static auto mouse_button_down_subscription =
				event_manager::on<mouseButtonDown>([](const mouseButtonDown& e)
				{
					std::lock_guard lock(state_mutex);
					const uint32_t mask = 1U << (e.button - 1);
					update_mouse_position(
						e.position,
						!gui_wants_mouse.load(std::memory_order_relaxed));
					if (!gui_wants_mouse.load(std::memory_order_relaxed))
					{
						mpressed |= mask;
					}
					else
					{
						mpressed &= ~mask;
						mholded &= ~mask;
					}
				});
			[[maybe_unused]] static auto mouse_button_up_subscription =
				event_manager::on<mouseButtonUp>([](const mouseButtonUp& e)
				{
					std::lock_guard lock(state_mutex);
					const uint32_t mask = 1U << (e.button - 1);
					const bool captured = gui_wants_mouse.load(std::memory_order_relaxed);
					update_mouse_position(e.position, !captured);
					if (!captured)
					{
						mreleased |= mask;
					}
					else
					{
						mpressed &= ~mask;
						mreleased &= ~mask;
					}
					mholded &= ~mask;
				});
			[[maybe_unused]] static auto mouse_wheel_subscription =
				event_manager::on<mouseWheel>([](const mouseWheel& e)
				{
					std::lock_guard lock(state_mutex);
					const bool captured = gui_wants_mouse.load(std::memory_order_relaxed);
					update_mouse_position(e.mousePos, !captured);
					if (!captured)
					{
						mscrolldelta += e.scroll;
					}
				});
		}

		static inline std::set<scancode> pressed_keys;
		static inline std::set<scancode> holded_keys;
		static inline std::set<scancode> released_keys;

		static inline glm::vec2 mpos;
		static inline glm::vec2 mdelta;
		static inline glm::vec2 mscrolldelta;

		static void update_mouse_position(glm::vec2 position, bool track_delta = true)
		{
			if (track_delta)
			{
				mdelta += position - mpos;
			}
			mpos = position;
		}

		static inline uint32_t mpressed;
		static inline uint32_t mholded;
		static inline uint32_t mreleased;
		static inline std::mutex state_mutex;
		static inline std::atomic_bool gui_wants_mouse = false;
		static inline std::atomic_bool gui_wants_keyboard = false;

		friend class imgui_context;

		static void set_gui_capture(bool mouse, bool keyboard)
		{
			gui_wants_mouse.store(mouse, std::memory_order_relaxed);
			gui_wants_keyboard.store(keyboard, std::memory_order_relaxed);

			if (!mouse && !keyboard)
			{
				return;
			}

			std::lock_guard lock(state_mutex);
			if (keyboard)
			{
				pressed_keys.clear();
				holded_keys.clear();
				released_keys.clear();
			}
			if (mouse)
			{
				mpressed = 0;
				mholded = 0;
				mreleased = 0;
				mdelta = { 0, 0 };
				mscrolldelta = { 0, 0 };
			}
		}

		static void update_info()
		{
			std::lock_guard lock(state_mutex);
			if (gui_wants_keyboard.load(std::memory_order_relaxed))
			{
				pressed_keys.clear();
				holded_keys.clear();
				released_keys.clear();
			}
			else
			{
				holded_keys.insert(pressed_keys.begin(), pressed_keys.end());
				pressed_keys.clear();
				released_keys.clear();
			}
			if (gui_wants_mouse.load(std::memory_order_relaxed))
			{
				mpressed = 0;
				mholded = 0;
				mreleased = 0;
			}
			else
			{
				mholded |= mpressed;
				mreleased = 0;
				mpressed = 0;
			}
			mdelta = {0, 0};
			mscrolldelta = {0, 0};
		}

		static void pool_events()
		{
			update_info();
			SDL_PumpEvents();
			const auto imgui_events = event_manager::take_imgui_input_events();
			if (ImGui::GetCurrentContext() &&
				ImGui::GetIO().BackendPlatformUserData != nullptr)
			{
				for (const SDL_Event& event : imgui_events)
				{
					ImGui_ImplSDL3_ProcessEvent(&event);
				}
			}
#ifndef NO_SDLINPUT_QUEUE_FLUSH
			SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
#endif
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
			return dt.load(std::memory_order_relaxed);
		}

		// Ноль отключает ограничение частоты кадров.
		static void set_max_fps(uint32_t fps)
		{
			max_fps_limit.store(fps, std::memory_order_relaxed);
			frame_clock_initialized.store(false, std::memory_order_relaxed);
		}

		static uint32_t get_max_fps()
		{
			return max_fps_limit.load(std::memory_order_relaxed);
		}

		static bool continue_work()
		{
			init_all();
			if (!should_work.load(std::memory_order_acquire)) [[unlikely]]
				return false;
			wait_for_next_frame();
			pool_events();
			if (!should_work.load(std::memory_order_acquire)) [[unlikely]]
				return false;
			//перерасчёт всех дельт, и тому подобного
			update_dt();
			update_manager::tick(dt.load(std::memory_order_relaxed));
			return true;
		}

		static bool is_key_pressed(scancode code)
		{
			if (gui_wants_keyboard.load(std::memory_order_relaxed))
			{
				return false;
			}
			std::lock_guard lock(state_mutex);
			return pressed_keys.find(code) != pressed_keys.end();
		}
		static bool is_key_holded(scancode code)
		{
			if (gui_wants_keyboard.load(std::memory_order_relaxed))
			{
				return false;
			}
			std::lock_guard lock(state_mutex);
			return holded_keys.find(code) != holded_keys.end();
		}
		static bool is_key_released(scancode code)
		{
			if (gui_wants_keyboard.load(std::memory_order_relaxed))
			{
				return false;
			}
			std::lock_guard lock(state_mutex);
			return released_keys.find(code) != released_keys.end();
		}

		static glm::vec2 get_mouse_position()
		{
			std::lock_guard lock(state_mutex);
			return mpos;
		}
		static glm::vec2 get_mouse_delta()
		{
			if (gui_wants_mouse.load(std::memory_order_relaxed))
			{
				return { 0, 0 };
			}
			std::lock_guard lock(state_mutex);
			return mdelta;
		}
		static glm::vec2 get_mouse_scroll_delta()
		{
			if (gui_wants_mouse.load(std::memory_order_relaxed))
			{
				return { 0, 0 };
			}
			std::lock_guard lock(state_mutex);
			return mscrolldelta;
		}

		static bool is_mouse_button_pressed(mouse_button button)
		{
			if (gui_wants_mouse.load(std::memory_order_relaxed))
			{
				return false;
			}
			std::lock_guard lock(state_mutex);
			return mpressed & (1 << (static_cast<uint8_t>(button) - 1));
		}
		static bool is_mouse_button_holded(mouse_button button)
		{
			if (gui_wants_mouse.load(std::memory_order_relaxed))
			{
				return false;
			}
			std::lock_guard lock(state_mutex);
			return mholded & (1 << (static_cast<uint8_t>(button) - 1));
		}
		static bool is_mouse_button_released(mouse_button button)
		{
			if (gui_wants_mouse.load(std::memory_order_relaxed))
			{
				return false;
			}
			std::lock_guard lock(state_mutex);
			return mreleased & (1 << (static_cast<uint8_t>(button) - 1));
		}



		inline static std::atomic_bool should_work = true;
	};
}