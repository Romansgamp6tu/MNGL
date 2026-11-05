#pragma once

#include "core_events.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace mngl
{
	struct keyboard_device : event_base
	{
		std::uint32_t keyboard_id;
	};
	struct keymap_changed : event_base
	{
	};
	struct keyboard_added : event_base
	{
		std::uint32_t keyboard_id;
	};
	struct keyboard_removed : event_base
	{
		std::uint32_t keyboard_id;
	};
	struct screen_keyboard_shown : event_base
	{
	};
	struct screen_keyboard_hidden : event_base
	{
	};

	struct key_down : window_event
	{
		std::uint32_t keyboard_id;
		scancode code;
		keycode key;
		keymod mod;
		bool repeat;
		bool is_down;
	};
	struct key_up : window_event
	{
		std::uint32_t keyboard_id;
		scancode code;
		keycode key;
		keymod mod;
		bool repeat;
		bool is_down;
	};

	struct text_editing : window_event
	{
		std::string text;
		std::int32_t start;
		std::int32_t length;
	};
	struct text_input : window_event
	{
		std::string text;
	};
	struct text_editing_candidates : window_event
	{
		std::vector<std::string> candidates;
		std::int32_t selected;
		bool horizontal;
	};

	// --- Mouse ---
	struct mouse_device : event_base
	{
		std::uint32_t mouse_id;
	};
	struct mouse_added : event_base
	{
		std::uint32_t mouse_id;
	};
	struct mouse_removed : event_base
	{
		std::uint32_t mouse_id;
	};

	struct mouse_motion : window_event
	{
		std::uint32_t mouse_id;
		glm::vec2 position;
		glm::vec2 relative;
		std::uint32_t state;
	};
	struct mouse_button_down : window_event
	{
		std::uint32_t mouse_id;
		std::uint8_t button;
		std::uint8_t clicks;
		bool is_down;
		glm::vec2 position;
	};
	struct mouse_button_up : window_event
	{
		std::uint32_t mouse_id;
		std::uint8_t button;
		std::uint8_t clicks;
		bool is_down;
		glm::vec2 position;
	};
	struct mouse_wheel : window_event
	{
		std::uint32_t which;
		glm::vec2 scroll;
		glm::vec2 mouse_position;
		glm::ivec2 integer_scroll;
		wheel_direction direction;
	};

	// --- Joystick ---
	struct joystick_axis_moved : event_base
	{
		std::uint32_t which;
		std::uint8_t axis;
		std::int16_t value;
	};
	struct joystick_ball_moved : event_base
	{
		std::uint32_t which;
		std::uint8_t ball;
		glm::ivec2 relative;
	};
	struct joystick_hat_pressed : event_base
	{
		std::uint32_t which;
		std::uint8_t hat;
		hat_position value;
	};
	struct joystick_button_down : event_base
	{
		std::uint32_t which;
		std::uint8_t button;
		bool down;
	};
	struct joystick_button_up : event_base
	{
		std::uint32_t which;
		std::uint8_t button;
		bool down;
	};
	struct joystick_added : event_base
	{
		std::uint32_t which;
	};
	struct joystick_removed : event_base
	{
		std::uint32_t which;
	};
	struct joystick_update_complete : event_base
	{
		std::uint32_t which;
	};
	struct joystick_battery : event_base
	{
		std::uint32_t which;
		power_state state;
		float percent;
	};

	// --- Gamepad ---
	struct gamepad_axis_event : event_base
	{
		std::uint32_t which;
		gamepad_axis axis;
		std::int16_t value;
	};
	struct gamepad_button_down : event_base
	{
		std::uint32_t which;
		gamepad_button button;
		bool down;
	};
	struct gamepad_button_up : event_base
	{
		std::uint32_t which;
		gamepad_button button;
		bool down;
	};
	struct gamepad_added : event_base
	{
		std::uint32_t which;
	};
	struct gamepad_removed : event_base
	{
		std::uint32_t which;
	};
	struct gamepad_remapped : event_base
	{
		std::uint32_t which;
	};
	struct gamepad_update_complete : event_base
	{
		std::uint32_t which;
	};
	struct gamepad_steam_handle_updated : event_base
	{
		std::uint32_t which;
	};

	struct gamepad_touchpad_down : event_base
	{
		std::uint32_t which;
		std::int32_t touchpad;
		std::int32_t finger;
		glm::vec2 position;
		float pressure;
	};
	struct gamepad_touchpad_motion : event_base
	{
		std::uint32_t which;
		std::int32_t touchpad;
		std::int32_t finger;
		glm::vec2 position;
		float pressure;
	};
	struct gamepad_touchpad_up : event_base
	{
		std::uint32_t which;
		std::int32_t touchpad;
		std::int32_t finger;
		glm::vec2 position;
		float pressure;
	};
	struct gamepad_sensor : event_base
	{
		std::uint32_t which;
		sensor_type sensor;
		glm::vec3 data;
		std::uint64_t sensor_timestamp;
	};

	// --- Touch ---
	struct finger_down : event_base
	{
		std::uint64_t touch_id;
		std::uint64_t finger_id;
		std::uint32_t window_id;
		glm::vec2 position;
		glm::vec2 delta;
		float pressure;
	};
	struct finger_motion : event_base
	{
		std::uint64_t touch_id;
		std::uint64_t finger_id;
		std::uint32_t window_id;
		glm::vec2 position;
		glm::vec2 delta;
		float pressure;
	};
	struct finger_up : event_base
	{
		std::uint64_t touch_id;
		std::uint64_t finger_id;
		std::uint32_t window_id;
		glm::vec2 position;
		glm::vec2 delta;
		float pressure;
	};
	struct finger_canceled : event_base
	{
		std::uint64_t touch_id;
		std::uint64_t finger_id;
		std::uint32_t window_id;
		glm::vec2 position;
		glm::vec2 delta;
		float pressure;
	};

	// --- Pinch ---
	struct pinch_begin : event_base
	{
		std::uint32_t window_id;
		float scale;
	};
	struct pinch_update : event_base
	{
		std::uint32_t window_id;
		float scale;
	};
	struct pinch_end : event_base
	{
		std::uint32_t window_id;
		float scale;
	};

	// --- Clipboard ---

} // namespace mngl
