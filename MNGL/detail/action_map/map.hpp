#pragma once

#include "../../input.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <variant>

namespace mngl
{
	// Mouse motion and wheel axes are sampled once per input::next_frame() frame.
	enum class mouse_axis : std::uint8_t
	{
		delta_x,
		delta_y,
		scroll_x,
		scroll_y
	};

	struct action_state
	{
		// pressed/released are one-frame aggregate transitions; down is the final
		// threshold state. value is the scaled, clamped aggregate analog value.
		bool pressed{};
		bool down{};
		bool released{};
		float value{};
		input_release_reason release_reason{input_release_reason::none};
	};

	// Named action bindings. Copies share storage so existing InputContexts see edits.
	class action_map
	{
		struct key_binding
		{
			scancode code{};
			std::optional<SDL_KeyboardID> device;
			float scale{1.0f};
		};
		struct mouse_button_binding
		{
			mouse_button button{};
			std::optional<SDL_MouseID> device;
			float scale{1.0f};
		};
		struct mouse_axis_binding
		{
			mouse_axis axis{};
			std::optional<SDL_MouseID> device;
			float scale{1.0f};
			float dead_zone{};
			float threshold{0.5f};
		};
		struct joystick_button_binding
		{
			std::size_t button{};
			std::optional<SDL_JoystickID> device;
			float scale{1.0f};
		};
		struct gamepad_button_binding
		{
			gamepad_button button{};
			std::optional<SDL_JoystickID> device;
			float scale{1.0f};
		};
		struct touch_contact_binding
		{
			std::optional<SDL_TouchID> device;
			float scale{1.0f};
		};
		struct pen_button_binding
		{
			std::size_t button{}; // 0 is tip contact; 1-5 are pen buttons.
			std::optional<SDL_PenID> device;
			float scale{1.0f};
		};
		struct joystick_axis_binding
		{
			std::size_t axis{};
			std::optional<SDL_JoystickID> device;
			float scale{1.0f};
			float dead_zone{0.15f};
			float threshold{0.5f};
		};
		struct gamepad_axis_binding
		{
			gamepad_axis axis{};
			std::optional<SDL_JoystickID> device;
			float scale{1.0f};
			float dead_zone{0.15f};
			float threshold{0.5f};
		};
		struct touch_pressure_binding
		{
			std::optional<SDL_TouchID> device;
			float scale{1.0f};
			float dead_zone{};
			float threshold{0.5f};
		};
		struct pen_axis_binding
		{
			pen_axis_type axis{};
			std::optional<SDL_PenID> device;
			float scale{1.0f};
			float dead_zone{};
			float threshold{0.5f};
		};

		using binding = std::variant<key_binding, mouse_button_binding, mouse_axis_binding,
		                             joystick_button_binding, gamepad_button_binding, touch_contact_binding,
		                             pen_button_binding, joystick_axis_binding, gamepad_axis_binding,
		                             touch_pressure_binding, pen_axis_binding>;

		struct storage
		{
			std::unordered_map<std::string, std::vector<binding>> actions;
			std::uint64_t revision{1};
		};

		std::shared_ptr<storage> m_storage{std::make_shared<storage>()};
		friend class input_context;

		static void validate_action_name(std::string_view name)
		{
			if (name.empty()) [[unlikely]]
				throw std::invalid_argument("action name cannot be empty");
		}

		static void validate_scale(float scale)
		{
			if (!std::isfinite(scale) || scale == 0.0f) [[unlikely]]
				throw std::invalid_argument("action binding scale must be finite and non-zero");
		}

		static void validate_axis_parameters(float scale, float dead_zone, float threshold)
		{
			validate_scale(scale);
			if (!std::isfinite(dead_zone) || dead_zone < 0.0f || dead_zone >= 1.0f) [[unlikely]]
				throw std::invalid_argument("axis dead zone must be in [0, 1)");
			if (!std::isfinite(threshold) || threshold <= 0.0f || threshold > 1.0f) [[unlikely]]
				throw std::invalid_argument("axis action threshold must be in (0, 1]");
		}

		template <typename Binding> void append(std::string_view action, Binding value)
		{
			validate_action_name(action);
			m_storage->actions[std::string(action)].emplace_back(std::move(value));
			++m_storage->revision;
		}

	public:
		// Bindings are additive: one action may be triggered by any number of devices.
		// An omitted device ID means any connected device of that type.
		void bind_key(std::string_view action, scancode code,
		              std::optional<SDL_KeyboardID> keyboard = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			[[maybe_unused]] std::size_t index{};
			if (!input::key_index(code, index)) [[unlikely]]
				throw std::invalid_argument("invalid keyboard scancode");
			append(action, key_binding{code, keyboard, scale});
		}

		void bind_mouse_button(std::string_view action, mouse_button button,
		                       std::optional<SDL_MouseID> mouse = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			[[maybe_unused]] std::size_t index{};
			if (!input::mouse_button_index(button, index)) [[unlikely]]
				throw std::invalid_argument("invalid mouse button");
			append(action, mouse_button_binding{button, mouse, scale});
		}

		void bind_mouse_axis(std::string_view action, mouse_axis axis, float scale = 1.0f,
		                     float dead_zone = 0.0f, float threshold = 0.5f,
		                     std::optional<SDL_MouseID> mouse = std::nullopt)
		{
			validate_axis_parameters(scale, dead_zone, threshold);
			if (static_cast<std::uint8_t>(axis) > static_cast<std::uint8_t>(mouse_axis::scroll_y)) [[unlikely]]
				throw std::invalid_argument("invalid mouse axis");
			append(action, mouse_axis_binding{axis, mouse, scale, dead_zone, threshold});
		}

		void bind_joystick_button(std::string_view action, std::size_t button,
		                          std::optional<SDL_JoystickID> joystick = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			append(action, joystick_button_binding{button, joystick, scale});
		}

		void bind_gamepad_button(std::string_view action, gamepad_button button,
		                         std::optional<SDL_JoystickID> gamepad = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			[[maybe_unused]] std::size_t index{};
			if (!input::gamepad_button_index(button, index)) [[unlikely]]
				throw std::invalid_argument("invalid gamepad button");
			append(action, gamepad_button_binding{button, gamepad, scale});
		}

		void bind_touch_contact(std::string_view action,
		                        std::optional<SDL_TouchID> touch_device = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			append(action, touch_contact_binding{touch_device, scale});
		}

		void bind_pen_tip(std::string_view action, std::optional<SDL_PenID> pen_device = std::nullopt,
		                  float scale = 1.0f)
		{
			validate_scale(scale);
			append(action, pen_button_binding{0, pen_device, scale});
		}

		void bind_pen_button(std::string_view action, pen_button button,
		                     std::optional<SDL_PenID> pen_device = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			const auto index = static_cast<std::size_t>(button);
			if (index < 1 || index > 5) [[unlikely]]
				throw std::invalid_argument("pen button must be button1 through button5");
			append(action, pen_button_binding{index, pen_device, scale});
		}

		void bind_joystick_axis(std::string_view action, std::size_t axis, float scale = 1.0f,
		                        float dead_zone = 0.15f, float threshold = 0.5f,
		                        std::optional<SDL_JoystickID> joystick = std::nullopt)
		{
			validate_axis_parameters(scale, dead_zone, threshold);
			append(action, joystick_axis_binding{axis, joystick, scale, dead_zone, threshold});
		}

		void bind_gamepad_axis(std::string_view action, gamepad_axis axis, float scale = 1.0f,
		                       float dead_zone = 0.15f, float threshold = 0.5f,
		                       std::optional<SDL_JoystickID> gamepad = std::nullopt)
		{
			validate_axis_parameters(scale, dead_zone, threshold);
			[[maybe_unused]] std::size_t index{};
			if (!input::gamepad_axis_index(axis, index)) [[unlikely]]
				throw std::invalid_argument("invalid gamepad axis");
			append(action, gamepad_axis_binding{axis, gamepad, scale, dead_zone, threshold});
		}

		void bind_touch_pressure(std::string_view action, float scale = 1.0f, float dead_zone = 0.0f,
		                         float threshold = 0.5f,
		                         std::optional<SDL_TouchID> touch_device = std::nullopt)
		{
			validate_axis_parameters(scale, dead_zone, threshold);
			append(action, touch_pressure_binding{touch_device, scale, dead_zone, threshold});
		}

		void bind_pen_axis(std::string_view action, pen_axis_type axis, float scale = 1.0f,
		                   float dead_zone = 0.0f, float threshold = 0.5f,
		                   std::optional<SDL_PenID> pen_device = std::nullopt)
		{
			validate_axis_parameters(scale, dead_zone, threshold);
			[[maybe_unused]] std::size_t index{};
			if (!input::pen_axis_index(axis, index)) [[unlikely]]
				throw std::invalid_argument("invalid pen axis");
			append(action, pen_axis_binding{axis, pen_device, scale, dead_zone, threshold});
		}

		void clear()
		{
			m_storage->actions.clear();
			++m_storage->revision;
		}

		void clear_action(std::string_view action)
		{
			if (m_storage->actions.erase(std::string(action)) != 0)
				++m_storage->revision;
		}

		[[nodiscard]] bool contains(std::string_view action) const
		{
			return m_storage->actions.contains(std::string(action));
		}

		[[nodiscard]] std::size_t binding_count(std::string_view action) const
		{
			const auto it = m_storage->actions.find(std::string(action));
			return it == m_storage->actions.end() ? 0 : it->second.size();
		}
	};
} // namespace mngl
