#pragma once

#include "input.hpp"
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
	// Mouse motion and wheel axes are sampled once per Input::continue_work() frame.
	enum class mouse_axis : uint8_t
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
		input_release_reason release_reason{ input_release_reason::none };
	};

	// Named action bindings. Copies share storage so existing InputContexts see edits.
	class ActionMap
	{
		struct key_binding
		{
			scancode code{};
			std::optional<SDL_KeyboardID> device;
			float scale{ 1.0f };
		};
		struct mouse_button_binding
		{
			mouse_button button{};
			std::optional<SDL_MouseID> device;
			float scale{ 1.0f };
		};
		struct mouse_axis_binding
		{
			mouse_axis axis{};
			std::optional<SDL_MouseID> device;
			float scale{ 1.0f };
			float dead_zone{};
			float threshold{ 0.5f };
		};
		struct joystick_button_binding
		{
			std::size_t button{};
			std::optional<SDL_JoystickID> device;
			float scale{ 1.0f };
		};
		struct gamepad_button_binding
		{
			gamepad_button button{};
			std::optional<SDL_JoystickID> device;
			float scale{ 1.0f };
		};
		struct touch_contact_binding
		{
			std::optional<SDL_TouchID> device;
			float scale{ 1.0f };
		};
		struct pen_button_binding
		{
			std::size_t button{}; // 0 is tip contact; 1-5 are pen buttons.
			std::optional<SDL_PenID> device;
			float scale{ 1.0f };
		};
		struct joystick_axis_binding
		{
			std::size_t axis{};
			std::optional<SDL_JoystickID> device;
			float scale{ 1.0f };
			float dead_zone{ 0.15f };
			float threshold{ 0.5f };
		};
		struct gamepad_axis_binding
		{
			gamepad_axis axis{};
			std::optional<SDL_JoystickID> device;
			float scale{ 1.0f };
			float dead_zone{ 0.15f };
			float threshold{ 0.5f };
		};
		struct touch_pressure_binding
		{
			std::optional<SDL_TouchID> device;
			float scale{ 1.0f };
			float dead_zone{};
			float threshold{ 0.5f };
		};
		struct pen_axis_binding
		{
			pen_axis_type axis{};
			std::optional<SDL_PenID> device;
			float scale{ 1.0f };
			float dead_zone{};
			float threshold{ 0.5f };
		};

		using binding = std::variant<
			key_binding,
			mouse_button_binding,
			mouse_axis_binding,
			joystick_button_binding,
			gamepad_button_binding,
			touch_contact_binding,
			pen_button_binding,
			joystick_axis_binding,
			gamepad_axis_binding,
			touch_pressure_binding,
			pen_axis_binding>;

		struct storage
		{
			std::unordered_map<std::string, std::vector<binding>> actions;
			uint64_t revision{ 1 };
		};

		std::shared_ptr<storage> m_storage{ std::make_shared<storage>() };
		friend class InputContext;

		static void validate_action_name(std::string_view name)
		{
			if (name.empty())
				throw std::invalid_argument("action name cannot be empty");
		}

		static void validate_scale(float scale)
		{
			if (!std::isfinite(scale) || scale == 0.0f)
				throw std::invalid_argument("action binding scale must be finite and non-zero");
		}

		static void validate_axis_parameters(float scale, float dead_zone, float threshold)
		{
			validate_scale(scale);
			if (!std::isfinite(dead_zone) || dead_zone < 0.0f || dead_zone >= 1.0f)
				throw std::invalid_argument("axis dead zone must be in [0, 1)");
			if (!std::isfinite(threshold) || threshold <= 0.0f || threshold > 1.0f)
				throw std::invalid_argument("axis action threshold must be in (0, 1]");
		}

		template<typename Binding>
		void append(std::string_view action, Binding value)
		{
			validate_action_name(action);
			m_storage->actions[std::string(action)].emplace_back(std::move(value));
			++m_storage->revision;
		}

	public:
		// Bindings are additive: one action may be triggered by any number of devices.
		// An omitted device ID means any connected device of that type.
		void bind_key(
			std::string_view action, scancode code,
			std::optional<SDL_KeyboardID> keyboard = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			[[maybe_unused]] std::size_t index{};
			if (!Input::key_index(code, index))
				throw std::invalid_argument("invalid keyboard scancode");
			append(action, key_binding{ code, keyboard, scale });
		}

		void bind_mouse_button(
			std::string_view action, mouse_button button,
			std::optional<SDL_MouseID> mouse = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			[[maybe_unused]] std::size_t index{};
			if (!Input::mouse_button_index(button, index))
				throw std::invalid_argument("invalid mouse button");
			append(action, mouse_button_binding{ button, mouse, scale });
		}

		void bind_mouse_axis(
			std::string_view action, mouse_axis axis,
			float scale = 1.0f, float dead_zone = 0.0f, float threshold = 0.5f,
			std::optional<SDL_MouseID> mouse = std::nullopt)
		{
			validate_axis_parameters(scale, dead_zone, threshold);
			if (static_cast<uint8_t>(axis) > static_cast<uint8_t>(mouse_axis::scroll_y))
				throw std::invalid_argument("invalid mouse axis");
			append(action, mouse_axis_binding{ axis, mouse, scale, dead_zone, threshold });
		}

		void bind_joystick_button(
			std::string_view action, std::size_t button,
			std::optional<SDL_JoystickID> joystick = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			append(action, joystick_button_binding{ button, joystick, scale });
		}

		void bind_gamepad_button(
			std::string_view action, gamepad_button button,
			std::optional<SDL_JoystickID> gamepad = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			[[maybe_unused]] std::size_t index{};
			if (!Input::gamepad_button_index(button, index))
				throw std::invalid_argument("invalid gamepad button");
			append(action, gamepad_button_binding{ button, gamepad, scale });
		}

		void bind_touch_contact(
			std::string_view action,
			std::optional<SDL_TouchID> touch_device = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			append(action, touch_contact_binding{ touch_device, scale });
		}

		void bind_pen_tip(
			std::string_view action,
			std::optional<SDL_PenID> pen_device = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			append(action, pen_button_binding{ 0, pen_device, scale });
		}

		void bind_pen_button(
			std::string_view action, pen_button button,
			std::optional<SDL_PenID> pen_device = std::nullopt, float scale = 1.0f)
		{
			validate_scale(scale);
			const auto index = static_cast<std::size_t>(button);
			if (index < 1 || index > 5)
				throw std::invalid_argument("pen button must be button1 through button5");
			append(action, pen_button_binding{ index, pen_device, scale });
		}

		void bind_joystick_axis(
			std::string_view action, std::size_t axis,
			float scale = 1.0f, float dead_zone = 0.15f, float threshold = 0.5f,
			std::optional<SDL_JoystickID> joystick = std::nullopt)
		{
			validate_axis_parameters(scale, dead_zone, threshold);
			append(action, joystick_axis_binding{ axis, joystick, scale, dead_zone, threshold });
		}

		void bind_gamepad_axis(
			std::string_view action, gamepad_axis axis,
			float scale = 1.0f, float dead_zone = 0.15f, float threshold = 0.5f,
			std::optional<SDL_JoystickID> gamepad = std::nullopt)
		{
			validate_axis_parameters(scale, dead_zone, threshold);
			[[maybe_unused]] std::size_t index{};
			if (!Input::gamepad_axis_index(axis, index))
				throw std::invalid_argument("invalid gamepad axis");
			append(action, gamepad_axis_binding{ axis, gamepad, scale, dead_zone, threshold });
		}

		void bind_touch_pressure(
			std::string_view action,
			float scale = 1.0f, float dead_zone = 0.0f, float threshold = 0.5f,
			std::optional<SDL_TouchID> touch_device = std::nullopt)
		{
			validate_axis_parameters(scale, dead_zone, threshold);
			append(action, touch_pressure_binding{ touch_device, scale, dead_zone, threshold });
		}

		void bind_pen_axis(
			std::string_view action, pen_axis_type axis,
			float scale = 1.0f, float dead_zone = 0.0f, float threshold = 0.5f,
			std::optional<SDL_PenID> pen_device = std::nullopt)
		{
			validate_axis_parameters(scale, dead_zone, threshold);
			[[maybe_unused]] std::size_t index{};
			if (!Input::pen_axis_index(axis, index))
				throw std::invalid_argument("invalid pen axis");
			append(action, pen_axis_binding{ axis, pen_device, scale, dead_zone, threshold });
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

	// A per-window, enableable view over an ActionMap. Contexts do not consume input.
	class InputContext
	{
		struct binding_sample
		{
			bool down{};
			bool pressed{};
			bool released{};
			float value{};
			input_release_reason release_reason{ input_release_reason::none };
		};

		std::shared_ptr<ActionMap::storage> m_map;
		SDL_WindowID m_window_id{};
		bool m_enabled{ true };
		mutable uint64_t m_last_frame{ std::numeric_limits<uint64_t>::max() };
		mutable uint64_t m_last_revision{};
		mutable bool m_baseline{ true };
		mutable bool m_force_refresh{ true };
		mutable bool m_was_focused{ true };
		mutable bool m_wait_for_neutral{};
		mutable uint64_t m_last_focus_loss_count{};
		mutable std::unordered_map<std::string, action_state> m_states;

		static bool window_matches(SDL_WindowID binding_window, SDL_WindowID context_window)
		{
			return context_window == 0 || binding_window == 0 || binding_window == context_window;
		}

		static float apply_dead_zone(float value, float dead_zone)
		{
			const float magnitude = std::abs(value);
			if (magnitude <= dead_zone) return 0.0f;
			const float scaled = (magnitude - dead_zone) / (1.0f - dead_zone);
			return std::copysign(scaled, value);
		}

		static float normalize_joystick_axis(int16_t value)
		{
			return value < 0 ? value / 32768.0f : value / 32767.0f;
		}

		static void merge_button(
			binding_sample& result, bool down, bool pressed, bool released,
			input_release_reason reason, float scale)
		{
			result.down = result.down || down;
			result.pressed = result.pressed || pressed;
			result.released = result.released || released;
			if (down) result.value = scale;
			if (released && reason != input_release_reason::none &&
				result.release_reason == input_release_reason::none)
				result.release_reason = reason;
		}

		static void merge_axis(
			binding_sample& result, float value, float threshold)
		{
			if (std::abs(value) > std::abs(result.value))
				result.value = value;
			result.down = result.down || std::abs(value) >= threshold;
		}

		void prepare_devices() const
		{
			bool keyboards = false;
			bool mice = false;
			bool joysticks = false;
			bool gamepads = false;
			bool touches = false;
			bool pens = false;
			for (const auto& [name, bindings] : m_map->actions)
			{
				(void)name;
				for (const auto& item : bindings)
				{
					std::visit([&](const auto& source)
					{
						using T = std::decay_t<decltype(source)>;
						if constexpr (std::is_same_v<T, ActionMap::key_binding>) keyboards = true;
						else if constexpr (std::is_same_v<T, ActionMap::mouse_button_binding> ||
							std::is_same_v<T, ActionMap::mouse_axis_binding>) mice = true;
						else if constexpr (std::is_same_v<T, ActionMap::joystick_button_binding> ||
							std::is_same_v<T, ActionMap::joystick_axis_binding>) joysticks = true;
						else if constexpr (std::is_same_v<T, ActionMap::gamepad_button_binding> ||
							std::is_same_v<T, ActionMap::gamepad_axis_binding>) gamepads = true;
						else if constexpr (std::is_same_v<T, ActionMap::touch_contact_binding> ||
							std::is_same_v<T, ActionMap::touch_pressure_binding>) touches = true;
						else if constexpr (std::is_same_v<T, ActionMap::pen_button_binding> ||
							std::is_same_v<T, ActionMap::pen_axis_binding>) pens = true;
					}, item);
				}
			}
			if (keyboards) (void)Input::keyboards();
			if (mice) (void)Input::mice();
			if (joysticks) (void)Input::joysticks();
			if (gamepads) (void)Input::gamepads();
			if (touches) (void)Input::touch_devices();
			if (pens) (void)Input::pens();
		}

		binding_sample sample_binding(const ActionMap::binding& binding) const
		{
			binding_sample result;
			std::visit([&](const auto& source)
			{
				using T = std::decay_t<decltype(source)>;
				if constexpr (std::is_same_v<T, ActionMap::key_binding>)
				{
					std::size_t index{};
					if (!Input::key_index(source.code, index)) return;
					for (const auto& [id, state] : Input::keyboard_states)
					{
						if ((source.device && *source.device != id) ||
							(!state.connected && !state.released.test(index))) continue;
						merge_button(result, state.down.test(index) && window_matches(state.key_windows[index], m_window_id),
							state.pressed.test(index) && window_matches(state.pressed_windows[index], m_window_id),
							state.released.test(index) && window_matches(state.released_windows[index], m_window_id),
							state.release_reasons[index], source.scale);
					}
				}
				else if constexpr (std::is_same_v<T, ActionMap::mouse_button_binding>)
				{
					std::size_t index{};
					if (!Input::mouse_button_index(source.button, index)) return;
					for (const auto& [id, state] : Input::mouse_states)
					{
						if ((source.device && *source.device != id) ||
							(!state.connected && !state.released.test(index))) continue;
						merge_button(result, state.down.test(index) && window_matches(state.button_windows[index], m_window_id),
							state.pressed.test(index) && window_matches(state.pressed_windows[index], m_window_id),
							state.released.test(index) && window_matches(state.released_windows[index], m_window_id),
							state.release_reasons[index], source.scale);
					}
				}
				else if constexpr (std::is_same_v<T, ActionMap::mouse_axis_binding>)
				{
					for (const auto& [id, state] : Input::mouse_states)
					{
						if ((source.device && *source.device != id) ||
							!window_matches(state.window_id, m_window_id)) continue;
						if (!state.connected)
						{
							result.release_reason = input_release_reason::device_removed;
							continue;
						}
						float raw = 0.0f;
						switch (source.axis)
						{
						case mouse_axis::delta_x: raw = state.delta.x; break;
						case mouse_axis::delta_y: raw = state.delta.y; break;
						case mouse_axis::scroll_x: raw = state.scroll_delta.x; break;
						case mouse_axis::scroll_y: raw = state.scroll_delta.y; break;
						}
						const float value = apply_dead_zone(raw, source.dead_zone) * source.scale;
						merge_axis(result, value, source.threshold);
					}
				}
				else if constexpr (std::is_same_v<T, ActionMap::joystick_button_binding>)
				{
					for (const auto& [id, state] : Input::joystick_states)
					{
						if ((source.device && *source.device != id) ||
							source.button >= state.buttons.size()) continue;
						const auto& button = state.buttons[source.button];
						if (!state.connected && !button.released) continue;
						merge_button(result, button.down, button.pressed, button.released,
							button.release_reason, source.scale);
					}
				}
				else if constexpr (std::is_same_v<T, ActionMap::gamepad_button_binding>)
				{
					std::size_t index{};
					if (!Input::gamepad_button_index(source.button, index)) return;
					for (const auto& [id, state] : Input::gamepad_states)
					{
						if (source.device && *source.device != id) continue;
						const auto& button = state.buttons[index];
						if (!state.connected && !button.released) continue;
						merge_button(result, button.down, button.pressed, button.released,
							button.release_reason, source.scale);
					}
				}
				else if constexpr (std::is_same_v<T, ActionMap::touch_contact_binding>)
				{
					for (const auto& [id, device] : Input::touch_device_states)
					{
						if (source.device && *source.device != id) continue;
						for (const auto& [finger_id, finger] : device.fingers)
						{
							(void)finger_id;
							if (!window_matches(finger.window_id, m_window_id)) continue;
							merge_button(result, finger.down, finger.pressed, finger.released,
								finger.release_reason, source.scale);
						}
					}
				}
				else if constexpr (std::is_same_v<T, ActionMap::pen_button_binding>)
				{
					for (const auto& [id, state] : Input::pen_states)
					{
						if ((source.device && *source.device != id) ||
							!window_matches(state.window_id, m_window_id)) continue;
						const auto& button = state.buttons[source.button];
						merge_button(result, button.down, button.pressed, button.released,
							button.release_reason, source.scale);
					}
				}
				else if constexpr (std::is_same_v<T, ActionMap::joystick_axis_binding>)
				{
					for (const auto& [id, state] : Input::joystick_states)
					{
						if ((source.device && *source.device != id) ||
							source.axis >= state.axes.size()) continue;
						if (!state.connected)
						{
							result.release_reason = input_release_reason::device_removed;
							continue;
						}
						const float value = apply_dead_zone(
							normalize_joystick_axis(state.axes[source.axis]), source.dead_zone) * source.scale;
						merge_axis(result, value, source.threshold);
					}
				}
				else if constexpr (std::is_same_v<T, ActionMap::gamepad_axis_binding>)
				{
					std::size_t index{};
					if (!Input::gamepad_axis_index(source.axis, index)) return;
					for (const auto& [id, state] : Input::gamepad_states)
					{
						if (source.device && *source.device != id) continue;
						if (!state.connected)
						{
							result.release_reason = input_release_reason::device_removed;
							continue;
						}
						float raw = normalize_joystick_axis(state.axes[index]);
						if (source.axis == gamepad_axis::lefttrigger || source.axis == gamepad_axis::righttrigger)
							raw = std::max(0.0f, raw);
						const float value = apply_dead_zone(raw, source.dead_zone) * source.scale;
						merge_axis(result, value, source.threshold);
					}
				}
				else if constexpr (std::is_same_v<T, ActionMap::touch_pressure_binding>)
				{
					for (const auto& [id, device] : Input::touch_device_states)
					{
						if (source.device && *source.device != id) continue;
						if (!device.connected)
							result.release_reason = input_release_reason::device_removed;
						for (const auto& [finger_id, finger] : device.fingers)
						{
							(void)finger_id;
							if (!finger.down || !window_matches(finger.window_id, m_window_id)) continue;
							const float value = apply_dead_zone(finger.pressure, source.dead_zone) * source.scale;
							merge_axis(result, value, source.threshold);
						}
					}
				}
				else if constexpr (std::is_same_v<T, ActionMap::pen_axis_binding>)
				{
					std::size_t index{};
					if (!Input::pen_axis_index(source.axis, index)) return;
					for (const auto& [id, state] : Input::pen_states)
					{
						if ((source.device && *source.device != id) || !state.in_proximity ||
							!window_matches(state.window_id, m_window_id)) continue;
						const float value = apply_dead_zone(state.axes[index], source.dead_zone) * source.scale;
						merge_axis(result, value, source.threshold);
					}
				}
			}, binding);
			return result;
		}

		void refresh() const
		{
			const uint64_t frame = Input::input_frame_index.load(std::memory_order_acquire);
			const uint64_t revision = m_map->revision;
			if (!m_force_refresh && frame == m_last_frame && revision == m_last_revision) return;

			if (revision != m_last_revision) prepare_devices();
			const bool focused = m_window_id == 0 || Input::window_focused(m_window_id);
			const uint64_t focus_loss_count = m_window_id == 0
				? 0 : Input::window_focus_loss_count(m_window_id);
			const bool focus_interrupted = m_window_id != 0 &&
				focus_loss_count != m_last_focus_loss_count;
			if (m_window_id != 0 && !focused)
			{
				m_wait_for_neutral = true;
			}
			else if (m_window_id != 0 && !m_was_focused && focused)
			{
				m_wait_for_neutral = true;
			}
			if (focus_interrupted) m_wait_for_neutral = true;
			m_was_focused = focused;
			m_last_focus_loss_count = focus_loss_count;
			const input_release_reason inactive_reason = focused
				? input_release_reason::none : Input::window_inactive_reason(m_window_id);
			bool baseline = m_baseline || revision != m_last_revision;
			const bool waiting_for_neutral = m_wait_for_neutral && focused;
			std::lock_guard lock(Input::state_mutex);
			std::unordered_map<std::string, action_state> next;
			next.reserve(m_map->actions.size());
			bool any_binding_active = false;
			for (const auto& [name, bindings] : m_map->actions)
			{
				bool current_down = false;
				bool source_pressed = false;
				bool source_released = false;
				float value = 0.0f;
				input_release_reason reason = input_release_reason::none;
				if (focused)
				{
					for (const auto& item : bindings)
					{
						const binding_sample sample = sample_binding(item);
						if (sample.down || sample.pressed || sample.released ||
							std::abs(sample.value) > 0.0001f)
							any_binding_active = true;
						if (m_enabled)
						{
							current_down = current_down || sample.down;
							source_pressed = source_pressed || sample.pressed;
							source_released = source_released || sample.released;
							value = std::clamp(value + sample.value, -1.0f, 1.0f);
							if (reason == input_release_reason::none &&
								sample.release_reason != input_release_reason::none)
								reason = sample.release_reason;
						}
					}
				}
				if (!focused) reason = inactive_reason;
				if (waiting_for_neutral)
				{
					current_down = false;
					source_pressed = false;
					source_released = false;
					value = 0.0f;
				}

				const auto previous = m_states.find(name);
				const bool previous_down = previous != m_states.end() && previous->second.down;
				action_state state;
				state.down = current_down;
				state.value = m_enabled ? value : 0.0f;
				if (!baseline)
				{
					const bool same_frame_press_and_release =
						!previous_down && !current_down && source_pressed && source_released;
					state.pressed = (!previous_down && current_down) || same_frame_press_and_release;
					state.released = (previous_down && !current_down) || same_frame_press_and_release;
					if (!m_enabled && previous_down)
						reason = input_release_reason::canceled;
					if (!focused && previous_down)
						reason = inactive_reason;
					else if (focus_interrupted && previous_down)
						reason = input_release_reason::focus_lost;
					if (state.released) state.release_reason = reason;
				}
				next.emplace(name, state);
			}
			if (waiting_for_neutral && !any_binding_active)
			{
				m_wait_for_neutral = false;
				baseline = true;
			}

			m_states = std::move(next);
			m_last_frame = frame;
			m_last_revision = revision;
			m_baseline = false;
			m_force_refresh = false;
		}

	public:
		// window_id == 0 routes window-bound devices from any window. Joysticks and
		// gamepads are application-wide and are visible from every context.
		explicit InputContext(const ActionMap& map, SDL_WindowID window_id = 0)
			: m_map(map.m_storage), m_window_id(window_id),
				m_was_focused(window_id == 0 || Input::window_focused(window_id)),
				m_last_focus_loss_count(window_id == 0 ? 0 : Input::window_focus_loss_count(window_id))
		{
			refresh(); // Establish a baseline so held inputs do not fake a press.
		}

		void set_enabled(bool enabled)
		{
			if (m_enabled == enabled) return;
			m_enabled = enabled;
			m_baseline = enabled;
			m_force_refresh = true;
		}

		[[nodiscard]] bool enabled() const noexcept { return m_enabled; }

		void set_window_id(SDL_WindowID window_id)
		{
			if (m_window_id == window_id) return;
			m_window_id = window_id;
			m_was_focused = window_id == 0 || Input::window_focused(window_id);
			m_wait_for_neutral = false;
			m_last_focus_loss_count = window_id == 0
				? 0 : Input::window_focus_loss_count(window_id);
			m_baseline = true;
			m_force_refresh = true;
		}

		[[nodiscard]] SDL_WindowID window_id() const noexcept { return m_window_id; }

		[[nodiscard]] action_state state(std::string_view action) const
		{
			refresh();
			const auto it = m_states.find(std::string(action));
			return it == m_states.end() ? action_state{} : it->second;
		}

		[[nodiscard]] bool pressed(std::string_view action) const { return state(action).pressed; }
		[[nodiscard]] bool down(std::string_view action) const { return state(action).down; }
		[[nodiscard]] bool released(std::string_view action) const { return state(action).released; }
		[[nodiscard]] float value(std::string_view action) const { return state(action).value; }
		[[nodiscard]] input_release_reason release_reason(std::string_view action) const
		{
			return state(action).release_reason;
		}
	};

	namespace action_presets
	{
		struct platformer_action_names
		{
			std::string_view move_left{ "move_left" };
			std::string_view move_right{ "move_right" };
			std::string_view jump{ "jump" };
			std::string_view pause{ "pause" };
		};

		inline void platformer(ActionMap& map, platformer_action_names names = {})
		{
			auto bind_key = [&](std::string_view action, scancode key)
			{
				if (!action.empty()) map.bind_key(action, key);
			};
			auto bind_pad = [&](std::string_view action, gamepad_button button)
			{
				if (!action.empty()) map.bind_gamepad_button(action, button);
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
			std::string_view confirm{ "confirm" };
			std::string_view cancel{ "cancel" };
			std::string_view up{ "up" };
			std::string_view down{ "down" };
			std::string_view left{ "left" };
			std::string_view right{ "right" };
		};

		inline void menu(ActionMap& map, menu_action_names names = {})
		{
			auto bind_key = [&](std::string_view action, scancode key)
			{
				if (!action.empty()) map.bind_key(action, key);
			};
			auto bind_pad = [&](std::string_view action, gamepad_button button)
			{
				if (!action.empty()) map.bind_gamepad_button(action, button);
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
	}
}
