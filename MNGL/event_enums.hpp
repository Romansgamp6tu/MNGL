#pragma once

#include "utilities.hpp"

namespace mngl
{
    enum class Events
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
        audio_device_added = SDL_EVENT_AUDIO_DEVICE_ADDED,
        audio_device_removed = SDL_EVENT_AUDIO_DEVICE_REMOVED,
        audio_device_format_changed = SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED,

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

    enum class hat_position : uint8_t { centered, up, leftup, left, leftdown, down, rightdown, right, rightup, none = 0xFF };
    enum class power_state : uint8_t { unknown, on_battery, no_battery, charging, charged, error = 0xFF };
    enum class wheel_direction : uint8_t { normal, flipped };
    enum class drop_type : uint8_t { none, file, text, begin, complete, position };
    enum class camera_status : uint8_t { added, removed, approved, denied };
    enum class render_reason : uint8_t { targets_reset, device_reset, device_lost };
    enum class pen_axis_type : uint8_t { pressure, xtilt, ytilt, rotation, slider, distance };
    enum class gamepad_axis : uint8_t { invalid, leftx, lefty, rightx, righty, lefttrigger, righttrigger };
    enum class gamepad_button : uint8_t
    {
        invalid, a, b, x, y, back, guide, start,
        leftstick, rightstick, leftshoulder, rightshoulder,
        dpad_up, dpad_down, dpad_left, dpad_right, misc1, misc2
    };
    enum class sensor_type : uint8_t { invalid, accelerometer, gyroscope, accelerometer_uncalibrated, gyroscope_uncalibrated };
    enum class display_orientation
    {
        unknown = SDL_ORIENTATION_UNKNOWN,
        landscape = SDL_ORIENTATION_LANDSCAPE,
        landscape_flipped = SDL_ORIENTATION_LANDSCAPE_FLIPPED,
        portrait = SDL_ORIENTATION_PORTRAIT,
        portrait_flipped = SDL_ORIENTATION_PORTRAIT_FLIPPED
    };
    enum class keymod : uint16_t
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

#define KEYS_SCANCODE_X_MACROS_DATA \
X(a, SDL_SCANCODE_A)\
    X(b, SDL_SCANCODE_B)\
        X(c, SDL_SCANCODE_C)\
        X(d, SDL_SCANCODE_D)\
        X(e, SDL_SCANCODE_E)\
        X(f, SDL_SCANCODE_F)\
        X(g, SDL_SCANCODE_G)\
        X(h, SDL_SCANCODE_H)\
        X(i, SDL_SCANCODE_I)\
        X(j, SDL_SCANCODE_J)\
        X(k, SDL_SCANCODE_K)\
        X(l, SDL_SCANCODE_L)\
        X(m, SDL_SCANCODE_M)\
        X(n, SDL_SCANCODE_N)\
        X(o, SDL_SCANCODE_O)\
        X(p, SDL_SCANCODE_P)\
        X(q, SDL_SCANCODE_Q)\
        X(r, SDL_SCANCODE_R)\
        X(s, SDL_SCANCODE_S)\
        X(t, SDL_SCANCODE_T)\
        X(u, SDL_SCANCODE_U)\
        X(v, SDL_SCANCODE_V)\
        X(w, SDL_SCANCODE_W)\
        X(x, SDL_SCANCODE_X)\
        X(y, SDL_SCANCODE_Y)\
        X(z, SDL_SCANCODE_Z)\
        X(num1, SDL_SCANCODE_1)\
        X(num2, SDL_SCANCODE_2)\
        X(num3, SDL_SCANCODE_3)\
        X(num4, SDL_SCANCODE_4)\
        X(num5, SDL_SCANCODE_5)\
        X(num6, SDL_SCANCODE_6)\
        X(num7, SDL_SCANCODE_7)\
        X(num8, SDL_SCANCODE_8)\
        X(num9, SDL_SCANCODE_9)\
        X(num0, SDL_SCANCODE_0)\
        X(return_key, SDL_SCANCODE_RETURN)\
        X(escape, SDL_SCANCODE_ESCAPE)\
        X(backspace, SDL_SCANCODE_BACKSPACE)\
        X(tab, SDL_SCANCODE_TAB)\
        X(space, SDL_SCANCODE_SPACE)\
        X(minus, SDL_SCANCODE_MINUS)\
        X(equals, SDL_SCANCODE_EQUALS)\
        X(leftbracket, SDL_SCANCODE_LEFTBRACKET)\
        X(rightbracket, SDL_SCANCODE_RIGHTBRACKET)\
        X(backslash, SDL_SCANCODE_BACKSLASH)\
        X(nonushash, SDL_SCANCODE_NONUSHASH)\
        X(semicolon, SDL_SCANCODE_SEMICOLON)\
        X(apostrophe, SDL_SCANCODE_APOSTROPHE)\
        X(grave, SDL_SCANCODE_GRAVE)\
        X(comma, SDL_SCANCODE_COMMA)\
        X(period, SDL_SCANCODE_PERIOD)\
        X(slash, SDL_SCANCODE_SLASH)\
        X(capslock, SDL_SCANCODE_CAPSLOCK)\
        X(f1, SDL_SCANCODE_F1)\
        X(f2, SDL_SCANCODE_F2)\
        X(f3, SDL_SCANCODE_F3)\
        X(f4, SDL_SCANCODE_F4)\
        X(f5, SDL_SCANCODE_F5)\
        X(f6, SDL_SCANCODE_F6)\
        X(f7, SDL_SCANCODE_F7)\
        X(f8, SDL_SCANCODE_F8)\
        X(f9, SDL_SCANCODE_F9)\
        X(f10, SDL_SCANCODE_F10)\
        X(f11, SDL_SCANCODE_F11)\
        X(f12, SDL_SCANCODE_F12)\
        X(printscreen, SDL_SCANCODE_PRINTSCREEN)\
        X(scrolllock, SDL_SCANCODE_SCROLLLOCK)\
        X(pause, SDL_SCANCODE_PAUSE)\
        X(insert, SDL_SCANCODE_INSERT)\
        X(home, SDL_SCANCODE_HOME)\
        X(pageup, SDL_SCANCODE_PAGEUP)\
        X(delete_key, SDL_SCANCODE_DELETE)\
        X(end, SDL_SCANCODE_END)\
        X(pagedown, SDL_SCANCODE_PAGEDOWN)\
        X(right, SDL_SCANCODE_RIGHT)\
        X(left, SDL_SCANCODE_LEFT)\
        X(down, SDL_SCANCODE_DOWN)\
        X(up, SDL_SCANCODE_UP)\
        X(numlockclear, SDL_SCANCODE_NUMLOCKCLEAR)\
        X(kp_divide, SDL_SCANCODE_KP_DIVIDE)\
        X(kp_multiply, SDL_SCANCODE_KP_MULTIPLY)\
        X(kp_minus, SDL_SCANCODE_KP_MINUS)\
        X(kp_plus, SDL_SCANCODE_KP_PLUS)\
        X(kp_enter, SDL_SCANCODE_KP_ENTER)\
        X(kp_1, SDL_SCANCODE_KP_1)\
        X(kp_2, SDL_SCANCODE_KP_2)\
        X(kp_3, SDL_SCANCODE_KP_3)\
        X(kp_4, SDL_SCANCODE_KP_4)\
        X(kp_5, SDL_SCANCODE_KP_5)\
        X(kp_6, SDL_SCANCODE_KP_6)\
        X(kp_7, SDL_SCANCODE_KP_7)\
        X(kp_8, SDL_SCANCODE_KP_8)\
        X(kp_9, SDL_SCANCODE_KP_9)\
        X(kp_0, SDL_SCANCODE_KP_0)\
        X(kp_period, SDL_SCANCODE_KP_PERIOD)\
        X(nonusbackslash, SDL_SCANCODE_NONUSBACKSLASH)\
        X(application, SDL_SCANCODE_APPLICATION)\
        X(power, SDL_SCANCODE_POWER)\
        X(kp_equals, SDL_SCANCODE_KP_EQUALS)\
        X(f13, SDL_SCANCODE_F13)\
        X(f14, SDL_SCANCODE_F14)\
        X(f15, SDL_SCANCODE_F15)\
        X(f16, SDL_SCANCODE_F16)\
        X(f17, SDL_SCANCODE_F17)\
        X(f18, SDL_SCANCODE_F18)\
        X(f19, SDL_SCANCODE_F19)\
        X(f20, SDL_SCANCODE_F20)\
        X(f21, SDL_SCANCODE_F21)\
        X(f22, SDL_SCANCODE_F22)\
        X(f23, SDL_SCANCODE_F23)\
        X(f24, SDL_SCANCODE_F24)\
        X(execute, SDL_SCANCODE_EXECUTE)\
        X(help, SDL_SCANCODE_HELP)\
        X(menu, SDL_SCANCODE_MENU)\
        X(select, SDL_SCANCODE_SELECT)\
        X(stop, SDL_SCANCODE_STOP)\
        X(again, SDL_SCANCODE_AGAIN)\
        X(undo, SDL_SCANCODE_UNDO)\
        X(cut, SDL_SCANCODE_CUT)\
        X(copy, SDL_SCANCODE_COPY)\
        X(paste, SDL_SCANCODE_PASTE)\
        X(find, SDL_SCANCODE_FIND)\
        X(mute, SDL_SCANCODE_MUTE)\
        X(volumeup, SDL_SCANCODE_VOLUMEUP)\
        X(volumedown, SDL_SCANCODE_VOLUMEDOWN)\
        X(kp_comma, SDL_SCANCODE_KP_COMMA)\
        X(kp_equalsas400, SDL_SCANCODE_KP_EQUALSAS400)\
        X(international1, SDL_SCANCODE_INTERNATIONAL1)\
        X(international2, SDL_SCANCODE_INTERNATIONAL2)\
        X(international3, SDL_SCANCODE_INTERNATIONAL3)\
        X(international4, SDL_SCANCODE_INTERNATIONAL4)\
        X(international5, SDL_SCANCODE_INTERNATIONAL5)\
        X(international6, SDL_SCANCODE_INTERNATIONAL6)\
        X(international7, SDL_SCANCODE_INTERNATIONAL7)\
        X(international8, SDL_SCANCODE_INTERNATIONAL8)\
        X(international9, SDL_SCANCODE_INTERNATIONAL9)\
        X(lang1, SDL_SCANCODE_LANG1)\
        X(lang2, SDL_SCANCODE_LANG2)\
        X(lang3, SDL_SCANCODE_LANG3)\
        X(lang4, SDL_SCANCODE_LANG4)\
        X(lang5, SDL_SCANCODE_LANG5)\
        X(lang6, SDL_SCANCODE_LANG6)\
        X(lang7, SDL_SCANCODE_LANG7)\
        X(lang8, SDL_SCANCODE_LANG8)\
        X(lang9, SDL_SCANCODE_LANG9)\
        X(alterase, SDL_SCANCODE_ALTERASE)\
        X(sysreq, SDL_SCANCODE_SYSREQ)\
        X(cancel, SDL_SCANCODE_CANCEL)\
        X(clear, SDL_SCANCODE_CLEAR)\
        X(prior, SDL_SCANCODE_PRIOR)\
        X(return2, SDL_SCANCODE_RETURN2)\
        X(separator, SDL_SCANCODE_SEPARATOR)\
        X(out, SDL_SCANCODE_OUT)\
        X(oper, SDL_SCANCODE_OPER)\
        X(clearagain, SDL_SCANCODE_CLEARAGAIN)\
        X(crsel, SDL_SCANCODE_CRSEL)\
        X(exsel, SDL_SCANCODE_EXSEL)\
        X(kp_00, SDL_SCANCODE_KP_00)\
        X(kp_000, SDL_SCANCODE_KP_000)\
        X(thousandsseparator, SDL_SCANCODE_THOUSANDSSEPARATOR)\
        X(decimalseparator, SDL_SCANCODE_DECIMALSEPARATOR)\
        X(currencyunit, SDL_SCANCODE_CURRENCYUNIT)\
        X(currencysubunit, SDL_SCANCODE_CURRENCYSUBUNIT)\
        X(kp_leftparen, SDL_SCANCODE_KP_LEFTPAREN)\
        X(kp_rightparen, SDL_SCANCODE_KP_RIGHTPAREN)\
        X(kp_leftbrace, SDL_SCANCODE_KP_LEFTBRACE)\
        X(kp_rightbrace, SDL_SCANCODE_KP_RIGHTBRACE)\
        X(kp_tab, SDL_SCANCODE_KP_TAB)\
        X(kp_backspace, SDL_SCANCODE_KP_BACKSPACE)\
        X(kp_a, SDL_SCANCODE_KP_A)\
        X(kp_b, SDL_SCANCODE_KP_B)\
        X(kp_c, SDL_SCANCODE_KP_C)\
        X(kp_d, SDL_SCANCODE_KP_D)\
        X(kp_e, SDL_SCANCODE_KP_E)\
        X(kp_f, SDL_SCANCODE_KP_F)\
        X(kp_xor, SDL_SCANCODE_KP_XOR)\
        X(kp_power, SDL_SCANCODE_KP_POWER)\
        X(kp_percent, SDL_SCANCODE_KP_PERCENT)\
        X(kp_less, SDL_SCANCODE_KP_LESS)\
        X(kp_greater, SDL_SCANCODE_KP_GREATER)\
        X(kp_ampersand, SDL_SCANCODE_KP_AMPERSAND)\
        X(kp_dblampersand, SDL_SCANCODE_KP_DBLAMPERSAND)\
        X(kp_verticalbar, SDL_SCANCODE_KP_VERTICALBAR)\
        X(kp_dblverticalbar, SDL_SCANCODE_KP_DBLVERTICALBAR)\
        X(kp_colon, SDL_SCANCODE_KP_COLON)\
        X(kp_hash, SDL_SCANCODE_KP_HASH)\
        X(kp_space, SDL_SCANCODE_KP_SPACE)\
        X(kp_at, SDL_SCANCODE_KP_AT)\
        X(kp_exclam, SDL_SCANCODE_KP_EXCLAM)\
        X(kp_memstore, SDL_SCANCODE_KP_MEMSTORE)\
        X(kp_memrecall, SDL_SCANCODE_KP_MEMRECALL)\
        X(kp_memclear, SDL_SCANCODE_KP_MEMCLEAR)\
        X(kp_memadd, SDL_SCANCODE_KP_MEMADD)\
        X(kp_memsubtract, SDL_SCANCODE_KP_MEMSUBTRACT)\
        X(kp_memmultiply, SDL_SCANCODE_KP_MEMMULTIPLY)\
        X(kp_memdivide, SDL_SCANCODE_KP_MEMDIVIDE)\
        X(kp_plusminus, SDL_SCANCODE_KP_PLUSMINUS)\
        X(kp_clear, SDL_SCANCODE_KP_CLEAR)\
        X(kp_clearentry, SDL_SCANCODE_KP_CLEARENTRY)\
        X(kp_binary, SDL_SCANCODE_KP_BINARY)\
        X(kp_octal, SDL_SCANCODE_KP_OCTAL)\
        X(kp_decimal, SDL_SCANCODE_KP_DECIMAL)\
        X(kp_hexadecimal, SDL_SCANCODE_KP_HEXADECIMAL)\
        X(lctrl, SDL_SCANCODE_LCTRL)\
        X(lshift, SDL_SCANCODE_LSHIFT)\
        X(lalt, SDL_SCANCODE_LALT)\
        X(lgui, SDL_SCANCODE_LGUI)\
        X(rctrl, SDL_SCANCODE_RCTRL)\
        X(rshift, SDL_SCANCODE_RSHIFT)\
        X(ralt, SDL_SCANCODE_RALT)\
        X(rgui, SDL_SCANCODE_RGUI)\
        X(mode, SDL_SCANCODE_MODE)\
        X(sleep, SDL_SCANCODE_SLEEP)\
        X(wake, SDL_SCANCODE_WAKE)\
        X(channel_increment, SDL_SCANCODE_CHANNEL_INCREMENT)\
        X(channel_decrement, SDL_SCANCODE_CHANNEL_DECREMENT)\
        X(media_play, SDL_SCANCODE_MEDIA_PLAY)\
        X(media_pause, SDL_SCANCODE_MEDIA_PAUSE)\
        X(media_record, SDL_SCANCODE_MEDIA_RECORD)\
        X(media_fast_forward, SDL_SCANCODE_MEDIA_FAST_FORWARD)\
        X(media_rewind, SDL_SCANCODE_MEDIA_REWIND)\
        X(media_next_track, SDL_SCANCODE_MEDIA_NEXT_TRACK)\
        X(media_previous_track, SDL_SCANCODE_MEDIA_PREVIOUS_TRACK)\
        X(media_stop, SDL_SCANCODE_MEDIA_STOP)\
        X(media_eject, SDL_SCANCODE_MEDIA_EJECT)\
        X(media_play_pause, SDL_SCANCODE_MEDIA_PLAY_PAUSE)\
        X(media_select, SDL_SCANCODE_MEDIA_SELECT)\
        X(ac_new, SDL_SCANCODE_AC_NEW)\
        X(ac_open, SDL_SCANCODE_AC_OPEN)\
        X(ac_close, SDL_SCANCODE_AC_CLOSE)\
        X(ac_exit, SDL_SCANCODE_AC_EXIT)\
        X(ac_save, SDL_SCANCODE_AC_SAVE)\
        X(ac_print, SDL_SCANCODE_AC_PRINT)\
        X(ac_properties, SDL_SCANCODE_AC_PROPERTIES)\
        X(ac_search, SDL_SCANCODE_AC_SEARCH)\
        X(ac_home, SDL_SCANCODE_AC_HOME)\
        X(ac_back, SDL_SCANCODE_AC_BACK)\
        X(ac_forward, SDL_SCANCODE_AC_FORWARD)\
        X(ac_stop, SDL_SCANCODE_AC_STOP)\
        X(ac_refresh, SDL_SCANCODE_AC_REFRESH)\
        X(ac_bookmarks, SDL_SCANCODE_AC_BOOKMARKS)\
        X(softleft, SDL_SCANCODE_SOFTLEFT)\
        X(softright, SDL_SCANCODE_SOFTRIGHT)\
        X(call, SDL_SCANCODE_CALL)\
        X(endcall, SDL_SCANCODE_ENDCALL)\
        X(reserved, SDL_SCANCODE_RESERVED)\
        X(count, SDL_SCANCODE_COUNT)
    enum class scancode : uint16_t
    {
    #define X(a, b) a = b,
		KEYS_SCANCODE_X_MACROS_DATA
    #undef X
        unknown = SDL_SCANCODE_UNKNOWN
    };

    inline std::string to_string(scancode what)
    {
        switch (what)
        {
    #define X(a, b) case scancode::a: return #a;
            KEYS_SCANCODE_X_MACROS_DATA
    #undef X
        case scancode::unknown: return "unknown";
        [[unlikely]] default: return "invalid";
        }
    }
}

#undef KEYS_SCANCODE_X_MACROS_DATA