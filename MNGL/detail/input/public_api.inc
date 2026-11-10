		static std::uint64_t performance_counter() { return SDL_GetPerformanceCounter(); }
		static std::uint64_t performance_frequency() { return SDL_GetPerformanceFrequency(); }
		static double get_dt() { return dt.load(std::memory_order_relaxed); }
		[[nodiscard]] static double delta_time() { return get_dt(); }

		// Ноль отключает ограничение частоты кадров.
		static void set_max_fps(std::uint32_t fps)
		{
			max_fps_limit.store(fps, std::memory_order_relaxed);
			frame_clock_initialized.store(false, std::memory_order_relaxed);
		}

		static std::uint32_t get_max_fps() { return max_fps_limit.load(std::memory_order_relaxed); }
		static void max_fps(std::uint32_t fps) { set_max_fps(fps); }
		[[nodiscard]] static std::uint32_t max_fps() { return get_max_fps(); }

		static void request_quit() noexcept { should_work.store(false, std::memory_order_release); }

		// Список соответствует текущему порядку устройств, возвращённому SDL.
		// Вызывайте эти методы на том же потоке, что и input::continue_work().
		[[nodiscard]] static std::vector<::mngl::keyboard> keyboards()
		{
			init_all();
			const auto ids = enumerate_keyboard_ids();
			std::vector<::mngl::keyboard> result;
			result.reserve(ids.size());
			std::lock_guard lock(state_mutex);
			for (const SDL_KeyboardID id : ids)
			{
				const auto& state = ensure_keyboard_state_locked(id);
				result.push_back(::mngl::keyboard(id, state.generation));
			}
			return result;
		}

		[[nodiscard]] static ::mngl::keyboard keyboard(std::size_t num_from_list = 0)
		{
			init_all();
			const auto ids = enumerate_keyboard_ids();
			if (num_from_list >= ids.size()) [[unlikely]]
				throw std::out_of_range("keyboard index is outside input::keyboards()");

			std::lock_guard lock(state_mutex);
			const SDL_KeyboardID id = ids[num_from_list];
			const auto& state = ensure_keyboard_state_locked(id);
			return ::mngl::keyboard(id, state.generation);
		}

		[[nodiscard]] static std::vector<::mngl::mouse> mice()
		{
			init_all();
			const auto ids = enumerate_mouse_ids();
			std::vector<::mngl::mouse> result;
			result.reserve(ids.size());
			std::lock_guard lock(state_mutex);
			for (const SDL_MouseID id : ids)
			{
				const auto& state = ensure_mouse_state_locked(id);
				result.push_back(::mngl::mouse(id, state.generation));
			}
			return result;
		}

		[[nodiscard]] static ::mngl::mouse mouse(std::size_t num_from_list = 0)
		{
			init_all();
			const auto ids = enumerate_mouse_ids();
			if (num_from_list >= ids.size()) [[unlikely]]
				throw std::out_of_range("mouse index is outside input::mice()");

			std::lock_guard lock(state_mutex);
			const SDL_MouseID id = ids[num_from_list];
			const auto& state = ensure_mouse_state_locked(id);
			return ::mngl::mouse(id, state.generation);
		}

		[[nodiscard]] static std::vector<::mngl::touch_device> touch_devices()
		{
			init_all();
			const auto ids = enumerate_touch_ids();
			std::vector<::mngl::touch_device> result;
			result.reserve(ids.size());
			std::lock_guard lock(state_mutex);
			sync_touch_devices_locked(ids);
			for (const SDL_TouchID id : ids)
			{
				if (!is_physical_touch_id(id))
					continue;
				const auto& state = ensure_touch_device_state_locked(id);
				result.push_back(::mngl::touch_device(id, state.generation));
			}
			return result;
		}

		[[nodiscard]] static ::mngl::touch_device touch_device(std::size_t num_from_list = 0)
		{
			init_all();
			const auto ids = enumerate_touch_ids();
			std::lock_guard lock(state_mutex);
			sync_touch_devices_locked(ids);
			for (const SDL_TouchID id : ids)
			{
				if (!is_physical_touch_id(id))
					continue;
				if (num_from_list == 0)
				{
					const auto& state = ensure_touch_device_state_locked(id);
					return ::mngl::touch_device(id, state.generation);
				}
				--num_from_list;
			}
			throw std::out_of_range("touch device index is outside input::touch_devices()");
		}

		// SDL exposes pen IDs through input events, not a device enumeration API.
		[[nodiscard]] static std::vector<::mngl::pen> pens()
		{
			init_all();
			std::lock_guard lock(state_mutex);
			std::vector<SDL_PenID> ids;
			ids.reserve(pen_states.size());
			for (const auto& [id, state] : pen_states)
			{
				(void)state;
				ids.push_back(id);
			}
			std::sort(ids.begin(), ids.end());
			std::vector<::mngl::pen> result;
			result.reserve(ids.size());
			for (const SDL_PenID id : ids)
				result.push_back(::mngl::pen(id, pen_states.at(id).generation));
			return result;
		}

		[[nodiscard]] static ::mngl::pen pen(std::size_t num_from_list = 0)
		{
			const auto devices = pens();
			if (num_from_list >= devices.size()) [[unlikely]]
				throw std::out_of_range("pen index is outside input::pens()");
			return devices[num_from_list];
		}

		[[nodiscard]] static std::vector<::mngl::joystick> joysticks()
		{
			init_all();
			const auto ids = enumerate_joystick_ids();
			std::vector<::mngl::joystick> result;
			result.reserve(ids.size());
			std::lock_guard lock(state_mutex);
			for (const SDL_JoystickID id : ids)
			{
				auto& state = ensure_joystick_state_locked(id);
				open_joystick_locked(id, state);
				result.push_back(::mngl::joystick(id, state.generation));
			}
			return result;
		}

		[[nodiscard]] static ::mngl::joystick joystick(std::size_t num_from_list = 0)
		{
			init_all();
			const auto ids = enumerate_joystick_ids();
			if (num_from_list >= ids.size()) [[unlikely]]
				throw std::out_of_range("joystick index is outside input::joysticks()");

			std::lock_guard lock(state_mutex);
			const SDL_JoystickID id = ids[num_from_list];
			auto& state = ensure_joystick_state_locked(id);
			open_joystick_locked(id, state);
			return ::mngl::joystick(id, state.generation);
		}

		[[nodiscard]] static bool has_gamepad()
		{
			init_all();
			return SDL_HasGamepad();
		}

		[[nodiscard]] static bool is_gamepad(SDL_JoystickID id)
		{
			init_all();
			return SDL_IsGamepad(id);
		}

		[[nodiscard]] static std::vector<::mngl::gamepad> gamepads()
		{
			init_all();
			const auto ids = enumerate_gamepad_ids();
			std::vector<::mngl::gamepad> result;
			result.reserve(ids.size());
			std::lock_guard lock(state_mutex);
			for (const SDL_JoystickID id : ids)
			{
				auto& state = ensure_gamepad_state_locked(id);
				open_gamepad_locked(id, state);
				result.push_back(::mngl::gamepad(id, state.generation));
			}
			return result;
		}

		[[nodiscard]] static ::mngl::gamepad gamepad(std::size_t num_from_list = 0)
		{
			init_all();
			const auto ids = enumerate_gamepad_ids();
			if (num_from_list >= ids.size()) [[unlikely]]
				throw std::out_of_range("gamepad index is outside input::gamepads()");

			std::lock_guard lock(state_mutex);
			const SDL_JoystickID id = ids[num_from_list];
			auto& state = ensure_gamepad_state_locked(id);
			open_gamepad_locked(id, state);
			return ::mngl::gamepad(id, state.generation);
		}

		// SDL returns 1 when added, 0 when updated and -1 on failure.
		static int add_gamepad_mapping(std::string_view mapping)
		{
			init_all();
			const std::string value(mapping);
			return SDL_AddGamepadMapping(value.c_str());
		}

		static bool reload_gamepad_mappings()
		{
			init_all();
			return SDL_ReloadGamepadMappings();
		}

		static int add_gamepad_mappings_from_file(std::string_view path)
		{
			init_all();
			const std::string value(path);
			return SDL_AddGamepadMappingsFromFile(value.c_str());
		}

		[[nodiscard]] static std::vector<std::string> gamepad_mappings()
		{
			init_all();
			int count = 0;
			auto** raw_mappings = SDL_GetGamepadMappings(&count);
			if (!raw_mappings) [[unlikely]]
				throw sdl_error("failed to enumerate gamepad mappings: ");
			std::unique_ptr<char*, decltype(&SDL_free)> mappings(raw_mappings, &SDL_free);
			std::vector<std::string> result;
			result.reserve(static_cast<std::size_t>(std::max(count, 0)));
			for (int i = 0; i < count; ++i)
				if (mappings.get()[i])
					result.emplace_back(mappings.get()[i]);
			return result;
		}

		[[nodiscard]] static std::string gamepad_mapping_for_id(SDL_JoystickID id)
		{
			init_all();
			std::unique_ptr<char, decltype(&SDL_free)> mapping(SDL_GetGamepadMappingForID(id), &SDL_free);
			return mapping ? std::string(mapping.get()) : std::string{};
		}

		[[nodiscard]] static std::string gamepad_mapping_for_guid(SDL_GUID guid)
		{
			init_all();
			std::unique_ptr<char, decltype(&SDL_free)> mapping(SDL_GetGamepadMappingForGUID(guid), &SDL_free);
			return mapping ? std::string(mapping.get()) : std::string{};
		}

		static bool continue_work()
		{
			init_all();
			if (!should_work.load(std::memory_order_acquire)) [[unlikely]]
				return false;
			wait_for_next_frame();
			pool_events();
			input_frame_index.fetch_add(1, std::memory_order_release);
			if (!should_work.load(std::memory_order_acquire)) [[unlikely]]
				return false;
			// перерасчёт всех дельт, и тому подобного
			update_dt();
			update_manager::tick(dt.load(std::memory_order_relaxed));
			return true;
		}

		// One application-loop iteration: handle events, update input/time, and
		// return false after the application has been asked to close.
		[[nodiscard]] static bool next_frame() { return continue_work(); }

		[[deprecated("use input::keyboards() and keyboard::pressed()")]]
		static bool is_key_pressed(scancode code)
		{
			return legacy_keyboard_state(code, 0);
		}
		[[deprecated("use input::keyboards() and keyboard::down()")]]
		static bool is_key_holded(scancode code)
		{
			return legacy_keyboard_state(code, 1);
		}
		[[deprecated("use input::keyboards() and keyboard::released()")]]
		static bool is_key_released(scancode code)
		{
			return legacy_keyboard_state(code, 2);
		}

		[[deprecated("use input::mouse().position()")]]
		static glm::vec2 get_mouse_position()
		{
			return legacy_mouse_position();
		}
		[[deprecated("use input::mouse().delta()")]]
		static glm::vec2 get_mouse_delta()
		{
			return legacy_mouse_delta(false);
		}
		[[deprecated("use input::mouse().scroll_delta()")]]
		static glm::vec2 get_mouse_scroll_delta()
		{
			return legacy_mouse_delta(true);
		}

		[[deprecated("use input::mice() and mouse::pressed()")]]
		static bool is_mouse_button_pressed(mouse_button button)
		{
			return legacy_mouse_button_state(button, 0);
		}
		[[deprecated("use input::mice() and mouse::down()")]]
		static bool is_mouse_button_holded(mouse_button button)
		{
			return legacy_mouse_button_state(button, 1);
		}
		[[deprecated("use input::mice() and mouse::released()")]]
		static bool is_mouse_button_released(mouse_button button)
		{
			return legacy_mouse_button_state(button, 2);
		}

		inline static std::atomic_bool should_work = true;
