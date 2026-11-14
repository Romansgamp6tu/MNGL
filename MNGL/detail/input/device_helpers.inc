		static keyboard_frame_state& ensure_keyboard_state_locked(SDL_KeyboardID id)
		{
			auto [it, inserted] = keyboard_states.try_emplace(id);
			if (inserted || !it->second.connected)
			{
				it->second = {};
				it->second.connected = true;
				it->second.generation = next_device_generation++;
			}
			return it->second;
		}

		static mouse_frame_state& ensure_mouse_state_locked(SDL_MouseID id)
		{
			auto [it, inserted] = mouse_states.try_emplace(id);
			if (inserted || !it->second.connected)
			{
				it->second = {};
				it->second.connected = true;
				it->second.generation = next_device_generation++;
			}
			return it->second;
		}

		static bool is_physical_touch_id(SDL_TouchID id)
		{
			return id != 0 && id != SDL_MOUSE_TOUCHID && id != SDL_PEN_TOUCHID;
		}

		static touch_device_frame_state& ensure_touch_device_state_locked(SDL_TouchID id)
		{
			auto [it, inserted] = touch_device_states.try_emplace(id);
			if (inserted || !it->second.connected)
			{
				it->second = {};
				const char* name = SDL_GetTouchDeviceName(id);
				it->second.name = name ? std::string(name) : std::string{};
				it->second.type = SDL_GetTouchDeviceType(id);
				it->second.connected = true;
				it->second.generation = next_device_generation++;
			}
			return it->second;
		}

		static pen_frame_state& ensure_pen_state_locked(SDL_PenID id)
		{
			auto [it, inserted] = pen_states.try_emplace(id);
			if (inserted)
			{
				it->second.generation = next_device_generation++;
				it->second.type = SDL_GetPenDeviceType(id);
			}
			return it->second;
		}

		static void remove_touch_device_locked(touch_device_frame_state& state)
		{
			for (auto& [id, finger] : state.fingers)
			{
				(void)id;
				if (finger.down)
				{
					finger.down = false;
					finger.released = true;
					finger.release_reason = input_release_reason::device_removed;
					finger.delta = {};
					finger.pressure = 0.0f;
				}
			}
			state.connected = false;
		}

		static void sync_touch_devices_locked(const std::vector<SDL_TouchID>& ids)
		{
			for (const SDL_TouchID id : ids)
			{
				if (is_physical_touch_id(id))
					ensure_touch_device_state_locked(id);
			}
			for (auto& [id, state] : touch_device_states)
			{
				if (!state.connected || !is_physical_touch_id(id))
					continue;
				if (std::find(ids.begin(), ids.end(), id) == ids.end())
					remove_touch_device_locked(state);
			}
		}

		static joystick_frame_state& ensure_joystick_state_locked(SDL_JoystickID id)
		{
			auto [it, inserted] = joystick_states.try_emplace(id);
			if (inserted || !it->second.connected)
			{
				it->second = {};
				it->second.connected = true;
				it->second.generation = next_device_generation++;
			}
			return it->second;
		}

		static gamepad_frame_state& ensure_gamepad_state_locked(SDL_JoystickID id)
		{
			auto [it, inserted] = gamepad_states.try_emplace(id);
			if (inserted || !it->second.connected)
			{
				it->second = {};
				it->second.connected = true;
				it->second.generation = next_device_generation++;
			}
			return it->second;
		}

		template <typename Result, typename Function>
		static Result query_gamepad(SDL_JoystickID id, std::uint64_t generation, Function&& function,
		                            Result fallback = {})
		{
			std::lock_guard lock(state_mutex);
			const auto it = gamepad_states.find(id);
			if (it == gamepad_states.end() || it->second.generation != generation || !it->second.handle)
				return fallback;
			return function(it->second.handle.get());
		}

		static bool gamepad_button_index(gamepad_button button, std::size_t& index)
		{
			const int value = static_cast<int>(button);
			if (value < 0 || value >= SDL_GAMEPAD_BUTTON_COUNT)
				return false;
			index = static_cast<std::size_t>(value);
			return true;
		}

		static bool gamepad_axis_index(gamepad_axis axis, std::size_t& index)
		{
			const int value = static_cast<int>(axis);
			if (value < 0 || value >= SDL_GAMEPAD_AXIS_COUNT)
				return false;
			index = static_cast<std::size_t>(value);
			return true;
		}

		static bool gamepad_sensor_valid(sensor_type sensor)
		{
			const int value = static_cast<int>(sensor);
			return value >= 0 && value < SDL_SENSOR_COUNT;
		}

		static bool key_index(scancode code, std::size_t& index)
		{
			index = static_cast<std::size_t>(code);
			return index < SDL_SCANCODE_COUNT;
		}

		static bool mouse_button_index(mouse_button button, std::size_t& index)
		{
			const auto value = static_cast<std::uint8_t>(button);
			if (value == 0 || value > 32)
			{
				return false;
			}
			index = value - 1;
			return true;
		}

		static hat_position decode_hat_position(std::uint8_t value)
		{
			switch (value)
			{
			case SDL_HAT_CENTERED:
				return hat_position::centered;
			case SDL_HAT_UP:
				return hat_position::up;
			case SDL_HAT_RIGHT:
				return hat_position::right;
			case SDL_HAT_RIGHTUP:
				return hat_position::rightup;
			case SDL_HAT_DOWN:
				return hat_position::down;
			case SDL_HAT_RIGHTDOWN:
				return hat_position::rightdown;
			case SDL_HAT_LEFT:
				return hat_position::left;
			case SDL_HAT_LEFTUP:
				return hat_position::leftup;
			case SDL_HAT_LEFTDOWN:
				return hat_position::leftdown;
			default:
				return hat_position::none;
			}
		}

		static bool is_physical_mouse_id(SDL_MouseID id)
		{
			return id != 0 && id != SDL_TOUCH_MOUSEID && id != SDL_PEN_MOUSEID;
		}

		static void set_mouse_window_locked(mouse_frame_state& state, SDL_WindowID window_id)
		{
			if (state.window_id != window_id)
			{
				state.delta = {};
				state.scroll_delta = {};
			}
			state.window_id = window_id;
		}

		static void remove_keyboard_locked(SDL_KeyboardID id)
		{
			auto& state = ensure_keyboard_state_locked(id);
			for (std::size_t i = 0; i < SDL_SCANCODE_COUNT; ++i)
			{
				if (state.down.test(i))
				{
					state.down.reset(i);
					state.released.set(i);
					state.release_reasons[i] = input_release_reason::device_removed;
					state.released_windows[i] = state.key_windows[i];
				}
			}
			state.key_windows.fill(0);
			state.connected = false;
		}

		static void remove_mouse_locked(SDL_MouseID id)
		{
			auto& state = ensure_mouse_state_locked(id);
			for (std::size_t i = 0; i < state.down.size(); ++i)
			{
				if (state.down.test(i))
				{
					state.down.reset(i);
					state.released.set(i);
					state.release_reasons[i] = input_release_reason::device_removed;
					state.released_windows[i] = state.button_windows[i];
				}
			}
			state.button_windows.fill(0);
			state.delta = {};
			state.scroll_delta = {};
			state.connected = false;
		}

		static void release_touch_fingers_locked(touch_device_frame_state& state, input_release_reason reason,
		                                         std::optional<SDL_WindowID> window_id = std::nullopt)
		{
			for (auto& [id, finger] : state.fingers)
			{
				(void)id;
				if (finger.down && (!window_id || finger.window_id == *window_id || finger.window_id == 0))
				{
					finger.down = false;
					finger.released = true;
					finger.release_reason = reason;
					finger.delta = {};
					finger.pressure = 0.0f;
				}
			}
		}

		static void remove_joystick_locked(SDL_JoystickID id)
		{
			auto& state = ensure_joystick_state_locked(id);
			for (auto& button : state.buttons)
			{
				if (button.down)
				{
					button.down = false;
					button.released = true;
				}
				if (button.released)
					button.release_reason = input_release_reason::device_removed;
			}
			std::fill(state.axes.begin(), state.axes.end(), 0);
			std::fill(state.hats.begin(), state.hats.end(), hat_position::centered);
			std::fill(state.ball_deltas.begin(), state.ball_deltas.end(), glm::ivec2{});
			state.handle.reset();
			state.connected = false;
		}

		static void remove_gamepad_locked(SDL_JoystickID id)
		{
			auto& state = ensure_gamepad_state_locked(id);
			for (auto& button : state.buttons)
			{
				if (button.down)
				{
					button.down = false;
					button.released = true;
				}
				if (button.released)
					button.release_reason = input_release_reason::device_removed;
			}
			state.axes.fill(0);
			state.handle.reset();
			state.connected = false;
		}

		static void refresh_gamepad_mapping_locked(gamepad_frame_state& state)
		{
			if (!state.handle)
				return;
			for (std::size_t i = 0; i < state.buttons.size(); ++i)
			{
				auto& button = state.buttons[i];
				const bool is_down =
				    SDL_GetGamepadButton(state.handle.get(), static_cast<SDL_GamepadButton>(i));
				if (button.down && !is_down)
				{
					button.released = true;
					button.release_reason = input_release_reason::mapping_changed;
				}
				else if (!button.down && is_down)
				{
					button.pressed = true;
				}
				button.down = is_down;
			}
			for (std::size_t i = 0; i < state.axes.size(); ++i)
				state.axes[i] = SDL_GetGamepadAxis(state.handle.get(), static_cast<SDL_GamepadAxis>(i));
		}

