#pragma once

#include "context.hpp"

namespace mngl
{
	namespace action_presets
	{
		struct platformer_action_names
		{
			std::string_view move_left{"move_left"};
			std::string_view move_right{"move_right"};
			std::string_view jump{"jump"};
			std::string_view pause{"pause"};
		};

		inline void platformer(action_map& map, platformer_action_names names = {})
		{
			auto bind_key = [&](std::string_view action, scancode key)
			{
				if (!action.empty())
					map.bind_key(action, key);
			};
			auto bind_pad = [&](std::string_view action, gamepad_button button)
			{
				if (!action.empty())
					map.bind_gamepad_button(action, button);
			};
			bind_key(names.move_left, scancode::a);
			bind_key(names.move_left, scancode::left);
			bind_pad(names.move_left, gamepad_button::dpad_left);
			bind_key(names.move_right, scancode::d);
			bind_key(names.move_right, scancode::right);
			bind_pad(names.move_right, gamepad_button::dpad_right);
			bind_key(names.jump, scancode::space);
			bind_pad(names.jump, gamepad_button::a);
			bind_key(names.pause, scancode::escape);
			bind_pad(names.pause, gamepad_button::start);
		}

		struct menu_action_names
		{
			std::string_view confirm{"confirm"};
			std::string_view cancel{"cancel"};
			std::string_view up{"up"};
			std::string_view down{"down"};
			std::string_view left{"left"};
			std::string_view right{"right"};
		};

		inline void menu(action_map& map, menu_action_names names = {})
		{
			auto bind_key = [&](std::string_view action, scancode key)
			{
				if (!action.empty())
					map.bind_key(action, key);
			};
			auto bind_pad = [&](std::string_view action, gamepad_button button)
			{
				if (!action.empty())
					map.bind_gamepad_button(action, button);
			};
			bind_key(names.confirm, scancode::return_key);
			bind_key(names.confirm, scancode::kp_enter);
			bind_pad(names.confirm, gamepad_button::a);
			bind_key(names.cancel, scancode::escape);
			bind_pad(names.cancel, gamepad_button::b);
			bind_key(names.up, scancode::up);
			bind_key(names.up, scancode::w);
			bind_pad(names.up, gamepad_button::dpad_up);
			bind_key(names.down, scancode::down);
			bind_key(names.down, scancode::s);
			bind_pad(names.down, gamepad_button::dpad_down);
			bind_key(names.left, scancode::left);
			bind_key(names.left, scancode::a);
			bind_pad(names.left, gamepad_button::dpad_left);
			bind_key(names.right, scancode::right);
			bind_key(names.right, scancode::d);
			bind_pad(names.right, gamepad_button::dpad_right);
		}
	} // namespace action_presets
} // namespace mngl
