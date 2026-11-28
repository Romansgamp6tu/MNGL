#pragma once

#include "input_events.hpp"
#include "move_only_function.hpp"
#include "system_events.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mngl
{
	class input;
	class imgui_context;
	inline void poll_events();

	template <typename T>
	concept event_payload = std::derived_from<T, event_base>;

	namespace detail
	{
		struct mouse_pointer_event
		{
			SDL_MouseID mouse_id{};
			SDL_WindowID window_id{};
			float x{};
			float y{};
		};

		class mouse_pointer_tracker
		{
		public:
			void observe(const SDL_Event& event)
			{
				SDL_WindowID window_id = 0;
				SDL_MouseID mouse_id = 0;
				float x = 0.0f;
				float y = 0.0f;

				switch (event.type)
				{
				case SDL_EVENT_MOUSE_MOTION:
					window_id = event.motion.windowID;
					mouse_id = event.motion.which;
					x = event.motion.x;
					y = event.motion.y;
					break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
				case SDL_EVENT_MOUSE_BUTTON_UP:
					window_id = event.button.windowID;
					mouse_id = event.button.which;
					x = event.button.x;
					y = event.button.y;
					break;
				case SDL_EVENT_MOUSE_WHEEL:
					window_id = event.wheel.windowID;
					mouse_id = event.wheel.which;
					x = event.wheel.mouse_x;
					y = event.wheel.mouse_y;
					break;
				case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				{
					std::lock_guard lock(m_mutex);
					if (const auto window = m_windows.find(event.window.windowID); window != m_windows.end())
					{
						window->second.latest_mouse_id.reset();
					}
					return;
				}
				case SDL_EVENT_MOUSE_REMOVED:
				{
					std::lock_guard lock(m_mutex);
					for (auto window = m_windows.begin(); window != m_windows.end();)
					{
						window->second.positions.erase(event.mdevice.which);
						if (window->second.latest_mouse_id == event.mdevice.which)
						{
							window->second.latest_mouse_id.reset();
						}
						if (window->second.positions.empty() && !window->second.latest_mouse_id)
						{
							window = m_windows.erase(window);
						}
						else
						{
							++window;
						}
					}
					return;
				}
				case SDL_EVENT_WINDOW_DESTROYED:
				{
					std::lock_guard lock(m_mutex);
					m_windows.erase(event.window.windowID);
					return;
				}
				default:
					return;
				}

				if (window_id == 0)
				{
					return;
				}

				std::lock_guard lock(m_mutex);
				auto& window = m_windows[window_id];
				window.latest_mouse_id = mouse_id;
				window.positions.insert_or_assign(mouse_id, mouse_pointer_event{mouse_id, window_id, x, y});
			}

			[[nodiscard]] std::optional<mouse_pointer_event> latest_for_window(SDL_WindowID window_id) const
			{
				std::lock_guard lock(m_mutex);
				const auto window = m_windows.find(window_id);
				if (window == m_windows.end() || !window->second.latest_mouse_id)
				{
					return std::nullopt;
				}
				const auto position = window->second.positions.find(*window->second.latest_mouse_id);
				return position == window->second.positions.end() ? std::nullopt
				                                                  : std::optional(position->second);
			}

			[[nodiscard]] std::optional<mouse_pointer_event> for_mouse(SDL_WindowID window_id,
			                                                           SDL_MouseID mouse_id) const
			{
				std::lock_guard lock(m_mutex);
				const auto window = m_windows.find(window_id);
				if (window == m_windows.end())
				{
					return std::nullopt;
				}
				const auto position = window->second.positions.find(mouse_id);
				return position == window->second.positions.end() ? std::nullopt
				                                                  : std::optional(position->second);
			}

		private:
			struct window_state
			{
				std::optional<SDL_MouseID> latest_mouse_id;
				std::unordered_map<SDL_MouseID, mouse_pointer_event> positions;
			};

			mutable std::mutex m_mutex;
			std::unordered_map<SDL_WindowID, window_state> m_windows;
		};

		struct event_subscription_state
		{
			std::atomic_bool connected = true;
		};

		struct queued_imgui_input_event
		{
			SDL_Event event{};
			std::unique_ptr<char, decltype(&SDL_free)> text{nullptr, &SDL_free};
		};
	} // namespace detail

	class event_connection
	{
	public:
		event_connection() = default;
		event_connection(const event_connection&) = delete;
		event_connection& operator=(const event_connection&) = delete;

		event_connection(event_connection&& other) noexcept;
		event_connection& operator=(event_connection&& other) noexcept;
		~event_connection();

		void disconnect() noexcept;

		[[nodiscard]] bool connected() const noexcept
		{
			return m_state && m_state->connected.load(std::memory_order_acquire);
		}

		[[nodiscard]] explicit operator bool() const noexcept { return connected(); }

	private:
		friend class event_manager;
		friend class event_bus;

		explicit event_connection(std::shared_ptr<detail::event_subscription_state> state) noexcept
		    : m_state(std::move(state))
		{
		}

		std::shared_ptr<detail::event_subscription_state> m_state;
	};

	// =========================================================================
	// 2. ЦЕНТРАЛЬНЫЙ МЕНЕДЖЕР СОБЫТИЙ (EVENT BUS + SDL WATCHER)
	// =========================================================================
	class event_manager
	{
	public:
		template <event_payload T> using callback = detail::move_only_function<void(const T&)>;

		event_manager(const event_manager&) = delete;
		event_manager& operator=(const event_manager&) = delete;

		static event_manager& instance()
		{
			static event_manager inst;
			return inst;
		}

		template <event_payload T> [[nodiscard]] static event_connection on(callback<T> cb)
		{
			instance();
			auto subscription = std::make_shared<listener<T>>(std::move(cb));
			auto& listeners = listeners_for<T>();
			std::lock_guard lock(listeners.writer_mutex);
			const auto current = std::atomic_load_explicit(&listeners.callbacks, std::memory_order_acquire);
			auto updated = std::make_shared<typename listener_collection<T>::listener_vector>();
			updated->reserve(current->size() + 1);
			for (const auto& existing : *current)
			{
				if (auto live = existing.lock(); live && live->connected.load(std::memory_order_acquire))
				{
					updated->push_back(live);
				}
			}
			updated->push_back(subscription);
			std::shared_ptr<const typename listener_collection<T>::listener_vector> snapshot =
			    std::move(updated);
			std::atomic_store_explicit(&listeners.callbacks, std::move(snapshot), std::memory_order_release);
			return event_connection(std::move(subscription));
		}

		template <event_payload T> static void emit(const T& event)
		{
			if (event_collector)
			{
				if constexpr (std::copy_constructible<T>)
				{
					auto listeners = snapshot_listeners<T>();
					if (!listeners.empty())
					{
						event_collector->emplace_back([event, listeners = std::move(listeners)]
						                              { dispatch_typed_event(event, listeners); });
					}
				}
				else
				{
					throw std::logic_error("SDL event payloads must be copyable for deferred delivery");
				}
				return;
			}
			dispatch_typed_event(event);
		}

		static std::vector<SDL_Event> take_imgui_input_events()
		{
			// Text pointers remain valid until the next call on this thread.
			static thread_local std::vector<detail::queued_imgui_input_event> owned_events;
			owned_events = take_owned_imgui_input_events();
			std::vector<SDL_Event> events;
			events.reserve(owned_events.size());
			for (const auto& event : owned_events)
				events.push_back(event.event);
			return events;
		}

		static void set_imgui_input_enabled(bool enabled)
		{
			std::lock_guard lock(imgui_input_events_mutex);
			imgui_input_enabled.store(enabled, std::memory_order_relaxed);
			if (!enabled)
			{
				imgui_input_events.clear();
			}
		}

	private:
		friend class event_connection;
		friend class event_bus;
		friend class input;
		friend class imgui_context;
		friend void poll_events();

		using deferred_callback = detail::move_only_function<void()>;

		struct pending_event_batch
		{
			std::vector<deferred_callback> callbacks;
			std::size_t next_callback = 0;
		};

		// Callbacks are serialized while allowing nested emits and disconnects.
		static inline std::recursive_mutex dispatch_mutex;
		static inline std::mutex pending_events_mutex;
		static inline std::deque<pending_event_batch> pending_events;
		static inline thread_local std::vector<deferred_callback>* event_collector = nullptr;
		static inline std::vector<std::vector<deferred_callback>> reusable_callback_buffers;
		static inline std::mutex imgui_input_events_mutex;
		static inline std::vector<detail::queued_imgui_input_event> imgui_input_events;
		static inline detail::mouse_pointer_tracker mouse_pointer_events;
		static inline std::atomic_bool imgui_input_enabled = false;

		[[nodiscard]] static std::optional<detail::mouse_pointer_event> latest_mouse_pointer_for_window(
		    SDL_WindowID window_id)
		{
			return mouse_pointer_events.latest_for_window(window_id);
		}

		[[nodiscard]] static std::optional<detail::mouse_pointer_event> mouse_pointer_for_window(
		    SDL_WindowID window_id, SDL_MouseID mouse_id)
		{
			return mouse_pointer_events.for_mouse(window_id, mouse_id);
		}

		static std::vector<detail::queued_imgui_input_event> take_owned_imgui_input_events()
		{
			std::lock_guard lock(imgui_input_events_mutex);
			std::vector<detail::queued_imgui_input_event> events;
			events.swap(imgui_input_events);
			return events;
		}

		template <event_payload T> struct listener;

		template <event_payload T> static void dispatch_typed_event(const T& event)
		{
			auto listeners = snapshot_listeners<T>();
			dispatch_typed_event(event, listeners);
		}

		template <event_payload T>
		static void dispatch_typed_event(const T& event,
		                                 const std::vector<std::shared_ptr<listener<T>>>& listeners)
		{
			std::lock_guard lock(dispatch_mutex);
			for (const auto& listener : listeners)
			{
				if (listener->connected.load(std::memory_order_acquire) && listener->handler)
				{
					listener->handler(event);
				}
			}
		}

		template <event_payload T> static std::vector<std::shared_ptr<listener<T>>> snapshot_listeners()
		{
			const auto current = std::atomic_load_explicit(&listeners_for<T>().callbacks, std::memory_order_acquire);
			std::vector<std::shared_ptr<listener<T>>> snapshot;
			snapshot.reserve(current->size());
			for (const auto& weak_listener : *current)
			{
				if (auto listener = weak_listener.lock();
				    listener && listener->connected.load(std::memory_order_acquire))
				{
					snapshot.push_back(std::move(listener));
				}
			}
			return snapshot;
		}

		void queue_sdl_event(const SDL_Event& event)
		{
			// Copy pointer-backed SDL fields into typed payloads before the event
			// leaves the watcher.
			std::vector<deferred_callback> callbacks;
			{
				std::lock_guard lock(pending_events_mutex);
				if (!reusable_callback_buffers.empty())
				{
					callbacks = std::move(reusable_callback_buffers.back());
					reusable_callback_buffers.pop_back();
					callbacks.clear();
				}
			}
			auto* previous_collector = std::exchange(event_collector, &callbacks);
			try
			{
				dispatch_event(event);
			}
			catch (...)
			{
				event_collector = previous_collector;
				throw;
			}
			event_collector = previous_collector;

			if (callbacks.empty())
			{
				if (callbacks.capacity() <= 64)
				{
					std::lock_guard lock(pending_events_mutex);
					if (reusable_callback_buffers.size() < 64)
					{
						try
						{
							reusable_callback_buffers.push_back(std::move(callbacks));
						}
						catch (...)
						{
							// Reuse is an optimization; an empty SDL event has no work to queue.
						}
					}
				}
				return;
			}

			std::lock_guard lock(pending_events_mutex);
			pending_events.push_back({std::move(callbacks), 0});
		}

		static void dispatch_pending_events()
		{
			std::size_t callbacks_to_dispatch = 0;
			{
				std::lock_guard lock(pending_events_mutex);
				for (const auto& batch : pending_events)
				{
					callbacks_to_dispatch += batch.callbacks.size() - batch.next_callback;
				}
			}

			for (std::size_t callback_index = 0; callback_index < callbacks_to_dispatch; ++callback_index)
			{
				deferred_callback callback;
				{
					std::lock_guard lock(pending_events_mutex);
					if (pending_events.empty())
					{
						return;
					}

					auto& batch = pending_events.front();
					callback = std::move(batch.callbacks[batch.next_callback++]);
					if (batch.next_callback == batch.callbacks.size())
					{
						batch.callbacks.clear();
						if (batch.callbacks.capacity() <= 64 && reusable_callback_buffers.size() < 64)
						{
							try
							{
								reusable_callback_buffers.push_back(std::move(batch.callbacks));
							}
							catch (...)
							{
								// Reuse is an optimization; dispatch must still finish if the pool is full.
							}
						}
						pending_events.pop_front();
					}
				}
				callback();
			}
		}

		static bool is_imgui_input_event(Uint32 type) noexcept
		{
			switch (type)
			{
			case SDL_EVENT_MOUSE_MOTION:
			case SDL_EVENT_MOUSE_WHEEL:
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			case SDL_EVENT_TEXT_INPUT:
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
			case SDL_EVENT_WINDOW_MOUSE_ENTER:
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			case SDL_EVENT_WINDOW_FOCUS_LOST:
			case SDL_EVENT_GAMEPAD_ADDED:
			case SDL_EVENT_GAMEPAD_REMOVED:
				return true;
			default:
				return false;
			}
		}

		static void queue_imgui_input_event(const SDL_Event& event)
		{
			if (!imgui_input_enabled.load(std::memory_order_relaxed) || !is_imgui_input_event(event.type))
			{
				return;
			}

			try
			{
				std::lock_guard lock(imgui_input_events_mutex);
				if (!imgui_input_enabled.load(std::memory_order_relaxed))
				{
					return;
				}
				detail::queued_imgui_input_event queued;
				queued.event = event;
				if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text)
				{
					queued.text.reset(SDL_strdup(event.text.text));
					if (!queued.text)
						throw std::bad_alloc();
					queued.event.text.text = queued.text.get();
				}
				imgui_input_events.push_back(std::move(queued));
			}
			catch (const std::bad_alloc&)
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				             "Unable to queue an SDL input event for Dear ImGui.");
			}
		}

		event_manager() { SDL_AddEventWatch(sdl_event_watcher, this); }

		~event_manager() { SDL_RemoveEventWatch(sdl_event_watcher, this); }

		static bool SDLCALL sdl_event_watcher(void* user_data, SDL_Event* event) noexcept
		{
			auto* manager = static_cast<event_manager*>(user_data);
			try
			{
				mouse_pointer_events.observe(*event);
				queue_imgui_input_event(*event);
				manager->queue_sdl_event(*event);
			}
			catch (const std::exception& error)
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to queue SDL event for MNGL: %s",
				             error.what());
			}
			catch (...)
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to queue SDL event for MNGL.");
			}
			return true;
		}

		template <event_payload T> struct listener : detail::event_subscription_state
		{
			callback<T> handler;

			explicit listener(callback<T> value) : handler(std::move(value)) {}
		};

		template <event_payload T> struct listener_collection
		{
			using listener_vector = std::vector<std::weak_ptr<listener<T>>>;

			std::mutex writer_mutex;
			std::shared_ptr<const listener_vector> callbacks{
			    std::make_shared<const listener_vector>()};

			bool empty() const
			{
				const auto snapshot = std::atomic_load_explicit(&callbacks, std::memory_order_acquire);
				for (const auto& weak_listener : *snapshot)
				{
					if (auto listener = weak_listener.lock();
					    listener && listener->connected.load(std::memory_order_acquire))
					{
						return false;
					}
				}
				return true;
			}
		};

		template <event_payload T> static listener_collection<T>& listeners_for()
		{
			static listener_collection<T> listeners;
			return listeners;
		}

		void dispatch_event(const SDL_Event& e)
		{
			const auto type = static_cast<event_type>(e.type);
			const std::uint64_t ts = e.common.timestamp;

			switch (type)
			{
				// --- Application event_type ---
			case event_type::quit:
				if (!listeners_for<app_quit>().empty())
					emit(app_quit{{type, ts}});
				break;
			case event_type::terminating:
				if (!listeners_for<app_terminating>().empty())
					emit(app_terminating{{type, ts}});
				break;
			case event_type::low_memory:
				if (!listeners_for<app_low_memory>().empty())
					emit(app_low_memory{{type, ts}});
				break;
			case event_type::will_enter_background:
				if (!listeners_for<app_will_enter_background>().empty())
					emit(app_will_enter_background{{type, ts}});
				break;
			case event_type::did_enter_background:
				if (!listeners_for<app_did_enter_background>().empty())
					emit(app_did_enter_background{{type, ts}});
				break;
			case event_type::will_enter_foreground:
				if (!listeners_for<app_will_enter_foreground>().empty())
					emit(app_will_enter_foreground{{type, ts}});
				break;
			case event_type::did_enter_foreground:
				if (!listeners_for<app_did_enter_foreground>().empty())
					emit(app_did_enter_foreground{{type, ts}});
				break;
			case event_type::locale_changed:
				if (!listeners_for<app_locale_changed>().empty())
					emit(app_locale_changed{{type, ts}});
				break;
			case event_type::system_theme_changed:
				if (!listeners_for<app_system_theme_changed>().empty())
					emit(app_system_theme_changed{{type, ts}});
				break;

				// --- Display event_type ---
			case event_type::display_orientation:
				if (!listeners_for<display_orientation_event>().empty())
					emit(display_orientation_event{{{type, ts}, e.display.displayID},
					                               static_cast<display_orientation>(e.display.data1)});
				break;
			case event_type::display_added:
				if (!listeners_for<display_added>().empty())
					emit(display_added{{{type, ts}, e.display.displayID}});
				break;
			case event_type::display_removed:
				if (!listeners_for<display_removed>().empty())
					emit(display_removed{{{type, ts}, e.display.displayID}});
				break;
			case event_type::display_moved:
				if (!listeners_for<display_moved>().empty())
					emit(display_moved{{{type, ts}, e.display.displayID}});
				break;
			case event_type::display_desktop_mode_changed:
				if (!listeners_for<display_desktop_mode_changed>().empty())
					emit(display_desktop_mode_changed{{{type, ts}, e.display.displayID}});
				break;
			case event_type::display_current_mode_changed:
				if (!listeners_for<display_current_mode_changed>().empty())
					emit(display_current_mode_changed{{{type, ts}, e.display.displayID}});
				break;
			case event_type::display_content_scale_changed:
				if (!listeners_for<display_content_scale_changed>().empty())
					emit(display_content_scale_changed{{{type, ts}, e.display.displayID},
					                                   (float)e.display.data1});
				break;
			case event_type::display_usable_bounds_changed:
				if (!listeners_for<display_usable_bounds_changed>().empty())
					emit(display_usable_bounds_changed{{{type, ts}, e.display.displayID}});
				break;

				// --- Window event_type ---
			case event_type::window_shown:
				if (!listeners_for<window_shown>().empty())
					emit(window_shown{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_hidden:
				if (!listeners_for<window_hidden>().empty())
					emit(window_hidden{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_exposed:
				if (!listeners_for<window_exposed>().empty())
					emit(window_exposed{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_moved:
				if (!listeners_for<window_moved>().empty())
					emit(window_moved{{{type, ts}, e.window.windowID}, {e.window.data1, e.window.data2}});
				break;
			case event_type::window_resized:
				if (!listeners_for<window_resized>().empty())
					emit(window_resized{{{type, ts}, e.window.windowID}, {e.window.data1, e.window.data2}});
				break;
			case event_type::window_pixel_size_changed:
				if (!listeners_for<window_pixel_size_changed>().empty())
					emit(window_pixel_size_changed{{{type, ts}, e.window.windowID},
					                               {e.window.data1, e.window.data2}});
				break;
			case event_type::window_metal_view_resized:
				if (!listeners_for<window_metal_view_resized>().empty())
					emit(window_metal_view_resized{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_minimized:
				if (!listeners_for<window_minimized>().empty())
					emit(window_minimized{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_maximized:
				if (!listeners_for<window_maximized>().empty())
					emit(window_maximized{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_restored:
				if (!listeners_for<window_restored>().empty())
					emit(window_restored{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_mouse_enter:
				if (!listeners_for<window_mouse_enter>().empty())
					emit(window_mouse_enter{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_mouse_leave:
				if (!listeners_for<window_mouse_leave>().empty())
					emit(window_mouse_leave{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_focus_gained:
				if (!listeners_for<window_focus_gained>().empty())
					emit(window_focus_gained{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_focus_lost:
				if (!listeners_for<window_focus_lost>().empty())
					emit(window_focus_lost{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_close_request:
				if (!listeners_for<window_close_request>().empty())
					emit(window_close_request{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_hit_test:
				if (!listeners_for<window_hit_test>().empty())
					emit(window_hit_test{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_iccprof_changed:
				if (!listeners_for<window_icc_prof_changed>().empty())
					emit(window_icc_prof_changed{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_display_changed:
				if (!listeners_for<window_display_changed>().empty())
					emit(window_display_changed{{{type, ts}, e.window.windowID},
					                            static_cast<std::uint32_t>(e.window.data1)});
				break;
			case event_type::window_display_scale_changed:
				if (!listeners_for<window_display_scale_changed>().empty())
					emit(window_display_scale_changed{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_safe_area_changed:
				if (!listeners_for<window_safe_area_changed>().empty())
					emit(window_safe_area_changed{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_occluded:
				if (!listeners_for<window_occluded>().empty())
					emit(window_occluded{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_enter_fullscreen:
				if (!listeners_for<window_enter_fullscreen>().empty())
					emit(window_enter_fullscreen{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_leave_fullscreen:
				if (!listeners_for<window_leave_fullscreen>().empty())
					emit(window_leave_fullscreen{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_destroyed:
				if (!listeners_for<window_destroyed>().empty())
					emit(window_destroyed{{{type, ts}, e.window.windowID}});
				break;
			case event_type::window_hdr_state_changed:
				if (!listeners_for<window_hdr_state_changed>().empty())
					emit(window_hdr_state_changed{{{type, ts}, e.window.windowID}});
				break;

				// --- Keyboard event_type ---
			case event_type::key_down:
				if (!listeners_for<key_down>().empty())
				{
					emit(key_down{{{type, ts}, e.key.windowID},
					              e.key.which,
					              static_cast<scancode>(e.key.scancode),
					              keycode{e.key.key},
					              static_cast<keymod>(e.key.mod),
					              e.key.repeat != 0,
					              e.key.down != 0});
				}
				break;
			case event_type::key_up:
				if (!listeners_for<key_up>().empty())
				{
					emit(key_up{{{type, ts}, e.key.windowID},
					            e.key.which,
					            static_cast<scancode>(e.key.scancode),
					            keycode{e.key.key},
					            static_cast<keymod>(e.key.mod),
					            e.key.repeat != 0,
					            e.key.down != 0});
				}
				break;
			case event_type::text_editing:
				if (!listeners_for<text_editing>().empty())
				{
					emit(text_editing{{{type, ts}, e.edit.windowID},
					                  e.edit.text ? e.edit.text : "",
					                  e.edit.start,
					                  e.edit.length});
				}
				break;
			case event_type::text_input:
				if (!listeners_for<text_input>().empty())
					emit(text_input{{{type, ts}, e.text.windowID}, e.text.text ? e.text.text : ""});
				break;
			case event_type::keymap_changed:
				if (!listeners_for<keymap_changed>().empty())
					emit(keymap_changed{{type, ts}});
				break;
			case event_type::keyboard_added:
				if (!listeners_for<keyboard_added>().empty())
					emit(keyboard_added{{type, ts}, e.kdevice.which});
				break;
			case event_type::keyboard_removed:
				if (!listeners_for<keyboard_removed>().empty())
					emit(keyboard_removed{{type, ts}, e.kdevice.which});
				break;
			case event_type::text_editing_candidates:
				if (!listeners_for<text_editing_candidates>().empty())
				{
					text_editing_candidates candidates{{{type, ts}, e.edit_candidates.windowID},
					                                   {},
					                                   e.edit_candidates.selected_candidate,
					                                   e.edit_candidates.horizontal};
					const auto candidate_count = std::max(e.edit_candidates.num_candidates, 0);
					if (e.edit_candidates.candidates)
					{
						candidates.candidates.reserve(static_cast<std::size_t>(candidate_count));
						for (std::int32_t index = 0; index < candidate_count; ++index)
						{
							const char* candidate = e.edit_candidates.candidates[index];
							candidates.candidates.emplace_back(candidate ? candidate : "");
						}
					}
					emit(candidates);
				}
				break;
			case event_type::screen_keyboard_shown:
				if (!listeners_for<screen_keyboard_shown>().empty())
					emit(screen_keyboard_shown{{type, ts}});
				break;
			case event_type::screen_keyboard_hidden:
				if (!listeners_for<screen_keyboard_hidden>().empty())
					emit(screen_keyboard_hidden{{type, ts}});
				break;

				// --- Mouse event_type ---
			case event_type::mouse_motion:
				if (!listeners_for<mouse_motion>().empty())
				{
					emit(mouse_motion{{{type, ts}, e.motion.windowID},
					                  e.motion.which,
					                  {e.motion.x, e.motion.y},
					                  {e.motion.xrel, e.motion.yrel},
					                  e.motion.state});
				}
				break;
			case event_type::mouse_button_down:
				if (!listeners_for<mouse_button_down>().empty())
				{
					emit(mouse_button_down{{{type, ts}, e.button.windowID},
					                       e.button.which,
					                       e.button.button,
					                       e.button.clicks,
					                       e.button.down != 0,
					                       {e.button.x, e.button.y}});
				}
				break;
			case event_type::mouse_button_up:
				if (!listeners_for<mouse_button_up>().empty())
				{
					emit(mouse_button_up{{{type, ts}, e.button.windowID},
					                     e.button.which,
					                     e.button.button,
					                     e.button.clicks,
					                     e.button.down != 0,
					                     {e.button.x, e.button.y}});
				}
				break;
			case event_type::mouse_wheel:
				if (!listeners_for<mouse_wheel>().empty())
				{
					emit(mouse_wheel{{{type, ts}, e.wheel.windowID},
					                 e.wheel.which,
					                 {e.wheel.x, e.wheel.y},
					                 {e.wheel.mouse_x, e.wheel.mouse_y},
					                 {e.wheel.integer_x, e.wheel.integer_y},
					                 static_cast<wheel_direction>(e.wheel.direction)});
				}
				break;
			case event_type::mouse_added:
				if (!listeners_for<mouse_added>().empty())
					emit(mouse_added{{type, ts}, e.mdevice.which});
				break;
			case event_type::mouse_removed:
				if (!listeners_for<mouse_removed>().empty())
					emit(mouse_removed{{type, ts}, e.mdevice.which});
				break;

				// --- Joystick event_type ---
			case event_type::joystick_axis_motion:
				if (!listeners_for<joystick_axis_moved>().empty())
					emit(joystick_axis_moved{{type, ts}, e.jaxis.which, e.jaxis.axis, e.jaxis.value});
				break;
			case event_type::joystick_ball_motion:
				if (!listeners_for<joystick_ball_moved>().empty())
					emit(joystick_ball_moved{
					    {type, ts}, e.jball.which, e.jball.ball, {e.jball.xrel, e.jball.yrel}});
				break;
			case event_type::joystick_hat_motion:
				if (!listeners_for<joystick_hat_pressed>().empty())
					emit(joystick_hat_pressed{
					    {type, ts}, e.jhat.which, e.jhat.hat, static_cast<hat_position>(e.jhat.value)});
				break;
			case event_type::joystick_button_down:
				if (!listeners_for<joystick_button_down>().empty())
					emit(joystick_button_down{
					    {type, ts}, e.jbutton.which, e.jbutton.button, e.jbutton.down != 0});
				break;
			case event_type::joystick_button_up:
				if (!listeners_for<joystick_button_up>().empty())
					emit(joystick_button_up{
					    {type, ts}, e.jbutton.which, e.jbutton.button, e.jbutton.down != 0});
				break;
			case event_type::joystick_added:
				if (!listeners_for<joystick_added>().empty())
					emit(joystick_added{{type, ts}, e.jdevice.which});
				break;
			case event_type::joystick_removed:
				if (!listeners_for<joystick_removed>().empty())
					emit(joystick_removed{{type, ts}, e.jdevice.which});
				break;
			case event_type::joystick_update_complete:
				if (!listeners_for<joystick_update_complete>().empty())
					emit(joystick_update_complete{{type, ts}, e.jdevice.which});
				break;
			case event_type::joystick_battery_updated:
				if (!listeners_for<joystick_battery>().empty())
					emit(joystick_battery{{type, ts},
					                      e.jbattery.which,
					                      static_cast<power_state>(e.jbattery.state),
					                      (float)e.jbattery.percent});
				break;

				// --- Gamepad event_type ---
			case event_type::gamepad_axis_motion:
				if (!listeners_for<gamepad_axis_event>().empty())
					emit(gamepad_axis_event{
					    {type, ts}, e.gaxis.which, static_cast<gamepad_axis>(e.gaxis.axis), e.gaxis.value});
				break;
			case event_type::gamepad_button_down:
				if (!listeners_for<gamepad_button_down>().empty())
					emit(gamepad_button_down{{type, ts},
					                         e.gbutton.which,
					                         static_cast<gamepad_button>(e.gbutton.button),
					                         e.gbutton.down != 0});
				break;
			case event_type::gamepad_button_up:
				if (!listeners_for<gamepad_button_up>().empty())
					emit(gamepad_button_up{{type, ts},
					                       e.gbutton.which,
					                       static_cast<gamepad_button>(e.gbutton.button),
					                       e.gbutton.down != 0});
				break;
			case event_type::gamepad_added:
				if (!listeners_for<gamepad_added>().empty())
					emit(gamepad_added{{type, ts}, e.gdevice.which});
				break;
			case event_type::gamepad_removed:
				if (!listeners_for<gamepad_removed>().empty())
					emit(gamepad_removed{{type, ts}, e.gdevice.which});
				break;
			case event_type::gamepad_remapped:
				if (!listeners_for<gamepad_remapped>().empty())
					emit(gamepad_remapped{{type, ts}, e.gdevice.which});
				break;
			case event_type::gamepad_update_complete:
				if (!listeners_for<gamepad_update_complete>().empty())
					emit(gamepad_update_complete{{type, ts}, e.gdevice.which});
				break;
			case event_type::gamepad_steam_handle_updated:
				if (!listeners_for<gamepad_steam_handle_updated>().empty())
					emit(gamepad_steam_handle_updated{{type, ts}, e.gdevice.which});
				break;
			case event_type::gamepad_touchpad_down:
				if (!listeners_for<gamepad_touchpad_down>().empty())
					emit(gamepad_touchpad_down{{type, ts},
					                           e.gtouchpad.which,
					                           e.gtouchpad.touchpad,
					                           e.gtouchpad.finger,
					                           {e.gtouchpad.x, e.gtouchpad.y},
					                           e.gtouchpad.pressure});
				break;
			case event_type::gamepad_touchpad_motion:
				if (!listeners_for<gamepad_touchpad_motion>().empty())
					emit(gamepad_touchpad_motion{{type, ts},
					                             e.gtouchpad.which,
					                             e.gtouchpad.touchpad,
					                             e.gtouchpad.finger,
					                             {e.gtouchpad.x, e.gtouchpad.y},
					                             e.gtouchpad.pressure});
				break;
			case event_type::gamepad_touchpad_up:
				if (!listeners_for<gamepad_touchpad_up>().empty())
					emit(gamepad_touchpad_up{{type, ts},
					                         e.gtouchpad.which,
					                         e.gtouchpad.touchpad,
					                         e.gtouchpad.finger,
					                         {e.gtouchpad.x, e.gtouchpad.y},
					                         e.gtouchpad.pressure});
				break;
			case event_type::gamepad_sensor_update:
				if (!listeners_for<gamepad_sensor>().empty())
					emit(gamepad_sensor{{type, ts},
					                    e.gsensor.which,
					                    static_cast<sensor_type>(e.gsensor.sensor),
					                    {e.gsensor.data[0], e.gsensor.data[1], e.gsensor.data[2]},
					                    e.gsensor.sensor_timestamp});
				break;

				// --- Touch event_type ---
			case event_type::finger_down:
				if (!listeners_for<finger_down>().empty())
					emit(finger_down{{type, ts},
					                 e.tfinger.touchID,
					                 e.tfinger.fingerID,
					                 e.tfinger.windowID,
					                 {e.tfinger.x, e.tfinger.y},
					                 {e.tfinger.dx, e.tfinger.dy},
					                 e.tfinger.pressure});
				break;
			case event_type::finger_motion:
				if (!listeners_for<finger_motion>().empty())
					emit(finger_motion{{type, ts},
					                   e.tfinger.touchID,
					                   e.tfinger.fingerID,
					                   e.tfinger.windowID,
					                   {e.tfinger.x, e.tfinger.y},
					                   {e.tfinger.dx, e.tfinger.dy},
					                   e.tfinger.pressure});
				break;
			case event_type::finger_up:
				if (!listeners_for<finger_up>().empty())
					emit(finger_up{{type, ts},
					               e.tfinger.touchID,
					               e.tfinger.fingerID,
					               e.tfinger.windowID,
					               {e.tfinger.x, e.tfinger.y},
					               {e.tfinger.dx, e.tfinger.dy},
					               e.tfinger.pressure});
				break;
			case event_type::finger_canceled:
				if (!listeners_for<finger_canceled>().empty())
					emit(finger_canceled{{type, ts},
					                     e.tfinger.touchID,
					                     e.tfinger.fingerID,
					                     e.tfinger.windowID,
					                     {e.tfinger.x, e.tfinger.y},
					                     {e.tfinger.dx, e.tfinger.dy},
					                     e.tfinger.pressure});
				break;
			case event_type::pinch_begin:
				if (!listeners_for<pinch_begin>().empty())
					emit(pinch_begin{{type, ts}, e.pinch.windowID, e.pinch.scale});
				break;
			case event_type::pinch_update:
				if (!listeners_for<pinch_update>().empty())
					emit(pinch_update{{type, ts}, e.pinch.windowID, e.pinch.scale});
				break;
			case event_type::pinch_end:
				if (!listeners_for<pinch_end>().empty())
					emit(pinch_end{{type, ts}, e.pinch.windowID, e.pinch.scale});
				break;
			case event_type::poll_sentinel:
				if (!listeners_for<poll_sentinel>().empty())
					emit(poll_sentinel{{type, ts}});
				break;

				// --- Clipboard event_type ---
			case event_type::clipboard_update:
				if (!listeners_for<clipboard_update>().empty())
				{
					clipboard_update update{{type, ts}, e.clipboard.owner, {}};
					if (e.clipboard.mime_types && e.clipboard.num_mime_types > 0)
					{
						update.mime_types.reserve(static_cast<std::size_t>(e.clipboard.num_mime_types));
						for (Sint32 index = 0; index < e.clipboard.num_mime_types; ++index)
						{
							const char* mime_type = e.clipboard.mime_types[index];
							if (mime_type)
							{
								update.mime_types.emplace_back(mime_type);
							}
						}
					}
					emit(update);
				}
				break;

				// --- Drop event_type ---
			case event_type::drop_file:
				if (!listeners_for<drop_file>().empty())
					emit(drop_file{{{type, ts}, e.drop.windowID},
					               {e.drop.x, e.drop.y},
					               e.drop.source ? e.drop.source : "",
					               e.drop.data ? e.drop.data : ""});
				break;
			case event_type::drop_text:
				if (!listeners_for<drop_text>().empty())
					emit(drop_text{{{type, ts}, e.drop.windowID},
					               {e.drop.x, e.drop.y},
					               e.drop.source ? e.drop.source : "",
					               e.drop.data ? e.drop.data : ""});
				break;
			case event_type::drop_begin:
				if (!listeners_for<drop_begin>().empty())
					emit(drop_begin{{{type, ts}, e.drop.windowID},
					                {e.drop.x, e.drop.y},
					                e.drop.source ? e.drop.source : ""});
				break;
			case event_type::drop_complete:
				if (!listeners_for<drop_complete>().empty())
					emit(drop_complete{{{type, ts}, e.drop.windowID},
					                   {e.drop.x, e.drop.y},
					                   e.drop.source ? e.drop.source : ""});
				break;
			case event_type::drop_position:
				if (!listeners_for<drop_position>().empty())
					emit(drop_position{{{type, ts}, e.drop.windowID},
					                   {e.drop.x, e.drop.y},
					                   e.drop.source ? e.drop.source : "",
					                   e.drop.data ? e.drop.data : ""});
				break;

				// --- Audio Device event_type ---
#if !defined(MNGL_CUSTOM_AUDIO_BACKEND)
			case event_type::audio_device_added:
				if (!listeners_for<audio_device_added>().empty())
					emit(audio_device_added{{type, ts}, e.adevice.which, e.adevice.recording != 0});
				break;
			case event_type::audio_device_removed:
				if (!listeners_for<audio_device_removed>().empty())
					emit(audio_device_removed{{type, ts}, e.adevice.which, e.adevice.recording != 0});
				break;
			case event_type::audio_device_format_changed:
				if (!listeners_for<audio_device_format_changed>().empty())
					emit(audio_device_format_changed{{type, ts}, e.adevice.which, e.adevice.recording != 0});
				break;
#endif

				// --- Sensor event_type ---
			case event_type::sensor_update:
				if (!listeners_for<sensor_event>().empty())
				{
					sensor_event se{{type, ts}, e.sensor.which, {}, e.sensor.sensor_timestamp};
					std::copy(std::begin(e.sensor.data), std::end(e.sensor.data), std::begin(se.data));
					emit(se);
				}
				break;

				// --- Pen event_type ---
			case event_type::pen_proximity_in:
				if (!listeners_for<pen_proximity_in>().empty())
					emit(pen_proximity_in{{type, ts}, e.pproximity.which, e.pproximity.windowID, {}});
				break;
			case event_type::pen_proximity_out:
				if (!listeners_for<pen_proximity_out>().empty())
					emit(pen_proximity_out{{type, ts}, e.pproximity.which, e.pproximity.windowID, {}});
				break;
			case event_type::pen_down:
				if (!listeners_for<pen_down>().empty())
					emit(pen_down{{type, ts},
					              e.ptouch.which,
					              e.ptouch.windowID,
					              {e.ptouch.x, e.ptouch.y},
					              e.ptouch.pen_state,
					              e.ptouch.eraser});
				break;
			case event_type::pen_up:
				if (!listeners_for<pen_up>().empty())
					emit(pen_up{{type, ts},
					            e.ptouch.which,
					            e.ptouch.windowID,
					            {e.ptouch.x, e.ptouch.y},
					            e.ptouch.pen_state,
					            e.ptouch.eraser});
				break;
			case event_type::pen_button_down:
				if (!listeners_for<pen_button_down>().empty())
					emit(pen_button_down{{type, ts},
					                     e.pbutton.which,
					                     e.pbutton.windowID,
					                     {e.pbutton.x, e.pbutton.y},
					                     e.pbutton.button,
					                     e.pbutton.pen_state});
				break;
			case event_type::pen_button_up:
				if (!listeners_for<pen_button_up>().empty())
					emit(pen_button_up{{type, ts},
					                   e.pbutton.which,
					                   e.pbutton.windowID,
					                   {e.pbutton.x, e.pbutton.y},
					                   e.pbutton.button,
					                   e.pbutton.pen_state});
				break;
			case event_type::pen_motion:
				if (!listeners_for<pen_motion>().empty())
					emit(pen_motion{{type, ts},
					                e.pmotion.which,
					                e.pmotion.windowID,
					                {e.pmotion.x, e.pmotion.y},
					                e.pmotion.pen_state});
				break;
			case event_type::pen_axis:
				if (!listeners_for<pen_axis>().empty())
					emit(pen_axis{{type, ts},
					              e.paxis.which,
					              e.paxis.windowID,
					              {e.paxis.x, e.paxis.y},
					              static_cast<pen_axis_type>(e.paxis.axis),
					              e.paxis.value,
					              e.paxis.pen_state});
				break;

				// --- Camera Device event_type ---
			case event_type::camera_device_added:
				if (!listeners_for<camera_device_added>().empty())
					emit(camera_device_added{{type, ts}, e.cdevice.which});
				break;
			case event_type::camera_device_removed:
				if (!listeners_for<camera_device_removed>().empty())
					emit(camera_device_removed{{type, ts}, e.cdevice.which});
				break;
			case event_type::camera_device_approved:
				if (!listeners_for<camera_device_approved>().empty())
					emit(camera_device_approved{{type, ts}, e.cdevice.which});
				break;
			case event_type::camera_device_denied:
				if (!listeners_for<camera_device_denied>().empty())
					emit(camera_device_denied{{type, ts}, e.cdevice.which});
				break;

				// --- Render event_type ---
			case event_type::render_targets_reset:
				if (!listeners_for<render_targets_reset>().empty())
					emit(render_targets_reset{{{type, ts}, e.render.windowID}});
				break;
			case event_type::render_device_reset:
				if (!listeners_for<render_device_reset>().empty())
					emit(render_device_reset{{{type, ts}, e.render.windowID}});
				break;
			case event_type::render_device_lost:
				if (!listeners_for<render_device_lost>().empty())
					emit(render_device_lost{{{type, ts}, e.render.windowID}});
				break;

			default:
				if (e.type >= SDL_EVENT_USER && e.type < SDL_EVENT_LAST &&
				    !listeners_for<user_event>().empty())
				{
					emit(user_event{{static_cast<event_type>(e.type), ts},
					                e.user.windowID,
					                e.user.code,
					                e.user.data1,
					                e.user.data2});
				}
				break;
			}
		}
	};

	// An instance-local event bus with synchronous emit() and an optional deferred queue.
	class event_bus
	{
	public:
		template <event_payload T> using callback = event_manager::callback<T>;

		event_bus() = default;
		explicit event_bus(std::size_t max_pending_events) noexcept
		    : m_max_pending_events(max_pending_events)
		{
		}

		event_bus(const event_bus&) = delete;
		event_bus& operator=(const event_bus&) = delete;
		event_bus(event_bus&&) = delete;
		event_bus& operator=(event_bus&&) = delete;

		~event_bus() { clear(); }

		template <event_payload T> [[nodiscard]] event_connection on(callback<T> cb)
		{
			auto subscription = std::make_shared<listener<T>>(std::move(cb));
			{
				std::lock_guard lock(m_registry_mutex);
				auto& listeners = listeners_for_locked<T>();
				const auto current = listeners.callbacks.load(std::memory_order_acquire);
				auto updated = std::make_shared<typename listener_collection<T>::listener_vector>();
				updated->reserve(current->size() + 1);
				for (const auto& existing : *current)
				{
					if (auto live = existing.lock(); live && live->connected.load(std::memory_order_acquire))
					{
						updated->push_back(live);
					}
				}
				updated->push_back(subscription);
				std::shared_ptr<const typename listener_collection<T>::listener_vector> snapshot =
				    std::move(updated);
				listeners.callbacks.store(std::move(snapshot), std::memory_order_release);
				std::erase_if(m_subscriptions, [](const auto& existing) { return existing.expired(); });
				m_subscriptions.push_back(subscription);
			}
			return event_connection(std::move(subscription));
		}

		// Call listeners immediately in the caller's thread.
		template <event_payload T> void emit(const T& event) const
		{
			std::lock_guard lock(event_manager::dispatch_mutex);
			auto listeners = snapshot_listeners<T>();
			if (listeners.empty())
			{
				return;
			}

			for (const auto& listener : listeners)
			{
				if (listener->connected.load(std::memory_order_acquire) && listener->handler)
				{
					listener->handler(event);
				}
			}
		}

		// Queue an owned event for delivery by dispatch_pending() in that call's thread.
		// A zero queue limit means unbounded; a full bounded queue throws length_error.
		template <typename T>
			requires event_payload<std::remove_cvref_t<T>> &&
			         std::move_constructible<std::remove_cvref_t<T>>
		void post(T&& event)
		{
			std::lock_guard lock(m_queue_mutex);
			if (queue_is_full()) [[unlikely]]
			{
				throw std::length_error("event_bus pending queue is full");
			}
			enqueue_locked(std::forward<T>(event));
		}

		// Like post(), but returns false instead of throwing when the bounded queue is full.
		template <typename T>
			requires event_payload<std::remove_cvref_t<T>> &&
			         std::move_constructible<std::remove_cvref_t<T>>
		[[nodiscard]] bool try_post(T&& event)
		{
			std::lock_guard lock(m_queue_mutex);
			if (queue_is_full())
			{
				return false;
			}
			enqueue_locked(std::forward<T>(event));
			return true;
		}

		[[nodiscard]] std::size_t pending_count() const
		{
			std::lock_guard lock(m_queue_mutex);
			return m_pending_events.size();
		}

		// Zero means that the queue has no configured limit.
		[[nodiscard]] std::size_t max_pending_events() const noexcept { return m_max_pending_events; }

		// Delivers a snapshot of queued events in FIFO order. Events posted by callbacks
		// are left for the next call. Call from the thread that should run the callbacks.
		[[nodiscard]] std::size_t dispatch_pending(
		    std::size_t max_events = std::numeric_limits<std::size_t>::max())
		{
			std::lock_guard dispatch_lock(m_pending_dispatch_mutex);
			std::size_t batch_size{};
			std::uint64_t batch_generation{};
			{
				std::lock_guard queue_lock(m_queue_mutex);
				batch_size = std::min(max_events, m_pending_events.size());
				batch_generation = m_pending_generation;
			}

			std::size_t dispatched = 0;
			for (std::size_t i = 0; i < batch_size; ++i)
			{
				detail::move_only_function<bool()> queued_event;
				{
					std::lock_guard queue_lock(m_queue_mutex);
					if (batch_generation != m_pending_generation || m_pending_events.empty())
					{
						break;
					}
					queued_event = std::move(m_pending_events.front());
					m_pending_events.pop_front();
				}

				if (queued_event())
				{
					++dispatched;
				}
			}
			return dispatched;
		}

		// Drop currently queued events without disconnecting listeners.
		void clear_pending() noexcept
		{
			auto discarded = [&]
			{
				std::lock_guard dispatch_lock(event_manager::dispatch_mutex);
				std::lock_guard queue_lock(m_queue_mutex);
				return take_pending_locked();
			}();
			(void)discarded; // Keep queued payloads alive until after the locks are released.
		}

		// Disconnect every subscription and discard queued events; the bus remains reusable.
		void clear() noexcept
		{
			auto discarded = [&]
			{
				std::lock_guard dispatch_lock(event_manager::dispatch_mutex);
				auto queued = [&]
				{
					std::lock_guard queue_lock(m_queue_mutex);
					return take_pending_locked();
				}();
				{
					std::lock_guard registry_lock(m_registry_mutex);
					for (const auto& subscription : m_subscriptions)
					{
						if (auto live = subscription.lock())
						{
							live->connected.store(false, std::memory_order_release);
						}
					}
					m_subscriptions.clear();
					m_listener_collections.clear();
				}
				return queued;
			}();
			(void)discarded; // Keep queued payloads alive until after the locks are released.
		}

	private:
		template <event_payload T> struct listener : detail::event_subscription_state
		{
			callback<T> handler;

			explicit listener(callback<T> value) : handler(std::move(value)) {}
		};

		template <event_payload T> struct listener_collection
		{
			using listener_vector = std::vector<std::weak_ptr<listener<T>>>;
			std::atomic<std::shared_ptr<const listener_vector>> callbacks{
			    std::make_shared<const listener_vector>()};
		};

		template <event_payload T> [[nodiscard]] static const void* type_key() noexcept
		{
			static const char key{};
			return &key;
		}

		template <event_payload T> listener_collection<T>& listeners_for_locked()
		{
			const auto key = type_key<T>();
			if (const auto found = m_listener_collections.find(key); found != m_listener_collections.end())
			{
				return *std::static_pointer_cast<listener_collection<T>>(found->second);
			}

			auto listeners = std::make_shared<listener_collection<T>>();
			m_listener_collections.emplace(key, listeners);
			return *listeners;
		}

		template <event_payload T>
		[[nodiscard]] std::vector<std::shared_ptr<listener<T>>> snapshot_listeners() const
		{
			std::shared_ptr<listener_collection<T>> listeners;
			{
				std::lock_guard lock(m_registry_mutex);
				const auto found = m_listener_collections.find(type_key<T>());
				if (found == m_listener_collections.end())
				{
					return {};
				}
				listeners = std::static_pointer_cast<listener_collection<T>>(found->second);
			}

			const auto current = listeners->callbacks.load(std::memory_order_acquire);
			std::vector<std::shared_ptr<listener<T>>> snapshot;
			snapshot.reserve(current->size());
			for (const auto& weak_listener : *current)
			{
				if (auto live = weak_listener.lock(); live && live->connected.load(std::memory_order_acquire))
				{
					snapshot.push_back(std::move(live));
				}
			}
			return snapshot;
		}

		[[nodiscard]] bool queue_is_full() const noexcept
		{
			return m_max_pending_events != 0 && m_pending_events.size() >= m_max_pending_events;
		}

		void advance_pending_generation_locked() noexcept
		{
			if (++m_pending_generation == 0)
			{
				++m_pending_generation;
			}
		}

		[[nodiscard]] std::deque<detail::move_only_function<bool()>> take_pending_locked() noexcept
		{
			advance_pending_generation_locked();
			auto discarded = std::move(m_pending_events);
			m_pending_events.clear();
			return discarded;
		}

		template <typename T> void enqueue_locked(T&& event)
		{
			using event_type = std::remove_cvref_t<T>;
			const std::uint64_t generation = m_pending_generation;
			m_pending_events.emplace_back(
			    [this, generation, payload = event_type(std::forward<T>(event))]() mutable
			    {
				    std::lock_guard dispatch_lock(event_manager::dispatch_mutex);
				    {
					    std::lock_guard queue_lock(m_queue_mutex);
					    if (generation != m_pending_generation)
						    return false;
				    }
				    emit(payload);
				    return true;
			    });
		}

		const std::size_t m_max_pending_events = 0;
		mutable std::mutex m_registry_mutex;
		mutable std::mutex m_queue_mutex;
		std::recursive_mutex m_pending_dispatch_mutex;
		std::unordered_map<const void*, std::shared_ptr<void>> m_listener_collections;
		std::vector<std::weak_ptr<detail::event_subscription_state>> m_subscriptions;
		std::deque<detail::move_only_function<bool()>> m_pending_events;
		std::uint64_t m_pending_generation = 1;
	};
	inline event_connection::event_connection(event_connection&& other) noexcept
	    : m_state(std::move(other.m_state))
	{
	}

	inline event_connection& event_connection::operator=(event_connection&& other) noexcept
	{
		if (this != &other)
		{
			disconnect();
			m_state = std::move(other.m_state);
		}
		return *this;
	}

	inline event_connection::~event_connection()
	{
		disconnect();
	}

	inline void event_connection::disconnect() noexcept
	{
		if (m_state)
		{
			std::lock_guard lock(event_manager::dispatch_mutex);
			m_state->connected.store(false, std::memory_order_release);
			m_state.reset();
		}
	}

	inline void poll_events()
	{
		SDL_PumpEvents();

		// Consume a fixed snapshot of the SDL queue. This keeps the standalone event
		// loop from filling SDL's bounded queue while leaving events posted during
		// this drain for the next call.
		constexpr std::size_t event_batch_size = 64;
		std::array<SDL_Event, event_batch_size> events{};
		int remaining_events = SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
		if (remaining_events < 0)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to inspect the SDL event queue: %s",
			             SDL_GetError());
			remaining_events = 0;
		}
		while (remaining_events > 0)
		{
			const int requested = std::min(remaining_events, static_cast<int>(events.size()));
			const int removed =
			    SDL_PeepEvents(events.data(), requested, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
			if (removed < 0)
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to drain the SDL event queue: %s",
				             SDL_GetError());
				break;
			}
			if (removed == 0)
			{
				break;
			}
			remaining_events -= removed;
		}

		event_manager::dispatch_pending_events();
	}
} // namespace mngl
