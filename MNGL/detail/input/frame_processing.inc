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
		}

		static void update_info()
		{
			const auto touch_ids = enumerate_touch_ids();
			std::lock_guard lock(state_mutex);
			for (auto it = keyboard_states.begin(); it != keyboard_states.end();)
			{
				it->second.pressed.reset();
				it->second.released.reset();
				it->second.release_reasons.fill(input_release_reason::none);
				it->second.pressed_windows.fill(0);
				it->second.released_windows.fill(0);
				if (!it->second.connected)
					it = keyboard_states.erase(it);
				else
					++it;
			}
			for (auto it = mouse_states.begin(); it != mouse_states.end();)
			{
				it->second.pressed.reset();
				it->second.released.reset();
				it->second.release_reasons.fill(input_release_reason::none);
				it->second.pressed_windows.fill(0);
				it->second.released_windows.fill(0);
				it->second.delta = {};
				it->second.scroll_delta = {};
				if (!it->second.connected)
					it = mouse_states.erase(it);
				else
					++it;
			}
			for (auto it = touch_device_states.begin(); it != touch_device_states.end();)
			{
				for (auto finger = it->second.fingers.begin(); finger != it->second.fingers.end();)
				{
					finger->second.pressed = false;
					finger->second.released = false;
					finger->second.release_reason = input_release_reason::none;
					finger->second.delta = {};
					if (!finger->second.down)
						finger = it->second.fingers.erase(finger);
					else
						++finger;
				}
				if (!it->second.connected)
					it = touch_device_states.erase(it);
				else
					++it;
			}
			sync_touch_devices_locked(touch_ids);
			for (auto& [id, state] : pen_states)
			{
				(void)id;
				for (auto& button : state.buttons)
				{
					button.pressed = false;
					button.released = false;
					button.release_reason = input_release_reason::none;
				}
				state.delta = {};
			}
			for (auto it = joystick_states.begin(); it != joystick_states.end();)
			{
				for (auto& button : it->second.buttons)
				{
					button.pressed = false;
					button.released = false;
					button.release_reason = input_release_reason::none;
				}
				std::fill(it->second.ball_deltas.begin(), it->second.ball_deltas.end(), glm::ivec2{});
				if (!it->second.connected)
					it = joystick_states.erase(it);
				else
					++it;
			}
			for (auto it = gamepad_states.begin(); it != gamepad_states.end();)
			{
				for (auto& button : it->second.buttons)
				{
					button.pressed = false;
					button.released = false;
					button.release_reason = input_release_reason::none;
				}
				if (!it->second.connected)
					it = gamepad_states.erase(it);
				else
					++it;
			}
		}

		static void pool_events()
		{
			update_info();
			poll_events();
			const auto imgui_events = event_manager::take_owned_imgui_input_events();
			if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendPlatformUserData != nullptr)
			{
				for (const auto& event : imgui_events)
				{
					ImGui_ImplSDL3_ProcessEvent(&event.event);
				}
			}
#ifndef NO_SDLINPUT_QUEUE_FLUSH
			SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
#endif
		}
