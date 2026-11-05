	inline bool touch_finger::pressed() const
	{
		return input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).pressed;
	}

	inline bool touch_finger::down() const
	{
		return input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).down;
	}

	inline bool touch_finger::released() const
	{
		return input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).released;
	}

	inline input_release_reason touch_finger::release_reason() const
	{
		return input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).release_reason;
	}

	inline glm::vec2 touch_finger::position() const
	{
		return input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).position;
	}

	inline glm::vec2 touch_finger::position_in_window(glm::vec2 logical_size) const
	{
		const auto value = position();
		return {value.x * logical_size.x, value.y * logical_size.y};
	}

	inline glm::vec2 touch_finger::delta() const
	{
		return input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).delta;
	}

	inline float touch_finger::pressure() const
	{
		return input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).pressure;
	}

	inline SDL_WindowID touch_finger::window_id() const
	{
		return input::touch_finger_snapshot(m_touch_id, m_device_generation, m_finger_id).window_id;
	}

	inline std::string touch_device::name() const
	{
		return input::touch_device_name(m_id, m_generation);
	}

	inline SDL_TouchDeviceType touch_device::type() const
	{
		return input::touch_device_type(m_id, m_generation);
	}

	inline bool touch_device::connected() const
	{
		return input::touch_device_connected(m_id, m_generation);
	}

	inline std::vector<touch_finger> touch_device::fingers() const
	{
		const auto ids = input::touch_finger_ids(m_id, m_generation);
		std::vector<touch_finger> result;
		result.reserve(ids.size());
		for (const SDL_FingerID id : ids)
			result.push_back(touch_finger(m_id, m_generation, id));
		return result;
	}

	inline std::optional<touch_finger> touch_device::finger(SDL_FingerID id) const
	{
		if (!input::touch_finger_exists(m_id, m_generation, id))
			return std::nullopt;
		return touch_finger(m_id, m_generation, id);
	}

	inline SDL_PenDeviceType pen::type() const
	{
		return input::pen_snapshot(m_id, m_generation).type;
	}

	inline bool pen::in_proximity() const
	{
		return input::pen_snapshot(m_id, m_generation).in_proximity;
	}

	inline bool pen::has_position() const
	{
		return input::pen_snapshot(m_id, m_generation).has_position;
	}

	inline glm::vec2 pen::position() const
	{
		return input::pen_snapshot(m_id, m_generation).position;
	}

	inline glm::vec2 pen::delta() const
	{
		return input::pen_snapshot(m_id, m_generation).delta;
	}

	inline SDL_WindowID pen::window_id() const
	{
		return input::pen_snapshot(m_id, m_generation).window_id;
	}

	inline bool pen::is_eraser() const
	{
		return input::pen_snapshot(m_id, m_generation).eraser;
	}

	inline bool pen::pressed() const
	{
		return input::pen_snapshot(m_id, m_generation).buttons[0].pressed;
	}

	inline bool pen::down() const
	{
		return input::pen_snapshot(m_id, m_generation).buttons[0].down;
	}

	inline bool pen::released() const
	{
		return input::pen_snapshot(m_id, m_generation).buttons[0].released;
	}

	inline input_release_reason pen::release_reason() const
	{
		return input::pen_snapshot(m_id, m_generation).buttons[0].release_reason;
	}

	inline bool pen::button_pressed(pen_button button) const
	{
		return input::pen_button_state_value(m_id, m_generation, button, 0);
	}

	inline bool pen::button_down(pen_button button) const
	{
		return input::pen_button_state_value(m_id, m_generation, button, 1);
	}

	inline bool pen::button_released(pen_button button) const
	{
		return input::pen_button_state_value(m_id, m_generation, button, 2);
	}

	inline input_release_reason pen::button_release_reason(pen_button button) const
	{
		return input::pen_button_release_reason(m_id, m_generation, button);
	}

	inline float pen::axis(pen_axis_type axis) const
	{
		return input::pen_axis_value(m_id, m_generation, axis);
	}

	inline float pen::pressure() const
	{
		return axis(pen_axis_type::pressure);
	}

	inline glm::vec2 pen::tilt() const
	{
		return {axis(pen_axis_type::xtilt), axis(pen_axis_type::ytilt)};
	}
