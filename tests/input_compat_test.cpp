#define APP_NAME "MNGL input Compatibility Test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)
#include "../MNGL/input.hpp"
#include "../MNGL/action_map.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

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

	void test_action_aggregation()
	{
		constexpr SDL_KeyboardID keyboard_id = 7003;
		mngl::action_map map;
		map.bind_key("movement", mngl::scancode::a, keyboard_id, 1.0f);
		map.bind_key("movement", mngl::scancode::a, keyboard_id, 1.0f);
		map.bind_key("movement", mngl::scancode::a, keyboard_id, -1.0f);
		map.bind_key("reordered", mngl::scancode::a, keyboard_id, 1.0f);
		map.bind_key("reordered", mngl::scancode::a, keyboard_id, -1.0f);
		map.bind_key("reordered", mngl::scancode::a, keyboard_id, 1.0f);
		mngl::event_manager::emit(key_down(keyboard_id));
		mngl::input_context context(map);
		require(context.value("movement") == 1.0f,
		        "action bindings were saturated before their aggregate was computed");
		require(context.value("movement") == context.value("reordered"),
		        "action value depended on binding registration order");
		mngl::event_manager::emit(key_up(keyboard_id));
	}

	void test_flipped_wheel()
	{
		constexpr SDL_MouseID mouse_id = 7004;
		mngl::action_map map;
		map.bind_mouse_axis("scroll", mngl::mouse_axis::scroll_y, 1.0f, 0.0f, 0.5f, mouse_id);
		const float scale = std::numeric_limits<float>::max();
		map.bind_mouse_axis("opposing", mngl::mouse_axis::scroll_y, scale, 0.0f, 0.5f, mouse_id);
		map.bind_mouse_axis("opposing", mngl::mouse_axis::scroll_y, -scale, 0.0f, 0.5f, mouse_id);
		map.bind_mouse_axis("saturated", mngl::mouse_axis::scroll_y, scale, 0.0f, 0.5f, mouse_id);
		mngl::input_context context(map);
		SDL_Event event{};
		event.type = SDL_EVENT_MOUSE_WHEEL;
		event.wheel.which = mouse_id;
		event.wheel.y = 2.0f;
		event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
		require(SDL_PushEvent(&event), "failed to queue a flipped wheel event");
		require(mngl::input::continue_work(), "input loop stopped during wheel direction test");
		require(context.value("scroll") == -1.0f, "flipped wheel did not normalize scroll direction");
		require(context.value("opposing") == 0.0f,
		        "large opposing scales produced a non-finite action value");
		require(context.value("saturated") == -1.0f,
		        "large finite scale did not saturate to the action range");
	}

	void test_imgui_text_ownership()
	{
		mngl::event_manager::set_imgui_input_enabled(true);
		char text[] = "owned text";
		SDL_Event event{};
		event.type = SDL_EVENT_TEXT_INPUT;
		event.text.text = text;
		require(SDL_PushEvent(&event), "failed to queue an ImGui text event");
		const auto events = mngl::event_manager::take_imgui_input_events();
		const auto found = std::find_if(events.begin(), events.end(), [](const SDL_Event& item)
		                                { return item.type == SDL_EVENT_TEXT_INPUT; });
		require(found != events.end(), "text event was missing from the ImGui queue");
		require(found->text.text != text, "ImGui queue retained a borrowed text pointer");
		text[0] = 'X';
		SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
		SDL_PumpEvents();
		require(std::string_view(found->text.text) == "owned text",
		        "queued ImGui text did not survive its SDL event payload");
		mngl::event_manager::set_imgui_input_enabled(false);
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
		test_action_aggregation();
		test_flipped_wheel();
		test_imgui_text_ownership();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
