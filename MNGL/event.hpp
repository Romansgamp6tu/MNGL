#pragma once

#include "utilities.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

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

#undef KEYS_SCANCODE_X_MACROS_DATA
    // =========================================================================
    // БАЗОВЫЕ СТРУКТУРЫ (Для иерархии данных)
    // =========================================================================
    struct eventBase
    {
        Events type;
        uint64_t timestamp;
    };

    struct windowEvent : eventBase
    {
        uint32_t windowID;
    };

    struct displayEvent : eventBase
    {
        uint32_t displayID;
    };

    // =========================================================================
    // ХЕЛПЕРЫ ВВОДА
    // =========================================================================
    struct Keycode
    {
        uint32_t value;

        constexpr bool isScancode() const noexcept
        {
            return value & SDLK_SCANCODE_MASK;
        }

        constexpr scancode get_scancode() const noexcept
        {
            return static_cast<scancode>(value & ~SDLK_SCANCODE_MASK);
        }

        constexpr char ascii() const noexcept
        {
            return static_cast<char>(value);
        }
    };

    // =========================================================================
    // СТРУКТУРЫ СОБЫТИЙ (Транзитные типы для шины событий)
    // =========================================================================

    // --- Application ---
    struct appQuit : eventBase { };
    struct appTerminating : eventBase { };
    struct appLowMemory : eventBase { };
    struct appWillEnterBackground : eventBase { };
    struct appDidEnterBackground : eventBase { };
    struct appWillEnterForeground : eventBase { };
    struct appDidEnterForeground : eventBase { };
    struct appLocaleChanged : eventBase { };
    struct appSystemThemeChanged : eventBase { };

    // --- Display ---
    struct displayAdded : displayEvent { };
    struct displayRemoved : displayEvent { };
    struct displayDesktopModeChanged : displayEvent { int width; int height; };
    struct displayCurrentModeChanged : displayEvent { int width; int height; };

    // --- Window ---
    struct windowShown : windowEvent { };
    struct windowHidden : windowEvent { };
    struct windowMinimized : windowEvent { };
    struct windowMaximized : windowEvent { };
    struct windowRestored : windowEvent { };
    struct windowOccluded : windowEvent { };
    struct windowEnterFullscreen : windowEvent { };
    struct windowLeaveFullscreen : windowEvent { };
    struct windowDestroyed : windowEvent { };
    struct windowMouseEnter : windowEvent { };
    struct windowMouseLeave : windowEvent { };
    struct windowFocusGained : windowEvent { };
    struct windowFocusLost : windowEvent { };
    struct windowPixelSizeChanged : windowEvent { glm::ivec2 size; };
    struct windowMetalViewResized : windowEvent { glm::ivec2 size; };
    struct windowCloseRequest : windowEvent { };
    struct windowIccProfChanged : windowEvent { };

    // --- Keyboard ---
    struct keyboardDevice : eventBase { uint32_t keyboardID; };
    struct keymapChanged : eventBase { };
    struct keyboardAdded : eventBase { uint32_t keyboardID; };
    struct keyboardRemoved : eventBase { uint32_t keyboardID; };
    struct screenKeyboardShown : eventBase { };
    struct screenKeyboardHidden : eventBase { };

    struct keyDown : windowEvent
    {
        uint32_t keyboardID; scancode code; Keycode key;
        keymod mod; bool repeat; bool is_down;
    };
    struct keyUp : windowEvent
    {
        uint32_t keyboardID; scancode code; Keycode key;
        keymod mod; bool repeat; bool is_down;
    };

    struct textEditing : windowEvent
    {
        std::string text; int32_t start; int32_t length;
    };
    struct textInput : windowEvent { std::string text; };
    struct textEditingCandidates : windowEvent
    {
        std::vector<std::string> candidates; int32_t selected; bool horizontal;
    };

    // --- Mouse ---
    struct mouseDevice : eventBase { uint32_t mouseID; };
    struct mouseAdded : eventBase { uint32_t mouseID; };
    struct mouseRemoved : eventBase { uint32_t mouseID; };

    struct mouseMotion : windowEvent
    {
        uint32_t mouseID; glm::vec2 position; glm::vec2 relative; uint32_t state;
    };
    struct mouseButtonDown : windowEvent
    {
        uint32_t mouseID; uint8_t button; uint8_t clicks; bool is_down; glm::vec2 position;
    };
    struct mouseButtonUp : windowEvent
    {
        uint32_t mouseID; uint8_t button; uint8_t clicks; bool is_down; glm::vec2 position;
    };
    struct mouseWheel : windowEvent
    {
        uint32_t mouseID; glm::vec2 scroll; glm::vec2 mousePos;
        int32_t intScroll[2]; wheel_direction direction;
    };

    // --- Joystick ---
    struct joystickEvent : eventBase { uint32_t joystickID; };
    struct joystickDevice : joystickEvent { };
    struct joystickAxisMoved : joystickEvent { uint8_t axis; int16_t value; };
    struct joystickBallMoved : joystickEvent { uint8_t ball; glm::ivec2 rel; };
    struct joystickHatPressed : joystickEvent { uint8_t hat; hat_position position; };
    struct joystickButtonDown : joystickEvent { uint8_t button; bool pressed; };
    struct joystickButtonUp : joystickEvent { uint8_t button; bool pressed; };
    struct joystickAdded : joystickEvent { };
    struct joystickRemoved : joystickEvent { };
    struct joystickUpdateComplete : joystickEvent { };
    struct joystickBattery : joystickEvent { power_state state; float percent; };

    // --- Gamepad ---
    struct gamepadEvent : eventBase { uint32_t joystickID; };
    struct gamepadDevice : gamepadEvent { };
    struct gamepadAdded : gamepadEvent { };
    struct gamepadRemoved : gamepadEvent { };
    struct gamepadRemapped : gamepadEvent { };
    struct gamepadUpdateComplete : gamepadEvent { };
    struct gamepadSteamHandleUpdated : gamepadEvent { };
    struct gamepadAxis : gamepadEvent { gamepad_axis axis; int16_t value; };
    struct gamepadButtonDown : gamepadEvent { gamepad_button button; bool is_down; };
    struct gamepadButtonUp : gamepadEvent { gamepad_button button; bool is_down; };

    struct gamepadTouchpadDown : gamepadEvent
    {
        int32_t touchpad; int32_t finger; glm::vec2 position; float pressure;
    };
    struct gamepadTouchpadMotion : gamepadEvent
    {
        int32_t touchpad; int32_t finger; glm::vec2 position; float pressure;
    };
    struct gamepadTouchpadUp : gamepadEvent
    {
        int32_t touchpad; int32_t finger; glm::vec2 position; float pressure;
    };
    struct gamepadSensor : gamepadEvent
    {
        sensor_type sensor; float data[3]; uint64_t timestamp;
    };

    // --- Touch ---
    struct touchEvent : eventBase
    {
        uint64_t touchID; uint64_t fingerID; uint32_t windowID;
        glm::vec2 position; glm::vec2 delta; float pressure; bool canceled;
    };
    struct fingerDown : eventBase
    {
        uint64_t touchID; uint64_t fingerID; uint32_t windowID;
        glm::vec2 position; glm::vec2 delta; float pressure;
    };
    struct fingerUp : eventBase
    {
        uint64_t touchID; uint64_t fingerID; uint32_t windowID;
        glm::vec2 position; glm::vec2 delta; float pressure;
    };
    struct fingerMotion : eventBase
    {
        uint64_t touchID; uint64_t fingerID; uint32_t windowID;
        glm::vec2 position; glm::vec2 delta; float pressure;
    };
    struct fingerCanceled : eventBase
    {
        uint64_t touchID; uint64_t fingerID; uint32_t windowID;
        glm::vec2 position; glm::vec2 delta; float pressure;
    };

    // --- Pinch ---
    struct pinchBegin : eventBase { uint32_t windowID; float scale; };
    struct pinchUpdate : eventBase { uint32_t windowID; float scale; };
    struct pinchEnd : eventBase { uint32_t windowID; float scale; };

    // --- Drag & Drop ---
    struct dropFile : windowEvent { glm::vec2 position; std::string source; std::string data; };
    struct dropText : windowEvent { glm::vec2 position; std::string source; std::string data; };
    struct dropBegin : windowEvent { glm::vec2 position; std::string source; };
    struct dropComplete : windowEvent { glm::vec2 position; std::string source; };
    struct dropPosition : windowEvent { glm::vec2 position; std::string source; std::string data; };

    // --- Audio ---
    struct audioDeviceAdded : eventBase { uint32_t deviceID; bool recording; };
    struct audioDeviceRemoved : eventBase { uint32_t deviceID; bool recording; };
    struct audioDeviceFormatChanged : eventBase { uint32_t deviceID; bool recording; };

    // --- Sensor ---
    struct sensorEvent : eventBase
    {
        uint32_t sensorID; float data[6]; uint64_t timestamp;
    };

    // --- Pen ---
    struct penProximityIn : eventBase { uint32_t penID; uint32_t windowID; glm::vec2 position; };
    struct penProximityOut : eventBase { uint32_t penID; uint32_t windowID; glm::vec2 position; };
    struct penMotion : eventBase { uint32_t penID; uint32_t windowID; glm::vec2 position; };

    // --- Camera ---
    struct cameraDeviceAdded : eventBase { uint32_t cameraID; };
    struct cameraDeviceRemoved : eventBase { uint32_t cameraID; };
    struct cameraDeviceApproved : eventBase { uint32_t cameraID; };
    struct cameraDeviceDenied : eventBase { uint32_t cameraID; };

    // --- Render ---
    struct renderTargetsReset : windowEvent { };
    struct renderDeviceReset : windowEvent { };
    struct renderDeviceLost : windowEvent { };

    // --- Internal ---
    struct pollSentinel : eventBase { };
    struct userEvent : eventBase
    {
        uint32_t windowID; int32_t code; void* data1; void* data2;
    };

    // =========================================================================
    // ДОБАВЛЕННЫЕ И ИСПРАВЛЕННЫЕ СТРУКТУРЫ ДЛЯ EVENT_STRUCTS.HPP
    // =========================================================================

    // --- Дополнительные оконные и дисплейные события, которых не хватало ---
    struct displayOrientation : displayEvent { display_orientation orientation; };
    struct displayAdded : displayEvent { };
    struct displayRemoved : displayEvent { };
    struct displayMoved : displayEvent { };
    struct displayDesktopModeChanged : displayEvent { };
    struct displayCurrentModeChanged : displayEvent { };
    struct displayContentScaleChanged : displayEvent { float contentScale; };

    struct windowShown : windowEvent { };
    struct windowHidden : windowEvent { };
    struct windowExposed : windowEvent { };
    struct windowMoved : windowEvent { glm::ivec2 position; };
    struct windowResized : windowEvent { glm::ivec2 size; };
    struct windowHitTest : windowEvent { };
    struct windowDisplayChanged : windowEvent { uint32_t displayID; };
    struct windowDisplayScaleChanged : windowEvent { };
    struct windowSafeAreaChanged : windowEvent { };

    // --- Исправленные события ввода (убираем narrowing conversion и исправляем типы) ---

    // Мышь (wheelDirectionEnum вместо wheelDirection)
    struct mouseWheel : windowEvent
    {
        uint32_t which;
        glm::vec2 scroll;
        glm::vec2 mousePos;
        glm::ivec2 integerScroll;
        wheel_direction direction; // в твоем enum_class это wheel_direction или wheelDirectionEnum
    };

    // Джойстики
    struct joystickAxisMoved : eventBase { uint32_t which; uint8_t axis; int16_t value; };
    struct joystickBallMoved : eventBase { uint32_t which; uint8_t ball; glm::ivec2 relative; };
    struct joystickHatPressed : eventBase { uint32_t which; uint8_t hat; hat_position value; };
    struct joystickButtonDown : eventBase { uint32_t which; uint8_t button; bool down; };
    struct joystickButtonUp : eventBase { uint32_t which; uint8_t button; bool down; };
    struct joystickAdded : eventBase { uint32_t which; };
    struct joystickRemoved : eventBase { uint32_t which; };
    struct joystickBattery : eventBase { uint32_t which; power_state state; float percent; };

    // Геймпады
    struct gamepadAxis : eventBase { uint32_t which; gamepad_axis axis; int16_t value; };
    struct gamepadButtonDown : eventBase { uint32_t which; gamepad_button button; bool down; };
    struct gamepadButtonUp : eventBase { uint32_t which; gamepad_button button; bool down; };
    struct gamepadAdded : eventBase { uint32_t which; };
    struct gamepadRemoved : eventBase { uint32_t which; };
    struct gamepadRemapped : eventBase { uint32_t which; };

    // В SDL3 координаты тачпадов геймпада — это float, поэтому строго glm::vec2!
    struct gamepadTouchpadDown : eventBase { uint32_t which; int32_t touchpad; int32_t finger; glm::vec2 position; float pressure; };
    struct gamepadTouchpadMotion : eventBase { uint32_t which; int32_t touchpad; int32_t finger; glm::vec2 position; float pressure; };
    struct gamepadTouchpadUp : eventBase { uint32_t which; int32_t touchpad; int32_t finger; glm::vec2 position; float pressure; };
    struct gamepadSensor : eventBase { uint32_t which; sensor_type sensor; glm::vec3 data; uint64_t sensorTimestamp; };

    // Тач (В SDL3 координаты нормализованные float, d — это тоже float дельта!)
    struct fingerDown : eventBase { uint64_t touchID; uint64_t fingerID; uint32_t windowID; glm::vec2 position; glm::vec2 delta; float pressure; };
    struct fingerMotion : eventBase { uint64_t touchID; uint64_t fingerID; uint32_t windowID; glm::vec2 position; glm::vec2 delta; float pressure; };
    struct fingerUp : eventBase { uint64_t touchID; uint64_t fingerID; uint32_t windowID; glm::vec2 position; glm::vec2 delta; float pressure; };

    // Дропы (В SDL3 оконные координаты x и y — это строго float!)
    struct dropFile : windowEvent { glm::vec2 position; std::string source; std::string data; };
    struct dropText : windowEvent { glm::vec2 position; std::string source; std::string data; };
    struct dropBegin : windowEvent { glm::vec2 position; std::string source; };
    struct dropComplete : windowEvent { glm::vec2 position; std::string source; };
    struct dropPosition : windowEvent { glm::vec2 position; std::string source; std::string data; };

    // =========================================================================
    // 1. КОНЦЕПТ ДЛЯ ПРОВЕРКИ ТИПА СОБЫТИЯ
    // =========================================================================
    // Проверяет, что тип T унаследован от eventBase. 
    // Это отсечет int, кастомные классы и пропустит только твои структуры событий.
    template<typename T>
    concept EventType = std::derived_from<T, eventBase>;

    // =========================================================================
    // 2. ЦЕНТРАЛЬНЫЙ МЕНЕДЖЕР СОБЫТИЙ (EVENT BUS + SDL WATCHER)
    // =========================================================================
    class event_manager
    {
    public:
        // Используем move_only_function для максимальной скорости и поддержки лямбд с захватом move-only объектов
        template<EventType T>
        using callback = std::move_only_function<void(const T&)>;

        event_manager(const event_manager&) = delete;
        event_manager& operator=(const event_manager&) = delete;

        static event_manager& instance()
        {
            static event_manager inst;
            return inst;
        }

        // Подписка на события с ограничением через концепт EventType
        template<EventType T>
        static void on(callback<T> cb)
        {
            getListeners<T>().push_back(std::move(cb));
        }

        // Ручной вызов события (если понадобится сгенерировать фейковое событие внутри движка)
        template<EventType T>
        static void emit(const T& event)
        {
            for (auto& cb : getListeners<T>())
            {
                if (cb) cb(event);
            }
        }

    private:
        event_manager()
        {
            // Регистрируем низкоуровневый фильтр SDL3, который перехватывает абсолютно все события на лету
            SDL_AddEventWatch(sdlEventWatcher, this);
        }

        ~event_manager()
        {
            SDL_RemoveEventWatch(sdlEventWatcher, this);
        }

        static bool SDLCALL sdlEventWatcher(void* userData, SDL_Event* event)
        {
            auto* manager = static_cast<event_manager*>(userData);
            manager->dispatchEvent(*event);
            return true; // Возвращаем true, чтобы событие оставалось в системной очереди SDL для PollEvent
        }

        // Хранилище подписчиков на основе статических векторов для каждого типа структуры
        template<EventType T>
        static auto& getListeners()
        {
            static std::vector<callback<T>> listeners;
            return listeners;
        }

        // Основной диспетчер: трансляция из структуры SDL_Event в структуры mngl и мгновенный emit
        void dispatchEvent(const SDL_Event& e)
        {
            const auto type = static_cast<Events>(e.type);
            const uint64_t ts = e.common.timestamp;

            // Вызываем только если на данный тип события есть активные подписчики
            switch (type)
            {
                // --- Application Events ---
            case Events::quit:
                if (!getListeners<appQuit>().empty()) emit(appQuit{ {type, ts} });
                break;
            case Events::terminating:
                if (!getListeners<appTerminating>().empty()) emit(appTerminating{ {type, ts} });
                break;
            case Events::low_memory:
                if (!getListeners<appLowMemory>().empty()) emit(appLowMemory{ {type, ts} });
                break;
            case Events::will_enter_background:
                if (!getListeners<appWillEnterBackground>().empty()) emit(appWillEnterBackground{ {type, ts} });
                break;
            case Events::did_enter_background:
                if (!getListeners<appDidEnterBackground>().empty()) emit(appDidEnterBackground{ {type, ts} });
                break;
            case Events::will_enter_foreground:
                if (!getListeners<appWillEnterForeground>().empty()) emit(appWillEnterForeground{ {type, ts} });
                break;
            case Events::did_enter_foreground:
                if (!getListeners<appDidEnterForeground>().empty()) emit(appDidEnterForeground{ {type, ts} });
                break;
            case Events::locale_changed:
                if (!getListeners<appLocaleChanged>().empty()) emit(appLocaleChanged{ {type, ts} });
                break;
            case Events::system_theme_changed:
                if (!getListeners<appSystemThemeChanged>().empty()) emit(appSystemThemeChanged{ {type, ts} });
                break;

                // --- Display Events ---
            case Events::display_orientation:
                if (!getListeners<displayOrientation>().empty()) emit(displayOrientation{ {{type, ts}, e.display.displayID}, static_cast<display_orientation>(e.display.data1) });
                break;
            case Events::display_added:
                if (!getListeners<displayAdded>().empty()) emit(displayAdded{ {{type, ts}, e.display.displayID} });
                break;
            case Events::display_removed:
                if (!getListeners<displayRemoved>().empty()) emit(displayRemoved{ {{type, ts}, e.display.displayID} });
                break;
            case Events::display_moved:
                if (!getListeners<displayMoved>().empty()) emit(displayMoved{ {{type, ts}, e.display.displayID} });
                break;
            case Events::display_desktop_mode_changed:
                if (!getListeners<displayDesktopModeChanged>().empty()) emit(displayDesktopModeChanged{ {{type, ts}, e.display.displayID} });
                break;
            case Events::display_current_mode_changed:
                if (!getListeners<displayCurrentModeChanged>().empty()) emit(displayCurrentModeChanged{ {{type, ts}, e.display.displayID} });
                break;
            case Events::display_content_scale_changed:
                if (!getListeners<displayContentScaleChanged>().empty()) emit(displayContentScaleChanged{ {{type, ts}, e.display.displayID}, (float)e.display.data1 });
                break;

                // --- Window Events ---
            case Events::window_shown:
                if (!getListeners<windowShown>().empty()) emit(windowShown{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_hidden:
                if (!getListeners<windowHidden>().empty()) emit(windowHidden{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_exposed:
                if (!getListeners<windowExposed>().empty()) emit(windowExposed{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_moved:
                if (!getListeners<windowMoved>().empty()) emit(windowMoved{ {{type, ts}, e.window.windowID}, {e.window.data1, e.window.data2} });
                break;
            case Events::window_resized:
                if (!getListeners<windowResized>().empty()) emit(windowResized{ {{type, ts}, e.window.windowID}, {e.window.data1, e.window.data2} });
                break;
            case Events::window_pixel_size_changed:
                if (!getListeners<windowPixelSizeChanged>().empty()) emit(windowPixelSizeChanged{ {{type, ts}, e.window.windowID}, {e.window.data1, e.window.data2} });
                break;
            case Events::window_metal_view_resized:
                if (!getListeners<windowMetalViewResized>().empty()) emit(windowMetalViewResized{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_minimized:
                if (!getListeners<windowMinimized>().empty()) emit(windowMinimized{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_maximized:
                if (!getListeners<windowMaximized>().empty()) emit(windowMaximized{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_restored:
                if (!getListeners<windowRestored>().empty()) emit(windowRestored{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_mouse_enter:
                if (!getListeners<windowMouseEnter>().empty()) emit(windowMouseEnter{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_mouse_leave:
                if (!getListeners<windowMouseLeave>().empty()) emit(windowMouseLeave{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_focus_gained:
                if (!getListeners<windowFocusGained>().empty()) emit(windowFocusGained{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_focus_lost:
                if (!getListeners<windowFocusLost>().empty()) emit(windowFocusLost{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_close_request:
                if (!getListeners<windowCloseRequest>().empty()) emit(windowCloseRequest{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_hit_test:
                if (!getListeners<windowHitTest>().empty()) emit(windowHitTest{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_display_changed:
                if (!getListeners<windowDisplayChanged>().empty()) emit(windowDisplayChanged{ {{type, ts}, e.window.windowID}, static_cast<uint32_t>(e.window.data1) });
                break;
            case Events::window_display_scale_changed:
                if (!getListeners<windowDisplayScaleChanged>().empty()) emit(windowDisplayScaleChanged{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_safe_area_changed:
                if (!getListeners<windowSafeAreaChanged>().empty()) emit(windowSafeAreaChanged{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_occluded:
                if (!getListeners<windowOccluded>().empty()) emit(windowOccluded{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_enter_fullscreen:
                if (!getListeners<windowEnterFullscreen>().empty()) emit(windowEnterFullscreen{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_leave_fullscreen:
                if (!getListeners<windowLeaveFullscreen>().empty()) emit(windowLeaveFullscreen{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_destroyed:
                if (!getListeners<windowDestroyed>().empty()) emit(windowDestroyed{ {{type, ts}, e.window.windowID} });
                break;

                // --- Keyboard Events ---
            case Events::key_down:
                if (!getListeners<keyDown>().empty()) {
                    emit(keyDown{
                        {{type, ts}, e.key.windowID}, e.key.which,
                        static_cast<scancode>(e.key.scancode), Keycode{e.key.key},
                        static_cast<keymod>(e.key.mod), e.key.repeat != 0, e.key.down != 0
                        });
                }
                break;
            case Events::key_up:
                if (!getListeners<keyUp>().empty()) {
                    emit(keyUp{
                        {{type, ts}, e.key.windowID}, e.key.which,
                        static_cast<scancode>(e.key.scancode), Keycode{e.key.key},
                        static_cast<keymod>(e.key.mod), e.key.repeat != 0, e.key.down != 0
                        });
                }
                break;
            case Events::text_editing:
                if (!getListeners<textEditing>().empty()) {
                    emit(textEditing{
                        {{type, ts}, e.edit.windowID}, e.edit.text ? e.edit.text : "",
                        e.edit.start, e.edit.length
                        });
                }
                break;
            case Events::text_input:
                if (!getListeners<textInput>().empty()) emit(textInput{ {{type, ts}, e.text.windowID}, e.text.text ? e.text.text : "" });
                break;
            case Events::keymap_changed:
                if (!getListeners<keymapChanged>().empty()) emit(keymapChanged{ {type, ts} });
                break;

                // --- Mouse Events ---
            case Events::mouse_motion:
                if (!getListeners<mouseMotion>().empty()) {
                    emit(mouseMotion{
                        {{type, ts}, e.motion.windowID}, e.motion.which,
                        {e.motion.x, e.motion.y}, {e.motion.xrel, e.motion.yrel}, e.motion.state
                        });
                }
                break;
            case Events::mouse_button_down:
                if (!getListeners<mouseButtonDown>().empty()) {
                    emit(mouseButtonDown{
                        {{type, ts}, e.button.windowID}, e.button.which,
                        e.button.button, e.button.clicks, e.button.down != 0, {e.button.x, e.button.y}
                        });
                }
                break;
            case Events::mouse_button_up:
                if (!getListeners<mouseButtonUp>().empty()) {
                    emit(mouseButtonUp{
                        {{type, ts}, e.button.windowID}, e.button.which,
                        e.button.button, e.button.clicks, e.button.down != 0, {e.button.x, e.button.y}
                        });
                }
                break;
            case Events::mouse_wheel:
                if (!getListeners<mouseWheel>().empty()) {
                    emit(mouseWheel{
                        {{type, ts}, e.wheel.windowID}, e.wheel.which,
                        {e.wheel.x, e.wheel.y}, {e.wheel.mouse_x, e.wheel.mouse_y},
                        {e.wheel.integer_x, e.wheel.integer_y}, static_cast<wheel_direction>(e.wheel.direction)
                        });
                }
                break;

                // --- Joystick Events ---
            case Events::joystick_axis_motion:
                if (!getListeners<joystickAxisMoved>().empty()) emit(joystickAxisMoved{ {type, ts}, e.jaxis.which, e.jaxis.axis, e.jaxis.value });
                break;
            case Events::joystick_ball_motion:
                if (!getListeners<joystickBallMoved>().empty()) emit(joystickBallMoved{ {type, ts}, e.jball.which, e.jball.ball, {e.jball.xrel, e.jball.yrel} });
                break;
            case Events::joystick_hat_motion:
                if (!getListeners<joystickHatPressed>().empty()) emit(joystickHatPressed{ {type, ts}, e.jhat.which, e.jhat.hat, static_cast<hat_position>(e.jhat.value) });
                break;
            case Events::joystick_button_down:
                if (!getListeners<joystickButtonDown>().empty()) emit(joystickButtonDown{ {type, ts}, e.jbutton.which, e.jbutton.button, e.jbutton.down != 0 });
                break;
            case Events::joystick_button_up:
                if (!getListeners<joystickButtonUp>().empty()) emit(joystickButtonUp{ {type, ts}, e.jbutton.which, e.jbutton.button, e.jbutton.down != 0 });
                break;
            case Events::joystick_added:
                if (!getListeners<joystickAdded>().empty()) emit(joystickAdded{ {type, ts}, e.jdevice.which });
                break;
            case Events::joystick_removed:
                if (!getListeners<joystickRemoved>().empty()) emit(joystickRemoved{ {type, ts}, e.jdevice.which });
                break;
            case Events::joystick_battery_updated:
                if (!getListeners<joystickBattery>().empty()) emit(joystickBattery{ {type, ts}, e.jbattery.which, static_cast<power_state>(e.jbattery.state), (float)e.jbattery.percent });
                break;

                // --- Gamepad Events ---
            case Events::gamepad_axis_motion:
                if (!getListeners<gamepadAxis>().empty()) emit(gamepadAxis{ {type, ts}, e.gaxis.which, static_cast<gamepad_axis>(e.gaxis.axis), e.gaxis.value });
                break;
            case Events::gamepad_button_down:
                if (!getListeners<gamepadButtonDown>().empty()) emit(gamepadButtonDown{ {type, ts}, e.gbutton.which, static_cast<gamepad_button>(e.gbutton.button), e.gbutton.down != 0 });
                break;
            case Events::gamepad_button_up:
                if (!getListeners<gamepadButtonUp>().empty()) emit(gamepadButtonUp{ {type, ts}, e.gbutton.which, static_cast<gamepad_button>(e.gbutton.button), e.gbutton.down != 0 });
                break;
            case Events::gamepad_added:
                if (!getListeners<gamepadAdded>().empty()) emit(gamepadAdded{ {type, ts}, e.gdevice.which });
                break;
            case Events::gamepad_removed:
                if (!getListeners<gamepadRemoved>().empty()) emit(gamepadRemoved{ {type, ts}, e.gdevice.which });
                break;
            case Events::gamepad_remapped:
                if (!getListeners<gamepadRemapped>().empty()) emit(gamepadRemapped{ {type, ts}, e.gdevice.which });
                break;
            case Events::gamepad_touchpad_down:
                if (!getListeners<gamepadTouchpadDown>().empty()) emit(gamepadTouchpadDown{ {type, ts}, e.gtouchpad.which, e.gtouchpad.touchpad, e.gtouchpad.finger, {e.gtouchpad.x, e.gtouchpad.y}, e.gtouchpad.pressure });
                break;
            case Events::gamepad_touchpad_motion:
                if (!getListeners<gamepadTouchpadMotion>().empty()) emit(gamepadTouchpadMotion{ {type, ts}, e.gtouchpad.which, e.gtouchpad.touchpad, e.gtouchpad.finger, {e.gtouchpad.x, e.gtouchpad.y}, e.gtouchpad.pressure });
                break;
            case Events::gamepad_touchpad_up:
                if (!getListeners<gamepadTouchpadUp>().empty()) emit(gamepadTouchpadUp{ {type, ts}, e.gtouchpad.which, e.gtouchpad.touchpad, e.gtouchpad.finger, {e.gtouchpad.x, e.gtouchpad.y}, e.gtouchpad.pressure });
                break;
            case Events::gamepad_sensor_update:
                if (!getListeners<gamepadSensor>().empty()) emit(gamepadSensor{ {type, ts}, e.gsensor.which, static_cast<sensor_type>(e.gsensor.sensor), {e.gsensor.data[0], e.gsensor.data[1], e.gsensor.data[2]}, e.gsensor.sensor_timestamp });
                break;

                // --- Touch Events ---
            case Events::finger_down:
                if (!getListeners<fingerDown>().empty()) emit(fingerDown{ {type, ts}, e.tfinger.touchID, e.tfinger.fingerID, e.tfinger.windowID, {e.tfinger.x, e.tfinger.y}, {e.tfinger.dx, e.tfinger.dy}, e.tfinger.pressure });
                break;
            case Events::finger_motion:
                if (!getListeners<fingerMotion>().empty()) emit(fingerMotion{ {type, ts}, e.tfinger.touchID, e.tfinger.fingerID, e.tfinger.windowID, {e.tfinger.x, e.tfinger.y}, {e.tfinger.dx, e.tfinger.dy}, e.tfinger.pressure });
                break;
            case Events::finger_up:
                if (!getListeners<fingerUp>().empty()) emit(fingerUp{ {type, ts}, e.tfinger.touchID, e.tfinger.fingerID, e.tfinger.windowID, {e.tfinger.x, e.tfinger.y}, {e.tfinger.dx, e.tfinger.dy}, e.tfinger.pressure });
                break;

                // --- Drop Events ---
            case Events::drop_file:
                if (!getListeners<dropFile>().empty()) emit(dropFile{ {{type, ts}, e.drop.windowID}, {e.drop.x, e.drop.y}, e.drop.source ? e.drop.source : "", e.drop.data ? e.drop.data : "" });
                break;
            case Events::drop_text:
                if (!getListeners<dropText>().empty()) emit(dropText{ {{type, ts}, e.drop.windowID}, {e.drop.x, e.drop.y}, e.drop.source ? e.drop.source : "", e.drop.data ? e.drop.data : "" });
                break;
            case Events::drop_begin:
                if (!getListeners<dropBegin>().empty()) emit(dropBegin{ {{type, ts}, e.drop.windowID}, {e.drop.x, e.drop.y}, e.drop.source ? e.drop.source : "" });
                break;
            case Events::drop_complete:
                if (!getListeners<dropComplete>().empty()) emit(dropComplete{ {{type, ts}, e.drop.windowID}, {e.drop.x, e.drop.y}, e.drop.source ? e.drop.source : "" });
                break;
            case Events::drop_position:
                if (!getListeners<dropPosition>().empty()) emit(dropPosition{ {{type, ts}, e.drop.windowID}, {e.drop.x, e.drop.y}, e.drop.source ? e.drop.source : "", e.drop.data ? e.drop.data : "" });
                break;

                // --- Audio Device Events ---
            case Events::audio_device_added:
                if (!getListeners<audioDeviceAdded>().empty()) emit(audioDeviceAdded{ {type, ts}, e.adevice.which, e.adevice.recording != 0 });
                break;
            case Events::audio_device_removed:
                if (!getListeners<audioDeviceRemoved>().empty()) emit(audioDeviceRemoved{ {type, ts}, e.adevice.which, e.adevice.recording != 0 });
                break;
            case Events::audio_device_format_changed:
                if (!getListeners<audioDeviceFormatChanged>().empty()) emit(audioDeviceFormatChanged{ {type, ts}, e.adevice.which, e.adevice.recording != 0 });
                break;

                // --- Sensor Events ---
            case Events::sensor_update:
                if (!getListeners<sensorEvent>().empty()) {
                    sensorEvent se{ {type, ts}, e.sensor.which, {}, e.sensor.sensor_timestamp };
                    std::copy(std::begin(e.sensor.data), std::end(e.sensor.data), std::begin(se.data));
                    emit(se);
                }
                break;

                // --- Pen Events ---
            case Events::pen_proximity_in:
                if (!getListeners<penProximityIn>().empty()) emit(penProximityIn{ {type, ts}, e.pmotion.which, e.pmotion.windowID, {e.pmotion.x, e.pmotion.y} });
                break;
            case Events::pen_proximity_out:
                if (!getListeners<penProximityOut>().empty()) emit(penProximityOut{ {type, ts}, e.pmotion.which, e.pmotion.windowID, {e.pmotion.x, e.pmotion.y} });
                break;
            case Events::pen_motion:
                if (!getListeners<penMotion>().empty()) emit(penMotion{ {type, ts}, e.pmotion.which, e.pmotion.windowID, {e.pmotion.x, e.pmotion.y} });
                break;

                // --- Camera Device Events ---
            case Events::camera_device_added:
                if (!getListeners<cameraDeviceAdded>().empty()) emit(cameraDeviceAdded{ {type, ts}, e.cdevice.which });
                break;
            case Events::camera_device_removed:
                if (!getListeners<cameraDeviceRemoved>().empty()) emit(cameraDeviceRemoved{ {type, ts}, e.cdevice.which });
                break;
            case Events::camera_device_approved:
                if (!getListeners<cameraDeviceApproved>().empty()) emit(cameraDeviceApproved{ {type, ts}, e.cdevice.which });
                break;
            case Events::camera_device_denied:
                if (!getListeners<cameraDeviceDenied>().empty()) emit(cameraDeviceDenied{ {type, ts}, e.cdevice.which });
                break;

                // --- Render Events ---
            case Events::render_targets_reset:
                if (!getListeners<renderTargetsReset>().empty()) emit(renderTargetsReset{ {{type, ts}, e.window.windowID} });
                break;
            case Events::render_device_reset:
                if (!getListeners<renderDeviceReset>().empty()) emit(renderDeviceReset{ {{type, ts}, e.window.windowID} });
                break;

            default:
                break;
            }
        }
    };

    // Облегченная глобальная функция для кадра: только прокачивает очередь SDL3.
    // Нам больше не нужно самим бегать циклом по событиям ради инпутов — watcher все сделает за нас внутри SDL_PumpEvents / SDL_PollEvent.
    inline void pollEvents()
    {
        SDL_PumpEvents();
    }
}