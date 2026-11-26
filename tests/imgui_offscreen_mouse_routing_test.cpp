#include <MNGL/detail/event/manager.hpp>

#include <iostream>
#include <stdexcept>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
		{
			throw std::runtime_error(message);
		}
	}

	SDL_Event motion(SDL_WindowID window_id, SDL_MouseID mouse_id, float x, float y)
	{
		SDL_Event event{};
		event.motion.type = SDL_EVENT_MOUSE_MOTION;
		event.motion.windowID = window_id;
		event.motion.which = mouse_id;
		event.motion.x = x;
		event.motion.y = y;
		return event;
	}

	void test_latest_mouse_event_wins_for_its_window()
	{
		mngl::detail::mouse_pointer_tracker tracker;
		tracker.observe(motion(51, 101, 10.0f, 10.0f));
		tracker.observe(motion(51, 202, 100.0f, 100.0f));
		tracker.observe(motion(52, 303, 25.0f, 40.0f));

		const auto active = tracker.latest_for_window(51);
		require(active.has_value(), "A window with mouse motion did not retain its pointer");
		require(active->mouse_id == 202 && active->x == 100.0f && active->y == 100.0f,
		        "Offscreen routing did not select the last mouse event for the "
		        "target window");
		const auto other_window = tracker.latest_for_window(52);
		require(other_window && other_window->mouse_id == 303,
		        "Mouse activity in another window replaced the target window pointer");
	}

	void test_drag_capture_keeps_its_mouse_id()
	{
		mngl::detail::mouse_pointer_tracker tracker;
		tracker.observe(motion(61, 111, 20.0f, 30.0f));
		tracker.observe(motion(61, 222, 120.0f, 130.0f));

		const auto newest = tracker.latest_for_window(61);
		const auto captured = tracker.for_mouse(61, 111);
		require(newest && newest->mouse_id == 222,
		        "The second mouse should own the latest uncaptured pointer event");
		require(captured && captured->mouse_id == 111 && captured->x == 20.0f && captured->y == 30.0f,
		        "An offscreen drag lost the pointer that began the capture");

		SDL_Event leave{};
		leave.window.type = SDL_EVENT_WINDOW_MOUSE_LEAVE;
		leave.window.windowID = 61;
		tracker.observe(leave);
		require(!tracker.latest_for_window(61),
		        "A window without a current mouse position kept routing a stale "
		        "hover pointer");
		require(tracker.for_mouse(61, 111).has_value(),
		        "A captured drag should retain its last position when the pointer "
		        "leaves the window");
	}

	void test_missing_pointer_has_no_fabricated_origin()
	{
		mngl::detail::mouse_pointer_tracker tracker;
		require(!tracker.latest_for_window(71), "A window without mouse events acquired a pointer at (0, 0)");
		require(!tracker.for_mouse(71, 777), "A missing captured mouse returned a fabricated position");
	}
} // namespace

int main()
{
	try
	{
		test_latest_mouse_event_wins_for_its_window();
		test_drag_capture_keeps_its_mouse_id();
		test_missing_pointer_has_no_fabricated_origin();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Offscreen ImGui mouse routing regression test failed: " << error.what() << '\n';
		return 1;
	}
}
