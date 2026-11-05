#pragma once

#include "../../utilities.hpp"
#include <cstdint>

namespace mngl
{
	enum class event_type
	{
		nothing = -1,

		// --- Application ---
		quit = SDL_EVENT_QUIT,
		terminating = SDL_EVENT_TERMINATING,
		low_memory = SDL_EVENT_LOW_MEMORY,
		will_enter_background = SDL_EVENT_WILL_ENTER_BACKGROUND,
		did_enter_background = SDL_EVENT_DID_ENTER_BACKGROUND,
		will_enter_foreground = SDL_EVENT_WILL_ENTER_FOREGROUND,
		did_enter_foreground = SDL_EVENT_DID_ENTER_FOREGROUND,
		locale_changed = SDL_EVENT_LOCALE_CHANGED,
		system_theme_changed = SDL_EVENT_SYSTEM_THEME_CHANGED,

		// --- Display ---
		display_orientation = SDL_EVENT_DISPLAY_ORIENTATION,
		display_added = SDL_EVENT_DISPLAY_ADDED,
		display_removed = SDL_EVENT_DISPLAY_REMOVED,
		display_moved = SDL_EVENT_DISPLAY_MOVED,
		display_desktop_mode_changed = SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED,
		display_current_mode_changed = SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED,
		display_content_scale_changed = SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED,
		display_usable_bounds_changed = SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED,

		// --- Window ---
		window_shown = SDL_EVENT_WINDOW_SHOWN,
		window_hidden = SDL_EVENT_WINDOW_HIDDEN,
		window_exposed = SDL_EVENT_WINDOW_EXPOSED,
		window_moved = SDL_EVENT_WINDOW_MOVED,
		window_resized = SDL_EVENT_WINDOW_RESIZED,
		window_pixel_size_changed = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,
		window_metal_view_resized = SDL_EVENT_WINDOW_METAL_VIEW_RESIZED,
		window_minimized = SDL_EVENT_WINDOW_MINIMIZED,
		window_maximized = SDL_EVENT_WINDOW_MAXIMIZED,
		window_restored = SDL_EVENT_WINDOW_RESTORED,
		window_mouse_enter = SDL_EVENT_WINDOW_MOUSE_ENTER,
		window_mouse_leave = SDL_EVENT_WINDOW_MOUSE_LEAVE,
		window_focus_gained = SDL_EVENT_WINDOW_FOCUS_GAINED,
		window_focus_lost = SDL_EVENT_WINDOW_FOCUS_LOST,
		window_close_request = SDL_EVENT_WINDOW_CLOSE_REQUESTED,
		window_hit_test = SDL_EVENT_WINDOW_HIT_TEST,
		window_iccprof_changed = SDL_EVENT_WINDOW_ICCPROF_CHANGED,
		window_display_changed = SDL_EVENT_WINDOW_DISPLAY_CHANGED,
		window_display_scale_changed = SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED,
		window_safe_area_changed = SDL_EVENT_WINDOW_SAFE_AREA_CHANGED,
		window_occluded = SDL_EVENT_WINDOW_OCCLUDED,
		window_enter_fullscreen = SDL_EVENT_WINDOW_ENTER_FULLSCREEN,
		window_leave_fullscreen = SDL_EVENT_WINDOW_LEAVE_FULLSCREEN,
		window_destroyed = SDL_EVENT_WINDOW_DESTROYED,
		window_hdr_state_changed = SDL_EVENT_WINDOW_HDR_STATE_CHANGED,

		// --- Keyboard ---
		key_down = SDL_EVENT_KEY_DOWN,
		key_up = SDL_EVENT_KEY_UP,
		text_editing = SDL_EVENT_TEXT_EDITING,
		text_input = SDL_EVENT_TEXT_INPUT,
		keymap_changed = SDL_EVENT_KEYMAP_CHANGED,
		keyboard_added = SDL_EVENT_KEYBOARD_ADDED,
		keyboard_removed = SDL_EVENT_KEYBOARD_REMOVED,
		text_editing_candidates = SDL_EVENT_TEXT_EDITING_CANDIDATES,
		screen_keyboard_shown = SDL_EVENT_SCREEN_KEYBOARD_SHOWN,
		screen_keyboard_hidden = SDL_EVENT_SCREEN_KEYBOARD_HIDDEN,

		// --- Mouse ---
		mouse_motion = SDL_EVENT_MOUSE_MOTION,
		mouse_button_down = SDL_EVENT_MOUSE_BUTTON_DOWN,
		mouse_button_up = SDL_EVENT_MOUSE_BUTTON_UP,
		mouse_wheel = SDL_EVENT_MOUSE_WHEEL,
		mouse_added = SDL_EVENT_MOUSE_ADDED,
		mouse_removed = SDL_EVENT_MOUSE_REMOVED,

		// --- Joystick ---
		joystick_axis_motion = SDL_EVENT_JOYSTICK_AXIS_MOTION,
		joystick_ball_motion = SDL_EVENT_JOYSTICK_BALL_MOTION,
		joystick_hat_motion = SDL_EVENT_JOYSTICK_HAT_MOTION,
		joystick_button_down = SDL_EVENT_JOYSTICK_BUTTON_DOWN,
		joystick_button_up = SDL_EVENT_JOYSTICK_BUTTON_UP,
		joystick_added = SDL_EVENT_JOYSTICK_ADDED,
		joystick_removed = SDL_EVENT_JOYSTICK_REMOVED,
		joystick_battery_updated = SDL_EVENT_JOYSTICK_BATTERY_UPDATED,
		joystick_update_complete = SDL_EVENT_JOYSTICK_UPDATE_COMPLETE,

		// --- Gamepad ---
		gamepad_axis_motion = SDL_EVENT_GAMEPAD_AXIS_MOTION,
		gamepad_button_down = SDL_EVENT_GAMEPAD_BUTTON_DOWN,
		gamepad_button_up = SDL_EVENT_GAMEPAD_BUTTON_UP,
		gamepad_added = SDL_EVENT_GAMEPAD_ADDED,
		gamepad_removed = SDL_EVENT_GAMEPAD_REMOVED,
		gamepad_remapped = SDL_EVENT_GAMEPAD_REMAPPED,
		gamepad_touchpad_down = SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN,
		gamepad_touchpad_motion = SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION,
		gamepad_touchpad_up = SDL_EVENT_GAMEPAD_TOUCHPAD_UP,
		gamepad_sensor_update = SDL_EVENT_GAMEPAD_SENSOR_UPDATE,
		gamepad_update_complete = SDL_EVENT_GAMEPAD_UPDATE_COMPLETE,
		gamepad_steam_handle_updated = SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED,

		// --- Touch ---
		finger_down = SDL_EVENT_FINGER_DOWN,
		finger_up = SDL_EVENT_FINGER_UP,
		finger_motion = SDL_EVENT_FINGER_MOTION,
		finger_canceled = SDL_EVENT_FINGER_CANCELED,

		// --- Gesture (pinch) ---
		pinch_begin = SDL_EVENT_PINCH_BEGIN,
		pinch_update = SDL_EVENT_PINCH_UPDATE,
		pinch_end = SDL_EVENT_PINCH_END,

		// --- Clipboard ---
		clipboard_update = SDL_EVENT_CLIPBOARD_UPDATE,

		// --- Drag & Drop ---
		drop_file = SDL_EVENT_DROP_FILE,
		drop_text = SDL_EVENT_DROP_TEXT,
		drop_begin = SDL_EVENT_DROP_BEGIN,
		drop_complete = SDL_EVENT_DROP_COMPLETE,
		drop_position = SDL_EVENT_DROP_POSITION,

		// --- Audio ---
#if !defined(MNGL_CUSTOM_AUDIO_BACKEND)
		audio_device_added = SDL_EVENT_AUDIO_DEVICE_ADDED,
		audio_device_removed = SDL_EVENT_AUDIO_DEVICE_REMOVED,
		audio_device_format_changed = SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED,
#endif

		// --- Sensor ---
		sensor_update = SDL_EVENT_SENSOR_UPDATE,

		// --- Pen ---
		pen_proximity_in = SDL_EVENT_PEN_PROXIMITY_IN,
		pen_proximity_out = SDL_EVENT_PEN_PROXIMITY_OUT,
		pen_down = SDL_EVENT_PEN_DOWN,
		pen_up = SDL_EVENT_PEN_UP,
		pen_button_down = SDL_EVENT_PEN_BUTTON_DOWN,
		pen_button_up = SDL_EVENT_PEN_BUTTON_UP,
		pen_motion = SDL_EVENT_PEN_MOTION,
		pen_axis = SDL_EVENT_PEN_AXIS,

		// --- Camera ---
		camera_device_added = SDL_EVENT_CAMERA_DEVICE_ADDED,
		camera_device_removed = SDL_EVENT_CAMERA_DEVICE_REMOVED,
		camera_device_approved = SDL_EVENT_CAMERA_DEVICE_APPROVED,
		camera_device_denied = SDL_EVENT_CAMERA_DEVICE_DENIED,

		// --- Render ---
		render_targets_reset = SDL_EVENT_RENDER_TARGETS_RESET,
		render_device_reset = SDL_EVENT_RENDER_DEVICE_RESET,
		render_device_lost = SDL_EVENT_RENDER_DEVICE_LOST,

		// --- Internal / misc ---
		poll_sentinel = SDL_EVENT_POLL_SENTINEL,

		// --- User ---
		user = SDL_EVENT_USER,
	};

	enum class hat_position : std::uint8_t
	{
		centered = SDL_HAT_CENTERED,
		up = SDL_HAT_UP,
		right = SDL_HAT_RIGHT,
		rightup = SDL_HAT_RIGHTUP,
		down = SDL_HAT_DOWN,
		rightdown = SDL_HAT_RIGHTDOWN,
		left = SDL_HAT_LEFT,
		leftup = SDL_HAT_LEFTUP,
		leftdown = SDL_HAT_LEFTDOWN,
		none = 0xFF
	};
	enum class power_state : std::uint8_t
	{
		unknown,
		on_battery,
		no_battery,
		charging,
		charged,
		error = 0xFF
	};
	enum class wheel_direction : std::uint8_t
	{
		normal,
		flipped
	};
	enum class drop_type : std::uint8_t
	{
		none,
		file,
		text,
		begin,
		complete,
		position
	};
	enum class camera_status : std::uint8_t
	{
		added,
		removed,
		approved,
		denied
	};
	enum class render_reason : std::uint8_t
	{
		targets_reset,
		device_reset,
		device_lost
	};
	enum class pen_axis_type : std::uint8_t
	{
		pressure = SDL_PEN_AXIS_PRESSURE,
		xtilt = SDL_PEN_AXIS_XTILT,
		ytilt = SDL_PEN_AXIS_YTILT,
		distance = SDL_PEN_AXIS_DISTANCE,
		rotation = SDL_PEN_AXIS_ROTATION,
		slider = SDL_PEN_AXIS_SLIDER,
		tangential_pressure = SDL_PEN_AXIS_TANGENTIAL_PRESSURE
	};
	enum class gamepad_axis : std::int8_t
	{
		invalid = SDL_GAMEPAD_AXIS_INVALID,
		leftx = SDL_GAMEPAD_AXIS_LEFTX,
		lefty = SDL_GAMEPAD_AXIS_LEFTY,
		rightx = SDL_GAMEPAD_AXIS_RIGHTX,
		righty = SDL_GAMEPAD_AXIS_RIGHTY,
		lefttrigger = SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
		righttrigger = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
	};
	enum class gamepad_button : std::int8_t
	{
		invalid = SDL_GAMEPAD_BUTTON_INVALID,
		a = SDL_GAMEPAD_BUTTON_SOUTH,
		b = SDL_GAMEPAD_BUTTON_EAST,
		x = SDL_GAMEPAD_BUTTON_WEST,
		y = SDL_GAMEPAD_BUTTON_NORTH,
		back = SDL_GAMEPAD_BUTTON_BACK,
		guide = SDL_GAMEPAD_BUTTON_GUIDE,
		start = SDL_GAMEPAD_BUTTON_START,
		leftstick = SDL_GAMEPAD_BUTTON_LEFT_STICK,
		rightstick = SDL_GAMEPAD_BUTTON_RIGHT_STICK,
		leftshoulder = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
		rightshoulder = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
		dpad_up = SDL_GAMEPAD_BUTTON_DPAD_UP,
		dpad_down = SDL_GAMEPAD_BUTTON_DPAD_DOWN,
		dpad_left = SDL_GAMEPAD_BUTTON_DPAD_LEFT,
		dpad_right = SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
		misc1 = SDL_GAMEPAD_BUTTON_MISC1,
		right_paddle1 = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,
		left_paddle1 = SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
		right_paddle2 = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,
		left_paddle2 = SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
		touchpad = SDL_GAMEPAD_BUTTON_TOUCHPAD,
		misc2 = SDL_GAMEPAD_BUTTON_MISC2,
		misc3 = SDL_GAMEPAD_BUTTON_MISC3,
		misc4 = SDL_GAMEPAD_BUTTON_MISC4,
		misc5 = SDL_GAMEPAD_BUTTON_MISC5,
		misc6 = SDL_GAMEPAD_BUTTON_MISC6,
		count = SDL_GAMEPAD_BUTTON_COUNT,
		south = a,
		east = b,
		west = x,
		north = y
	};
	enum class sensor_type : std::int8_t
	{
		invalid = SDL_SENSOR_INVALID,
		unknown = SDL_SENSOR_UNKNOWN,
		accelerometer = SDL_SENSOR_ACCEL,
		gyroscope = SDL_SENSOR_GYRO,
		accelerometer_left = SDL_SENSOR_ACCEL_L,
		gyroscope_left = SDL_SENSOR_GYRO_L,
		accelerometer_right = SDL_SENSOR_ACCEL_R,
		gyroscope_right = SDL_SENSOR_GYRO_R
	};
	enum class display_orientation
	{
		unknown = SDL_ORIENTATION_UNKNOWN,
		landscape = SDL_ORIENTATION_LANDSCAPE,
		landscape_flipped = SDL_ORIENTATION_LANDSCAPE_FLIPPED,
		portrait = SDL_ORIENTATION_PORTRAIT,
		portrait_flipped = SDL_ORIENTATION_PORTRAIT_FLIPPED
	};
	enum class keymod : std::uint16_t
	{
		none = SDL_KMOD_NONE,
		lshift = SDL_KMOD_LSHIFT,
		rshift = SDL_KMOD_RSHIFT,
		lctrl = SDL_KMOD_LCTRL,
		rctrl = SDL_KMOD_RCTRL,
		lalt = SDL_KMOD_LALT,
		ralt = SDL_KMOD_RALT,
		lgui = SDL_KMOD_LGUI,
		rgui = SDL_KMOD_RGUI,
		num = SDL_KMOD_NUM,
		caps = SDL_KMOD_CAPS,
		mode = SDL_KMOD_MODE,
		scroll = SDL_KMOD_SCROLL,

		shift = SDL_KMOD_SHIFT,
		ctrl = SDL_KMOD_CTRL,
		alt = SDL_KMOD_ALT,
		gui = SDL_KMOD_GUI,
	};

#define MNGL_DETAIL_SCANCODES                                                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(a, SDL_SCANCODE_A)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(b, SDL_SCANCODE_B)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(c, SDL_SCANCODE_C)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(d, SDL_SCANCODE_D)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(e, SDL_SCANCODE_E)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(f, SDL_SCANCODE_F)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(g, SDL_SCANCODE_G)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(h, SDL_SCANCODE_H)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(i, SDL_SCANCODE_I)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(j, SDL_SCANCODE_J)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(k, SDL_SCANCODE_K)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(l, SDL_SCANCODE_L)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(m, SDL_SCANCODE_M)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(n, SDL_SCANCODE_N)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(o, SDL_SCANCODE_O)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(p, SDL_SCANCODE_P)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(q, SDL_SCANCODE_Q)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(r, SDL_SCANCODE_R)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(s, SDL_SCANCODE_S)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(t, SDL_SCANCODE_T)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(u, SDL_SCANCODE_U)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(v, SDL_SCANCODE_V)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(w, SDL_SCANCODE_W)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(x, SDL_SCANCODE_X)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(y, SDL_SCANCODE_Y)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(z, SDL_SCANCODE_Z)                                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(num1, SDL_SCANCODE_1)                                                         \
	MNGL_DETAIL_SCANCODE_ENTRY(num2, SDL_SCANCODE_2)                                                         \
	MNGL_DETAIL_SCANCODE_ENTRY(num3, SDL_SCANCODE_3)                                                         \
	MNGL_DETAIL_SCANCODE_ENTRY(num4, SDL_SCANCODE_4)                                                         \
	MNGL_DETAIL_SCANCODE_ENTRY(num5, SDL_SCANCODE_5)                                                         \
	MNGL_DETAIL_SCANCODE_ENTRY(num6, SDL_SCANCODE_6)                                                         \
	MNGL_DETAIL_SCANCODE_ENTRY(num7, SDL_SCANCODE_7)                                                         \
	MNGL_DETAIL_SCANCODE_ENTRY(num8, SDL_SCANCODE_8)                                                         \
	MNGL_DETAIL_SCANCODE_ENTRY(num9, SDL_SCANCODE_9)                                                         \
	MNGL_DETAIL_SCANCODE_ENTRY(num0, SDL_SCANCODE_0)                                                         \
	MNGL_DETAIL_SCANCODE_ENTRY(return_key, SDL_SCANCODE_RETURN)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(escape, SDL_SCANCODE_ESCAPE)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(backspace, SDL_SCANCODE_BACKSPACE)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(tab, SDL_SCANCODE_TAB)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(space, SDL_SCANCODE_SPACE)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(minus, SDL_SCANCODE_MINUS)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(equals, SDL_SCANCODE_EQUALS)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(leftbracket, SDL_SCANCODE_LEFTBRACKET)                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(rightbracket, SDL_SCANCODE_RIGHTBRACKET)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(backslash, SDL_SCANCODE_BACKSLASH)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(nonushash, SDL_SCANCODE_NONUSHASH)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(semicolon, SDL_SCANCODE_SEMICOLON)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(apostrophe, SDL_SCANCODE_APOSTROPHE)                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(grave, SDL_SCANCODE_GRAVE)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(comma, SDL_SCANCODE_COMMA)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(period, SDL_SCANCODE_PERIOD)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(slash, SDL_SCANCODE_SLASH)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(capslock, SDL_SCANCODE_CAPSLOCK)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(f1, SDL_SCANCODE_F1)                                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(f2, SDL_SCANCODE_F2)                                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(f3, SDL_SCANCODE_F3)                                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(f4, SDL_SCANCODE_F4)                                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(f5, SDL_SCANCODE_F5)                                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(f6, SDL_SCANCODE_F6)                                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(f7, SDL_SCANCODE_F7)                                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(f8, SDL_SCANCODE_F8)                                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(f9, SDL_SCANCODE_F9)                                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(f10, SDL_SCANCODE_F10)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f11, SDL_SCANCODE_F11)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f12, SDL_SCANCODE_F12)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(printscreen, SDL_SCANCODE_PRINTSCREEN)                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(scrolllock, SDL_SCANCODE_SCROLLLOCK)                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(pause, SDL_SCANCODE_PAUSE)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(insert, SDL_SCANCODE_INSERT)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(home, SDL_SCANCODE_HOME)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(pageup, SDL_SCANCODE_PAGEUP)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(delete_key, SDL_SCANCODE_DELETE)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(end, SDL_SCANCODE_END)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(pagedown, SDL_SCANCODE_PAGEDOWN)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(right, SDL_SCANCODE_RIGHT)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(left, SDL_SCANCODE_LEFT)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(down, SDL_SCANCODE_DOWN)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(up, SDL_SCANCODE_UP)                                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(numlockclear, SDL_SCANCODE_NUMLOCKCLEAR)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_divide, SDL_SCANCODE_KP_DIVIDE)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_multiply, SDL_SCANCODE_KP_MULTIPLY)                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_minus, SDL_SCANCODE_KP_MINUS)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_plus, SDL_SCANCODE_KP_PLUS)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_enter, SDL_SCANCODE_KP_ENTER)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_1, SDL_SCANCODE_KP_1)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_2, SDL_SCANCODE_KP_2)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_3, SDL_SCANCODE_KP_3)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_4, SDL_SCANCODE_KP_4)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_5, SDL_SCANCODE_KP_5)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_6, SDL_SCANCODE_KP_6)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_7, SDL_SCANCODE_KP_7)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_8, SDL_SCANCODE_KP_8)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_9, SDL_SCANCODE_KP_9)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_0, SDL_SCANCODE_KP_0)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_period, SDL_SCANCODE_KP_PERIOD)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(nonusbackslash, SDL_SCANCODE_NONUSBACKSLASH)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(application, SDL_SCANCODE_APPLICATION)                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(power, SDL_SCANCODE_POWER)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_equals, SDL_SCANCODE_KP_EQUALS)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(f13, SDL_SCANCODE_F13)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f14, SDL_SCANCODE_F14)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f15, SDL_SCANCODE_F15)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f16, SDL_SCANCODE_F16)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f17, SDL_SCANCODE_F17)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f18, SDL_SCANCODE_F18)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f19, SDL_SCANCODE_F19)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f20, SDL_SCANCODE_F20)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f21, SDL_SCANCODE_F21)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f22, SDL_SCANCODE_F22)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f23, SDL_SCANCODE_F23)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(f24, SDL_SCANCODE_F24)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(execute, SDL_SCANCODE_EXECUTE)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(help, SDL_SCANCODE_HELP)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(menu, SDL_SCANCODE_MENU)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(select, SDL_SCANCODE_SELECT)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(stop, SDL_SCANCODE_STOP)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(again, SDL_SCANCODE_AGAIN)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(undo, SDL_SCANCODE_UNDO)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(cut, SDL_SCANCODE_CUT)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(copy, SDL_SCANCODE_COPY)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(paste, SDL_SCANCODE_PASTE)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(find, SDL_SCANCODE_FIND)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(mute, SDL_SCANCODE_MUTE)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(volumeup, SDL_SCANCODE_VOLUMEUP)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(volumedown, SDL_SCANCODE_VOLUMEDOWN)                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_comma, SDL_SCANCODE_KP_COMMA)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_equalsas400, SDL_SCANCODE_KP_EQUALSAS400)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(international1, SDL_SCANCODE_INTERNATIONAL1)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(international2, SDL_SCANCODE_INTERNATIONAL2)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(international3, SDL_SCANCODE_INTERNATIONAL3)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(international4, SDL_SCANCODE_INTERNATIONAL4)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(international5, SDL_SCANCODE_INTERNATIONAL5)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(international6, SDL_SCANCODE_INTERNATIONAL6)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(international7, SDL_SCANCODE_INTERNATIONAL7)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(international8, SDL_SCANCODE_INTERNATIONAL8)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(international9, SDL_SCANCODE_INTERNATIONAL9)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(lang1, SDL_SCANCODE_LANG1)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(lang2, SDL_SCANCODE_LANG2)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(lang3, SDL_SCANCODE_LANG3)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(lang4, SDL_SCANCODE_LANG4)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(lang5, SDL_SCANCODE_LANG5)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(lang6, SDL_SCANCODE_LANG6)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(lang7, SDL_SCANCODE_LANG7)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(lang8, SDL_SCANCODE_LANG8)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(lang9, SDL_SCANCODE_LANG9)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(alterase, SDL_SCANCODE_ALTERASE)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(sysreq, SDL_SCANCODE_SYSREQ)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(cancel, SDL_SCANCODE_CANCEL)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(clear, SDL_SCANCODE_CLEAR)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(prior, SDL_SCANCODE_PRIOR)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(return2, SDL_SCANCODE_RETURN2)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(separator, SDL_SCANCODE_SEPARATOR)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(out, SDL_SCANCODE_OUT)                                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(oper, SDL_SCANCODE_OPER)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(clearagain, SDL_SCANCODE_CLEARAGAIN)                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(crsel, SDL_SCANCODE_CRSEL)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(exsel, SDL_SCANCODE_EXSEL)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_00, SDL_SCANCODE_KP_00)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_000, SDL_SCANCODE_KP_000)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(thousandsseparator, SDL_SCANCODE_THOUSANDSSEPARATOR)                          \
	MNGL_DETAIL_SCANCODE_ENTRY(decimalseparator, SDL_SCANCODE_DECIMALSEPARATOR)                              \
	MNGL_DETAIL_SCANCODE_ENTRY(currencyunit, SDL_SCANCODE_CURRENCYUNIT)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(currencysubunit, SDL_SCANCODE_CURRENCYSUBUNIT)                                \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_leftparen, SDL_SCANCODE_KP_LEFTPAREN)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_rightparen, SDL_SCANCODE_KP_RIGHTPAREN)                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_leftbrace, SDL_SCANCODE_KP_LEFTBRACE)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_rightbrace, SDL_SCANCODE_KP_RIGHTBRACE)                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_tab, SDL_SCANCODE_KP_TAB)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_backspace, SDL_SCANCODE_KP_BACKSPACE)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_a, SDL_SCANCODE_KP_A)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_b, SDL_SCANCODE_KP_B)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_c, SDL_SCANCODE_KP_C)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_d, SDL_SCANCODE_KP_D)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_e, SDL_SCANCODE_KP_E)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_f, SDL_SCANCODE_KP_F)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_xor, SDL_SCANCODE_KP_XOR)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_power, SDL_SCANCODE_KP_POWER)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_percent, SDL_SCANCODE_KP_PERCENT)                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_less, SDL_SCANCODE_KP_LESS)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_greater, SDL_SCANCODE_KP_GREATER)                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_ampersand, SDL_SCANCODE_KP_AMPERSAND)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_dblampersand, SDL_SCANCODE_KP_DBLAMPERSAND)                                \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_verticalbar, SDL_SCANCODE_KP_VERTICALBAR)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_dblverticalbar, SDL_SCANCODE_KP_DBLVERTICALBAR)                            \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_colon, SDL_SCANCODE_KP_COLON)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_hash, SDL_SCANCODE_KP_HASH)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_space, SDL_SCANCODE_KP_SPACE)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_at, SDL_SCANCODE_KP_AT)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_exclam, SDL_SCANCODE_KP_EXCLAM)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_memstore, SDL_SCANCODE_KP_MEMSTORE)                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_memrecall, SDL_SCANCODE_KP_MEMRECALL)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_memclear, SDL_SCANCODE_KP_MEMCLEAR)                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_memadd, SDL_SCANCODE_KP_MEMADD)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_memsubtract, SDL_SCANCODE_KP_MEMSUBTRACT)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_memmultiply, SDL_SCANCODE_KP_MEMMULTIPLY)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_memdivide, SDL_SCANCODE_KP_MEMDIVIDE)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_plusminus, SDL_SCANCODE_KP_PLUSMINUS)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_clear, SDL_SCANCODE_KP_CLEAR)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_clearentry, SDL_SCANCODE_KP_CLEARENTRY)                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_binary, SDL_SCANCODE_KP_BINARY)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_octal, SDL_SCANCODE_KP_OCTAL)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_decimal, SDL_SCANCODE_KP_DECIMAL)                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(kp_hexadecimal, SDL_SCANCODE_KP_HEXADECIMAL)                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(lctrl, SDL_SCANCODE_LCTRL)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(lshift, SDL_SCANCODE_LSHIFT)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(lalt, SDL_SCANCODE_LALT)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(lgui, SDL_SCANCODE_LGUI)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(rctrl, SDL_SCANCODE_RCTRL)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(rshift, SDL_SCANCODE_RSHIFT)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(ralt, SDL_SCANCODE_RALT)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(rgui, SDL_SCANCODE_RGUI)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(mode, SDL_SCANCODE_MODE)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(sleep, SDL_SCANCODE_SLEEP)                                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(wake, SDL_SCANCODE_WAKE)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(channel_increment, SDL_SCANCODE_CHANNEL_INCREMENT)                            \
	MNGL_DETAIL_SCANCODE_ENTRY(channel_decrement, SDL_SCANCODE_CHANNEL_DECREMENT)                            \
	MNGL_DETAIL_SCANCODE_ENTRY(media_play, SDL_SCANCODE_MEDIA_PLAY)                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(media_pause, SDL_SCANCODE_MEDIA_PAUSE)                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(media_record, SDL_SCANCODE_MEDIA_RECORD)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(media_fast_forward, SDL_SCANCODE_MEDIA_FAST_FORWARD)                          \
	MNGL_DETAIL_SCANCODE_ENTRY(media_rewind, SDL_SCANCODE_MEDIA_REWIND)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(media_next_track, SDL_SCANCODE_MEDIA_NEXT_TRACK)                              \
	MNGL_DETAIL_SCANCODE_ENTRY(media_previous_track, SDL_SCANCODE_MEDIA_PREVIOUS_TRACK)                      \
	MNGL_DETAIL_SCANCODE_ENTRY(media_stop, SDL_SCANCODE_MEDIA_STOP)                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(media_eject, SDL_SCANCODE_MEDIA_EJECT)                                        \
	MNGL_DETAIL_SCANCODE_ENTRY(media_play_pause, SDL_SCANCODE_MEDIA_PLAY_PAUSE)                              \
	MNGL_DETAIL_SCANCODE_ENTRY(media_select, SDL_SCANCODE_MEDIA_SELECT)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_new, SDL_SCANCODE_AC_NEW)                                                  \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_open, SDL_SCANCODE_AC_OPEN)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_close, SDL_SCANCODE_AC_CLOSE)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_exit, SDL_SCANCODE_AC_EXIT)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_save, SDL_SCANCODE_AC_SAVE)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_print, SDL_SCANCODE_AC_PRINT)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_properties, SDL_SCANCODE_AC_PROPERTIES)                                    \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_search, SDL_SCANCODE_AC_SEARCH)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_home, SDL_SCANCODE_AC_HOME)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_back, SDL_SCANCODE_AC_BACK)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_forward, SDL_SCANCODE_AC_FORWARD)                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_stop, SDL_SCANCODE_AC_STOP)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_refresh, SDL_SCANCODE_AC_REFRESH)                                          \
	MNGL_DETAIL_SCANCODE_ENTRY(ac_bookmarks, SDL_SCANCODE_AC_BOOKMARKS)                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(softleft, SDL_SCANCODE_SOFTLEFT)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(softright, SDL_SCANCODE_SOFTRIGHT)                                            \
	MNGL_DETAIL_SCANCODE_ENTRY(call, SDL_SCANCODE_CALL)                                                      \
	MNGL_DETAIL_SCANCODE_ENTRY(endcall, SDL_SCANCODE_ENDCALL)                                                \
	MNGL_DETAIL_SCANCODE_ENTRY(reserved, SDL_SCANCODE_RESERVED)                                              \
	MNGL_DETAIL_SCANCODE_ENTRY(count, SDL_SCANCODE_COUNT)

	enum class scancode : std::uint16_t
	{
#define MNGL_DETAIL_SCANCODE_ENTRY(a, b) a = b,
		MNGL_DETAIL_SCANCODES
#undef MNGL_DETAIL_SCANCODE_ENTRY
		    unknown = SDL_SCANCODE_UNKNOWN
	};

	inline std::string to_string(scancode what)
	{
		switch (what)
		{
#define MNGL_DETAIL_SCANCODE_ENTRY(a, b)                                                                     \
	case scancode::a:                                                                                        \
		return #a;
			MNGL_DETAIL_SCANCODES
#undef MNGL_DETAIL_SCANCODE_ENTRY
		case scancode::unknown:
			return "unknown";
		[[unlikely]] default:
			return "invalid";
		}
	}

#undef MNGL_DETAIL_SCANCODES

	// =========================================================================
	// БАЗОВЫЕ СТРУКТУРЫ
	// =========================================================================

} // namespace mngl
