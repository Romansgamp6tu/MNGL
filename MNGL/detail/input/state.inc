		struct keyboard_frame_state
		{
			std::bitset<SDL_SCANCODE_COUNT> down;
			std::bitset<SDL_SCANCODE_COUNT> pressed;
			std::bitset<SDL_SCANCODE_COUNT> released;
			std::array<input_release_reason, SDL_SCANCODE_COUNT> release_reasons{};
			std::array<SDL_WindowID, SDL_SCANCODE_COUNT> key_windows{};
			std::array<SDL_WindowID, SDL_SCANCODE_COUNT> pressed_windows{};
			std::array<SDL_WindowID, SDL_SCANCODE_COUNT> released_windows{};
			std::uint64_t generation{};
			bool connected{};
		};

		struct mouse_frame_state
		{
			std::bitset<32> down;
			std::bitset<32> pressed;
			std::bitset<32> released;
			std::array<input_release_reason, 32> release_reasons{};
			std::array<SDL_WindowID, 32> button_windows{};
			std::array<SDL_WindowID, 32> pressed_windows{};
			std::array<SDL_WindowID, 32> released_windows{};
			glm::vec2 position{};
			glm::vec2 delta{};
			glm::vec2 scroll_delta{};
			SDL_WindowID window_id{};
			std::uint64_t generation{};
			bool has_position{};
			bool connected{};
		};

		struct touch_finger_frame_state
		{
			bool down{};
			bool pressed{};
			bool released{};
			input_release_reason release_reason{input_release_reason::none};
			glm::vec2 position{};
			glm::vec2 delta{};
			float pressure{};
			SDL_WindowID window_id{};
		};

		struct touch_device_frame_state
		{
			std::string name;
			SDL_TouchDeviceType type{SDL_TOUCH_DEVICE_INVALID};
			std::unordered_map<SDL_FingerID, touch_finger_frame_state> fingers;
			std::uint64_t generation{};
			bool connected{};
		};

		struct pen_button_frame_state
		{
			bool down{};
			bool pressed{};
			bool released{};
			input_release_reason release_reason{input_release_reason::none};
		};

		struct pen_frame_state
		{
			SDL_PenDeviceType type{SDL_PEN_DEVICE_TYPE_INVALID};
			std::array<pen_button_frame_state, 6> buttons{}; // tip at 0; SDL buttons 1-5 at their index
			std::array<float, SDL_PEN_AXIS_COUNT> axes{};
			glm::vec2 position{};
			glm::vec2 delta{};
			SDL_WindowID window_id{};
			std::uint64_t generation{};
			bool has_position{};
			bool in_proximity{};
			bool eraser{};
		};

		struct joystick_button_frame_state
		{
			bool down{};
			bool pressed{};
			bool released{};
			input_release_reason release_reason{input_release_reason::none};
		};

		struct joystick_frame_state
		{
			std::unique_ptr<SDL_Joystick, decltype(&SDL_CloseJoystick)> handle{nullptr, &SDL_CloseJoystick};
			std::string name;
			SDL_JoystickType type{SDL_JOYSTICK_TYPE_UNKNOWN};
			std::vector<joystick_button_frame_state> buttons;
			std::vector<std::int16_t> axes;
			std::vector<hat_position> hats;
			std::vector<glm::ivec2> ball_deltas;
			std::uint64_t generation{};
			bool metadata_initialized{};
			bool connected{};
		};

		struct gamepad_frame_state
		{
			std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)> handle{nullptr, &SDL_CloseGamepad};
			std::array<joystick_button_frame_state, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
			std::array<std::int16_t, SDL_GAMEPAD_AXIS_COUNT> axes{};
			std::uint64_t generation{};
			bool connected{};
		};

		static inline std::unordered_map<SDL_KeyboardID, keyboard_frame_state> keyboard_states;
		static inline std::unordered_map<SDL_MouseID, mouse_frame_state> mouse_states;
		static inline std::unordered_map<SDL_TouchID, touch_device_frame_state> touch_device_states;
		static inline std::unordered_map<SDL_PenID, pen_frame_state> pen_states;
		static inline std::unordered_map<SDL_JoystickID, joystick_frame_state> joystick_states;
		static inline std::unordered_map<SDL_JoystickID, gamepad_frame_state> gamepad_states;
		static inline std::unordered_map<SDL_WindowID, bool> window_focus_states;
		static inline std::unordered_map<SDL_WindowID, input_release_reason> window_inactive_reasons;
		static inline std::unordered_map<SDL_WindowID, std::uint64_t> window_focus_loss_counts;
		static inline std::uint64_t next_device_generation = 1;
		static inline std::atomic<std::uint64_t> input_frame_index = 0;

		static inline std::atomic<double> dt = 0.1;
		static inline std::atomic<std::uint32_t> max_fps_limit = 60;
		static inline std::uint64_t next_frame_ns = 0;
		static inline std::atomic_bool frame_clock_initialized = false;
