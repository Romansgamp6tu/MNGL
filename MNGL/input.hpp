#pragma once

#include "utilities.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "event.hpp"
#include "iupdate.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/imgui.h"

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

	enum class input_release_reason : uint8_t
	{
		none,
		physical_release,
		focus_lost,
		device_removed,
		window_destroyed,
		mapping_changed,
		canceled
	};

	enum class pen_button : uint8_t
	{
		button1 = 1,
		button2 = 2,
		button3 = 3,
		button4 = 4,
		button5 = 5
	};

	class keyboard
	{
		friend class Input;
		keyboard(SDL_KeyboardID id, uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_KeyboardID m_id{};
		uint64_t m_generation{};

	public:
		// Per-keyboard state is accumulated from SDL key events. SDL's keyboard
		// state getter is global and does not take a keyboard ID.
		[[nodiscard]] SDL_KeyboardID id() const noexcept { return m_id; }
		[[nodiscard]] std::string name() const;
		[[nodiscard]] bool connected() const;
		[[nodiscard]] bool pressed(scancode code) const;
		[[nodiscard]] bool down(scancode code) const;
		[[nodiscard]] bool released(scancode code) const;
		[[nodiscard]] input_release_reason release_reason(scancode code) const;
	};

	class mouse
	{
		friend class Input;
		mouse(SDL_MouseID id, uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_MouseID m_id{};
		uint64_t m_generation{};

	public:
		// Physical per-device state is accumulated from SDL events. SDL's synchronous
		// mouse-state getters do not take a mouse ID and cannot populate this view.
		[[nodiscard]] SDL_MouseID id() const noexcept { return m_id; }
		[[nodiscard]] std::string name() const;
		[[nodiscard]] bool connected() const;
		[[nodiscard]] bool pressed(mouse_button button) const;
		[[nodiscard]] bool down(mouse_button button) const;
		[[nodiscard]] bool released(mouse_button button) const;
		[[nodiscard]] input_release_reason release_reason(mouse_button button) const;
		[[nodiscard]] bool has_position() const;
		[[nodiscard]] glm::vec2 position() const;
		[[nodiscard]] SDL_WindowID window_id() const;
		[[nodiscard]] glm::vec2 delta() const;
		[[nodiscard]] glm::vec2 scroll_delta() const;
	};

	class touch_finger
	{
		friend class Input;
		friend class touch_device;
		touch_finger(SDL_TouchID touch_id, uint64_t device_generation,
			SDL_FingerID finger_id)
			: m_touch_id(touch_id), m_device_generation(device_generation), m_finger_id(finger_id) {}

		SDL_TouchID m_touch_id{};
		uint64_t m_device_generation{};
		SDL_FingerID m_finger_id{};

	public:
		// A finger's physical state is keyed by both its touch-device ID and finger ID.
		[[nodiscard]] SDL_FingerID id() const noexcept { return m_finger_id; }
		[[nodiscard]] bool pressed() const;
		[[nodiscard]] bool down() const;
		[[nodiscard]] bool released() const;
		[[nodiscard]] input_release_reason release_reason() const;
		// SDL touch coordinates and deltas are normalized relative to the window.
		[[nodiscard]] glm::vec2 position() const;
		[[nodiscard]] glm::vec2 position_in_window(glm::vec2 logical_size) const;
		[[nodiscard]] glm::vec2 delta() const;
		[[nodiscard]] float pressure() const;
		[[nodiscard]] SDL_WindowID window_id() const;
	};

	class touch_device
	{
		friend class Input;
		touch_device(SDL_TouchID id, uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_TouchID m_id{};
		uint64_t m_generation{};

	public:
		[[nodiscard]] SDL_TouchID id() const noexcept { return m_id; }
		[[nodiscard]] std::string name() const;
		[[nodiscard]] SDL_TouchDeviceType type() const;
		[[nodiscard]] bool connected() const;
		// Includes active fingers and fingers released or canceled this frame.
		[[nodiscard]] std::vector<touch_finger> fingers() const;
		[[nodiscard]] std::optional<touch_finger> finger(SDL_FingerID id) const;
	};

	class pen
	{
		friend class Input;
		pen(SDL_PenID id, uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_PenID m_id{};
		uint64_t m_generation{};

	public:
		// SDL reports pen state through events; proximity is the available activity
		// state because SDL does not expose pen enumeration or removal events.
		[[nodiscard]] SDL_PenID id() const noexcept { return m_id; }
		[[nodiscard]] SDL_PenDeviceType type() const;
		[[nodiscard]] bool in_proximity() const;
		[[nodiscard]] bool has_position() const;
		[[nodiscard]] glm::vec2 position() const;
		[[nodiscard]] glm::vec2 delta() const;
		[[nodiscard]] SDL_WindowID window_id() const;
		[[nodiscard]] bool is_eraser() const;

		// Tip contact. Pen barrel buttons are exposed separately below.
		[[nodiscard]] bool pressed() const;
		[[nodiscard]] bool down() const;
		[[nodiscard]] bool released() const;
		[[nodiscard]] input_release_reason release_reason() const;
		[[nodiscard]] bool button_pressed(pen_button button) const;
		[[nodiscard]] bool button_down(pen_button button) const;
		[[nodiscard]] bool button_released(pen_button button) const;
		[[nodiscard]] input_release_reason button_release_reason(pen_button button) const;
		[[nodiscard]] float axis(pen_axis_type axis) const;
		[[nodiscard]] float pressure() const;
		[[nodiscard]] glm::vec2 tilt() const;
	};

	class joystick
	{
		friend class Input;
		joystick(SDL_JoystickID id, uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_JoystickID m_id{};
		uint64_t m_generation{};

	public:
		// Initialized from this joystick's SDL handle, then updated from events
		// for this joystick ID during each continue_work() frame.
		[[nodiscard]] SDL_JoystickID id() const noexcept { return m_id; }
		[[nodiscard]] std::string name() const;
		[[nodiscard]] bool connected() const;
		[[nodiscard]] SDL_JoystickType type() const;
		[[nodiscard]] std::size_t button_count() const;
		[[nodiscard]] std::size_t axis_count() const;
		[[nodiscard]] std::size_t hat_count() const;
		[[nodiscard]] std::size_t ball_count() const;
		[[nodiscard]] bool pressed(std::size_t button) const;
		[[nodiscard]] bool down(std::size_t button) const;
		[[nodiscard]] bool released(std::size_t button) const;
		[[nodiscard]] input_release_reason release_reason(std::size_t button) const;
		[[nodiscard]] int16_t axis(std::size_t axis) const;
		[[nodiscard]] float normalized_axis(std::size_t axis) const;
		[[nodiscard]] hat_position hat(std::size_t hat) const;
		[[nodiscard]] glm::ivec2 ball_delta(std::size_t ball) const;
	};

	struct gamepad_touchpad_finger
	{
		bool down{};
		glm::vec2 position{};
		float pressure{};
	};

	struct gamepad_battery_info
	{
		power_state state{ power_state::unknown };
		int percent{ -1 };
	};

	class gamepad
	{
		friend class Input;
		gamepad(SDL_JoystickID id, uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_JoystickID m_id{};
		uint64_t m_generation{};

	public:
		// Initialized from this gamepad's SDL handle, then updated from events
		// for this gamepad ID during each continue_work() frame.
		[[nodiscard]] SDL_JoystickID id() const noexcept { return m_id; }
		[[nodiscard]] std::string name() const;
		[[nodiscard]] std::string path() const;
		[[nodiscard]] SDL_GamepadType type() const;
		[[nodiscard]] SDL_GamepadType real_type() const;
		[[nodiscard]] SDL_GUID guid() const;
		[[nodiscard]] bool connected() const;
		[[nodiscard]] SDL_JoystickConnectionState connection_state() const;
		[[nodiscard]] int player_index() const;
		[[nodiscard]] bool set_player_index(int player_index) const;
		[[nodiscard]] uint16_t vendor() const;
		[[nodiscard]] uint16_t product() const;
		[[nodiscard]] uint16_t product_version() const;
		[[nodiscard]] uint16_t firmware_version() const;
		[[nodiscard]] std::string serial() const;
		[[nodiscard]] uint64_t steam_handle() const;
		[[nodiscard]] gamepad_battery_info battery_info() const;

		[[nodiscard]] bool has_button(gamepad_button button) const;
		[[nodiscard]] bool pressed(gamepad_button button) const;
		[[nodiscard]] bool down(gamepad_button button) const;
		[[nodiscard]] bool released(gamepad_button button) const;
		[[nodiscard]] input_release_reason release_reason(gamepad_button button) const;
		[[nodiscard]] SDL_GamepadButtonLabel button_label(gamepad_button button) const;
		[[nodiscard]] bool has_axis(gamepad_axis axis) const;
		[[nodiscard]] int16_t axis(gamepad_axis axis) const;
		[[nodiscard]] float normalized_axis(gamepad_axis axis) const;

		[[nodiscard]] std::string mapping() const;
		[[nodiscard]] bool set_mapping(std::string_view mapping) const;
		[[nodiscard]] std::vector<SDL_GamepadBinding> bindings() const;

		[[nodiscard]] int touchpad_count() const;
		[[nodiscard]] int touchpad_finger_count(int touchpad) const;
		[[nodiscard]] std::optional<gamepad_touchpad_finger> touchpad_finger(
			int touchpad, int finger) const;

		[[nodiscard]] bool has_sensor(sensor_type sensor) const;
		[[nodiscard]] bool set_sensor_enabled(sensor_type sensor, bool enabled) const;
		[[nodiscard]] bool sensor_enabled(sensor_type sensor) const;
		[[nodiscard]] float sensor_data_rate(sensor_type sensor) const;
		[[nodiscard]] std::optional<std::vector<float>> sensor_data(
			sensor_type sensor, std::size_t value_count = 3) const;

		[[nodiscard]] bool has_rumble() const;
		[[nodiscard]] bool has_trigger_rumble() const;
		[[nodiscard]] bool has_rgb_led() const;
		[[nodiscard]] bool has_player_led() const;
		[[nodiscard]] bool has_mono_led() const;
		[[nodiscard]] bool rumble(uint16_t low_frequency, uint16_t high_frequency, uint32_t duration_ms) const;
		[[nodiscard]] bool rumble_triggers(uint16_t left, uint16_t right, uint32_t duration_ms) const;
		[[nodiscard]] bool set_led(uint8_t red, uint8_t green, uint8_t blue) const;
		[[nodiscard]] bool send_effect(std::span<const std::byte> effect) const;
		[[nodiscard]] std::string apple_button_symbol(gamepad_button button) const;
		[[nodiscard]] std::string apple_axis_symbol(gamepad_axis axis) const;
	};

	class Input
	{
		friend class keyboard;
		friend class mouse;
		friend class touch_finger;
		friend class touch_device;
		friend class pen;
		friend class joystick;
		friend class gamepad;
		friend class ActionMap;
		friend class InputContext;

	private:
		struct keyboard_frame_state
		{
			std::bitset<SDL_SCANCODE_COUNT> down;
			std::bitset<SDL_SCANCODE_COUNT> pressed;
			std::bitset<SDL_SCANCODE_COUNT> released;
			std::array<input_release_reason, SDL_SCANCODE_COUNT> release_reasons{};
			std::array<SDL_WindowID, SDL_SCANCODE_COUNT> key_windows{};
			std::array<SDL_WindowID, SDL_SCANCODE_COUNT> pressed_windows{};
			std::array<SDL_WindowID, SDL_SCANCODE_COUNT> released_windows{};
			uint64_t generation{};
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
			uint64_t generation{};
			bool has_position{};
			bool connected{};
		};

		struct touch_finger_frame_state
		{
			bool down{};
			bool pressed{};
			bool released{};
			input_release_reason release_reason{ input_release_reason::none };
			glm::vec2 position{};
			glm::vec2 delta{};
			float pressure{};
			SDL_WindowID window_id{};
		};

		struct touch_device_frame_state
		{
			std::string name;
			SDL_TouchDeviceType type{ SDL_TOUCH_DEVICE_INVALID };
			std::unordered_map<SDL_FingerID, touch_finger_frame_state> fingers;
			uint64_t generation{};
			bool connected{};
		};

		struct pen_button_frame_state
		{
			bool down{};
			bool pressed{};
			bool released{};
			input_release_reason release_reason{ input_release_reason::none };
		};

		struct pen_frame_state
		{
			SDL_PenDeviceType type{ SDL_PEN_DEVICE_TYPE_INVALID };
			std::array<pen_button_frame_state, 6> buttons{}; // tip at 0; SDL buttons 1-5 at their index
			std::array<float, SDL_PEN_AXIS_COUNT> axes{};
			glm::vec2 position{};
			glm::vec2 delta{};
			SDL_WindowID window_id{};
			uint64_t generation{};
			bool has_position{};
			bool in_proximity{};
			bool eraser{};
		};

		struct joystick_button_frame_state
		{
			bool down{};
			bool pressed{};
			bool released{};
			input_release_reason release_reason{ input_release_reason::none };
		};

		struct joystick_frame_state
		{
			std::unique_ptr<SDL_Joystick, decltype(&SDL_CloseJoystick)> handle{ nullptr, &SDL_CloseJoystick };
			std::string name;
			SDL_JoystickType type{ SDL_JOYSTICK_TYPE_UNKNOWN };
			std::vector<joystick_button_frame_state> buttons;
			std::vector<int16_t> axes;
			std::vector<hat_position> hats;
			std::vector<glm::ivec2> ball_deltas;
			uint64_t generation{};
			bool metadata_initialized{};
			bool connected{};
		};

		struct gamepad_frame_state
		{
			std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)> handle{ nullptr, &SDL_CloseGamepad };
			std::array<joystick_button_frame_state, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
			std::array<int16_t, SDL_GAMEPAD_AXIS_COUNT> axes{};
			uint64_t generation{};
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
		static inline std::unordered_map<SDL_WindowID, uint64_t> window_focus_loss_counts;
		static inline uint64_t next_device_generation = 1;
		static inline std::atomic<uint64_t> input_frame_index = 0;

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
				if (is_physical_touch_id(id)) ensure_touch_device_state_locked(id);
			}
			for (auto& [id, state] : touch_device_states)
			{
				if (!state.connected || !is_physical_touch_id(id)) continue;
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

		template<typename Result, typename Function>
		static Result query_gamepad(
			SDL_JoystickID id, uint64_t generation, Function&& function, Result fallback = {})
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
			if (value < 0 || value >= SDL_GAMEPAD_BUTTON_COUNT) return false;
			index = static_cast<std::size_t>(value);
			return true;
		}

		static bool gamepad_axis_index(gamepad_axis axis, std::size_t& index)
		{
			const int value = static_cast<int>(axis);
			if (value < 0 || value >= SDL_GAMEPAD_AXIS_COUNT) return false;
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
			const auto value = static_cast<uint8_t>(button);
			if (value == 0 || value > 32)
			{
				return false;
			}
			index = value - 1;
			return true;
		}

		static hat_position decode_hat_position(uint8_t value)
		{
			switch (value)
			{
			case SDL_HAT_CENTERED: return hat_position::centered;
			case SDL_HAT_UP: return hat_position::up;
			case SDL_HAT_RIGHT: return hat_position::right;
			case SDL_HAT_RIGHTUP: return hat_position::rightup;
			case SDL_HAT_DOWN: return hat_position::down;
			case SDL_HAT_RIGHTDOWN: return hat_position::rightdown;
			case SDL_HAT_LEFT: return hat_position::left;
			case SDL_HAT_LEFTUP: return hat_position::leftup;
			case SDL_HAT_LEFTDOWN: return hat_position::leftdown;
			default: return hat_position::none;
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

		static void release_touch_fingers_locked(
			touch_device_frame_state& state, input_release_reason reason,
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
					button.release_reason = input_release_reason::device_removed;
				}
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
					button.release_reason = input_release_reason::device_removed;
				}
			}
			state.axes.fill(0);
			state.handle.reset();
			state.connected = false;
		}

		static void refresh_gamepad_mapping_locked(gamepad_frame_state& state)
		{
			if (!state.handle) return;
			for (std::size_t i = 0; i < state.buttons.size(); ++i)
			{
				auto& button = state.buttons[i];
				const bool is_down = SDL_GetGamepadButton(
					state.handle.get(), static_cast<SDL_GamepadButton>(i));
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

		static bool keyboard_state(SDL_KeyboardID id, uint64_t generation, scancode code, int kind)
		{
			std::size_t index{};
			if (!key_index(code, index)) return false;
			std::lock_guard lock(state_mutex);
			const auto it = keyboard_states.find(id);
			if (it == keyboard_states.end() || it->second.generation != generation) return false;
			const auto& state = it->second;
			return kind == 0 ? state.pressed.test(index)
				: kind == 1 ? state.down.test(index)
				: state.released.test(index);
		}

		static input_release_reason keyboard_release_reason(
			SDL_KeyboardID id, uint64_t generation, scancode code)
		{
			std::size_t index{};
			if (!key_index(code, index)) return input_release_reason::none;
			std::lock_guard lock(state_mutex);
			const auto it = keyboard_states.find(id);
			if (it == keyboard_states.end() || it->second.generation != generation) return input_release_reason::none;
			return it->second.release_reasons[index];
		}

		static bool mouse_state(SDL_MouseID id, uint64_t generation, mouse_button button, int kind)
		{
			std::size_t index{};
			if (!mouse_button_index(button, index)) return false;
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			if (it == mouse_states.end() || it->second.generation != generation) return false;
			const auto& state = it->second;
			return kind == 0 ? state.pressed.test(index)
				: kind == 1 ? state.down.test(index)
				: state.released.test(index);
		}

		static bool legacy_keyboard_state(scancode code, int kind)
		{
			if (gui_wants_keyboard.load(std::memory_order_relaxed)) return false;
			std::size_t index{};
			if (!key_index(code, index)) return false;

			std::lock_guard lock(state_mutex);
			for (const auto& [id, state] : keyboard_states)
			{
				(void)id;
				if (!state.connected && kind != 2) continue;
				if (kind == 0 ? state.pressed.test(index)
					: kind == 1 ? (state.down.test(index) && !state.pressed.test(index))
					: state.released.test(index))
					return true;
			}
			return false;
		}

		static bool legacy_mouse_button_state(mouse_button button, int kind)
		{
			if (gui_wants_mouse.load(std::memory_order_relaxed)) return false;
			std::size_t index{};
			if (!mouse_button_index(button, index)) return false;

			std::lock_guard lock(state_mutex);
			for (const auto& [id, state] : mouse_states)
			{
				(void)id;
				if (!state.connected && kind != 2) continue;
				if (kind == 0 ? state.pressed.test(index)
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
			if (gui_wants_mouse.load(std::memory_order_relaxed)) return {};
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

		static input_release_reason mouse_release_reason(
			SDL_MouseID id, uint64_t generation, mouse_button button)
		{
			std::size_t index{};
			if (!mouse_button_index(button, index)) return input_release_reason::none;
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			if (it == mouse_states.end() || it->second.generation != generation) return input_release_reason::none;
			return it->second.release_reasons[index];
		}

		static bool joystick_button_state(
			SDL_JoystickID id, uint64_t generation, std::size_t button, int kind)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			if (it == joystick_states.end() || it->second.generation != generation ||
				button >= it->second.buttons.size()) return false;
			const auto& state = it->second.buttons[button];
			return kind == 0 ? state.pressed : kind == 1 ? state.down : state.released;
		}

		static input_release_reason joystick_release_reason(
			SDL_JoystickID id, uint64_t generation, std::size_t button)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			if (it == joystick_states.end() || it->second.generation != generation ||
				button >= it->second.buttons.size()) return input_release_reason::none;
			return it->second.buttons[button].release_reason;
		}

		static std::string joystick_name(SDL_JoystickID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation
				? it->second.name : std::string{};
		}

		static SDL_JoystickType joystick_type(SDL_JoystickID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation
				? it->second.type : SDL_JOYSTICK_TYPE_UNKNOWN;
		}

		static std::size_t joystick_control_count(
			SDL_JoystickID id, uint64_t generation, int kind)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			if (it == joystick_states.end() || it->second.generation != generation) return 0;
			const auto& state = it->second;
			return kind == 0 ? state.buttons.size()
				: kind == 1 ? state.axes.size()
				: kind == 2 ? state.hats.size() : state.ball_deltas.size();
		}

		static int16_t joystick_axis(SDL_JoystickID id, uint64_t generation, std::size_t axis)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation &&
				axis < it->second.axes.size() ? it->second.axes[axis] : 0;
		}

		static hat_position joystick_hat(SDL_JoystickID id, uint64_t generation, std::size_t hat)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation &&
				hat < it->second.hats.size() ? it->second.hats[hat] : hat_position::none;
		}

		static glm::ivec2 joystick_ball_delta(SDL_JoystickID id, uint64_t generation, std::size_t ball)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation &&
				ball < it->second.ball_deltas.size() ? it->second.ball_deltas[ball] : glm::ivec2{};
		}

		static bool gamepad_button_state(
			SDL_JoystickID id, uint64_t generation, std::size_t button, int kind)
		{
			std::lock_guard lock(state_mutex);
			const auto it = gamepad_states.find(id);
			if (it == gamepad_states.end() || it->second.generation != generation ||
				button >= it->second.buttons.size()) return false;
			const auto& state = it->second.buttons[button];
			return kind == 0 ? state.pressed : kind == 1 ? state.down : state.released;
		}

		static input_release_reason gamepad_release_reason(
			SDL_JoystickID id, uint64_t generation, std::size_t button)
		{
			std::lock_guard lock(state_mutex);
			const auto it = gamepad_states.find(id);
			if (it == gamepad_states.end() || it->second.generation != generation ||
				button >= it->second.buttons.size()) return input_release_reason::none;
			return it->second.buttons[button].release_reason;
		}

		static int16_t gamepad_axis_value(
			SDL_JoystickID id, uint64_t generation, std::size_t axis)
		{
			std::lock_guard lock(state_mutex);
			const auto it = gamepad_states.find(id);
			return it != gamepad_states.end() && it->second.generation == generation &&
				axis < it->second.axes.size() ? it->second.axes[axis] : 0;
		}

		static bool gamepad_connected(SDL_JoystickID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = gamepad_states.find(id);
			return it != gamepad_states.end() && it->second.generation == generation &&
				it->second.connected && it->second.handle && SDL_GamepadConnected(it->second.handle.get());
		}

		static bool keyboard_connected(SDL_KeyboardID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = keyboard_states.find(id);
			return it != keyboard_states.end() && it->second.generation == generation && it->second.connected;
		}

		static bool mouse_connected(SDL_MouseID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			return it != mouse_states.end() && it->second.generation == generation && it->second.connected;
		}

		static touch_finger_frame_state touch_finger_snapshot(
			SDL_TouchID touch_id, uint64_t device_generation, SDL_FingerID finger_id)
		{
			std::lock_guard lock(state_mutex);
			const auto device = touch_device_states.find(touch_id);
			if (device == touch_device_states.end() || device->second.generation != device_generation)
				return {};
			const auto finger = device->second.fingers.find(finger_id);
			return finger == device->second.fingers.end() ? touch_finger_frame_state{} : finger->second;
		}

		static bool touch_finger_exists(
			SDL_TouchID touch_id, uint64_t device_generation, SDL_FingerID finger_id)
		{
			std::lock_guard lock(state_mutex);
			const auto device = touch_device_states.find(touch_id);
			return device != touch_device_states.end() && device->second.generation == device_generation &&
				device->second.fingers.find(finger_id) != device->second.fingers.end();
		}

		static std::vector<SDL_FingerID> touch_finger_ids(SDL_TouchID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto device = touch_device_states.find(id);
			if (device == touch_device_states.end() || device->second.generation != generation) return {};
			std::vector<SDL_FingerID> result;
			result.reserve(device->second.fingers.size());
			for (const auto& [finger_id, finger] : device->second.fingers)
			{
				if (finger.down || finger.pressed || finger.released) result.push_back(finger_id);
			}
			std::sort(result.begin(), result.end());
			return result;
		}

		static bool touch_device_connected(SDL_TouchID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = touch_device_states.find(id);
			return it != touch_device_states.end() && it->second.generation == generation && it->second.connected;
		}

		static std::string touch_device_name(SDL_TouchID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = touch_device_states.find(id);
			return it != touch_device_states.end() && it->second.generation == generation
				? it->second.name : std::string{};
		}

		static SDL_TouchDeviceType touch_device_type(SDL_TouchID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = touch_device_states.find(id);
			return it != touch_device_states.end() && it->second.generation == generation
				? it->second.type : SDL_TOUCH_DEVICE_INVALID;
		}

		static void update_touch_finger(
			SDL_TouchID touch_id, SDL_FingerID finger_id, SDL_WindowID window_id,
			glm::vec2 position, glm::vec2 delta, float pressure, int event_kind)
		{
			if (!is_physical_touch_id(touch_id)) return;
			std::lock_guard lock(state_mutex);
			auto it = touch_device_states.find(touch_id);
			if (it != touch_device_states.end() && !it->second.connected) return;
			auto& device = it == touch_device_states.end()
				? ensure_touch_device_state_locked(touch_id) : it->second;
			auto& finger = device.fingers[finger_id];
			finger.position = position;
			finger.delta += delta;
			finger.pressure = pressure;
			finger.window_id = window_id;

			if (event_kind == 0)
			{
				if (!finger.down) finger.pressed = true;
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
				finger.release_reason = event_kind == 2
					? input_release_reason::physical_release : input_release_reason::canceled;
			}
		}

		static bool pen_button_index(pen_button button, std::size_t& index)
		{
			const auto value = static_cast<uint8_t>(button);
			if (value < 1 || value > 5) return false;
			index = value;
			return true;
		}

		static bool pen_axis_index(pen_axis_type axis, std::size_t& index)
		{
			const auto value = static_cast<uint8_t>(axis);
			if (value >= SDL_PEN_AXIS_COUNT) return false;
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

		static void update_pen_event(
			SDL_PenID id, SDL_WindowID window_id, glm::vec2 position,
			SDL_PenInputFlags flags, std::optional<bool> eraser = std::nullopt)
		{
			if (id == 0) return;
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
			if (id == 0) return;
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

		static pen_frame_state pen_snapshot(SDL_PenID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = pen_states.find(id);
			return it != pen_states.end() && it->second.generation == generation
				? it->second : pen_frame_state{};
		}

		static bool pen_button_state_value(
			SDL_PenID id, uint64_t generation, pen_button button, int kind)
		{
			std::size_t index{};
			if (!pen_button_index(button, index)) return false;
			const auto state = pen_snapshot(id, generation).buttons[index];
			return kind == 0 ? state.pressed : kind == 1 ? state.down : state.released;
		}

		static input_release_reason pen_button_release_reason(
			SDL_PenID id, uint64_t generation, pen_button button)
		{
			std::size_t index{};
			if (!pen_button_index(button, index)) return input_release_reason::none;
			return pen_snapshot(id, generation).buttons[index].release_reason;
		}

		static float pen_axis_value(SDL_PenID id, uint64_t generation, pen_axis_type axis)
		{
			std::size_t index{};
			if (!pen_axis_index(axis, index)) return 0.0f;
			return pen_snapshot(id, generation).axes[index];
		}

		static bool joystick_connected(SDL_JoystickID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = joystick_states.find(id);
			return it != joystick_states.end() && it->second.generation == generation && it->second.connected;
		}

		static glm::vec2 mouse_vector(SDL_MouseID id, uint64_t generation, int kind)
		{
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			if (it == mouse_states.end() || it->second.generation != generation) return {};
			if (kind == 0) return it->second.position;
			if (kind == 1) return it->second.delta;
			return it->second.scroll_delta;
		}

		static bool mouse_has_position(SDL_MouseID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			return it != mouse_states.end() && it->second.generation == generation && it->second.has_position;
		}

		static SDL_WindowID mouse_window_id(SDL_MouseID id, uint64_t generation)
		{
			std::lock_guard lock(state_mutex);
			const auto it = mouse_states.find(id);
			return it != mouse_states.end() && it->second.generation == generation
				? it->second.window_id : 0;
		}

		static void reset_window_state_locked(
			SDL_WindowID window_id,
			input_release_reason reason)
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
			if (window_id == 0) return true;
			{
				std::lock_guard lock(state_mutex);
				const auto it = window_focus_states.find(window_id);
				if (it != window_focus_states.end()) return it->second;
			}
			SDL_Window* window = SDL_GetWindowFromID(window_id);
			return !window || (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
		}

		static input_release_reason window_inactive_reason(SDL_WindowID window_id)
		{
			std::lock_guard lock(state_mutex);
			const auto it = window_inactive_reasons.find(window_id);
			return it != window_inactive_reasons.end() &&
				it->second == input_release_reason::window_destroyed
				? input_release_reason::window_destroyed : input_release_reason::focus_lost;
		}

		static uint64_t window_focus_loss_count(SDL_WindowID window_id)
		{
			std::lock_guard lock(state_mutex);
			const auto it = window_focus_loss_counts.find(window_id);
			return it == window_focus_loss_counts.end() ? 0 : it->second;
		}

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
				if (ids.get()[i] != 0) result.push_back(ids.get()[i]);
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
				if (ids.get()[i] != 0) result.push_back(ids.get()[i]);
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
				if (is_physical_touch_id(ids.get()[i])) result.push_back(ids.get()[i]);
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
				if (ids.get()[i] != 0) result.push_back(ids.get()[i]);
			}
			return result;
		}

		static void open_joystick_locked(SDL_JoystickID id, joystick_frame_state& state)
		{
			if (state.handle)
			{
				return;
			}

			std::unique_ptr<SDL_Joystick, decltype(&SDL_CloseJoystick)> handle(
				SDL_OpenJoystick(id), &SDL_CloseJoystick);
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
				state.hats[static_cast<std::size_t>(i)] = decode_hat_position(SDL_GetJoystickHat(handle.get(), i));
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
				if (ids.get()[i] != 0) result.push_back(ids.get()[i]);
			}
			return result;
		}

		static void open_gamepad_locked(SDL_JoystickID id, gamepad_frame_state& state)
		{
			if (state.handle) return;

			std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)> handle(
				SDL_OpenGamepad(id), &SDL_CloseGamepad);
			if (!handle)
				throw sdl_error("failed to open gamepad: ");

			for (std::size_t i = 0; i < state.buttons.size(); ++i)
				state.buttons[i].down = SDL_GetGamepadButton(
					handle.get(), static_cast<SDL_GamepadButton>(i));
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

		static void init_all()
		{
			[[maybe_unused]] static const bool input_device_events_enabled = []
			{
				SDL_SetJoystickEventsEnabled(true);
				SDL_SetGamepadEventsEnabled(true);
				return true;
			}();

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
			[[maybe_unused]] static auto window_focus_lost_subscription =
				event_manager::on<windowFocusLost>([](const windowFocusLost& e)
				{
					std::lock_guard lock(state_mutex);
					window_focus_states[e.windowID] = false;
					window_inactive_reasons[e.windowID] = input_release_reason::focus_lost;
					++window_focus_loss_counts[e.windowID];
					reset_window_state_locked(e.windowID, input_release_reason::focus_lost);
				});
			[[maybe_unused]] static auto window_focus_gained_subscription =
				event_manager::on<windowFocusGained>([](const windowFocusGained& e)
				{
					std::lock_guard lock(state_mutex);
					window_focus_states[e.windowID] = true;
					window_inactive_reasons[e.windowID] = input_release_reason::none;
				});
			[[maybe_unused]] static auto window_destroyed_subscription =
				event_manager::on<windowDestroyed>([](const windowDestroyed& e)
				{
					std::lock_guard lock(state_mutex);
					window_focus_states[e.windowID] = false;
					window_inactive_reasons[e.windowID] = input_release_reason::window_destroyed;
					reset_window_state_locked(e.windowID, input_release_reason::window_destroyed);
				});

			[[maybe_unused]] static auto keyboard_added_subscription =
				event_manager::on<keyboardAdded>([](const keyboardAdded& e)
				{
					if (e.keyboardID == 0) return;
					std::lock_guard lock(state_mutex);
					ensure_keyboard_state_locked(e.keyboardID);
				});
			[[maybe_unused]] static auto keyboard_removed_subscription =
				event_manager::on<keyboardRemoved>([](const keyboardRemoved& e)
				{
					if (e.keyboardID == 0) return;
					std::lock_guard lock(state_mutex);
					remove_keyboard_locked(e.keyboardID);
				});

			[[maybe_unused]] static auto mouse_added_subscription =
				event_manager::on<mouseAdded>([](const mouseAdded& e)
				{
					if (!is_physical_mouse_id(e.mouseID)) return;
					std::lock_guard lock(state_mutex);
					ensure_mouse_state_locked(e.mouseID);
				});
			[[maybe_unused]] static auto mouse_removed_subscription =
				event_manager::on<mouseRemoved>([](const mouseRemoved& e)
				{
					if (!is_physical_mouse_id(e.mouseID)) return;
					std::lock_guard lock(state_mutex);
					remove_mouse_locked(e.mouseID);
				});

			[[maybe_unused]] static auto key_down_subscription =
				event_manager::on<keyDown>([](const keyDown& e)
				{
					std::lock_guard lock(state_mutex);
					if (e.keyboardID != 0)
					{
						auto& state = ensure_keyboard_state_locked(e.keyboardID);
						std::size_t index{};
						if (key_index(e.code, index))
						{
							const bool was_down = state.down.test(index);
							state.down.set(index);
							state.key_windows[index] = e.windowID;
							if (!was_down && !e.repeat)
							{
								state.pressed.set(index);
								state.pressed_windows[index] = e.windowID;
							}
						}
					}
				});
			[[maybe_unused]] static auto key_up_subscription =
				event_manager::on<keyUp>([](const keyUp& e)
				{
					std::lock_guard lock(state_mutex);
					if (e.keyboardID != 0)
					{
						auto& state = ensure_keyboard_state_locked(e.keyboardID);
						std::size_t index{};
						if (key_index(e.code, index))
						{
							if (state.down.test(index))
							{
								state.released.set(index);
								state.release_reasons[index] = input_release_reason::physical_release;
								state.released_windows[index] = state.key_windows[index]
									? state.key_windows[index] : e.windowID;
							}
							state.down.reset(index);
							state.key_windows[index] = 0;
						}
					}
				});

			[[maybe_unused]] static auto mouse_motion_subscription =
				event_manager::on<mouseMotion>([](const mouseMotion& e)
				{
					std::lock_guard lock(state_mutex);
					if (is_physical_mouse_id(e.mouseID))
					{
						auto& state = ensure_mouse_state_locked(e.mouseID);
						set_mouse_window_locked(state, e.windowID);
						state.position = e.position;
						state.delta += e.relative;
						state.has_position = true;
					}
				});
			[[maybe_unused]] static auto mouse_button_down_subscription =
				event_manager::on<mouseButtonDown>([](const mouseButtonDown& e)
				{
					std::lock_guard lock(state_mutex);
					if (is_physical_mouse_id(e.mouseID))
					{
						auto& state = ensure_mouse_state_locked(e.mouseID);
						set_mouse_window_locked(state, e.windowID);
						std::size_t index{};
						if (mouse_button_index(static_cast<mouse_button>(e.button), index))
						{
							if (!state.down.test(index))
							{
								state.pressed.set(index);
								state.pressed_windows[index] = e.windowID;
							}
							state.down.set(index);
							state.button_windows[index] = e.windowID;
						}
						state.position = e.position;
						state.has_position = true;
					}
				});
			[[maybe_unused]] static auto mouse_button_up_subscription =
				event_manager::on<mouseButtonUp>([](const mouseButtonUp& e)
				{
					std::lock_guard lock(state_mutex);
					if (is_physical_mouse_id(e.mouseID))
					{
						auto& state = ensure_mouse_state_locked(e.mouseID);
						set_mouse_window_locked(state, e.windowID);
						std::size_t index{};
						if (mouse_button_index(static_cast<mouse_button>(e.button), index))
						{
							if (state.down.test(index))
							{
								state.released.set(index);
								state.release_reasons[index] = input_release_reason::physical_release;
								state.released_windows[index] = state.button_windows[index]
									? state.button_windows[index] : e.windowID;
							}
							state.down.reset(index);
							state.button_windows[index] = 0;
						}
						state.position = e.position;
						state.has_position = true;
					}
				});
			[[maybe_unused]] static auto mouse_wheel_subscription =
				event_manager::on<mouseWheel>([](const mouseWheel& e)
				{
					std::lock_guard lock(state_mutex);
					if (is_physical_mouse_id(e.which))
					{
						auto& state = ensure_mouse_state_locked(e.which);
						set_mouse_window_locked(state, e.windowID);
						state.position = e.mousePos;
						state.scroll_delta += e.scroll;
						state.has_position = true;
					}
				});

			[[maybe_unused]] static auto finger_down_subscription =
				event_manager::on<fingerDown>([](const fingerDown& e)
				{
					update_touch_finger(e.touchID, e.fingerID, e.windowID,
						e.position, e.delta, e.pressure, 0);
				});
			[[maybe_unused]] static auto finger_motion_subscription =
				event_manager::on<fingerMotion>([](const fingerMotion& e)
				{
					update_touch_finger(e.touchID, e.fingerID, e.windowID,
						e.position, e.delta, e.pressure, 1);
				});
			[[maybe_unused]] static auto finger_up_subscription =
				event_manager::on<fingerUp>([](const fingerUp& e)
				{
					update_touch_finger(e.touchID, e.fingerID, e.windowID,
						e.position, e.delta, e.pressure, 2);
				});
			[[maybe_unused]] static auto finger_canceled_subscription =
				event_manager::on<fingerCanceled>([](const fingerCanceled& e)
				{
					update_touch_finger(e.touchID, e.fingerID, e.windowID,
						e.position, e.delta, e.pressure, 3);
				});

			[[maybe_unused]] static auto pen_proximity_in_subscription =
				event_manager::on<penProximityIn>([](const penProximityIn& e)
				{
					update_pen_proximity(e.penID, e.windowID, true);
				});
			[[maybe_unused]] static auto pen_proximity_out_subscription =
				event_manager::on<penProximityOut>([](const penProximityOut& e)
				{
					update_pen_proximity(e.penID, e.windowID, false);
				});
			[[maybe_unused]] static auto pen_down_subscription =
				event_manager::on<penDown>([](const penDown& e)
				{
					update_pen_event(e.penID, e.windowID, e.position, e.state, e.eraser);
				});
			[[maybe_unused]] static auto pen_up_subscription =
				event_manager::on<penUp>([](const penUp& e)
				{
					update_pen_event(e.penID, e.windowID, e.position, e.state, e.eraser);
					std::lock_guard lock(state_mutex);
					const auto it = pen_states.find(e.penID);
					if (it != pen_states.end()) it->second.axes[SDL_PEN_AXIS_PRESSURE] = 0.0f;
				});
			[[maybe_unused]] static auto pen_button_down_subscription =
				event_manager::on<penButtonDown>([](const penButtonDown& e)
				{
					update_pen_event(e.penID, e.windowID, e.position, e.state);
				});
			[[maybe_unused]] static auto pen_button_up_subscription =
				event_manager::on<penButtonUp>([](const penButtonUp& e)
				{
					update_pen_event(e.penID, e.windowID, e.position, e.state);
				});
			[[maybe_unused]] static auto pen_motion_subscription =
				event_manager::on<penMotion>([](const penMotion& e)
				{
					update_pen_event(e.penID, e.windowID, e.position, e.state);
				});
			[[maybe_unused]] static auto pen_axis_subscription =
				event_manager::on<penAxis>([](const penAxis& e)
				{
					update_pen_event(e.penID, e.windowID, e.position, e.state);
					std::size_t index{};
					if (!pen_axis_index(e.axis, index)) return;
					std::lock_guard lock(state_mutex);
					const auto it = pen_states.find(e.penID);
					if (it != pen_states.end()) it->second.axes[index] = e.value;
				});

			[[maybe_unused]] static auto joystick_added_subscription =
				event_manager::on<joystickAdded>([](const joystickAdded& e)
				{
					if (e.which == 0) return;
					std::lock_guard lock(state_mutex);
					ensure_joystick_state_locked(e.which);
				});
			[[maybe_unused]] static auto joystick_removed_subscription =
				event_manager::on<joystickRemoved>([](const joystickRemoved& e)
				{
					if (e.which == 0) return;
					std::lock_guard lock(state_mutex);
					remove_joystick_locked(e.which);
				});
			[[maybe_unused]] static auto joystick_axis_subscription =
				event_manager::on<joystickAxisMoved>([](const joystickAxisMoved& e)
				{
					if (e.which == 0) return;
					std::lock_guard lock(state_mutex);
					auto& state = ensure_joystick_state_locked(e.which);
					if (!state.metadata_initialized && state.axes.size() <= e.axis)
						state.axes.resize(static_cast<std::size_t>(e.axis) + 1);
					if (e.axis < state.axes.size()) state.axes[e.axis] = e.value;
				});
			[[maybe_unused]] static auto joystick_button_down_subscription =
				event_manager::on<joystickButtonDown>([](const joystickButtonDown& e)
				{
					if (e.which == 0) return;
					std::lock_guard lock(state_mutex);
					auto& state = ensure_joystick_state_locked(e.which);
					if (!state.metadata_initialized && state.buttons.size() <= e.button)
						state.buttons.resize(static_cast<std::size_t>(e.button) + 1);
					if (e.button < state.buttons.size())
					{
						auto& button = state.buttons[e.button];
						if (!button.down) button.pressed = true;
						button.down = true;
					}
				});
			[[maybe_unused]] static auto joystick_button_up_subscription =
				event_manager::on<joystickButtonUp>([](const joystickButtonUp& e)
				{
					if (e.which == 0) return;
					std::lock_guard lock(state_mutex);
					auto& state = ensure_joystick_state_locked(e.which);
					if (!state.metadata_initialized && state.buttons.size() <= e.button)
						state.buttons.resize(static_cast<std::size_t>(e.button) + 1);
					if (e.button < state.buttons.size())
					{
						auto& button = state.buttons[e.button];
						if (button.down)
						{
							button.released = true;
							button.release_reason = input_release_reason::physical_release;
						}
						button.down = false;
					}
				});
			[[maybe_unused]] static auto joystick_hat_subscription =
				event_manager::on<joystickHatPressed>([](const joystickHatPressed& e)
				{
					if (e.which == 0) return;
					std::lock_guard lock(state_mutex);
					auto& state = ensure_joystick_state_locked(e.which);
					if (!state.metadata_initialized && state.hats.size() <= e.hat)
						state.hats.resize(static_cast<std::size_t>(e.hat) + 1, hat_position::centered);
					if (e.hat < state.hats.size()) state.hats[e.hat] = e.value;
				});
			[[maybe_unused]] static auto joystick_ball_subscription =
				event_manager::on<joystickBallMoved>([](const joystickBallMoved& e)
				{
					if (e.which == 0) return;
					std::lock_guard lock(state_mutex);
					auto& state = ensure_joystick_state_locked(e.which);
					if (!state.metadata_initialized && state.ball_deltas.size() <= e.ball)
						state.ball_deltas.resize(static_cast<std::size_t>(e.ball) + 1, glm::ivec2{});
					if (e.ball < state.ball_deltas.size()) state.ball_deltas[e.ball] += e.relative;
				});

			[[maybe_unused]] static auto gamepad_added_subscription =
				event_manager::on<gamepadAdded>([](const gamepadAdded& e)
				{
					if (e.which == 0) return;
					std::lock_guard lock(state_mutex);
					ensure_gamepad_state_locked(e.which);
				});
			[[maybe_unused]] static auto gamepad_removed_subscription =
				event_manager::on<gamepadRemoved>([](const gamepadRemoved& e)
				{
					if (e.which == 0) return;
					std::lock_guard lock(state_mutex);
					remove_gamepad_locked(e.which);
				});
			[[maybe_unused]] static auto gamepad_remapped_subscription =
				event_manager::on<gamepadRemapped>([](const gamepadRemapped& e)
				{
					if (e.which == 0) return;
					std::lock_guard lock(state_mutex);
					auto it = gamepad_states.find(e.which);
					if (it != gamepad_states.end() && it->second.connected)
						refresh_gamepad_mapping_locked(it->second);
				});
			[[maybe_unused]] static auto gamepad_axis_subscription =
				event_manager::on<gamepadAxis>([](const gamepadAxis& e)
				{
					if (e.which == 0) return;
					std::size_t index{};
					if (!gamepad_axis_index(e.axis, index)) return;
					std::lock_guard lock(state_mutex);
					auto& state = ensure_gamepad_state_locked(e.which);
					state.axes[index] = e.value;
				});
			[[maybe_unused]] static auto gamepad_button_down_subscription =
				event_manager::on<gamepadButtonDown>([](const gamepadButtonDown& e)
				{
					if (e.which == 0) return;
					std::size_t index{};
					if (!gamepad_button_index(e.button, index)) return;
					std::lock_guard lock(state_mutex);
					auto& state = ensure_gamepad_state_locked(e.which);
					auto& button = state.buttons[index];
					if (!button.down) button.pressed = true;
					button.down = true;
				});
			[[maybe_unused]] static auto gamepad_button_up_subscription =
				event_manager::on<gamepadButtonUp>([](const gamepadButtonUp& e)
				{
					if (e.which == 0) return;
					std::size_t index{};
					if (!gamepad_button_index(e.button, index)) return;
					std::lock_guard lock(state_mutex);
					auto& state = ensure_gamepad_state_locked(e.which);
					auto& button = state.buttons[index];
					if (button.down)
					{
						button.released = true;
						button.release_reason = input_release_reason::physical_release;
					}
					button.down = false;
				});
		}

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

		// Список соответствует текущему порядку устройств, возвращённому SDL.
		// Вызывайте эти методы на том же потоке, что и Input::continue_work().
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
			const auto devices = keyboards();
			if (num_from_list >= devices.size())
				throw std::out_of_range("keyboard index is outside Input::keyboards()");
			return devices[num_from_list];
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
			const auto devices = mice();
			if (num_from_list >= devices.size())
				throw std::out_of_range("mouse index is outside Input::mice()");
			return devices[num_from_list];
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
				if (!is_physical_touch_id(id)) continue;
				const auto& state = ensure_touch_device_state_locked(id);
				result.push_back(::mngl::touch_device(id, state.generation));
			}
			return result;
		}

		[[nodiscard]] static ::mngl::touch_device touch_device(std::size_t num_from_list = 0)
		{
			const auto devices = touch_devices();
			if (num_from_list >= devices.size())
				throw std::out_of_range("touch device index is outside Input::touch_devices()");
			return devices[num_from_list];
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
			if (num_from_list >= devices.size())
				throw std::out_of_range("pen index is outside Input::pens()");
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
			const auto devices = joysticks();
			if (num_from_list >= devices.size())
				throw std::out_of_range("joystick index is outside Input::joysticks()");
			return devices[num_from_list];
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
			const auto devices = gamepads();
			if (num_from_list >= devices.size())
				throw std::out_of_range("gamepad index is outside Input::gamepads()");
			return devices[num_from_list];
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
			if (!raw_mappings) throw sdl_error("failed to enumerate gamepad mappings: ");
			std::unique_ptr<char*, decltype(&SDL_free)> mappings(raw_mappings, &SDL_free);
			std::vector<std::string> result;
			result.reserve(static_cast<std::size_t>(std::max(count, 0)));
			for (int i = 0; i < count; ++i)
				if (mappings.get()[i]) result.emplace_back(mappings.get()[i]);
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
			//перерасчёт всех дельт, и тому подобного
			update_dt();
			update_manager::tick(dt.load(std::memory_order_relaxed));
			return true;
		}

		[[deprecated("use Input::keyboards() and keyboard::pressed()")]]
		static bool is_key_pressed(scancode code) { return legacy_keyboard_state(code, 0); }
		[[deprecated("use Input::keyboards() and keyboard::down()")]]
		static bool is_key_holded(scancode code) { return legacy_keyboard_state(code, 1); }
		[[deprecated("use Input::keyboards() and keyboard::released()")]]
		static bool is_key_released(scancode code) { return legacy_keyboard_state(code, 2); }

		[[deprecated("use Input::mouse().position()")]]
		static glm::vec2 get_mouse_position() { return legacy_mouse_position(); }
		[[deprecated("use Input::mouse().delta()")]]
		static glm::vec2 get_mouse_delta() { return legacy_mouse_delta(false); }
		[[deprecated("use Input::mouse().scroll_delta()")]]
		static glm::vec2 get_mouse_scroll_delta() { return legacy_mouse_delta(true); }

		[[deprecated("use Input::mice() and mouse::pressed()")]]
		static bool is_mouse_button_pressed(mouse_button button)
		{
			return legacy_mouse_button_state(button, 0);
		}
		[[deprecated("use Input::mice() and mouse::down()")]]
		static bool is_mouse_button_holded(mouse_button button)
		{
			return legacy_mouse_button_state(button, 1);
		}
		[[deprecated("use Input::mice() and mouse::released()")]]
		static bool is_mouse_button_released(mouse_button button)
		{
			return legacy_mouse_button_state(button, 2);
		}



		inline static std::atomic_bool should_work = true;
	};

	inline std::string keyboard::name() const
	{
		return Input::keyboard_name(m_id);
	}

	inline bool keyboard::connected() const
	{
		return Input::keyboard_connected(m_id, m_generation);
	}

	inline bool keyboard::pressed(scancode code) const
	{
		return Input::keyboard_state(m_id, m_generation, code, 0);
	}

	inline bool keyboard::down(scancode code) const
	{
		return Input::keyboard_state(m_id, m_generation, code, 1);
	}

	inline bool keyboard::released(scancode code) const
	{
		return Input::keyboard_state(m_id, m_generation, code, 2);
	}

	inline input_release_reason keyboard::release_reason(scancode code) const
	{
		return Input::keyboard_release_reason(m_id, m_generation, code);
	}

	inline std::string mouse::name() const
	{
		return Input::mouse_name(m_id);
	}

	inline bool mouse::connected() const
	{
		return Input::mouse_connected(m_id, m_generation);
	}

	inline bool mouse::pressed(mouse_button button) const
	{
		return Input::mouse_state(m_id, m_generation, button, 0);
	}

	inline bool mouse::down(mouse_button button) const
	{
		return Input::mouse_state(m_id, m_generation, button, 1);
	}

	inline bool mouse::released(mouse_button button) const
	{
		return Input::mouse_state(m_id, m_generation, button, 2);
	}

	inline input_release_reason mouse::release_reason(mouse_button button) const
	{
		return Input::mouse_release_reason(m_id, m_generation, button);
	}

	inline bool mouse::has_position() const
	{
		return Input::mouse_has_position(m_id, m_generation);
	}

	inline glm::vec2 mouse::position() const
	{
		return Input::mouse_vector(m_id, m_generation, 0);
	}

	inline SDL_WindowID mouse::window_id() const
	{
		return Input::mouse_window_id(m_id, m_generation);
	}

	inline bool touch_finger::pressed() const
	{
		return Input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).pressed;
	}

	inline bool touch_finger::down() const
	{
		return Input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).down;
	}

	inline bool touch_finger::released() const
	{
		return Input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).released;
	}

	inline input_release_reason touch_finger::release_reason() const
	{
		return Input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).release_reason;
	}

	inline glm::vec2 touch_finger::position() const
	{
		return Input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).position;
	}

	inline glm::vec2 touch_finger::position_in_window(glm::vec2 logical_size) const
	{
		const auto value = position();
		return { value.x * logical_size.x, value.y * logical_size.y };
	}

	inline glm::vec2 touch_finger::delta() const
	{
		return Input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).delta;
	}

	inline float touch_finger::pressure() const
	{
		return Input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).pressure;
	}

	inline SDL_WindowID touch_finger::window_id() const
	{
		return Input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).window_id;
	}

	inline std::string touch_device::name() const
	{
		return Input::touch_device_name(m_id, m_generation);
	}

	inline SDL_TouchDeviceType touch_device::type() const
	{
		return Input::touch_device_type(m_id, m_generation);
	}

	inline bool touch_device::connected() const
	{
		return Input::touch_device_connected(m_id, m_generation);
	}

	inline std::vector<touch_finger> touch_device::fingers() const
	{
		const auto ids = Input::touch_finger_ids(m_id, m_generation);
		std::vector<touch_finger> result;
		result.reserve(ids.size());
		for (const SDL_FingerID id : ids)
			result.push_back(touch_finger(m_id, m_generation, id));
		return result;
	}

	inline std::optional<touch_finger> touch_device::finger(SDL_FingerID id) const
	{
		if (!Input::touch_finger_exists(m_id, m_generation, id)) return std::nullopt;
		return touch_finger(m_id, m_generation, id);
	}

	inline SDL_PenDeviceType pen::type() const
	{
		return Input::pen_snapshot(m_id, m_generation).type;
	}

	inline bool pen::in_proximity() const
	{
		return Input::pen_snapshot(m_id, m_generation).in_proximity;
	}

	inline bool pen::has_position() const
	{
		return Input::pen_snapshot(m_id, m_generation).has_position;
	}

	inline glm::vec2 pen::position() const
	{
		return Input::pen_snapshot(m_id, m_generation).position;
	}

	inline glm::vec2 pen::delta() const
	{
		return Input::pen_snapshot(m_id, m_generation).delta;
	}

	inline SDL_WindowID pen::window_id() const
	{
		return Input::pen_snapshot(m_id, m_generation).window_id;
	}

	inline bool pen::is_eraser() const
	{
		return Input::pen_snapshot(m_id, m_generation).eraser;
	}

	inline bool pen::pressed() const
	{
		return Input::pen_snapshot(m_id, m_generation).buttons[0].pressed;
	}

	inline bool pen::down() const
	{
		return Input::pen_snapshot(m_id, m_generation).buttons[0].down;
	}

	inline bool pen::released() const
	{
		return Input::pen_snapshot(m_id, m_generation).buttons[0].released;
	}

	inline input_release_reason pen::release_reason() const
	{
		return Input::pen_snapshot(m_id, m_generation).buttons[0].release_reason;
	}

	inline bool pen::button_pressed(pen_button button) const
	{
		return Input::pen_button_state_value(m_id, m_generation, button, 0);
	}

	inline bool pen::button_down(pen_button button) const
	{
		return Input::pen_button_state_value(m_id, m_generation, button, 1);
	}

	inline bool pen::button_released(pen_button button) const
	{
		return Input::pen_button_state_value(m_id, m_generation, button, 2);
	}

	inline input_release_reason pen::button_release_reason(pen_button button) const
	{
		return Input::pen_button_release_reason(m_id, m_generation, button);
	}

	inline float pen::axis(pen_axis_type axis) const
	{
		return Input::pen_axis_value(m_id, m_generation, axis);
	}

	inline float pen::pressure() const
	{
		return axis(pen_axis_type::pressure);
	}

	inline glm::vec2 pen::tilt() const
	{
		return { axis(pen_axis_type::xtilt), axis(pen_axis_type::ytilt) };
	}

	inline glm::vec2 mouse::delta() const
	{
		return Input::mouse_vector(m_id, m_generation, 1);
	}

	inline glm::vec2 mouse::scroll_delta() const
	{
		return Input::mouse_vector(m_id, m_generation, 2);
	}

	inline std::string joystick::name() const
	{
		return Input::joystick_name(m_id, m_generation);
	}

	inline bool joystick::connected() const
	{
		return Input::joystick_connected(m_id, m_generation);
	}

	inline SDL_JoystickType joystick::type() const
	{
		return Input::joystick_type(m_id, m_generation);
	}

	inline std::size_t joystick::button_count() const
	{
		return Input::joystick_control_count(m_id, m_generation, 0);
	}

	inline std::size_t joystick::axis_count() const
	{
		return Input::joystick_control_count(m_id, m_generation, 1);
	}

	inline std::size_t joystick::hat_count() const
	{
		return Input::joystick_control_count(m_id, m_generation, 2);
	}

	inline std::size_t joystick::ball_count() const
	{
		return Input::joystick_control_count(m_id, m_generation, 3);
	}

	inline bool joystick::pressed(std::size_t button) const
	{
		return Input::joystick_button_state(m_id, m_generation, button, 0);
	}

	inline bool joystick::down(std::size_t button) const
	{
		return Input::joystick_button_state(m_id, m_generation, button, 1);
	}

	inline bool joystick::released(std::size_t button) const
	{
		return Input::joystick_button_state(m_id, m_generation, button, 2);
	}

	inline input_release_reason joystick::release_reason(std::size_t button) const
	{
		return Input::joystick_release_reason(m_id, m_generation, button);
	}

	inline int16_t joystick::axis(std::size_t axis_index) const
	{
		return Input::joystick_axis(m_id, m_generation, axis_index);
	}

	inline float joystick::normalized_axis(std::size_t axis_index) const
	{
		const int16_t value = axis(axis_index);
		return value < 0 ? value / 32768.0f : value / 32767.0f;
	}

	inline hat_position joystick::hat(std::size_t hat_index) const
	{
		return Input::joystick_hat(m_id, m_generation, hat_index);
	}

	inline glm::ivec2 joystick::ball_delta(std::size_t ball_index) const
	{
		return Input::joystick_ball_delta(m_id, m_generation, ball_index);
	}

	inline std::string gamepad::name() const
	{
		return Input::query_gamepad<std::string>(m_id, m_generation,
			[](SDL_Gamepad* value)
			{
				const char* name = SDL_GetGamepadName(value);
				return name ? std::string(name) : std::string{};
			});
	}

	inline std::string gamepad::path() const
	{
		return Input::query_gamepad<std::string>(m_id, m_generation,
			[](SDL_Gamepad* value)
			{
				const char* path = SDL_GetGamepadPath(value);
				return path ? std::string(path) : std::string{};
			});
	}

	inline SDL_GamepadType gamepad::type() const
	{
		return Input::query_gamepad<SDL_GamepadType>(m_id, m_generation,
			[](SDL_Gamepad* value) { return SDL_GetGamepadType(value); }, SDL_GAMEPAD_TYPE_UNKNOWN);
	}

	inline SDL_GamepadType gamepad::real_type() const
	{
		return Input::query_gamepad<SDL_GamepadType>(m_id, m_generation,
			[](SDL_Gamepad* value) { return SDL_GetRealGamepadType(value); }, SDL_GAMEPAD_TYPE_UNKNOWN);
	}

	inline SDL_GUID gamepad::guid() const
	{
		return Input::query_gamepad<SDL_GUID>(m_id, m_generation,
			[](SDL_Gamepad* value) { return SDL_GetJoystickGUID(SDL_GetGamepadJoystick(value)); }, SDL_GUID{});
	}

	inline bool gamepad::connected() const
	{
		return Input::gamepad_connected(m_id, m_generation);
	}

	inline SDL_JoystickConnectionState gamepad::connection_state() const
	{
		return Input::query_gamepad<SDL_JoystickConnectionState>(m_id, m_generation,
			[](SDL_Gamepad* value) { return SDL_GetGamepadConnectionState(value); },
			SDL_JOYSTICK_CONNECTION_INVALID);
	}

	inline int gamepad::player_index() const
	{
		return Input::query_gamepad<int>(m_id, m_generation,
			[](SDL_Gamepad* value) { return SDL_GetGamepadPlayerIndex(value); }, -1);
	}

	inline bool gamepad::set_player_index(int player_index) const
	{
		return Input::query_gamepad<bool>(m_id, m_generation,
			[player_index](SDL_Gamepad* value) { return SDL_SetGamepadPlayerIndex(value, player_index); }, false);
	}

	inline uint16_t gamepad::vendor() const
	{
		return Input::query_gamepad<uint16_t>(m_id, m_generation,
			[](SDL_Gamepad* value) { return SDL_GetGamepadVendor(value); }, 0);
	}

	inline uint16_t gamepad::product() const
	{
		return Input::query_gamepad<uint16_t>(m_id, m_generation,
			[](SDL_Gamepad* value) { return SDL_GetGamepadProduct(value); }, 0);
	}

	inline uint16_t gamepad::product_version() const
	{
		return Input::query_gamepad<uint16_t>(m_id, m_generation,
			[](SDL_Gamepad* value) { return SDL_GetGamepadProductVersion(value); }, 0);
	}

	inline uint16_t gamepad::firmware_version() const
	{
		return Input::query_gamepad<uint16_t>(m_id, m_generation,
			[](SDL_Gamepad* value) { return SDL_GetGamepadFirmwareVersion(value); }, 0);
	}

	inline std::string gamepad::serial() const
	{
		return Input::query_gamepad<std::string>(m_id, m_generation,
			[](SDL_Gamepad* value)
			{
				const char* serial = SDL_GetGamepadSerial(value);
				return serial ? std::string(serial) : std::string{};
			});
	}

	inline uint64_t gamepad::steam_handle() const
	{
		return Input::query_gamepad<uint64_t>(m_id, m_generation,
			[](SDL_Gamepad* value) { return SDL_GetGamepadSteamHandle(value); }, 0);
	}

	inline gamepad_battery_info gamepad::battery_info() const
	{
		return Input::query_gamepad<gamepad_battery_info>(m_id, m_generation,
			[](SDL_Gamepad* value)
			{
				int percent = -1;
				const SDL_PowerState state = SDL_GetGamepadPowerInfo(value, &percent);
				power_state converted = power_state::error;
				switch (state)
				{
				case SDL_POWERSTATE_UNKNOWN: converted = power_state::unknown; break;
				case SDL_POWERSTATE_ON_BATTERY: converted = power_state::on_battery; break;
				case SDL_POWERSTATE_NO_BATTERY: converted = power_state::no_battery; break;
				case SDL_POWERSTATE_CHARGING: converted = power_state::charging; break;
				case SDL_POWERSTATE_CHARGED: converted = power_state::charged; break;
				case SDL_POWERSTATE_ERROR: converted = power_state::error; break;
				}
				return gamepad_battery_info{ converted, percent };
			});
	}

	inline bool gamepad::has_button(gamepad_button button) const
	{
		std::size_t index{};
		if (!Input::gamepad_button_index(button, index)) return false;
		return Input::query_gamepad<bool>(m_id, m_generation,
			[button](SDL_Gamepad* value)
			{
				return SDL_GamepadHasButton(value, static_cast<SDL_GamepadButton>(button));
			}, false);
	}

	inline bool gamepad::pressed(gamepad_button button) const
	{
		std::size_t index{};
		return Input::gamepad_button_index(button, index) &&
			Input::gamepad_button_state(m_id, m_generation, index, 0);
	}

	inline bool gamepad::down(gamepad_button button) const
	{
		std::size_t index{};
		return Input::gamepad_button_index(button, index) &&
			Input::gamepad_button_state(m_id, m_generation, index, 1);
	}

	inline bool gamepad::released(gamepad_button button) const
	{
		std::size_t index{};
		return Input::gamepad_button_index(button, index) &&
			Input::gamepad_button_state(m_id, m_generation, index, 2);
	}

	inline input_release_reason gamepad::release_reason(gamepad_button button) const
	{
		std::size_t index{};
		return Input::gamepad_button_index(button, index)
			? Input::gamepad_release_reason(m_id, m_generation, index)
			: input_release_reason::none;
	}

	inline SDL_GamepadButtonLabel gamepad::button_label(gamepad_button button) const
	{
		std::size_t index{};
		if (!Input::gamepad_button_index(button, index)) return SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
		return Input::query_gamepad<SDL_GamepadButtonLabel>(m_id, m_generation,
			[button](SDL_Gamepad* value)
			{
				return SDL_GetGamepadButtonLabel(value, static_cast<SDL_GamepadButton>(button));
			}, SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN);
	}

	inline bool gamepad::has_axis(gamepad_axis axis) const
	{
		std::size_t index{};
		if (!Input::gamepad_axis_index(axis, index)) return false;
		return Input::query_gamepad<bool>(m_id, m_generation,
			[axis](SDL_Gamepad* value)
			{
				return SDL_GamepadHasAxis(value, static_cast<SDL_GamepadAxis>(axis));
			}, false);
	}

	inline int16_t gamepad::axis(gamepad_axis axis) const
	{
		std::size_t index{};
		return Input::gamepad_axis_index(axis, index)
			? Input::gamepad_axis_value(m_id, m_generation, index) : 0;
	}

	inline float gamepad::normalized_axis(gamepad_axis axis) const
	{
		const int16_t value = this->axis(axis);
		if (axis == gamepad_axis::lefttrigger || axis == gamepad_axis::righttrigger)
			return std::max(0.0f, value / 32767.0f);
		return value < 0 ? value / 32768.0f : value / 32767.0f;
	}

	inline std::string gamepad::mapping() const
	{
		return Input::query_gamepad<std::string>(m_id, m_generation,
			[](SDL_Gamepad* value)
			{
				std::unique_ptr<char, decltype(&SDL_free)> mapping(SDL_GetGamepadMapping(value), &SDL_free);
				return mapping ? std::string(mapping.get()) : std::string{};
			});
	}

	inline bool gamepad::set_mapping(std::string_view mapping) const
	{
		const std::string value(mapping);
		return Input::query_gamepad<bool>(m_id, m_generation,
			[&value](SDL_Gamepad* device)
			{
				return SDL_SetGamepadMapping(SDL_GetGamepadID(device), value.c_str());
			}, false);
	}

	inline std::vector<SDL_GamepadBinding> gamepad::bindings() const
	{
		return Input::query_gamepad<std::vector<SDL_GamepadBinding>>(m_id, m_generation,
			[](SDL_Gamepad* value)
			{
				int count = 0;
				auto** raw_bindings = SDL_GetGamepadBindings(value, &count);
				std::unique_ptr<SDL_GamepadBinding*, decltype(&SDL_free)> bindings(raw_bindings, &SDL_free);
				std::vector<SDL_GamepadBinding> result;
				if (!bindings || count <= 0) return result;
				result.reserve(static_cast<std::size_t>(count));
				for (int i = 0; i < count; ++i) result.push_back(*bindings.get()[i]);
				return result;
			});
	}

	inline int gamepad::touchpad_count() const
	{
		return Input::query_gamepad<int>(m_id, m_generation,
			[](SDL_Gamepad* value) { return SDL_GetNumGamepadTouchpads(value); }, 0);
	}

	inline int gamepad::touchpad_finger_count(int touchpad) const
	{
		return Input::query_gamepad<int>(m_id, m_generation,
			[touchpad](SDL_Gamepad* value) { return SDL_GetNumGamepadTouchpadFingers(value, touchpad); }, -1);
	}

	inline std::optional<gamepad_touchpad_finger> gamepad::touchpad_finger(int touchpad, int finger) const
	{
		return Input::query_gamepad<std::optional<gamepad_touchpad_finger>>(m_id, m_generation,
			[touchpad, finger](SDL_Gamepad* value) -> std::optional<gamepad_touchpad_finger>
			{
				bool down = false;
				float x = 0.0f, y = 0.0f, pressure = 0.0f;
				if (!SDL_GetGamepadTouchpadFinger(value, touchpad, finger, &down, &x, &y, &pressure))
					return std::nullopt;
				return gamepad_touchpad_finger{ down, { x, y }, pressure };
			}, std::nullopt);
	}

	inline bool gamepad::has_sensor(sensor_type sensor) const
	{
		if (!Input::gamepad_sensor_valid(sensor)) return false;
		return Input::query_gamepad<bool>(m_id, m_generation,
			[sensor](SDL_Gamepad* value)
			{
				return SDL_GamepadHasSensor(value, static_cast<SDL_SensorType>(sensor));
			}, false);
	}

	inline bool gamepad::set_sensor_enabled(sensor_type sensor, bool enabled) const
	{
		if (!Input::gamepad_sensor_valid(sensor)) return false;
		return Input::query_gamepad<bool>(m_id, m_generation,
			[sensor, enabled](SDL_Gamepad* value)
			{
				return SDL_SetGamepadSensorEnabled(value, static_cast<SDL_SensorType>(sensor), enabled);
			}, false);
	}

	inline bool gamepad::sensor_enabled(sensor_type sensor) const
	{
		if (!Input::gamepad_sensor_valid(sensor)) return false;
		return Input::query_gamepad<bool>(m_id, m_generation,
			[sensor](SDL_Gamepad* value)
			{
				return SDL_GamepadSensorEnabled(value, static_cast<SDL_SensorType>(sensor));
			}, false);
	}

	inline float gamepad::sensor_data_rate(sensor_type sensor) const
	{
		if (!Input::gamepad_sensor_valid(sensor)) return 0.0f;
		return Input::query_gamepad<float>(m_id, m_generation,
			[sensor](SDL_Gamepad* value)
			{
				return SDL_GetGamepadSensorDataRate(value, static_cast<SDL_SensorType>(sensor));
			}, 0.0f);
	}

	inline std::optional<std::vector<float>> gamepad::sensor_data(
		sensor_type sensor, std::size_t value_count) const
	{
		if (!Input::gamepad_sensor_valid(sensor) || value_count == 0 ||
			value_count > static_cast<std::size_t>(std::numeric_limits<int>::max())) return std::nullopt;
		return Input::query_gamepad<std::optional<std::vector<float>>>(m_id, m_generation,
			[sensor, value_count](SDL_Gamepad* value) -> std::optional<std::vector<float>>
			{
				std::vector<float> data(value_count);
				if (!SDL_GetGamepadSensorData(value, static_cast<SDL_SensorType>(sensor),
					data.data(), static_cast<int>(data.size()))) return std::nullopt;
				return data;
			}, std::nullopt);
	}

	inline bool gamepad::has_rumble() const
	{
		return Input::query_gamepad<bool>(m_id, m_generation,
			[](SDL_Gamepad* value)
			{
				return SDL_GetBooleanProperty(SDL_GetGamepadProperties(value),
					SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
			}, false);
	}

	inline bool gamepad::has_trigger_rumble() const
	{
		return Input::query_gamepad<bool>(m_id, m_generation,
			[](SDL_Gamepad* value)
			{
				return SDL_GetBooleanProperty(SDL_GetGamepadProperties(value),
					SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false);
			}, false);
	}

	inline bool gamepad::has_rgb_led() const
	{
		return Input::query_gamepad<bool>(m_id, m_generation,
			[](SDL_Gamepad* value)
			{
				return SDL_GetBooleanProperty(SDL_GetGamepadProperties(value),
					SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);
			}, false);
	}

	inline bool gamepad::has_player_led() const
	{
		return Input::query_gamepad<bool>(m_id, m_generation,
			[](SDL_Gamepad* value)
			{
				return SDL_GetBooleanProperty(SDL_GetGamepadProperties(value),
					SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN, false);
			}, false);
	}

	inline bool gamepad::has_mono_led() const
	{
		return Input::query_gamepad<bool>(m_id, m_generation,
			[](SDL_Gamepad* value)
			{
				return SDL_GetBooleanProperty(SDL_GetGamepadProperties(value),
					SDL_PROP_GAMEPAD_CAP_MONO_LED_BOOLEAN, false);
			}, false);
	}

	inline bool gamepad::rumble(uint16_t low_frequency, uint16_t high_frequency, uint32_t duration_ms) const
	{
		return Input::query_gamepad<bool>(m_id, m_generation,
			[low_frequency, high_frequency, duration_ms](SDL_Gamepad* value)
			{
				return SDL_RumbleGamepad(value, low_frequency, high_frequency, duration_ms);
			}, false);
	}

	inline bool gamepad::rumble_triggers(uint16_t left, uint16_t right, uint32_t duration_ms) const
	{
		return Input::query_gamepad<bool>(m_id, m_generation,
			[left, right, duration_ms](SDL_Gamepad* value)
			{
				return SDL_RumbleGamepadTriggers(value, left, right, duration_ms);
			}, false);
	}

	inline bool gamepad::set_led(uint8_t red, uint8_t green, uint8_t blue) const
	{
		return Input::query_gamepad<bool>(m_id, m_generation,
			[red, green, blue](SDL_Gamepad* value) { return SDL_SetGamepadLED(value, red, green, blue); }, false);
	}

	inline bool gamepad::send_effect(std::span<const std::byte> effect) const
	{
		if (effect.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
		return Input::query_gamepad<bool>(m_id, m_generation,
			[effect](SDL_Gamepad* value)
			{
				return SDL_SendGamepadEffect(value, effect.data(), static_cast<int>(effect.size()));
			}, false);
	}

	inline std::string gamepad::apple_button_symbol(gamepad_button button) const
	{
		std::size_t index{};
		if (!Input::gamepad_button_index(button, index)) return {};
		return Input::query_gamepad<std::string>(m_id, m_generation,
			[button](SDL_Gamepad* value)
			{
				const char* symbol = SDL_GetGamepadAppleSFSymbolsNameForButton(
					value, static_cast<SDL_GamepadButton>(button));
				return symbol ? std::string(symbol) : std::string{};
			});
	}

	inline std::string gamepad::apple_axis_symbol(gamepad_axis axis) const
	{
		std::size_t index{};
		if (!Input::gamepad_axis_index(axis, index)) return {};
		return Input::query_gamepad<std::string>(m_id, m_generation,
			[axis](SDL_Gamepad* value)
			{
				const char* symbol = SDL_GetGamepadAppleSFSymbolsNameForAxis(
					value, static_cast<SDL_GamepadAxis>(axis));
				return symbol ? std::string(symbol) : std::string{};
			});
	}
}
