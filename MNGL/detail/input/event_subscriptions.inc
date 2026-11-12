		static void init_all()
		{
			[[maybe_unused]] static const bool input_device_events_enabled = []
			{
				SDL_SetJoystickEventsEnabled(true);
				SDL_SetGamepadEventsEnabled(true);
				return true;
			}();

			[[maybe_unused]] static auto app_quit_subscription = event_manager::on<app_quit>(
			    [](const app_quit&) { should_work.store(false, std::memory_order_release); });
			[[maybe_unused]] static auto app_terminating_subscription = event_manager::on<app_terminating>(
			    [](const app_terminating&) { should_work.store(false, std::memory_order_release); });
			[[maybe_unused]] static auto window_close_subscription = event_manager::on<window_close_request>(
			    [](const window_close_request&) { should_work.store(false, std::memory_order_release); });
			[[maybe_unused]] static auto window_focus_lost_subscription =
			    event_manager::on<window_focus_lost>(
			        [](const window_focus_lost& e)
			        {
				        std::lock_guard lock(state_mutex);
				        window_focus_states[e.window_id] = false;
				        window_inactive_reasons[e.window_id] = input_release_reason::focus_lost;
				        ++window_focus_loss_counts[e.window_id];
				        reset_window_state_locked(e.window_id, input_release_reason::focus_lost);
			        });
			[[maybe_unused]] static auto window_focus_gained_subscription =
			    event_manager::on<window_focus_gained>(
			        [](const window_focus_gained& e)
			        {
				        std::lock_guard lock(state_mutex);
				        window_focus_states[e.window_id] = true;
				        window_inactive_reasons[e.window_id] = input_release_reason::none;
			        });
			[[maybe_unused]] static auto window_destroyed_subscription = event_manager::on<window_destroyed>(
			    [](const window_destroyed& e)
			    {
				    std::lock_guard lock(state_mutex);
				    window_focus_states[e.window_id] = false;
				    window_inactive_reasons[e.window_id] = input_release_reason::window_destroyed;
				    reset_window_state_locked(e.window_id, input_release_reason::window_destroyed);
			    });

			[[maybe_unused]] static auto keyboard_added_subscription = event_manager::on<keyboard_added>(
			    [](const keyboard_added& e)
			    {
				    if (e.keyboard_id == 0)
					    return;
				    std::lock_guard lock(state_mutex);
				    ensure_keyboard_state_locked(e.keyboard_id);
			    });
			[[maybe_unused]] static auto keyboard_removed_subscription = event_manager::on<keyboard_removed>(
			    [](const keyboard_removed& e)
			    {
				    if (e.keyboard_id == 0)
					    return;
				    std::lock_guard lock(state_mutex);
				    remove_keyboard_locked(e.keyboard_id);
			    });

			[[maybe_unused]] static auto mouse_added_subscription = event_manager::on<mouse_added>(
			    [](const mouse_added& e)
			    {
				    if (!is_physical_mouse_id(e.mouse_id))
					    return;
				    std::lock_guard lock(state_mutex);
				    ensure_mouse_state_locked(e.mouse_id);
			    });
			[[maybe_unused]] static auto mouse_removed_subscription = event_manager::on<mouse_removed>(
			    [](const mouse_removed& e)
			    {
				    if (!is_physical_mouse_id(e.mouse_id))
					    return;
				    std::lock_guard lock(state_mutex);
				    remove_mouse_locked(e.mouse_id);
			    });

			[[maybe_unused]] static auto key_down_subscription = event_manager::on<key_down>(
			    [](const key_down& e)
			    {
				    std::lock_guard lock(state_mutex);
				    auto& state = ensure_keyboard_state_locked(e.keyboard_id);
				    std::size_t index{};
				    if (key_index(e.code, index))
				    {
					    const bool was_down = state.down.test(index);
					    state.down.set(index);
					    state.key_windows[index] = e.window_id;
					    if (!was_down && !e.repeat)
					    {
						    state.pressed.set(index);
						    state.pressed_windows[index] = e.window_id;
					    }
				    }
			    });
			[[maybe_unused]] static auto key_up_subscription = event_manager::on<key_up>(
			    [](const key_up& e)
			    {
				    std::lock_guard lock(state_mutex);
				    auto& state = ensure_keyboard_state_locked(e.keyboard_id);
				    std::size_t index{};
				    if (key_index(e.code, index))
				    {
					    if (state.down.test(index))
					    {
						    state.released.set(index);
						    state.release_reasons[index] = input_release_reason::physical_release;
						    state.released_windows[index] =
						        state.key_windows[index] ? state.key_windows[index] : e.window_id;
					    }
					    state.down.reset(index);
					    state.key_windows[index] = 0;
				    }
			    });

			[[maybe_unused]] static auto mouse_motion_subscription = event_manager::on<mouse_motion>(
			    [](const mouse_motion& e)
			    {
				    std::lock_guard lock(state_mutex);
				    if (is_physical_mouse_id(e.mouse_id))
				    {
					    auto& state = ensure_mouse_state_locked(e.mouse_id);
					    set_mouse_window_locked(state, e.window_id);
					    state.position = e.position;
					    state.delta += e.relative;
					    state.has_position = true;
				    }
			    });
			[[maybe_unused]] static auto mouse_button_down_subscription =
			    event_manager::on<mouse_button_down>(
			        [](const mouse_button_down& e)
			        {
				        std::lock_guard lock(state_mutex);
				        if (is_physical_mouse_id(e.mouse_id))
				        {
					        auto& state = ensure_mouse_state_locked(e.mouse_id);
					        set_mouse_window_locked(state, e.window_id);
					        std::size_t index{};
					        if (mouse_button_index(static_cast<mouse_button>(e.button), index))
					        {
						        if (!state.down.test(index))
						        {
							        state.pressed.set(index);
							        state.pressed_windows[index] = e.window_id;
						        }
						        state.down.set(index);
						        state.button_windows[index] = e.window_id;
					        }
					        state.position = e.position;
					        state.has_position = true;
				        }
			        });
			[[maybe_unused]] static auto mouse_button_up_subscription = event_manager::on<mouse_button_up>(
			    [](const mouse_button_up& e)
			    {
				    std::lock_guard lock(state_mutex);
				    if (is_physical_mouse_id(e.mouse_id))
				    {
					    auto& state = ensure_mouse_state_locked(e.mouse_id);
					    set_mouse_window_locked(state, e.window_id);
					    std::size_t index{};
					    if (mouse_button_index(static_cast<mouse_button>(e.button), index))
					    {
						    if (state.down.test(index))
						    {
							    state.released.set(index);
							    state.release_reasons[index] = input_release_reason::physical_release;
							    state.released_windows[index] =
							        state.button_windows[index] ? state.button_windows[index] : e.window_id;
						    }
						    state.down.reset(index);
						    state.button_windows[index] = 0;
					    }
					    state.position = e.position;
					    state.has_position = true;
				    }
			    });
			[[maybe_unused]] static auto mouse_wheel_subscription = event_manager::on<mouse_wheel>(
			    [](const mouse_wheel& e)
			    {
				    std::lock_guard lock(state_mutex);
				    if (is_physical_mouse_id(e.which))
				    {
					    auto& state = ensure_mouse_state_locked(e.which);
					    set_mouse_window_locked(state, e.window_id);
					    state.position = e.mouse_position;
					    state.scroll_delta += e.direction == wheel_direction::flipped ? -e.scroll : e.scroll;
					    state.has_position = true;
				    }
			    });

			[[maybe_unused]] static auto finger_down_subscription = event_manager::on<finger_down>(
			    [](const finger_down& e)
			    {
				    update_touch_finger(e.touch_id, e.finger_id, e.window_id, e.position, e.delta, e.pressure,
					                    0);
			    });
			[[maybe_unused]] static auto finger_motion_subscription = event_manager::on<finger_motion>(
			    [](const finger_motion& e)
			    {
				    update_touch_finger(e.touch_id, e.finger_id, e.window_id, e.position, e.delta, e.pressure,
					                    1);
			    });
			[[maybe_unused]] static auto finger_up_subscription = event_manager::on<finger_up>(
			    [](const finger_up& e)
			    {
				    update_touch_finger(e.touch_id, e.finger_id, e.window_id, e.position, e.delta, e.pressure,
					                    2);
			    });
			[[maybe_unused]] static auto finger_canceled_subscription = event_manager::on<finger_canceled>(
			    [](const finger_canceled& e)
			    {
				    update_touch_finger(e.touch_id, e.finger_id, e.window_id, e.position, e.delta, e.pressure,
					                    3);
			    });

			[[maybe_unused]] static auto pen_proximity_in_subscription = event_manager::on<pen_proximity_in>(
			    [](const pen_proximity_in& e) { update_pen_proximity(e.pen_id, e.window_id, true); });
			[[maybe_unused]] static auto pen_proximity_out_subscription =
			    event_manager::on<pen_proximity_out>([](const pen_proximity_out& e)
				                                     { update_pen_proximity(e.pen_id, e.window_id, false); });
			[[maybe_unused]] static auto pen_down_subscription = event_manager::on<pen_down>(
			    [](const pen_down& e)
			    { update_pen_event(e.pen_id, e.window_id, e.position, e.state, e.eraser); });
			[[maybe_unused]] static auto pen_up_subscription = event_manager::on<pen_up>(
			    [](const pen_up& e)
			    {
				    update_pen_event(e.pen_id, e.window_id, e.position, e.state, e.eraser);
				    std::lock_guard lock(state_mutex);
				    const auto it = pen_states.find(e.pen_id);
				    if (it != pen_states.end())
					    it->second.axes[SDL_PEN_AXIS_PRESSURE] = 0.0f;
			    });
			[[maybe_unused]] static auto pen_button_down_subscription = event_manager::on<pen_button_down>(
			    [](const pen_button_down& e)
			    { update_pen_event(e.pen_id, e.window_id, e.position, e.state); });
			[[maybe_unused]] static auto pen_button_up_subscription = event_manager::on<pen_button_up>(
			    [](const pen_button_up& e) { update_pen_event(e.pen_id, e.window_id, e.position, e.state); });
			[[maybe_unused]] static auto pen_motion_subscription = event_manager::on<pen_motion>(
			    [](const pen_motion& e) { update_pen_event(e.pen_id, e.window_id, e.position, e.state); });
			[[maybe_unused]] static auto pen_axis_subscription = event_manager::on<pen_axis>(
			    [](const pen_axis& e)
			    {
				    update_pen_event(e.pen_id, e.window_id, e.position, e.state);
				    std::size_t index{};
				    if (!pen_axis_index(e.axis, index))
					    return;
				    std::lock_guard lock(state_mutex);
				    const auto it = pen_states.find(e.pen_id);
				    if (it != pen_states.end())
					    it->second.axes[index] = e.value;
			    });

			[[maybe_unused]] static auto joystick_added_subscription = event_manager::on<joystick_added>(
			    [](const joystick_added& e)
			    {
				    if (e.which == 0)
					    return;
				    std::lock_guard lock(state_mutex);
				    ensure_joystick_state_locked(e.which);
			    });
			[[maybe_unused]] static auto joystick_removed_subscription = event_manager::on<joystick_removed>(
			    [](const joystick_removed& e)
			    {
				    if (e.which == 0)
					    return;
				    std::lock_guard lock(state_mutex);
				    remove_joystick_locked(e.which);
			    });
			[[maybe_unused]] static auto joystick_axis_subscription = event_manager::on<joystick_axis_moved>(
			    [](const joystick_axis_moved& e)
			    {
				    if (e.which == 0)
					    return;
				    std::lock_guard lock(state_mutex);
				    auto& state = ensure_joystick_state_locked(e.which);
				    if (!state.metadata_initialized && state.axes.size() <= e.axis)
					    state.axes.resize(static_cast<std::size_t>(e.axis) + 1);
				    if (e.axis < state.axes.size())
					    state.axes[e.axis] = e.value;
			    });
			[[maybe_unused]] static auto joystick_button_down_subscription =
			    event_manager::on<joystick_button_down>(
			        [](const joystick_button_down& e)
			        {
				        if (e.which == 0)
					        return;
				        std::lock_guard lock(state_mutex);
				        auto& state = ensure_joystick_state_locked(e.which);
				        if (!state.metadata_initialized && state.buttons.size() <= e.button)
					        state.buttons.resize(static_cast<std::size_t>(e.button) + 1);
				        if (e.button < state.buttons.size())
				        {
					        auto& button = state.buttons[e.button];
					        if (!button.down)
						        button.pressed = true;
					        button.down = true;
				        }
			        });
			[[maybe_unused]] static auto joystick_button_up_subscription =
			    event_manager::on<joystick_button_up>(
			        [](const joystick_button_up& e)
			        {
				        if (e.which == 0)
					        return;
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
			[[maybe_unused]] static auto joystick_hat_subscription = event_manager::on<joystick_hat_pressed>(
			    [](const joystick_hat_pressed& e)
			    {
				    if (e.which == 0)
					    return;
				    std::lock_guard lock(state_mutex);
				    auto& state = ensure_joystick_state_locked(e.which);
				    if (!state.metadata_initialized && state.hats.size() <= e.hat)
					    state.hats.resize(static_cast<std::size_t>(e.hat) + 1, hat_position::centered);
				    if (e.hat < state.hats.size())
					    state.hats[e.hat] = e.value;
			    });
			[[maybe_unused]] static auto joystick_ball_subscription = event_manager::on<joystick_ball_moved>(
			    [](const joystick_ball_moved& e)
			    {
				    if (e.which == 0)
					    return;
				    std::lock_guard lock(state_mutex);
				    auto& state = ensure_joystick_state_locked(e.which);
				    if (!state.metadata_initialized && state.ball_deltas.size() <= e.ball)
					    state.ball_deltas.resize(static_cast<std::size_t>(e.ball) + 1, glm::ivec2{});
				    if (e.ball < state.ball_deltas.size())
					    state.ball_deltas[e.ball] += e.relative;
			    });

			[[maybe_unused]] static auto gamepad_added_subscription = event_manager::on<gamepad_added>(
			    [](const gamepad_added& e)
			    {
				    if (e.which == 0)
					    return;
				    std::lock_guard lock(state_mutex);
				    ensure_gamepad_state_locked(e.which);
			    });
			[[maybe_unused]] static auto gamepad_removed_subscription = event_manager::on<gamepad_removed>(
			    [](const gamepad_removed& e)
			    {
				    if (e.which == 0)
					    return;
				    std::lock_guard lock(state_mutex);
				    remove_gamepad_locked(e.which);
			    });
			[[maybe_unused]] static auto gamepad_remapped_subscription = event_manager::on<gamepad_remapped>(
			    [](const gamepad_remapped& e)
			    {
				    if (e.which == 0)
					    return;
				    std::lock_guard lock(state_mutex);
				    auto it = gamepad_states.find(e.which);
				    if (it != gamepad_states.end() && it->second.connected)
					    refresh_gamepad_mapping_locked(it->second);
			    });
			[[maybe_unused]] static auto gamepad_axis_subscription = event_manager::on<gamepad_axis_event>(
			    [](const gamepad_axis_event& e)
			    {
				    if (e.which == 0)
					    return;
				    std::size_t index{};
				    if (!gamepad_axis_index(e.axis, index))
					    return;
				    std::lock_guard lock(state_mutex);
				    auto& state = ensure_gamepad_state_locked(e.which);
				    state.axes[index] = e.value;
			    });
			[[maybe_unused]] static auto gamepad_button_down_subscription =
			    event_manager::on<gamepad_button_down>(
			        [](const gamepad_button_down& e)
			        {
				        if (e.which == 0)
					        return;
				        std::size_t index{};
				        if (!gamepad_button_index(e.button, index))
					        return;
				        std::lock_guard lock(state_mutex);
				        auto& state = ensure_gamepad_state_locked(e.which);
				        auto& button = state.buttons[index];
				        if (!button.down)
					        button.pressed = true;
				        button.down = true;
			        });
			[[maybe_unused]] static auto gamepad_button_up_subscription =
			    event_manager::on<gamepad_button_up>(
			        [](const gamepad_button_up& e)
			        {
				        if (e.which == 0)
					        return;
				        std::size_t index{};
				        if (!gamepad_button_index(e.button, index))
					        return;
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

