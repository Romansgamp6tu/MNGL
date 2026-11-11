#pragma once

#include "map.hpp"

namespace mngl
{
	// A per-window, enableable view over an action_map. Contexts do not consume input.
	class input_context
	{
		struct binding_sample
		{
			bool down{};
			bool pressed{};
			bool released{};
			double value{};
			input_release_reason release_reason{input_release_reason::none};
		};

		std::shared_ptr<action_map::storage> m_map;
		SDL_WindowID m_window_id{};
		bool m_enabled{true};
		mutable std::uint64_t m_last_frame{std::numeric_limits<std::uint64_t>::max()};
		mutable std::uint64_t m_last_revision{};
		mutable bool m_baseline{true};
		mutable bool m_force_refresh{true};
		mutable bool m_was_focused{true};
		mutable bool m_wait_for_neutral{};
		mutable std::uint64_t m_last_focus_loss_count{};
		using state_map =
		    std::unordered_map<std::string, action_state, detail::action_name_hash, std::equal_to<>>;
		mutable state_map m_states;

		static bool window_matches(SDL_WindowID binding_window, SDL_WindowID context_window)
		{
			return context_window == 0 || binding_window == 0 || binding_window == context_window;
		}

		static double apply_dead_zone(double value, double dead_zone)
		{
			if (!std::isfinite(value))
				return 0.0;
			const double magnitude = std::abs(value);
			if (magnitude <= dead_zone)
				return 0.0f;
			const double scaled = (magnitude - dead_zone) / (1.0 - dead_zone);
			return std::copysign(scaled, value);
		}

		static float normalize_joystick_axis(std::int16_t value)
		{
			return value < 0 ? value / 32768.0f : value / 32767.0f;
		}

		static void merge_button(binding_sample& result, bool down, bool pressed, bool released,
		                         input_release_reason reason, float scale)
		{
			result.down = result.down || down;
			result.pressed = result.pressed || pressed;
			result.released = result.released || released;
			if (down)
				result.value = scale;
			if (released && reason != input_release_reason::none &&
			    result.release_reason == input_release_reason::none)
				result.release_reason = reason;
		}

		static void merge_axis(binding_sample& result, double value, float threshold)
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
					std::visit(
					    [&](const auto& source)
					    {
						    using T = std::decay_t<decltype(source)>;
						    if constexpr (std::is_same_v<T, action_map::key_binding>)
							    keyboards = true;
						    else if constexpr (std::is_same_v<T, action_map::mouse_button_binding> ||
							                   std::is_same_v<T, action_map::mouse_axis_binding>)
							    mice = true;
						    else if constexpr (std::is_same_v<T, action_map::joystick_button_binding> ||
							                   std::is_same_v<T, action_map::joystick_axis_binding>)
							    joysticks = true;
						    else if constexpr (std::is_same_v<T, action_map::gamepad_button_binding> ||
							                   std::is_same_v<T, action_map::gamepad_axis_binding>)
							    gamepads = true;
						    else if constexpr (std::is_same_v<T, action_map::touch_contact_binding> ||
							                   std::is_same_v<T, action_map::touch_pressure_binding>)
							    touches = true;
						    else if constexpr (std::is_same_v<T, action_map::pen_button_binding> ||
							                   std::is_same_v<T, action_map::pen_axis_binding>)
							    pens = true;
					    },
					    item);
				}
			}
			if (keyboards)
				(void)input::keyboards();
			if (mice)
				(void)input::mice();
			if (joysticks)
				(void)input::joysticks();
			if (gamepads)
				(void)input::gamepads();
			if (touches)
				(void)input::touch_devices();
			if (pens)
				(void)input::pens();
		}

		binding_sample sample_binding(const action_map::binding& binding) const
		{
			binding_sample result;
			std::visit(
			    [&](const auto& source)
			    {
				    using T = std::decay_t<decltype(source)>;
				    if constexpr (std::is_same_v<T, action_map::key_binding>)
				    {
					    std::size_t index{};
					    if (!input::key_index(source.code, index))
						    return;
					    for (const auto& [id, state] : input::keyboard_states)
					    {
						    if ((source.device && *source.device != id) ||
							    (!state.connected && !state.released.test(index)))
							    continue;
						    merge_button(result,
							             state.down.test(index) &&
							                 window_matches(state.key_windows[index], m_window_id),
							             state.pressed.test(index) &&
							                 window_matches(state.pressed_windows[index], m_window_id),
							             state.released.test(index) &&
							                 window_matches(state.released_windows[index], m_window_id),
							             state.release_reasons[index], source.scale);
					    }
				    }
				    else if constexpr (std::is_same_v<T, action_map::mouse_button_binding>)
				    {
					    std::size_t index{};
					    if (!input::mouse_button_index(source.button, index))
						    return;
					    for (const auto& [id, state] : input::mouse_states)
					    {
						    if ((source.device && *source.device != id) ||
							    (!state.connected && !state.released.test(index)))
							    continue;
						    merge_button(result,
							             state.down.test(index) &&
							                 window_matches(state.button_windows[index], m_window_id),
							             state.pressed.test(index) &&
							                 window_matches(state.pressed_windows[index], m_window_id),
							             state.released.test(index) &&
							                 window_matches(state.released_windows[index], m_window_id),
							             state.release_reasons[index], source.scale);
					    }
				    }
				    else if constexpr (std::is_same_v<T, action_map::mouse_axis_binding>)
				    {
					    for (const auto& [id, state] : input::mouse_states)
					    {
						    if ((source.device && *source.device != id) ||
							    !window_matches(state.window_id, m_window_id))
							    continue;
						    if (!state.connected)
						    {
							    result.release_reason = input_release_reason::device_removed;
							    continue;
						    }
						    float raw = 0.0f;
						    switch (source.axis)
						    {
						    case mouse_axis::delta_x:
							    raw = state.delta.x;
							    break;
						    case mouse_axis::delta_y:
							    raw = state.delta.y;
							    break;
						    case mouse_axis::scroll_x:
							    raw = state.scroll_delta.x;
							    break;
						    case mouse_axis::scroll_y:
							    raw = state.scroll_delta.y;
							    break;
						    }
						    const double value = apply_dead_zone(raw, source.dead_zone) * source.scale;
						    merge_axis(result, value, source.threshold);
					    }
				    }
				    else if constexpr (std::is_same_v<T, action_map::joystick_button_binding>)
				    {
					    for (const auto& [id, state] : input::joystick_states)
					    {
						    if ((source.device && *source.device != id) ||
							    source.button >= state.buttons.size())
							    continue;
						    const auto& button = state.buttons[source.button];
						    if (!state.connected && !button.released)
							    continue;
						    merge_button(result, button.down, button.pressed, button.released,
							             button.release_reason, source.scale);
					    }
				    }
				    else if constexpr (std::is_same_v<T, action_map::gamepad_button_binding>)
				    {
					    std::size_t index{};
					    if (!input::gamepad_button_index(source.button, index))
						    return;
					    for (const auto& [id, state] : input::gamepad_states)
					    {
						    if (source.device && *source.device != id)
							    continue;
						    const auto& button = state.buttons[index];
						    if (!state.connected && !button.released)
							    continue;
						    merge_button(result, button.down, button.pressed, button.released,
							             button.release_reason, source.scale);
					    }
				    }
				    else if constexpr (std::is_same_v<T, action_map::touch_contact_binding>)
				    {
					    for (const auto& [id, device] : input::touch_device_states)
					    {
						    if (source.device && *source.device != id)
							    continue;
						    for (const auto& [finger_id, finger] : device.fingers)
						    {
							    (void)finger_id;
							    if (!window_matches(finger.window_id, m_window_id))
								    continue;
							    merge_button(result, finger.down, finger.pressed, finger.released,
								             finger.release_reason, source.scale);
						    }
					    }
				    }
				    else if constexpr (std::is_same_v<T, action_map::pen_button_binding>)
				    {
					    for (const auto& [id, state] : input::pen_states)
					    {
						    if ((source.device && *source.device != id) ||
							    !window_matches(state.window_id, m_window_id))
							    continue;
						    const auto& button = state.buttons[source.button];
						    merge_button(result, button.down, button.pressed, button.released,
							             button.release_reason, source.scale);
					    }
				    }
				    else if constexpr (std::is_same_v<T, action_map::joystick_axis_binding>)
				    {
					    for (const auto& [id, state] : input::joystick_states)
					    {
						    if ((source.device && *source.device != id) || source.axis >= state.axes.size())
							    continue;
						    if (!state.connected)
						    {
							    result.release_reason = input_release_reason::device_removed;
							    continue;
						    }
						    const double value =
						        apply_dead_zone(normalize_joystick_axis(state.axes[source.axis]),
								                source.dead_zone) *
						        source.scale;
						    merge_axis(result, value, source.threshold);
					    }
				    }
				    else if constexpr (std::is_same_v<T, action_map::gamepad_axis_binding>)
				    {
					    std::size_t index{};
					    if (!input::gamepad_axis_index(source.axis, index))
						    return;
					    for (const auto& [id, state] : input::gamepad_states)
					    {
						    if (source.device && *source.device != id)
							    continue;
						    if (!state.connected)
						    {
							    result.release_reason = input_release_reason::device_removed;
							    continue;
						    }
						    float raw = normalize_joystick_axis(state.axes[index]);
						    if (source.axis == gamepad_axis::lefttrigger ||
							    source.axis == gamepad_axis::righttrigger)
							    raw = std::max(0.0f, raw);
						    const double value = apply_dead_zone(raw, source.dead_zone) * source.scale;
						    merge_axis(result, value, source.threshold);
					    }
				    }
				    else if constexpr (std::is_same_v<T, action_map::touch_pressure_binding>)
				    {
					    for (const auto& [id, device] : input::touch_device_states)
					    {
						    if (source.device && *source.device != id)
							    continue;
						    if (!device.connected)
							    result.release_reason = input_release_reason::device_removed;
						    for (const auto& [finger_id, finger] : device.fingers)
						    {
							    (void)finger_id;
							    if (!finger.down || !window_matches(finger.window_id, m_window_id))
								    continue;
							    const double value =
							        apply_dead_zone(finger.pressure, source.dead_zone) * source.scale;
							    merge_axis(result, value, source.threshold);
						    }
					    }
				    }
				    else if constexpr (std::is_same_v<T, action_map::pen_axis_binding>)
				    {
					    std::size_t index{};
					    if (!input::pen_axis_index(source.axis, index))
						    return;
					    for (const auto& [id, state] : input::pen_states)
					    {
						    if ((source.device && *source.device != id) || !state.in_proximity ||
							    !window_matches(state.window_id, m_window_id))
							    continue;
						    const double value =
						        apply_dead_zone(state.axes[index], source.dead_zone) * source.scale;
						    merge_axis(result, value, source.threshold);
					    }
				    }
			    },
			    binding);
			return result;
		}

		void refresh() const
		{
			const std::uint64_t frame = input::input_frame_index.load(std::memory_order_acquire);
			const std::uint64_t revision = m_map->revision;
			if (!m_force_refresh && frame == m_last_frame && revision == m_last_revision)
				return;

			if (revision != m_last_revision)
				prepare_devices();
			const bool focused = m_window_id == 0 || input::window_focused(m_window_id);
			const std::uint64_t focus_loss_count =
			    m_window_id == 0 ? 0 : input::window_focus_loss_count(m_window_id);
			const bool focus_interrupted = m_window_id != 0 && focus_loss_count != m_last_focus_loss_count;
			if (m_window_id != 0 && !focused)
			{
				m_wait_for_neutral = true;
			}
			else if (m_window_id != 0 && !m_was_focused && focused)
			{
				m_wait_for_neutral = true;
			}
			if (focus_interrupted)
				m_wait_for_neutral = true;
			m_was_focused = focused;
			m_last_focus_loss_count = focus_loss_count;
			const input_release_reason inactive_reason =
			    focused ? input_release_reason::none : input::window_inactive_reason(m_window_id);
			bool baseline = m_baseline || revision != m_last_revision;
			const bool waiting_for_neutral = m_wait_for_neutral && focused;
			std::lock_guard lock(input::state_mutex);
			state_map next;
			next.reserve(m_map->actions.size());
			bool any_binding_active = false;
			for (const auto& [name, bindings] : m_map->actions)
			{
				bool current_down = false;
				bool source_pressed = false;
				bool source_released = false;
				double value = 0.0;
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
							value += sample.value;
							if (reason == input_release_reason::none &&
							    sample.release_reason != input_release_reason::none)
								reason = sample.release_reason;
						}
					}
				}
				if (!focused)
					reason = inactive_reason;
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
				state.value = m_enabled ? static_cast<float>(std::clamp(value, -1.0, 1.0)) : 0.0f;
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
					if (state.released)
						state.release_reason = reason;
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
		explicit input_context(const action_map& map, SDL_WindowID window_id = 0)
		    : m_map(map.m_storage), m_window_id(window_id),
		      m_was_focused(window_id == 0 || input::window_focused(window_id)),
		      m_last_focus_loss_count(window_id == 0 ? 0 : input::window_focus_loss_count(window_id))
		{
			refresh(); // Establish a baseline so held inputs do not fake a press.
		}

		void set_enabled(bool enabled)
		{
			if (m_enabled == enabled)
				return;
			m_enabled = enabled;
			m_baseline = enabled;
			m_force_refresh = true;
		}

		[[nodiscard]] bool enabled() const noexcept { return m_enabled; }

		void set_window_id(SDL_WindowID window_id)
		{
			if (m_window_id == window_id)
				return;
			m_window_id = window_id;
			m_was_focused = window_id == 0 || input::window_focused(window_id);
			m_wait_for_neutral = false;
			m_last_focus_loss_count = window_id == 0 ? 0 : input::window_focus_loss_count(window_id);
			m_baseline = true;
			m_force_refresh = true;
		}

		[[nodiscard]] SDL_WindowID window_id() const noexcept { return m_window_id; }

		[[nodiscard]] action_state state(std::string_view action) const
		{
			refresh();
			const auto it = m_states.find(action);
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
} // namespace mngl
