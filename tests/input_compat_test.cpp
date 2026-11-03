#define APP_NAME "MNGL Input Compatibility Test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)
#include "../MNGL/input.hpp"

#include <iostream>
#include <stdexcept>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	mngl::keyDown key_down(SDL_KeyboardID id)
	{
		mngl::keyDown event{};
		event.type = mngl::Events::key_down;
		event.keyboardID = id;
		event.code = mngl::scancode::a;
		event.key = { SDL_GetKeyFromScancode(SDL_SCANCODE_A, SDL_KMOD_NONE, false) };
		event.mod = mngl::keymod::none;
		event.repeat = false;
		event.is_down = true;
		return event;
	}

	mngl::keyUp key_up(SDL_KeyboardID id)
	{
		mngl::keyUp event{};
		event.type = mngl::Events::key_up;
		event.keyboardID = id;
		event.code = mngl::scancode::a;
		event.key = { SDL_GetKeyFromScancode(SDL_SCANCODE_A, SDL_KMOD_NONE, false) };
		event.mod = mngl::keymod::none;
		event.repeat = false;
		event.is_down = false;
		return event;
	}

	mngl::mouseButtonDown mouse_button_down(SDL_MouseID id)
	{
		mngl::mouseButtonDown event{};
		event.type = mngl::Events::mouse_button_down;
		event.mouseID = id;
		event.button = SDL_BUTTON_LEFT;
		event.clicks = 1;
		event.is_down = true;
		event.position = { 12.0f, 24.0f };
		return event;
	}

	mngl::mouseButtonUp mouse_button_up(SDL_MouseID id)
	{
		mngl::mouseButtonUp event{};
		event.type = mngl::Events::mouse_button_up;
		event.mouseID = id;
		event.button = SDL_BUTTON_LEFT;
		event.clicks = 1;
		event.is_down = false;
		event.position = { 12.0f, 24.0f };
		return event;
	}

	void test_keyboard_compatibility()
	{
		constexpr SDL_KeyboardID keyboard_id = 7001;
		mngl::event_manager::emit(key_down(keyboard_id));
		require(mngl::Input::is_key_pressed(mngl::scancode::a), "key press edge was not visible through the legacy facade");
		require(!mngl::Input::is_key_holded(mngl::scancode::a), "key was held on its press frame");

		require(mngl::Input::continue_work(), "input loop stopped during keyboard state test");
		require(!mngl::Input::is_key_pressed(mngl::scancode::a), "key press edge lasted longer than one frame");
		require(mngl::Input::is_key_holded(mngl::scancode::a), "key down state was lost between frames");

		mngl::event_manager::emit(key_up(keyboard_id));
		require(mngl::Input::is_key_released(mngl::scancode::a), "key release edge was not visible through the legacy facade");
		require(!mngl::Input::is_key_holded(mngl::scancode::a), "released key remained down");

		require(mngl::Input::continue_work(), "input loop stopped before keyboard removal test");
		mngl::event_manager::emit(key_down(keyboard_id));
		mngl::event_manager::emit(key_up(keyboard_id));
		require(mngl::Input::is_key_pressed(mngl::scancode::a), "same-frame key tap lost its press edge");
		require(mngl::Input::is_key_released(mngl::scancode::a), "same-frame key tap lost its release edge");
		require(!mngl::Input::is_key_holded(mngl::scancode::a), "same-frame key tap remained down");

		require(mngl::Input::continue_work(), "input loop stopped after same-frame key tap");
		mngl::event_manager::emit(key_down(keyboard_id));
		mngl::keyboardRemoved removed{};
		removed.type = mngl::Events::keyboard_removed;
		removed.keyboardID = keyboard_id;
		mngl::event_manager::emit(removed);
		require(mngl::Input::is_key_released(mngl::scancode::a), "keyboard removal did not release its held key");
		require(!mngl::Input::is_key_holded(mngl::scancode::a), "removed keyboard left its key down");

		require(mngl::Input::continue_work(), "input loop stopped after keyboard removal");
		require(!mngl::Input::is_key_released(mngl::scancode::a), "keyboard removal edge lasted longer than one frame");
	}

	void test_mouse_compatibility()
	{
		constexpr SDL_MouseID mouse_id = 7002;
		mngl::event_manager::emit(mouse_button_down(mouse_id));
		require(mngl::Input::is_mouse_button_pressed(mngl::mouse_button::left), "mouse press edge was not visible through the legacy facade");
		require(!mngl::Input::is_mouse_button_holded(mngl::mouse_button::left), "mouse button was held on its press frame");
		require(mngl::Input::get_mouse_position() == glm::vec2{ 12.0f, 24.0f }, "legacy mouse position did not read the device snapshot");

		require(mngl::Input::continue_work(), "input loop stopped during mouse state test");
		require(!mngl::Input::is_mouse_button_pressed(mngl::mouse_button::left), "mouse press edge lasted longer than one frame");
		require(mngl::Input::is_mouse_button_holded(mngl::mouse_button::left), "mouse down state was lost between frames");

		mngl::event_manager::emit(mouse_button_up(mouse_id));
		require(mngl::Input::is_mouse_button_released(mngl::mouse_button::left), "mouse release edge was not visible through the legacy facade");
		require(!mngl::Input::is_mouse_button_holded(mngl::mouse_button::left), "released mouse button remained down");

		require(mngl::Input::continue_work(), "input loop stopped before mouse removal test");
		mngl::event_manager::emit(mouse_button_down(mouse_id));
		mngl::event_manager::emit(mouse_button_up(mouse_id));
		require(mngl::Input::is_mouse_button_pressed(mngl::mouse_button::left), "same-frame mouse tap lost its press edge");
		require(mngl::Input::is_mouse_button_released(mngl::mouse_button::left), "same-frame mouse tap lost its release edge");
		require(!mngl::Input::is_mouse_button_holded(mngl::mouse_button::left), "same-frame mouse tap remained down");

		require(mngl::Input::continue_work(), "input loop stopped after same-frame mouse tap");
		mngl::event_manager::emit(mouse_button_down(mouse_id));
		mngl::mouseRemoved removed{};
		removed.type = mngl::Events::mouse_removed;
		removed.mouseID = mouse_id;
		mngl::event_manager::emit(removed);
		require(mngl::Input::is_mouse_button_released(mngl::mouse_button::left), "mouse removal did not release its held button");
		require(!mngl::Input::is_mouse_button_holded(mngl::mouse_button::left), "removed mouse left its button down");

		require(mngl::Input::continue_work(), "input loop stopped after mouse removal");
		require(!mngl::Input::is_mouse_button_released(mngl::mouse_button::left), "mouse removal edge lasted longer than one frame");
	}
}

int main()
{
	try
	{
		if (SDL_Runtime.error())
			throw std::runtime_error(SDL_Runtime.content());
		mngl::Input::set_max_fps(0);
		require(mngl::Input::continue_work(), "input loop failed to start");
		test_keyboard_compatibility();
		test_mouse_compatibility();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
