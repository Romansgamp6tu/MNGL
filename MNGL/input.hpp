#pragma once

#include <cstdint>

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

	enum class mouse_button : std::uint8_t
	{
		left = 1,   // SDL_BUTTON_LEFT
		middle = 2, // SDL_BUTTON_MIDDLE
		right = 3,  // SDL_BUTTON_RIGHT
		x1 = 4,     // SDL_BUTTON_X1
		x2 = 5      // SDL_BUTTON_X2
	};

	enum class input_release_reason : std::uint8_t
	{
		none,
		physical_release,
		focus_lost,
		device_removed,
		window_destroyed,
		mapping_changed,
		canceled
	};

	enum class pen_button : std::uint8_t
	{
		button1 = 1,
		button2 = 2,
		button3 = 3,
		button4 = 4,
		button5 = 5
	};

	class keyboard
	{
		friend class input;
		keyboard(SDL_KeyboardID id, std::uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_KeyboardID m_id{};
		std::uint64_t m_generation{};

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
		friend class input;
		mouse(SDL_MouseID id, std::uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_MouseID m_id{};
		std::uint64_t m_generation{};

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
		friend class input;
		friend class touch_device;
		touch_finger(SDL_TouchID touch_id, std::uint64_t device_generation, SDL_FingerID finger_id)
		    : m_touch_id(touch_id), m_device_generation(device_generation), m_finger_id(finger_id)
		{
		}

		SDL_TouchID m_touch_id{};
		std::uint64_t m_device_generation{};
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
		friend class input;
		touch_device(SDL_TouchID id, std::uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_TouchID m_id{};
		std::uint64_t m_generation{};

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
		friend class input;
		pen(SDL_PenID id, std::uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_PenID m_id{};
		std::uint64_t m_generation{};

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
		friend class input;
		joystick(SDL_JoystickID id, std::uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_JoystickID m_id{};
		std::uint64_t m_generation{};

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
		[[nodiscard]] std::int16_t axis(std::size_t axis) const;
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
		power_state state{power_state::unknown};
		int percent{-1};
	};

	class gamepad
	{
		friend class input;
		gamepad(SDL_JoystickID id, std::uint64_t generation) : m_id(id), m_generation(generation) {}

		SDL_JoystickID m_id{};
		std::uint64_t m_generation{};

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
		[[nodiscard]] std::uint16_t vendor() const;
		[[nodiscard]] std::uint16_t product() const;
		[[nodiscard]] std::uint16_t product_version() const;
		[[nodiscard]] std::uint16_t firmware_version() const;
		[[nodiscard]] std::string serial() const;
		[[nodiscard]] std::uint64_t steam_handle() const;
		[[nodiscard]] gamepad_battery_info battery_info() const;

		[[nodiscard]] bool has_button(gamepad_button button) const;
		[[nodiscard]] bool pressed(gamepad_button button) const;
		[[nodiscard]] bool down(gamepad_button button) const;
		[[nodiscard]] bool released(gamepad_button button) const;
		[[nodiscard]] input_release_reason release_reason(gamepad_button button) const;
		[[nodiscard]] SDL_GamepadButtonLabel button_label(gamepad_button button) const;
		[[nodiscard]] bool has_axis(gamepad_axis axis) const;
		[[nodiscard]] std::int16_t axis(gamepad_axis axis) const;
		[[nodiscard]] float normalized_axis(gamepad_axis axis) const;

		[[nodiscard]] std::string mapping() const;
		[[nodiscard]] bool set_mapping(std::string_view mapping) const;
		[[nodiscard]] std::vector<SDL_GamepadBinding> bindings() const;

		[[nodiscard]] int touchpad_count() const;
		[[nodiscard]] int touchpad_finger_count(int touchpad) const;
		[[nodiscard]] std::optional<gamepad_touchpad_finger> touchpad_finger(int touchpad, int finger) const;

		[[nodiscard]] bool has_sensor(sensor_type sensor) const;
		[[nodiscard]] bool set_sensor_enabled(sensor_type sensor, bool enabled) const;
		[[nodiscard]] bool sensor_enabled(sensor_type sensor) const;
		[[nodiscard]] float sensor_data_rate(sensor_type sensor) const;
		[[nodiscard]] std::optional<std::vector<float>> sensor_data(sensor_type sensor,
		                                                            std::size_t value_count = 3) const;

		[[nodiscard]] bool has_rumble() const;
		[[nodiscard]] bool has_trigger_rumble() const;
		[[nodiscard]] bool has_rgb_led() const;
		[[nodiscard]] bool has_player_led() const;
		[[nodiscard]] bool has_mono_led() const;
		[[nodiscard]] bool rumble(std::uint16_t low_frequency, std::uint16_t high_frequency,
		                          std::uint32_t duration_ms) const;
		[[nodiscard]] bool rumble_triggers(std::uint16_t left, std::uint16_t right,
		                                   std::uint32_t duration_ms) const;
		[[nodiscard]] bool set_led(std::uint8_t red, std::uint8_t green, std::uint8_t blue) const;
		[[nodiscard]] bool send_effect(std::span<const std::byte> effect) const;
		[[nodiscard]] std::string apple_button_symbol(gamepad_button button) const;
		[[nodiscard]] std::string apple_axis_symbol(gamepad_axis axis) const;
	};

	class input
	{
		friend class keyboard;
		friend class mouse;
		friend class touch_finger;
		friend class touch_device;
		friend class pen;
		friend class joystick;
		friend class gamepad;
		friend class action_map;
		friend class input_context;

	private:
#include "detail/input/state.inc"
#include "detail/input/frame_clock.inc"
#include "detail/input/device_helpers.inc"
#include "detail/input/device_state.inc"
#include "detail/input/device_enumeration.inc"
#include "detail/input/event_subscriptions.inc"
#include "detail/input/frame_processing.inc"
	public:
#include "detail/input/public_api.inc"
	};

#include "detail/input/keyboard_mouse_methods.inc"
#include "detail/input/touch_pen_methods.inc"
#include "detail/input/joystick_gamepad_methods.inc"
} // namespace mngl

namespace mngl
{
	using Input = input;
}
