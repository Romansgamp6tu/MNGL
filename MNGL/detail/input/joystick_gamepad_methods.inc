	inline std::string joystick::name() const
	{
		return input::joystick_name(m_id, m_generation);
	}

	inline bool joystick::connected() const
	{
		return input::joystick_connected(m_id, m_generation);
	}

	inline SDL_JoystickType joystick::type() const
	{
		return input::joystick_type(m_id, m_generation);
	}

	inline std::size_t joystick::button_count() const
	{
		return input::joystick_control_count(m_id, m_generation, 0);
	}

	inline std::size_t joystick::axis_count() const
	{
		return input::joystick_control_count(m_id, m_generation, 1);
	}

	inline std::size_t joystick::hat_count() const
	{
		return input::joystick_control_count(m_id, m_generation, 2);
	}

	inline std::size_t joystick::ball_count() const
	{
		return input::joystick_control_count(m_id, m_generation, 3);
	}

	inline bool joystick::pressed(std::size_t button) const
	{
		return input::joystick_button_state(m_id, m_generation, button, 0);
	}

	inline bool joystick::down(std::size_t button) const
	{
		return input::joystick_button_state(m_id, m_generation, button, 1);
	}

	inline bool joystick::released(std::size_t button) const
	{
		return input::joystick_button_state(m_id, m_generation, button, 2);
	}

	inline input_release_reason joystick::release_reason(std::size_t button) const
	{
		return input::joystick_release_reason(m_id, m_generation, button);
	}

	inline std::int16_t joystick::axis(std::size_t axis_index) const
	{
		return input::joystick_axis(m_id, m_generation, axis_index);
	}

	inline float joystick::normalized_axis(std::size_t axis_index) const
	{
		const std::int16_t value = axis(axis_index);
		return value < 0 ? value / 32768.0f : value / 32767.0f;
	}

	inline hat_position joystick::hat(std::size_t hat_index) const
	{
		return input::joystick_hat(m_id, m_generation, hat_index);
	}

	inline glm::ivec2 joystick::ball_delta(std::size_t ball_index) const
	{
		return input::joystick_ball_delta(m_id, m_generation, ball_index);
	}

	inline std::string gamepad::name() const
	{
		return input::query_gamepad<std::string>(m_id, m_generation,
		                                         [](SDL_Gamepad* value)
		                                         {
			                                         const char* name = SDL_GetGamepadName(value);
			                                         return name ? std::string(name) : std::string{};
		                                         });
	}

	inline std::string gamepad::path() const
	{
		return input::query_gamepad<std::string>(m_id, m_generation,
		                                         [](SDL_Gamepad* value)
		                                         {
			                                         const char* path = SDL_GetGamepadPath(value);
			                                         return path ? std::string(path) : std::string{};
		                                         });
	}

	inline SDL_GamepadType gamepad::type() const
	{
		return input::query_gamepad<SDL_GamepadType>(
		    m_id, m_generation, [](SDL_Gamepad* value) { return SDL_GetGamepadType(value); },
		    SDL_GAMEPAD_TYPE_UNKNOWN);
	}

	inline SDL_GamepadType gamepad::real_type() const
	{
		return input::query_gamepad<SDL_GamepadType>(
		    m_id, m_generation, [](SDL_Gamepad* value) { return SDL_GetRealGamepadType(value); },
		    SDL_GAMEPAD_TYPE_UNKNOWN);
	}

	inline SDL_GUID gamepad::guid() const
	{
		return input::query_gamepad<SDL_GUID>(
		    m_id, m_generation, [](SDL_Gamepad* value)
		    { return SDL_GetJoystickGUID(SDL_GetGamepadJoystick(value)); }, SDL_GUID{});
	}

	inline bool gamepad::connected() const
	{
		return input::gamepad_connected(m_id, m_generation);
	}

	inline SDL_JoystickConnectionState gamepad::connection_state() const
	{
		return input::query_gamepad<SDL_JoystickConnectionState>(
		    m_id, m_generation, [](SDL_Gamepad* value) { return SDL_GetGamepadConnectionState(value); },
		    SDL_JOYSTICK_CONNECTION_INVALID);
	}

	inline int gamepad::player_index() const
	{
		return input::query_gamepad<int>(
		    m_id, m_generation, [](SDL_Gamepad* value) { return SDL_GetGamepadPlayerIndex(value); }, -1);
	}

	inline bool gamepad::set_player_index(int player_index) const
	{
		return input::query_gamepad<bool>(
		    m_id, m_generation, [player_index](SDL_Gamepad* value)
		    { return SDL_SetGamepadPlayerIndex(value, player_index); }, false);
	}

	inline std::uint16_t gamepad::vendor() const
	{
		return input::query_gamepad<std::uint16_t>(
		    m_id, m_generation, [](SDL_Gamepad* value) { return SDL_GetGamepadVendor(value); }, 0);
	}

	inline std::uint16_t gamepad::product() const
	{
		return input::query_gamepad<std::uint16_t>(
		    m_id, m_generation, [](SDL_Gamepad* value) { return SDL_GetGamepadProduct(value); }, 0);
	}

	inline std::uint16_t gamepad::product_version() const
	{
		return input::query_gamepad<std::uint16_t>(
		    m_id, m_generation, [](SDL_Gamepad* value) { return SDL_GetGamepadProductVersion(value); }, 0);
	}

	inline std::uint16_t gamepad::firmware_version() const
	{
		return input::query_gamepad<std::uint16_t>(
		    m_id, m_generation, [](SDL_Gamepad* value) { return SDL_GetGamepadFirmwareVersion(value); }, 0);
	}

	inline std::string gamepad::serial() const
	{
		return input::query_gamepad<std::string>(m_id, m_generation,
		                                         [](SDL_Gamepad* value)
		                                         {
			                                         const char* serial = SDL_GetGamepadSerial(value);
			                                         return serial ? std::string(serial) : std::string{};
		                                         });
	}

	inline std::uint64_t gamepad::steam_handle() const
	{
		return input::query_gamepad<std::uint64_t>(
		    m_id, m_generation, [](SDL_Gamepad* value) { return SDL_GetGamepadSteamHandle(value); }, 0);
	}

	inline gamepad_battery_info gamepad::battery_info() const
	{
		return input::query_gamepad<gamepad_battery_info>(m_id, m_generation,
		                                                  [](SDL_Gamepad* value)
		                                                  {
			                                                  int percent = -1;
			                                                  const SDL_PowerState state =
			                                                      SDL_GetGamepadPowerInfo(value, &percent);
			                                                  power_state converted = power_state::error;
			                                                  switch (state)
			                                                  {
			                                                  case SDL_POWERSTATE_UNKNOWN:
				                                                  converted = power_state::unknown;
				                                                  break;
			                                                  case SDL_POWERSTATE_ON_BATTERY:
				                                                  converted = power_state::on_battery;
				                                                  break;
			                                                  case SDL_POWERSTATE_NO_BATTERY:
				                                                  converted = power_state::no_battery;
				                                                  break;
			                                                  case SDL_POWERSTATE_CHARGING:
				                                                  converted = power_state::charging;
				                                                  break;
			                                                  case SDL_POWERSTATE_CHARGED:
				                                                  converted = power_state::charged;
				                                                  break;
			                                                  case SDL_POWERSTATE_ERROR:
				                                                  converted = power_state::error;
				                                                  break;
			                                                  }
			                                                  return gamepad_battery_info{converted, percent};
		                                                  });
	}

	inline bool gamepad::has_button(gamepad_button button) const
	{
		std::size_t index{};
		if (!input::gamepad_button_index(button, index))
			return false;
		return input::query_gamepad<bool>(
		    m_id, m_generation, [button](SDL_Gamepad* value)
		    { return SDL_GamepadHasButton(value, static_cast<SDL_GamepadButton>(button)); }, false);
	}

	inline bool gamepad::pressed(gamepad_button button) const
	{
		std::size_t index{};
		return input::gamepad_button_index(button, index) &&
		       input::gamepad_button_state(m_id, m_generation, index, 0);
	}

	inline bool gamepad::down(gamepad_button button) const
	{
		std::size_t index{};
		return input::gamepad_button_index(button, index) &&
		       input::gamepad_button_state(m_id, m_generation, index, 1);
	}

	inline bool gamepad::released(gamepad_button button) const
	{
		std::size_t index{};
		return input::gamepad_button_index(button, index) &&
		       input::gamepad_button_state(m_id, m_generation, index, 2);
	}

	inline input_release_reason gamepad::release_reason(gamepad_button button) const
	{
		std::size_t index{};
		return input::gamepad_button_index(button, index)
		           ? input::gamepad_release_reason(m_id, m_generation, index)
				   : input_release_reason::none;
	}

	inline SDL_GamepadButtonLabel gamepad::button_label(gamepad_button button) const
	{
		std::size_t index{};
		if (!input::gamepad_button_index(button, index))
			return SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
		return input::query_gamepad<SDL_GamepadButtonLabel>(
		    m_id, m_generation, [button](SDL_Gamepad* value)
		    { return SDL_GetGamepadButtonLabel(value, static_cast<SDL_GamepadButton>(button)); },
		    SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN);
	}

	inline bool gamepad::has_axis(gamepad_axis axis) const
	{
		std::size_t index{};
		if (!input::gamepad_axis_index(axis, index))
			return false;
		return input::query_gamepad<bool>(
		    m_id, m_generation, [axis](SDL_Gamepad* value)
		    { return SDL_GamepadHasAxis(value, static_cast<SDL_GamepadAxis>(axis)); }, false);
	}

	inline std::int16_t gamepad::axis(gamepad_axis axis) const
	{
		std::size_t index{};
		return input::gamepad_axis_index(axis, index) ? input::gamepad_axis_value(m_id, m_generation, index)
		                                              : 0;
	}

	inline float gamepad::normalized_axis(gamepad_axis axis) const
	{
		const std::int16_t value = this->axis(axis);
		if (axis == gamepad_axis::lefttrigger || axis == gamepad_axis::righttrigger)
			return std::max(0.0f, value / 32767.0f);
		return value < 0 ? value / 32768.0f : value / 32767.0f;
	}

	inline std::string gamepad::mapping() const
	{
		return input::query_gamepad<std::string>(
		    m_id, m_generation,
		    [](SDL_Gamepad* value)
		    {
			    std::unique_ptr<char, decltype(&SDL_free)> mapping(SDL_GetGamepadMapping(value), &SDL_free);
			    return mapping ? std::string(mapping.get()) : std::string{};
		    });
	}

	inline bool gamepad::set_mapping(std::string_view mapping) const
	{
		const std::string value(mapping);
		return input::query_gamepad<bool>(
		    m_id, m_generation, [&value](SDL_Gamepad* device)
		    { return SDL_SetGamepadMapping(SDL_GetGamepadID(device), value.c_str()); }, false);
	}

	inline std::vector<SDL_GamepadBinding> gamepad::bindings() const
	{
		return input::query_gamepad<std::vector<SDL_GamepadBinding>>(
		    m_id, m_generation,
		    [](SDL_Gamepad* value)
		    {
			    int count = 0;
			    auto** raw_bindings = SDL_GetGamepadBindings(value, &count);
			    std::unique_ptr<SDL_GamepadBinding*, decltype(&SDL_free)> bindings(raw_bindings, &SDL_free);
			    std::vector<SDL_GamepadBinding> result;
			    if (!bindings || count <= 0)
				    return result;
			    result.reserve(static_cast<std::size_t>(count));
			    for (int i = 0; i < count; ++i)
				    result.push_back(*bindings.get()[i]);
			    return result;
		    });
	}

	inline int gamepad::touchpad_count() const
	{
		return input::query_gamepad<int>(
		    m_id, m_generation, [](SDL_Gamepad* value) { return SDL_GetNumGamepadTouchpads(value); }, 0);
	}

	inline int gamepad::touchpad_finger_count(int touchpad) const
	{
		return input::query_gamepad<int>(
		    m_id, m_generation,
		    [touchpad](SDL_Gamepad* value) { return SDL_GetNumGamepadTouchpadFingers(value, touchpad); }, -1);
	}

	inline std::optional<gamepad_touchpad_finger> gamepad::touchpad_finger(int touchpad, int finger) const
	{
		return input::query_gamepad<std::optional<gamepad_touchpad_finger>>(
		    m_id, m_generation,
		    [touchpad, finger](SDL_Gamepad* value) -> std::optional<gamepad_touchpad_finger>
		    {
			    bool down = false;
			    float x = 0.0f, y = 0.0f, pressure = 0.0f;
			    if (!SDL_GetGamepadTouchpadFinger(value, touchpad, finger, &down, &x, &y, &pressure))
				    return std::nullopt;
			    return gamepad_touchpad_finger{down, {x, y}, pressure};
		    },
		    std::nullopt);
	}

	inline bool gamepad::has_sensor(sensor_type sensor) const
	{
		if (!input::gamepad_sensor_valid(sensor))
			return false;
		return input::query_gamepad<bool>(
		    m_id, m_generation, [sensor](SDL_Gamepad* value)
		    { return SDL_GamepadHasSensor(value, static_cast<SDL_SensorType>(sensor)); }, false);
	}

	inline bool gamepad::set_sensor_enabled(sensor_type sensor, bool enabled) const
	{
		if (!input::gamepad_sensor_valid(sensor))
			return false;
		return input::query_gamepad<bool>(
		    m_id, m_generation, [sensor, enabled](SDL_Gamepad* value)
		    { return SDL_SetGamepadSensorEnabled(value, static_cast<SDL_SensorType>(sensor), enabled); },
		    false);
	}

	inline bool gamepad::sensor_enabled(sensor_type sensor) const
	{
		if (!input::gamepad_sensor_valid(sensor))
			return false;
		return input::query_gamepad<bool>(
		    m_id, m_generation, [sensor](SDL_Gamepad* value)
		    { return SDL_GamepadSensorEnabled(value, static_cast<SDL_SensorType>(sensor)); }, false);
	}

	inline float gamepad::sensor_data_rate(sensor_type sensor) const
	{
		if (!input::gamepad_sensor_valid(sensor))
			return 0.0f;
		return input::query_gamepad<float>(
		    m_id, m_generation, [sensor](SDL_Gamepad* value)
		    { return SDL_GetGamepadSensorDataRate(value, static_cast<SDL_SensorType>(sensor)); }, 0.0f);
	}

	inline std::optional<std::vector<float>> gamepad::sensor_data(sensor_type sensor,
	                                                              std::size_t value_count) const
	{
		if (!input::gamepad_sensor_valid(sensor) || value_count == 0 ||
		    value_count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return std::nullopt;
		return input::query_gamepad<std::optional<std::vector<float>>>(
		    m_id, m_generation,
		    [sensor, value_count](SDL_Gamepad* value) -> std::optional<std::vector<float>>
		    {
			    std::vector<float> data(value_count);
			    if (!SDL_GetGamepadSensorData(value, static_cast<SDL_SensorType>(sensor), data.data(),
				                              static_cast<int>(data.size())))
				    return std::nullopt;
			    return data;
		    },
		    std::nullopt);
	}

	inline bool gamepad::has_rumble() const
	{
		return input::query_gamepad<bool>(
		    m_id, m_generation,
		    [](SDL_Gamepad* value)
		    {
			    return SDL_GetBooleanProperty(SDL_GetGamepadProperties(value),
				                              SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
		    },
		    false);
	}

	inline bool gamepad::has_trigger_rumble() const
	{
		return input::query_gamepad<bool>(
		    m_id, m_generation,
		    [](SDL_Gamepad* value)
		    {
			    return SDL_GetBooleanProperty(SDL_GetGamepadProperties(value),
				                              SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false);
		    },
		    false);
	}

	inline bool gamepad::has_rgb_led() const
	{
		return input::query_gamepad<bool>(
		    m_id, m_generation,
		    [](SDL_Gamepad* value)
		    {
			    return SDL_GetBooleanProperty(SDL_GetGamepadProperties(value),
				                              SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);
		    },
		    false);
	}

	inline bool gamepad::has_player_led() const
	{
		return input::query_gamepad<bool>(
		    m_id, m_generation,
		    [](SDL_Gamepad* value)
		    {
			    return SDL_GetBooleanProperty(SDL_GetGamepadProperties(value),
				                              SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN, false);
		    },
		    false);
	}

	inline bool gamepad::has_mono_led() const
	{
		return input::query_gamepad<bool>(
		    m_id, m_generation,
		    [](SDL_Gamepad* value)
		    {
			    return SDL_GetBooleanProperty(SDL_GetGamepadProperties(value),
				                              SDL_PROP_GAMEPAD_CAP_MONO_LED_BOOLEAN, false);
		    },
		    false);
	}

	inline bool gamepad::rumble(std::uint16_t low_frequency, std::uint16_t high_frequency,
	                            std::uint32_t duration_ms) const
	{
		return input::query_gamepad<bool>(
		    m_id, m_generation, [low_frequency, high_frequency, duration_ms](SDL_Gamepad* value)
		    { return SDL_RumbleGamepad(value, low_frequency, high_frequency, duration_ms); }, false);
	}

	inline bool gamepad::rumble_triggers(std::uint16_t left, std::uint16_t right,
	                                     std::uint32_t duration_ms) const
	{
		return input::query_gamepad<bool>(
		    m_id, m_generation, [left, right, duration_ms](SDL_Gamepad* value)
		    { return SDL_RumbleGamepadTriggers(value, left, right, duration_ms); }, false);
	}

	inline bool gamepad::set_led(std::uint8_t red, std::uint8_t green, std::uint8_t blue) const
	{
		return input::query_gamepad<bool>(
		    m_id, m_generation, [red, green, blue](SDL_Gamepad* value)
		    { return SDL_SetGamepadLED(value, red, green, blue); }, false);
	}

	inline bool gamepad::send_effect(std::span<const std::byte> effect) const
	{
		if (effect.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return false;
		return input::query_gamepad<bool>(
		    m_id, m_generation, [effect](SDL_Gamepad* value)
		    { return SDL_SendGamepadEffect(value, effect.data(), static_cast<int>(effect.size())); }, false);
	}

	inline std::string gamepad::apple_button_symbol(gamepad_button button) const
	{
		std::size_t index{};
		if (!input::gamepad_button_index(button, index))
			return {};
		return input::query_gamepad<std::string>(m_id, m_generation,
		                                         [button](SDL_Gamepad* value)
		                                         {
			                                         const char* symbol =
			                                             SDL_GetGamepadAppleSFSymbolsNameForButton(
			                                                 value, static_cast<SDL_GamepadButton>(button));
			                                         return symbol ? std::string(symbol) : std::string{};
		                                         });
	}

	inline std::string gamepad::apple_axis_symbol(gamepad_axis axis) const
	{
		std::size_t index{};
		if (!input::gamepad_axis_index(axis, index))
			return {};
		return input::query_gamepad<std::string>(m_id, m_generation,
		                                         [axis](SDL_Gamepad* value)
		                                         {
			                                         const char* symbol =
			                                             SDL_GetGamepadAppleSFSymbolsNameForAxis(
			                                                 value, static_cast<SDL_GamepadAxis>(axis));
			                                         return symbol ? std::string(symbol) : std::string{};
		                                         });
	}
