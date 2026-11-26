#define APP_NAME "MNGL input Compatibility Test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)
#include "../MNGL/input.hpp"
#include "../MNGL/action_map.hpp"

#include <iostream>
#include <stdexcept>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	mngl::key_down key_down(SDL_KeyboardID id)
	{
		mngl::key_down event{};
		event.type = mngl::event_type::key_down;
		event.keyboard_id = id;
		event.code = mngl::scancode::a;
		event.key = {SDL_GetKeyFromScancode(SDL_SCANCODE_A, SDL_KMOD_NONE, false)};
		event.mod = mngl::keymod::none;
		event.repeat = false;
		event.is_down = true;
		return event;
	}

	mngl::key_up key_up(SDL_KeyboardID id)
	{
		mngl::key_up event{};
		event.type = mngl::event_type::key_up;
		event.keyboard_id = id;
		event.code = mngl::scancode::a;
		event.key = {SDL_GetKeyFromScancode(SDL_SCANCODE_A, SDL_KMOD_NONE, false)};
		event.mod = mngl::keymod::none;
		event.repeat = false;
		event.is_down = false;
		return event;
	}

	mngl::mouse_button_down mouse_button_down(SDL_MouseID id)
	{
		mngl::mouse_button_down event{};
		event.type = mngl::event_type::mouse_button_down;
		event.mouse_id = id;
		event.button = SDL_BUTTON_LEFT;
		event.clicks = 1;
		event.is_down = true;
		event.position = {12.0f, 24.0f};
		return event;
	}

	mngl::mouse_button_up mouse_button_up(SDL_MouseID id)
	{
		mngl::mouse_button_up event{};
		event.type = mngl::event_type::mouse_button_up;
		event.mouse_id = id;
		event.button = SDL_BUTTON_LEFT;
		event.clicks = 1;
		event.is_down = false;
		event.position = {12.0f, 24.0f};
		return event;
	}

	void test_keyboard_compatibility()
	{
		constexpr SDL_KeyboardID keyboard_id = 7001;
		mngl::event_manager::emit(key_down(keyboard_id));
		require(mngl::input::is_key_pressed(mngl::scancode::a),
		        "key press edge was not visible through the legacy facade");
		require(!mngl::input::is_key_holded(mngl::scancode::a), "key was held on its press frame");

		require(mngl::input::continue_work(), "input loop stopped during keyboard state test");
		require(!mngl::input::is_key_pressed(mngl::scancode::a),
		        "key press edge lasted longer than one frame");
		require(mngl::input::is_key_holded(mngl::scancode::a), "key down state was lost between frames");

		mngl::event_manager::emit(key_up(keyboard_id));
		require(mngl::input::is_key_released(mngl::scancode::a),
		        "key release edge was not visible through the legacy facade");
		require(!mngl::input::is_key_holded(mngl::scancode::a), "released key remained down");

		require(mngl::input::continue_work(), "input loop stopped before keyboard removal test");
		mngl::event_manager::emit(key_down(keyboard_id));
		mngl::event_manager::emit(key_up(keyboard_id));
		require(mngl::input::is_key_pressed(mngl::scancode::a), "same-frame key tap lost its press edge");
		require(mngl::input::is_key_released(mngl::scancode::a), "same-frame key tap lost its release edge");
		require(!mngl::input::is_key_holded(mngl::scancode::a), "same-frame key tap remained down");

		require(mngl::input::continue_work(), "input loop stopped after same-frame key tap");
		mngl::event_manager::emit(key_down(keyboard_id));
		mngl::keyboard_removed removed{};
		removed.type = mngl::event_type::keyboard_removed;
		removed.keyboard_id = keyboard_id;
		mngl::event_manager::emit(removed);
		require(mngl::input::is_key_released(mngl::scancode::a),
		        "keyboard removal did not release its held key");
		require(!mngl::input::is_key_holded(mngl::scancode::a), "removed keyboard left its key down");

		require(mngl::input::continue_work(), "input loop stopped after keyboard removal");
		require(!mngl::input::is_key_released(mngl::scancode::a),
		        "keyboard removal edge lasted longer than one frame");
	}

	void test_mouse_compatibility()
	{
		constexpr SDL_MouseID mouse_id = 7002;
		mngl::event_manager::emit(mouse_button_down(mouse_id));
		require(mngl::input::is_mouse_button_pressed(mngl::mouse_button::left),
		        "mouse press edge was not visible through the legacy facade");
		require(!mngl::input::is_mouse_button_holded(mngl::mouse_button::left),
		        "mouse button was held on its press frame");
		require(mngl::input::get_mouse_position() == glm::vec2{12.0f, 24.0f},
		        "legacy mouse position did not read the device snapshot");

		require(mngl::input::continue_work(), "input loop stopped during mouse state test");
		require(!mngl::input::is_mouse_button_pressed(mngl::mouse_button::left),
		        "mouse press edge lasted longer than one frame");
		require(mngl::input::is_mouse_button_holded(mngl::mouse_button::left),
		        "mouse down state was lost between frames");

		mngl::event_manager::emit(mouse_button_up(mouse_id));
		require(mngl::input::is_mouse_button_released(mngl::mouse_button::left),
		        "mouse release edge was not visible through the legacy facade");
		require(!mngl::input::is_mouse_button_holded(mngl::mouse_button::left),
		        "released mouse button remained down");

		require(mngl::input::continue_work(), "input loop stopped before mouse removal test");
		mngl::event_manager::emit(mouse_button_down(mouse_id));
		mngl::event_manager::emit(mouse_button_up(mouse_id));
		require(mngl::input::is_mouse_button_pressed(mngl::mouse_button::left),
		        "same-frame mouse tap lost its press edge");
		require(mngl::input::is_mouse_button_released(mngl::mouse_button::left),
		        "same-frame mouse tap lost its release edge");
		require(!mngl::input::is_mouse_button_holded(mngl::mouse_button::left),
		        "same-frame mouse tap remained down");

		require(mngl::input::continue_work(), "input loop stopped after same-frame mouse tap");
		mngl::event_manager::emit(mouse_button_down(mouse_id));
		mngl::mouse_removed removed{};
		removed.type = mngl::event_type::mouse_removed;
		removed.mouse_id = mouse_id;
		mngl::event_manager::emit(removed);
		require(mngl::input::is_mouse_button_released(mngl::mouse_button::left),
		        "mouse removal did not release its held button");
		require(!mngl::input::is_mouse_button_holded(mngl::mouse_button::left),
		        "removed mouse left its button down");

		require(mngl::input::continue_work(), "input loop stopped after mouse removal");
		require(!mngl::input::is_mouse_button_released(mngl::mouse_button::left),
		        "mouse removal edge lasted longer than one frame");
	}

	void test_action_context_reuses_state_across_frames_and_revisions()
	{
		constexpr SDL_KeyboardID keyboard_id = 7003;
		mngl::action_map actions;
		actions.bind_key("jump", mngl::scancode::a, keyboard_id);
		mngl::event_manager::emit(key_down(keyboard_id));

		mngl::input_context context(actions);
		require(context.down("jump"), "An action context must initialize its state for a held binding");
		require(!context.pressed("jump"), "Creating a context over a held binding must establish a neutral baseline");

		require(mngl::input::continue_work(), "input loop stopped during action context state reuse test");
		require(context.down("jump") && !context.pressed("jump"),
		        "Reusing action state across an input frame must preserve down without inventing a press");

		actions.bind_key("secondary", mngl::scancode::a, keyboard_id);
		require(context.down("jump") && !context.pressed("jump"),
		        "Adding an action must reconcile existing state without a false transition");
		require(context.down("secondary") && !context.pressed("secondary"),
		        "A newly added action must enter the context baseline without a false press");

		actions.clear_action("jump");
		require(!context.down("jump") && !context.pressed("jump"),
		        "Removing an action must erase its cached state before it is queried again");
		require(context.down("secondary"), "Reconciling one removed action must retain the other action state");
	}
} // namespace

int main()
{
	try
	{
		mngl::runtime::ensure();
		mngl::input::set_max_fps(0);
		require(mngl::input::continue_work(), "input loop failed to start");
		test_keyboard_compatibility();
		test_mouse_compatibility();
		test_action_context_reuses_state_across_frames_and_revisions();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
