#define APP_NAME "MNGL input hot-plug and window regression test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)

#include <MNGL/action_map.hpp>
#include <MNGL/input.hpp>
#include <MNGL/runtime.hpp>
#include <MNGL/window.hpp>

#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	void push_event(SDL_Event event)
	{
		require(SDL_PushEvent(&event), "SDL rejected an event used by the regression test");
	}

	SDL_JoystickID attach_virtual_gamepad()
	{
		SDL_VirtualJoystickDesc desc;
		SDL_INIT_INTERFACE(&desc);
		desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
		desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
		desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
		desc.button_mask = 1u << SDL_GAMEPAD_BUTTON_SOUTH;
		desc.axis_mask = 1u << SDL_GAMEPAD_AXIS_LEFTX;
		desc.name = "MNGL virtual gamepad regression";
		const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
		require(id != 0, "SDL could not attach a virtual gamepad");
		require(SDL_IsGamepad(id), "SDL did not recognize the virtual joystick as a gamepad");
		return id;
	}

	void test_hot_plugged_gamepad_and_joystick_bindings()
	{
		mngl::input::set_max_fps(0);
		mngl::action_map gamepad_map;
		gamepad_map.bind_gamepad_button("fire", mngl::gamepad_button::a);
		mngl::input_context gamepad_context(gamepad_map);
		mngl::action_map joystick_map;
		joystick_map.bind_joystick_button("fire", 0);
		mngl::input_context joystick_context(joystick_map);

		const SDL_JoystickID id = attach_virtual_gamepad();
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");
		require(SDL_GetGamepadFromID(id) != nullptr,
		        "The hot-plugged gamepad was not opened for an existing action context");
		require(SDL_GetJoystickFromID(id) != nullptr,
		        "The hot-plugged joystick was not opened for an existing action context");
		(void)gamepad_context.state("fire");
		(void)joystick_context.state("fire");

		std::unique_ptr<SDL_Joystick, decltype(&SDL_CloseJoystick)> joystick(SDL_OpenJoystick(id),
		                                                                       &SDL_CloseJoystick);
		require(joystick != nullptr, "SDL could not open the virtual joystick for input injection");
		require(SDL_SetJoystickVirtualButton(joystick.get(), SDL_GAMEPAD_BUTTON_SOUTH, true),
		        "SDL could not press the virtual gamepad button");
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");
		require(gamepad_context.state("fire").down && gamepad_context.state("fire").pressed,
		        "The gamepad action did not receive the first virtual button press");
		require(joystick_context.state("fire").down && joystick_context.state("fire").pressed,
		        "The joystick action did not receive the first virtual button press");

		require(SDL_DetachVirtualJoystick(id), "SDL could not detach the virtual gamepad");
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit after device removal");
		const auto gamepad_removed = gamepad_context.state("fire");
		const auto joystick_removed = joystick_context.state("fire");
		if (gamepad_removed.down || !gamepad_removed.released ||
		    gamepad_removed.release_reason != mngl::input_release_reason::device_removed)
		{
			std::cerr << "Gamepad removal state: down=" << gamepad_removed.down
			          << ", released=" << gamepad_removed.released
			          << ", reason=" << static_cast<int>(gamepad_removed.release_reason) << '\n';
		}
		require(!gamepad_removed.down && gamepad_removed.released &&
		            gamepad_removed.release_reason == mngl::input_release_reason::device_removed,
		        "Removing the gamepad did not release its held action");
		require(!joystick_removed.down && joystick_removed.released &&
		            joystick_removed.release_reason == mngl::input_release_reason::device_removed,
		        "Removing the joystick did not release its held action");
		joystick.reset();

		const SDL_JoystickID reconnected_id = attach_virtual_gamepad();
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit after reconnect");
		require(SDL_GetGamepadFromID(reconnected_id) != nullptr,
		        "The reconnected gamepad was not opened for an existing action context");
		require(SDL_GetJoystickFromID(reconnected_id) != nullptr,
		        "The reconnected joystick was not opened for an existing action context");
		require(SDL_DetachVirtualJoystick(reconnected_id), "SDL could not detach the reconnected gamepad");
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit after final device removal");
	}

	void test_secondary_window_close_does_not_quit()
	{
		mngl::window::flags flags{};
		flags.hidden = true;
		mngl::window primary("primary", {320, 240}, flags);
		mngl::window secondary("secondary", {320, 240}, flags);
		mngl::poll_events();

		SDL_Event event{};
		event.window.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
		event.window.windowID = secondary.id();
		push_event(event);
		require(mngl::input::next_frame(),
		        "Closing a secondary window unexpectedly ended the application loop");
	}

	void test_single_window_close_still_quits()
	{
		mngl::poll_events();
		mngl::window::flags flags{};
		flags.hidden = true;
		mngl::window only_window("single", {320, 240}, flags);

		SDL_Event event{};
		event.window.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
		event.window.windowID = only_window.id();
		push_event(event);
		require(!mngl::input::next_frame(),
		        "Closing the only window did not preserve the default quit behavior");
	}
} // namespace

int main()
{
	try
	{
		mngl::runtime::ensure();
		test_hot_plugged_gamepad_and_joystick_bindings();
		test_secondary_window_close_does_not_quit();
		test_single_window_close_still_quits();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
