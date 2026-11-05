	inline std::string keyboard::name() const
	{
		return input::keyboard_name(m_id);
	}

	inline bool keyboard::connected() const
	{
		return input::keyboard_connected(m_id, m_generation);
	}

	inline bool keyboard::pressed(scancode code) const
	{
		return input::keyboard_state(m_id, m_generation, code, 0);
	}

	inline bool keyboard::down(scancode code) const
	{
		return input::keyboard_state(m_id, m_generation, code, 1);
	}

	inline bool keyboard::released(scancode code) const
	{
		return input::keyboard_state(m_id, m_generation, code, 2);
	}

	inline input_release_reason keyboard::release_reason(scancode code) const
	{
		return input::keyboard_release_reason(m_id, m_generation, code);
	}

	inline std::string mouse::name() const
	{
		return input::mouse_name(m_id);
	}

	inline bool mouse::connected() const
	{
		return input::mouse_connected(m_id, m_generation);
	}

	inline bool mouse::pressed(mouse_button button) const
	{
		return input::mouse_state(m_id, m_generation, button, 0);
	}

	inline bool mouse::down(mouse_button button) const
	{
		return input::mouse_state(m_id, m_generation, button, 1);
	}

	inline bool mouse::released(mouse_button button) const
	{
		return input::mouse_state(m_id, m_generation, button, 2);
	}

	inline input_release_reason mouse::release_reason(mouse_button button) const
	{
		return input::mouse_release_reason(m_id, m_generation, button);
	}

	inline bool mouse::has_position() const
	{
		return input::mouse_has_position(m_id, m_generation);
	}

	inline glm::vec2 mouse::position() const
	{
		return input::mouse_vector(m_id, m_generation, 0);
	}

	inline SDL_WindowID mouse::window_id() const
	{
		return input::mouse_window_id(m_id, m_generation);
	}

	inline glm::vec2 mouse::delta() const
	{
		return input::mouse_vector(m_id, m_generation, 1);
	}

	inline glm::vec2 mouse::scroll_delta() const
	{
		return input::mouse_vector(m_id, m_generation, 2);
	}
