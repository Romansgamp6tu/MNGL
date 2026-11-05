		static bool keyboard_state(SDL_KeyboardID id, std::uint64_t generation, scancode code, int kind)
		{
			std::size_t index{};
			if (!key_index(code, index))
				return false;
			std::lock_guard lock(state_mutex);
			const auto it = keyboard_states.find(id);
			if (it == keyboard_states.end() || it->second.generation != generation)
				return false;
			const auto& state = it->second;
			return kind == 0   ? state.pressed.test(index)
			       : kind == 1 ? state.down.test(index)
			                   : state.released.test(index);
		}

		static input_release_reason keyboard_release_reason(SDL_KeyboardID id, std::uint64_t generation,
		                                                    scancode code)
		{
			std::size_t index{};
			if (!key_index(code, index))
				return input_release_reason::none;
			std::lock_guard lock(state_mutex);
			const auto it = keyboard_states.find(id);
			if (it == keyboard_states.end() || it->second.generation != generation)
				return input_release_reason::none;
			return it->second.release_reasons[index];
		}

		static bool mouse_state(SDL_MouseID id, std::uint64_t generation, mouse_button button, int kind)
		{
			std::size_t index{};
			if (!mouse_button_index(button, index))
				return false;
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			if (it == mouse_states.end() || it->second.generation != generation)
				return false;
			const auto& state = it->second;
			return kind == 0   ? state.pressed.test(index)
			       : kind == 1 ? state.down.test(index)
			                   : state.released.test(index);
		}

		static bool legacy_keyboard_state(scancode code, int kind)
		{
			if (gui_wants_keyboard.load(std::memory_order_relaxed))
				return false;
			std::size_t index{};
			if (!key_index(code, index))
				return false;

			std::lock_guard lock(state_mutex);
			for (const auto& [id, state] : keyboard_states)
			{
				(void)id;
				if (!state.connected && kind != 2)
					continue;
				if (kind == 0   ? state.pressed.test(index)
				    : kind == 1 ? (state.down.test(index) && !state.pressed.test(index))
				                : state.released.test(index))
					return true;
			}
			return false;
		}

		static bool legacy_mouse_button_state(mouse_button button, int kind)
		{
			if (gui_wants_mouse.load(std::memory_order_relaxed))
				return false;
			std::size_t index{};
			if (!mouse_button_index(button, index))
				return false;

			std::lock_guard lock(state_mutex);
			for (const auto& [id, state] : mouse_states)
			{
				(void)id;
				if (!state.connected && kind != 2)
					continue;
				if (kind == 0   ? state.pressed.test(index)
				    : kind == 1 ? (state.down.test(index) && !state.pressed.test(index))
				                : state.released.test(index))
					return true;
			}
			return false;
		}

		static glm::vec2 legacy_mouse_position()
		{
			std::lock_guard lock(state_mutex);
			const mouse_frame_state* first_mouse = nullptr;
			SDL_MouseID first_id = std::numeric_limits<SDL_MouseID>::max();
			for (const auto& [id, state] : mouse_states)
			{
				if (state.connected && state.has_position && id < first_id)
				{
					first_id = id;
					first_mouse = &state;
				}
			}
			return first_mouse ? first_mouse->position : glm::vec2{};
		}

		static glm::vec2 legacy_mouse_delta(bool scroll)
		{
			if (gui_wants_mouse.load(std::memory_order_relaxed))
				return {};
			std::lock_guard lock(state_mutex);
			glm::vec2 result{};
			for (const auto& [id, state] : mouse_states)
			{
				(void)id;
				if (state.connected)
					result += scroll ? state.scroll_delta : state.delta;
			}
			return result;
		}

		static input_release_reason mouse_release_reason(SDL_MouseID id, std::uint64_t generation,
		                                                 mouse_button button)
		{
			std::size_t index{};
			if (!mouse_button_index(button, index))
				return input_release_reason::none;
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			if (it == mouse_states.end() || it->second.generation != generation)
				return input_release_reason::none;
			return it->second.release_reasons[index];
		}

		static bool joystick_button_state(SDL_JoystickID id, std::uint64_t generation, std::size_t button,
		                                  int kind)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			if (it == joystick_states.end() || it->second.generation != generation ||
			    button >= it->second.buttons.size())
				return false;
			const auto& state = it->second.buttons[button];
			return kind == 0 ? state.pressed : kind == 1 ? state.down : state.released;
		}

		static input_release_reason joystick_release_reason(SDL_JoystickID id, std::uint64_t generation,
		                                                    std::size_t button)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			if (it == joystick_states.end() || it->second.generation != generation ||
			    button >= it->second.buttons.size())
				return input_release_reason::none;
			return it->second.buttons[button].release_reason;
		}

		static std::string joystick_name(SDL_JoystickID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation ? it->second.name
			                                                                          : std::string{};
		}

		static SDL_JoystickType joystick_type(SDL_JoystickID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation
			           ? it->second.type
					   : SDL_JOYSTICK_TYPE_UNKNOWN;
		}

		static std::size_t joystick_control_count(SDL_JoystickID id, std::uint64_t generation, int kind)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			if (it == joystick_states.end() || it->second.generation != generation)
				return 0;
			const auto& state = it->second;
			return kind == 0   ? state.buttons.size()
			       : kind == 1 ? state.axes.size()
			       : kind == 2 ? state.hats.size()
			                   : state.ball_deltas.size();
		}

		static std::int16_t joystick_axis(SDL_JoystickID id, std::uint64_t generation, std::size_t axis)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation &&
			               axis < it->second.axes.size()
			           ? it->second.axes[axis]
					   : 0;
		}

		static hat_position joystick_hat(SDL_JoystickID id, std::uint64_t generation, std::size_t hat)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation &&
			               hat < it->second.hats.size()
			           ? it->second.hats[hat]
					   : hat_position::none;
		}

		static glm::ivec2 joystick_ball_delta(SDL_JoystickID id, std::uint64_t generation, std::size_t ball)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation &&
			               ball < it->second.ball_deltas.size()
			           ? it->second.ball_deltas[ball]
					   : glm::ivec2{};
		}

		static bool gamepad_button_state(SDL_JoystickID id, std::uint64_t generation, std::size_t button,
		                                 int kind)
		{
			std::lock_guard lock(state_mutex);
			const auto it = gamepad_states.find(id);
			if (it == gamepad_states.end() || it->second.generation != generation ||
			    button >= it->second.buttons.size())
				return false;
			const auto& state = it->second.buttons[button];
			return kind == 0 ? state.pressed : kind == 1 ? state.down : state.released;
		}

		static input_release_reason gamepad_release_reason(SDL_JoystickID id, std::uint64_t generation,
		                                                   std::size_t button)
		{
			std::lock_guard lock(state_mutex);
			const auto it = gamepad_states.find(id);
			if (it == gamepad_states.end() || it->second.generation != generation ||
			    button >= it->second.buttons.size())
				return input_release_reason::none;
			return it->second.buttons[button].release_reason;
		}

		static std::int16_t gamepad_axis_value(SDL_JoystickID id, std::uint64_t generation, std::size_t axis)
		{
			std::lock_guard lock(state_mutex);
			const auto it = gamepad_states.find(id);
			return it != gamepad_states.end() && it->second.generation == generation &&
			               axis < it->second.axes.size()
			           ? it->second.axes[axis]
					   : 0;
		}

		static bool gamepad_connected(SDL_JoystickID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = gamepad_states.find(id);
			return it != gamepad_states.end() && it->second.generation == generation &&
			       it->second.connected && it->second.handle && SDL_GamepadConnected(it->second.handle.get());
		}

		static bool keyboard_connected(SDL_KeyboardID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = keyboard_states.find(id);
			return it != keyboard_states.end() && it->second.generation == generation && it->second.connected;
		}

		static bool mouse_connected(SDL_MouseID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			return it != mouse_states.end() && it->second.generation == generation && it->second.connected;
		}

		static touch_finger_frame_state touch_finger_snapshot(SDL_TouchID touch_id,
		                                                      std::uint64_t device_generation,
		                                                      SDL_FingerID finger_id)
		{
			std::lock_guard lock(state_mutex);
			const auto device = touch_device_states.find(touch_id);
			if (device == touch_device_states.end() || device->second.generation != device_generation)
				return {};
			const auto finger = device->second.fingers.find(finger_id);
			return finger == device->second.fingers.end() ? touch_finger_frame_state{} : finger->second;
		}

		static bool touch_finger_exists(SDL_TouchID touch_id, std::uint64_t device_generation,
		                                SDL_FingerID finger_id)
		{
			std::lock_guard lock(state_mutex);
			const auto device = touch_device_states.find(touch_id);
			return device != touch_device_states.end() && device->second.generation == device_generation &&
			       device->second.fingers.find(finger_id) != device->second.fingers.end();
		}

		static std::vector<SDL_FingerID> touch_finger_ids(SDL_TouchID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto device = touch_device_states.find(id);
			if (device == touch_device_states.end() || device->second.generation != generation)
				return {};
			std::vector<SDL_FingerID> result;
			result.reserve(device->second.fingers.size());
			for (const auto& [finger_id, finger] : device->second.fingers)
			{
				if (finger.down || finger.pressed || finger.released)
					result.push_back(finger_id);
			}
			std::sort(result.begin(), result.end());
			return result;
		}

		static bool touch_device_connected(SDL_TouchID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = touch_device_states.find(id);
			return it != touch_device_states.end() && it->second.generation == generation &&
			       it->second.connected;
		}

		static std::string touch_device_name(SDL_TouchID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = touch_device_states.find(id);
			return it != touch_device_states.end() && it->second.generation == generation ? it->second.name
			                                                                              : std::string{};
		}

		static SDL_TouchDeviceType touch_device_type(SDL_TouchID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = touch_device_states.find(id);
			return it != touch_device_states.end() && it->second.generation == generation
			           ? it->second.type
					   : SDL_TOUCH_DEVICE_INVALID;
		}

		static void update_touch_finger(SDL_TouchID touch_id, SDL_FingerID finger_id, SDL_WindowID window_id,
		                                glm::vec2 position, glm::vec2 delta, float pressure, int event_kind)
		{
			if (!is_physical_touch_id(touch_id))
				return;
			std::lock_guard lock(state_mutex);
			auto it = touch_device_states.find(touch_id);
			if (it != touch_device_states.end() && !it->second.connected)
				return;
			auto& device =
			    it == touch_device_states.end() ? ensure_touch_device_state_locked(touch_id) : it->second;
			auto& finger = device.fingers[finger_id];
			finger.position = position;
			finger.delta += delta;
			finger.pressure = pressure;
			finger.window_id = window_id;

			if (event_kind == 0)
			{
				if (!finger.down)
					finger.pressed = true;
				finger.down = true;
			}
			else if (event_kind == 1)
			{
				finger.down = true;
			}
			else
			{
				finger.down = false;
				finger.released = true;
				finger.release_reason =
				    event_kind == 2 ? input_release_reason::physical_release : input_release_reason::canceled;
			}
		}

		static bool pen_button_index(pen_button button, std::size_t& index)
		{
			const auto value = static_cast<std::uint8_t>(button);
			if (value < 1 || value > 5)
				return false;
			index = value;
			return true;
		}

		static bool pen_axis_index(pen_axis_type axis, std::size_t& index)
		{
			const auto value = static_cast<std::uint8_t>(axis);
			if (value >= SDL_PEN_AXIS_COUNT)
				return false;
			index = value;
			return true;
		}

		static void set_pen_button_state(
		    pen_button_frame_state& button, bool down,
		    input_release_reason release_reason = input_release_reason::physical_release)
		{
			if (!button.down && down)
				button.pressed = true;
			else if (button.down && !down)
			{
				button.released = true;
				button.release_reason = release_reason;
			}
			button.down = down;
		}

		static void update_pen_flags_locked(pen_frame_state& state, SDL_PenInputFlags flags)
		{
			set_pen_button_state(state.buttons[0], (flags & SDL_PEN_INPUT_DOWN) != 0);
			for (std::size_t button = 1; button <= 5; ++button)
				set_pen_button_state(state.buttons[button], (flags & (1u << button)) != 0);
		}

		static void update_pen_event(SDL_PenID id, SDL_WindowID window_id, glm::vec2 position,
		                             SDL_PenInputFlags flags, std::optional<bool> eraser = std::nullopt)
		{
			if (id == 0)
				return;
			std::lock_guard lock(state_mutex);
			auto& state = ensure_pen_state_locked(id);
			if (state.has_position && state.window_id == window_id)
				state.delta += position - state.position;
			else
				state.delta = {};
			state.position = position;
			state.window_id = window_id;
			state.has_position = true;
			state.in_proximity = true;
			state.eraser = eraser.value_or((flags & SDL_PEN_INPUT_ERASER_TIP) != 0);
			update_pen_flags_locked(state, flags);
		}

		static void update_pen_proximity(SDL_PenID id, SDL_WindowID window_id, bool in_proximity)
		{
			if (id == 0)
				return;
			std::lock_guard lock(state_mutex);
			auto& state = ensure_pen_state_locked(id);
			state.window_id = window_id;
			state.in_proximity = in_proximity;
			if (!in_proximity)
			{
				for (auto& button : state.buttons)
					set_pen_button_state(button, false, input_release_reason::canceled);
				state.axes[SDL_PEN_AXIS_PRESSURE] = 0.0f;
				state.delta = {};
				state.eraser = false;
			}
		}

		static void release_pen_state_locked(pen_frame_state& state, input_release_reason reason)
		{
			for (auto& button : state.buttons)
				set_pen_button_state(button, false, reason);
			state.axes[SDL_PEN_AXIS_PRESSURE] = 0.0f;
			state.delta = {};
			state.in_proximity = false;
			state.eraser = false;
		}

		static pen_frame_state pen_snapshot(SDL_PenID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = pen_states.find(id);
			return it != pen_states.end() && it->second.generation == generation ? it->second
			                                                                     : pen_frame_state{};
		}

		static bool pen_button_state_value(SDL_PenID id, std::uint64_t generation, pen_button button,
		                                   int kind)
		{
			std::size_t index{};
			if (!pen_button_index(button, index))
				return false;
			const auto state = pen_snapshot(id, generation).buttons[index];
			return kind == 0 ? state.pressed : kind == 1 ? state.down : state.released;
		}

		static input_release_reason pen_button_release_reason(SDL_PenID id, std::uint64_t generation,
		                                                      pen_button button)
		{
			std::size_t index{};
			if (!pen_button_index(button, index))
				return input_release_reason::none;
			return pen_snapshot(id, generation).buttons[index].release_reason;
		}

		static float pen_axis_value(SDL_PenID id, std::uint64_t generation, pen_axis_type axis)
		{
			std::size_t index{};
			if (!pen_axis_index(axis, index))
				return 0.0f;
			return pen_snapshot(id, generation).axes[index];
		}

		static bool joystick_connected(SDL_JoystickID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation && it->second.connected;
		}

		static glm::vec2 mouse_vector(SDL_MouseID id, std::uint64_t generation, int kind)
		{
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			if (it == mouse_states.end() || it->second.generation != generation)
				return {};
			if (kind == 0)
				return it->second.position;
			if (kind == 1)
				return it->second.delta;
			return it->second.scroll_delta;
		}

		static bool mouse_has_position(SDL_MouseID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			return it != mouse_states.end() && it->second.generation == generation && it->second.has_position;
		}

		static SDL_WindowID mouse_window_id(SDL_MouseID id, std::uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			return it != mouse_states.end() && it->second.generation == generation ? it->second.window_id : 0;
		}

		static void reset_window_state_locked(SDL_WindowID window_id, input_release_reason reason)
		{
			for (auto& entry : keyboard_states)
			{
				auto& state = entry.second;
				for (std::size_t i = 0; i < SDL_SCANCODE_COUNT; ++i)
				{
					if (state.down.test(i) &&
					    (state.key_windows[i] == window_id || state.key_windows[i] == 0))
					{
						state.down.reset(i);
						state.released.set(i);
						state.release_reasons[i] = reason;
						state.released_windows[i] = state.key_windows[i];
						state.key_windows[i] = 0;
					}
				}
			}
			for (auto& entry : mouse_states)
			{
				auto& state = entry.second;
				for (std::size_t i = 0; i < state.down.size(); ++i)
				{
					if (state.down.test(i) &&
					    (state.button_windows[i] == window_id || state.button_windows[i] == 0))
					{
						state.down.reset(i);
						state.released.set(i);
						state.release_reasons[i] = reason;
						state.released_windows[i] = state.button_windows[i];
						state.button_windows[i] = 0;
					}
				}
				if (state.window_id == window_id || state.window_id == 0)
				{
					state.delta = {};
					state.scroll_delta = {};
				}
			}
			for (auto& entry : touch_device_states)
				release_touch_fingers_locked(entry.second, reason, window_id);
			for (auto& [id, state] : pen_states)
			{
				(void)id;
				if (state.window_id == window_id || state.window_id == 0)
					release_pen_state_locked(state, reason);
			}
		}

		static bool window_focused(SDL_WindowID window_id)
		{
			if (window_id == 0)
				return true;
			{
				std::lock_guard lock(state_mutex);
				const auto it = window_focus_states.find(window_id);
				if (it != window_focus_states.end())
					return it->second;
			}
			SDL_Window* window = SDL_GetWindowFromID(window_id);
			return !window || (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
		}

		static input_release_reason window_inactive_reason(SDL_WindowID window_id)
		{
			std::lock_guard lock(state_mutex);
			const auto it = window_inactive_reasons.find(window_id);
			return it != window_inactive_reasons.end() && it->second == input_release_reason::window_destroyed
			           ? input_release_reason::window_destroyed
					   : input_release_reason::focus_lost;
		}

		static std::uint64_t window_focus_loss_count(SDL_WindowID window_id)
		{
			std::lock_guard lock(state_mutex);
			const auto it = window_focus_loss_counts.find(window_id);
			return it == window_focus_loss_counts.end() ? 0 : it->second;
		}
