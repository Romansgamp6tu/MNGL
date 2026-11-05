		static std::vector<SDL_KeyboardID> enumerate_keyboard_ids()
		{
			int count = 0;
			auto* raw_ids = SDL_GetKeyboards(&count);
			if (!raw_ids)
				throw sdl_error("failed to enumerate keyboards: ");

			std::unique_ptr<SDL_KeyboardID, decltype(&SDL_free)> ids(raw_ids, &SDL_free);
			std::vector<SDL_KeyboardID> result;
			result.reserve(static_cast<std::size_t>(std::max(count, 0)));
			for (int i = 0; i < count; ++i)
			{
				if (ids.get()[i] != 0)
					result.push_back(ids.get()[i]);
			}
			return result;
		}

		static std::vector<SDL_MouseID> enumerate_mouse_ids()
		{
			int count = 0;
			auto* raw_ids = SDL_GetMice(&count);
			if (!raw_ids)
				throw sdl_error("failed to enumerate mice: ");

			std::unique_ptr<SDL_MouseID, decltype(&SDL_free)> ids(raw_ids, &SDL_free);
			std::vector<SDL_MouseID> result;
			result.reserve(static_cast<std::size_t>(std::max(count, 0)));
			for (int i = 0; i < count; ++i)
			{
				if (ids.get()[i] != 0)
					result.push_back(ids.get()[i]);
			}
			return result;
		}

		static std::vector<SDL_TouchID> enumerate_touch_ids()
		{
			int count = 0;
			auto* raw_ids = SDL_GetTouchDevices(&count);
			if (!raw_ids)
				throw sdl_error("failed to enumerate touch devices: ");

			std::unique_ptr<SDL_TouchID, decltype(&SDL_free)> ids(raw_ids, &SDL_free);
			std::vector<SDL_TouchID> result;
			result.reserve(static_cast<std::size_t>(std::max(count, 0)));
			for (int i = 0; i < count; ++i)
			{
				if (is_physical_touch_id(ids.get()[i]))
					result.push_back(ids.get()[i]);
			}
			return result;
		}

		static std::vector<SDL_JoystickID> enumerate_joystick_ids()
		{
			int count = 0;
			auto* raw_ids = SDL_GetJoysticks(&count);
			if (!raw_ids)
				throw sdl_error("failed to enumerate joysticks: ");

			std::unique_ptr<SDL_JoystickID, decltype(&SDL_free)> ids(raw_ids, &SDL_free);
			std::vector<SDL_JoystickID> result;
			result.reserve(static_cast<std::size_t>(std::max(count, 0)));
			for (int i = 0; i < count; ++i)
			{
				if (ids.get()[i] != 0)
					result.push_back(ids.get()[i]);
			}
			return result;
		}

		static void open_joystick_locked(SDL_JoystickID id, joystick_frame_state& state)
		{
			if (state.handle)
			{
				return;
			}

			std::unique_ptr<SDL_Joystick, decltype(&SDL_CloseJoystick)> handle(SDL_OpenJoystick(id),
			                                                                   &SDL_CloseJoystick);
			if (!handle)
				throw sdl_error("failed to open joystick: ");

			const int button_count = SDL_GetNumJoystickButtons(handle.get());
			const int axis_count = SDL_GetNumJoystickAxes(handle.get());
			const int hat_count = SDL_GetNumJoystickHats(handle.get());
			const int ball_count = SDL_GetNumJoystickBalls(handle.get());
			if (button_count < 0 || axis_count < 0 || hat_count < 0 || ball_count < 0)
				throw sdl_error("failed to query joystick controls: ");

			const char* name = SDL_GetJoystickName(handle.get());
			state.name = name ? std::string(name) : std::string{};
			state.type = SDL_GetJoystickType(handle.get());
			state.buttons.resize(static_cast<std::size_t>(button_count));
			state.axes.resize(static_cast<std::size_t>(axis_count));
			state.hats.resize(static_cast<std::size_t>(hat_count));
			state.ball_deltas.resize(static_cast<std::size_t>(ball_count), glm::ivec2{});
			for (int i = 0; i < button_count; ++i)
				state.buttons[static_cast<std::size_t>(i)].down = SDL_GetJoystickButton(handle.get(), i);
			for (int i = 0; i < axis_count; ++i)
				state.axes[static_cast<std::size_t>(i)] = SDL_GetJoystickAxis(handle.get(), i);
			for (int i = 0; i < hat_count; ++i)
				state.hats[static_cast<std::size_t>(i)] =
				    decode_hat_position(SDL_GetJoystickHat(handle.get(), i));
			state.handle = std::move(handle);
			state.metadata_initialized = true;
		}

		static std::vector<SDL_JoystickID> enumerate_gamepad_ids()
		{
			int count = 0;
			auto* raw_ids = SDL_GetGamepads(&count);
			if (!raw_ids)
				throw sdl_error("failed to enumerate gamepads: ");

			std::unique_ptr<SDL_JoystickID, decltype(&SDL_free)> ids(raw_ids, &SDL_free);
			std::vector<SDL_JoystickID> result;
			result.reserve(static_cast<std::size_t>(std::max(count, 0)));
			for (int i = 0; i < count; ++i)
			{
				if (ids.get()[i] != 0)
					result.push_back(ids.get()[i]);
			}
			return result;
		}

		static void open_gamepad_locked(SDL_JoystickID id, gamepad_frame_state& state)
		{
			if (state.handle)
				return;

			std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)> handle(SDL_OpenGamepad(id),
			                                                                 &SDL_CloseGamepad);
			if (!handle)
				throw sdl_error("failed to open gamepad: ");

			for (std::size_t i = 0; i < state.buttons.size(); ++i)
				state.buttons[i].down = SDL_GetGamepadButton(handle.get(), static_cast<SDL_GamepadButton>(i));
			for (std::size_t i = 0; i < state.axes.size(); ++i)
				state.axes[i] = SDL_GetGamepadAxis(handle.get(), static_cast<SDL_GamepadAxis>(i));
			state.handle = std::move(handle);
		}

		static std::string keyboard_name(SDL_KeyboardID id)
		{
			const char* value = SDL_GetKeyboardNameForID(id);
			return value ? std::string(value) : std::string{};
		}

		static std::string mouse_name(SDL_MouseID id)
		{
			const char* value = SDL_GetMouseNameForID(id);
			return value ? std::string(value) : std::string{};
		}
