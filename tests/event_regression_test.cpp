#define APP_NAME "MNGL event regression test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#include <MNGL/event.hpp>
#include <MNGL/input.hpp>
#include <MNGL/runtime.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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

	void test_queued_events_keep_their_original_subscribers()
	{
		mngl::poll_events();
		int old_calls = 0;
		int replacement_calls = 0;
		auto old = mngl::event_manager::on<mngl::window_resized>([&](const auto&) { ++old_calls; });

		SDL_Event resize{};
		resize.window.type = SDL_EVENT_WINDOW_RESIZED;
		resize.window.windowID = 73;
		resize.window.data1 = 640;
		resize.window.data2 = 480;
		push_event(resize);

		old.disconnect();
		auto replacement =
		    mngl::event_manager::on<mngl::window_resized>([&](const auto&) { ++replacement_calls; });
		mngl::poll_events();
		require(old_calls == 0, "A disconnected listener received a queued SDL event");
		require(replacement_calls == 0, "A new listener received an SDL event queued before subscription");

		push_event(resize);
		mngl::poll_events();
		require(replacement_calls == 1, "A listener did not receive an event queued after subscription");
	}

	void test_missing_sdl_event_conversions_and_owned_payloads()
	{
		mngl::poll_events();
		std::vector<std::uint32_t> keyboard_added;
		std::vector<std::uint32_t> keyboard_removed;
		std::vector<std::uint32_t> mouse_added;
		std::vector<std::uint32_t> mouse_removed;
		std::vector<std::string> candidates;
		std::int32_t selected_candidate = -2;
		bool horizontal = false;
		int screen_keyboard_shown = 0;
		int screen_keyboard_hidden = 0;
		std::uint32_t joystick_completed = 0;
		std::uint32_t gamepad_completed = 0;
		std::uint32_t steam_handle_updated = 0;
		std::array<float, 3> pinch_scales{};
		std::uint32_t pinch_window = 0;
		std::uint32_t lost_render_window = 0;
		std::uint32_t usable_bounds_display = 0;
		std::uint32_t icc_window = 0;
		std::uint32_t hdr_window = 0;
		std::uint32_t registered_user_type = 0;
		void* user_data = reinterpret_cast<void*>(std::uintptr_t{0x1234});

		[[maybe_unused]] auto keyboard_added_connection = mngl::event_manager::on<mngl::keyboard_added>(
		    [&](const auto& event) { keyboard_added.push_back(event.keyboard_id); });
		[[maybe_unused]] auto keyboard_removed_connection = mngl::event_manager::on<mngl::keyboard_removed>(
		    [&](const auto& event) { keyboard_removed.push_back(event.keyboard_id); });
		[[maybe_unused]] auto mouse_added_connection = mngl::event_manager::on<mngl::mouse_added>(
		    [&](const auto& event) { mouse_added.push_back(event.mouse_id); });
		[[maybe_unused]] auto mouse_removed_connection = mngl::event_manager::on<mngl::mouse_removed>(
		    [&](const auto& event) { mouse_removed.push_back(event.mouse_id); });
		[[maybe_unused]] auto candidates_connection =
		    mngl::event_manager::on<mngl::text_editing_candidates>(
		        [&](const auto& event)
		        {
			        candidates = event.candidates;
			        selected_candidate = event.selected;
			        horizontal = event.horizontal;
		        });
		[[maybe_unused]] auto screen_shown_connection = mngl::event_manager::on<mngl::screen_keyboard_shown>(
		    [&](const auto&) { ++screen_keyboard_shown; });
		[[maybe_unused]] auto screen_hidden_connection = mngl::event_manager::on<mngl::screen_keyboard_hidden>(
		    [&](const auto&) { ++screen_keyboard_hidden; });
		[[maybe_unused]] auto joystick_connection = mngl::event_manager::on<mngl::joystick_update_complete>(
		    [&](const auto& event) { joystick_completed = event.which; });
		[[maybe_unused]] auto gamepad_connection = mngl::event_manager::on<mngl::gamepad_update_complete>(
		    [&](const auto& event) { gamepad_completed = event.which; });
		[[maybe_unused]] auto steam_connection =
		    mngl::event_manager::on<mngl::gamepad_steam_handle_updated>(
		        [&](const auto& event) { steam_handle_updated = event.which; });
		[[maybe_unused]] auto pinch_begin_connection = mngl::event_manager::on<mngl::pinch_begin>(
		    [&](const auto& event)
		    {
			    pinch_scales[0] = event.scale;
			    pinch_window = event.window_id;
		    });
		[[maybe_unused]] auto pinch_update_connection = mngl::event_manager::on<mngl::pinch_update>(
		    [&](const auto& event) { pinch_scales[1] = event.scale; });
		[[maybe_unused]] auto pinch_end_connection = mngl::event_manager::on<mngl::pinch_end>(
		    [&](const auto& event) { pinch_scales[2] = event.scale; });
		[[maybe_unused]] auto lost_connection = mngl::event_manager::on<mngl::render_device_lost>(
		    [&](const auto& event) { lost_render_window = event.window_id; });
		[[maybe_unused]] auto bounds_connection =
		    mngl::event_manager::on<mngl::display_usable_bounds_changed>(
		        [&](const auto& event) { usable_bounds_display = event.display_id; });
		[[maybe_unused]] auto icc_connection = mngl::event_manager::on<mngl::window_icc_prof_changed>(
		    [&](const auto& event) { icc_window = event.window_id; });
		[[maybe_unused]] auto hdr_connection = mngl::event_manager::on<mngl::window_hdr_state_changed>(
		    [&](const auto& event) { hdr_window = event.window_id; });
		const Uint32 registered_type = SDL_RegisterEvents(1);
		require(registered_type != 0, "SDL could not reserve a user event type");
		registered_user_type = registered_type;
		[[maybe_unused]] auto user_connection = mngl::event_manager::on<mngl::user_event>(
		    [&](const auto& event)
		    {
			    require(static_cast<Uint32>(event.type) == registered_user_type,
			            "Registered SDL event type was not preserved");
			    require(event.window_id == 51 && event.code == 17 && event.data1 == user_data,
			            "SDL user event fields were not preserved");
		    });

		SDL_Event event{};
		event.kdevice.type = SDL_EVENT_KEYBOARD_ADDED;
		event.kdevice.which = 301;
		push_event(event);
		event = {};
		event.kdevice.type = SDL_EVENT_KEYBOARD_REMOVED;
		event.kdevice.which = 302;
		push_event(event);
		event = {};
		event.mdevice.type = SDL_EVENT_MOUSE_ADDED;
		event.mdevice.which = 401;
		push_event(event);
		event = {};
		event.mdevice.type = SDL_EVENT_MOUSE_REMOVED;
		event.mdevice.which = 402;
		push_event(event);

		char first_candidate[] = "alpha";
		const char* candidate_text[]{first_candidate, "beta"};
		event = {};
		event.edit_candidates.type = SDL_EVENT_TEXT_EDITING_CANDIDATES;
		event.edit_candidates.windowID = 45;
		event.edit_candidates.candidates = candidate_text;
		event.edit_candidates.num_candidates = 2;
		event.edit_candidates.selected_candidate = 1;
		event.edit_candidates.horizontal = true;
		push_event(event);
		first_candidate[0] = 'X';

		event = {};
		event.type = SDL_EVENT_SCREEN_KEYBOARD_SHOWN;
		push_event(event);
		event = {};
		event.type = SDL_EVENT_SCREEN_KEYBOARD_HIDDEN;
		push_event(event);
		event = {};
		event.jdevice.type = SDL_EVENT_JOYSTICK_UPDATE_COMPLETE;
		event.jdevice.which = 501;
		push_event(event);
		event = {};
		event.gdevice.type = SDL_EVENT_GAMEPAD_UPDATE_COMPLETE;
		event.gdevice.which = 601;
		push_event(event);
		event = {};
		event.gdevice.type = SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED;
		event.gdevice.which = 602;
		push_event(event);

		event = {};
		event.pinch.type = SDL_EVENT_PINCH_BEGIN;
		event.pinch.windowID = 71;
		event.pinch.scale = 0.75f;
		push_event(event);
		event.pinch.type = SDL_EVENT_PINCH_UPDATE;
		event.pinch.scale = 1.25f;
		push_event(event);
		event.pinch.type = SDL_EVENT_PINCH_END;
		event.pinch.scale = 1.0f;
		push_event(event);

		event = {};
		event.render.type = SDL_EVENT_RENDER_DEVICE_LOST;
		event.render.windowID = 81;
		push_event(event);
		event = {};
		event.display.type = SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED;
		event.display.displayID = 91;
		push_event(event);
		event = {};
		event.window.type = SDL_EVENT_WINDOW_ICCPROF_CHANGED;
		event.window.windowID = 101;
		push_event(event);
		event.window.type = SDL_EVENT_WINDOW_HDR_STATE_CHANGED;
		event.window.windowID = 102;
		push_event(event);

		event = {};
		event.user.type = registered_type;
		event.user.windowID = 51;
		event.user.code = 17;
		event.user.data1 = user_data;
		push_event(event);

		mngl::poll_events();
		require(keyboard_added == std::vector<std::uint32_t>{301} &&
		            keyboard_removed == std::vector<std::uint32_t>{302},
		        "SDL keyboard hot-plug events were not converted");
		require(mouse_added == std::vector<std::uint32_t>{401} &&
		            mouse_removed == std::vector<std::uint32_t>{402},
		        "SDL mouse hot-plug events were not converted");
		require(candidates == std::vector<std::string>{"alpha", "beta"} && selected_candidate == 1 &&
		            horizontal,
		        "IME candidate data was not copied before deferred delivery");
		require(screen_keyboard_shown == 1 && screen_keyboard_hidden == 1,
		        "SDL on-screen keyboard events were not converted");
		require(joystick_completed == 501 && gamepad_completed == 601 && steam_handle_updated == 602,
		        "SDL joystick/gamepad completion events were not converted");
		require(pinch_scales == std::array<float, 3>{0.75f, 1.25f, 1.0f} && pinch_window == 71,
		        "SDL pinch gesture events were not converted");
		require(lost_render_window == 81, "SDL render-device loss event was not converted");
		require(usable_bounds_display == 91 && icc_window == 101 && hdr_window == 102,
		        "SDL display/window state events were not converted");
	}

	void test_input_releases_held_state_on_device_removal()
	{
		mngl::input::set_max_fps(0);
		constexpr SDL_KeyboardID keyboard_id = 0xF123;
		constexpr SDL_MouseID mouse_id = 0xF456;

		SDL_Event event{};
		event.kdevice.type = SDL_EVENT_KEYBOARD_ADDED;
		event.kdevice.which = keyboard_id;
		push_event(event);
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");
		event = {};
		event.key.type = SDL_EVENT_KEY_DOWN;
		event.key.which = keyboard_id;
		event.key.scancode = SDL_SCANCODE_Q;
		event.key.down = true;
		push_event(event);
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");
		require(mngl::input::is_key_holded(mngl::scancode::q), "Injected key did not enter held state");

		event = {};
		event.kdevice.type = SDL_EVENT_KEYBOARD_REMOVED;
		event.kdevice.which = keyboard_id;
		push_event(event);
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");
		require(!mngl::input::is_key_holded(mngl::scancode::q) &&
		            mngl::input::is_key_released(mngl::scancode::q),
		        "Keyboard removal left a key held instead of releasing it");

		event = {};
		event.mdevice.type = SDL_EVENT_MOUSE_ADDED;
		event.mdevice.which = mouse_id;
		push_event(event);
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");
		event = {};
		event.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
		event.button.which = mouse_id;
		event.button.button = SDL_BUTTON_LEFT;
		event.button.down = true;
		push_event(event);
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");
		require(mngl::input::is_mouse_button_holded(mngl::mouse_button::left),
		        "Injected mouse button did not enter held state");

		event = {};
		event.mdevice.type = SDL_EVENT_MOUSE_REMOVED;
		event.mdevice.which = mouse_id;
		push_event(event);
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");
		require(!mngl::input::is_mouse_button_holded(mngl::mouse_button::left) &&
		            mngl::input::is_mouse_button_released(mngl::mouse_button::left),
		        "Mouse removal left a button held instead of releasing it");
	}

	void test_poll_events_drains_more_than_the_sdl_queue_capacity()
	{
		mngl::poll_events();
		constexpr int total_events = 70'000;
		constexpr int events_per_batch = 8'000;
		const Uint32 registered_type = SDL_RegisterEvents(1);
		require(registered_type != 0, "SDL could not reserve a queue stress event type");
		SDL_Event event{};
		event.user.type = registered_type;
		int pushed_events = 0;
		for (int index = 0; index < total_events; ++index)
		{
			event.user.code = index;
			push_event(event);
			++pushed_events;
			if (pushed_events % events_per_batch == 0)
				mngl::poll_events();
		}
		mngl::poll_events();

		const int remaining = SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
		require(pushed_events > 65'535, "The regression did not exceed SDL's reported queue capacity");
		require(remaining == 0, "mngl::poll_events left the SDL queue occupied");
	}
} // namespace

int main()
{
	try
	{
		mngl::runtime::ensure();
		test_queued_events_keep_their_original_subscribers();
		test_missing_sdl_event_conversions_and_owned_payloads();
		test_input_releases_held_state_on_device_removal();
		test_poll_events_drains_more_than_the_sdl_queue_capacity();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
