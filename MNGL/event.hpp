#pragma once

#include "detail/event/enums.hpp"
#include "detail/event/core_events.hpp"
#include "detail/event/input_events.hpp"
#include "detail/event/system_events.hpp"
#include "detail/event/manager.hpp"

namespace mngl
{
	using eventBase = event_base;
	using windowEvent = window_event;
	using displayEvent = display_event;
	using appQuit = app_quit;
	using appTerminating = app_terminating;
	using appLowMemory = app_low_memory;
	using appWillEnterBackground = app_will_enter_background;
	using appDidEnterBackground = app_did_enter_background;
	using appWillEnterForeground = app_will_enter_foreground;
	using appDidEnterForeground = app_did_enter_foreground;
	using appLocaleChanged = app_locale_changed;
	using appSystemThemeChanged = app_system_theme_changed;
	using displayOrientation = display_orientation_event;
	using displayAdded = display_added;
	using displayRemoved = display_removed;
	using displayMoved = display_moved;
	using displayDesktopModeChanged = display_desktop_mode_changed;
	using displayCurrentModeChanged = display_current_mode_changed;
	using displayContentScaleChanged = display_content_scale_changed;
	using displayUsableBoundsChanged = display_usable_bounds_changed;
	using windowShown = window_shown;
	using windowHidden = window_hidden;
	using windowExposed = window_exposed;
	using windowMoved = window_moved;
	using windowResized = window_resized;
	using windowPixelSizeChanged = window_pixel_size_changed;
	using windowMetalViewResized = window_metal_view_resized;
	using windowMinimized = window_minimized;
	using windowMaximized = window_maximized;
	using windowRestored = window_restored;
	using windowMouseEnter = window_mouse_enter;
	using windowMouseLeave = window_mouse_leave;
	using windowFocusGained = window_focus_gained;
	using windowFocusLost = window_focus_lost;
	using windowCloseRequest = window_close_request;
	using windowHitTest = window_hit_test;
	using windowIccProfChanged = window_icc_prof_changed;
	using windowDisplayChanged = window_display_changed;
	using windowDisplayScaleChanged = window_display_scale_changed;
	using windowSafeAreaChanged = window_safe_area_changed;
	using windowOccluded = window_occluded;
	using windowEnterFullscreen = window_enter_fullscreen;
	using windowLeaveFullscreen = window_leave_fullscreen;
	using windowDestroyed = window_destroyed;
	using windowHdrStateChanged = window_hdr_state_changed;
	using keyboardDevice = keyboard_device;
	using keymapChanged = keymap_changed;
	using keyboardAdded = keyboard_added;
	using keyboardRemoved = keyboard_removed;
	using screenKeyboardShown = screen_keyboard_shown;
	using screenKeyboardHidden = screen_keyboard_hidden;
	using keyDown = key_down;
	using keyUp = key_up;
	using textEditing = text_editing;
	using textInput = text_input;
	using textEditingCandidates = text_editing_candidates;
	using mouseDevice = mouse_device;
	using mouseAdded = mouse_added;
	using mouseRemoved = mouse_removed;
	using mouseMotion = mouse_motion;
	using mouseButtonDown = mouse_button_down;
	using mouseButtonUp = mouse_button_up;
	using mouseWheel = mouse_wheel;
	using joystickAxisMoved = joystick_axis_moved;
	using joystickBallMoved = joystick_ball_moved;
	using joystickHatPressed = joystick_hat_pressed;
	using joystickButtonDown = joystick_button_down;
	using joystickButtonUp = joystick_button_up;
	using joystickAdded = joystick_added;
	using joystickRemoved = joystick_removed;
	using joystickUpdateComplete = joystick_update_complete;
	using joystickBattery = joystick_battery;
	using gamepadAxis = gamepad_axis_event;
	using gamepadButtonDown = gamepad_button_down;
	using gamepadButtonUp = gamepad_button_up;
	using gamepadAdded = gamepad_added;
	using gamepadRemoved = gamepad_removed;
	using gamepadRemapped = gamepad_remapped;
	using gamepadUpdateComplete = gamepad_update_complete;
	using gamepadSteamHandleUpdated = gamepad_steam_handle_updated;
	using gamepadTouchpadDown = gamepad_touchpad_down;
	using gamepadTouchpadMotion = gamepad_touchpad_motion;
	using gamepadTouchpadUp = gamepad_touchpad_up;
	using gamepadSensor = gamepad_sensor;
	using fingerDown = finger_down;
	using fingerMotion = finger_motion;
	using fingerUp = finger_up;
	using fingerCanceled = finger_canceled;
	using pinchBegin = pinch_begin;
	using pinchUpdate = pinch_update;
	using pinchEnd = pinch_end;
	using clipboardUpdate = clipboard_update;
	using dropFile = drop_file;
	using dropText = drop_text;
	using dropBegin = drop_begin;
	using dropComplete = drop_complete;
	using dropPosition = drop_position;
#if !defined(MNGL_CUSTOM_AUDIO_BACKEND)
	using audioDeviceAdded = audio_device_added;
	using audioDeviceRemoved = audio_device_removed;
	using audioDeviceFormatChanged = audio_device_format_changed;
#endif
	using sensorEvent = sensor_event;
	using penProximityIn = pen_proximity_in;
	using penProximityOut = pen_proximity_out;
	using penMotion = pen_motion;
	using penDown = pen_down;
	using penUp = pen_up;
	using penButtonDown = pen_button_down;
	using penButtonUp = pen_button_up;
	using penAxis = pen_axis;
	using cameraDeviceAdded = camera_device_added;
	using cameraDeviceRemoved = camera_device_removed;
	using cameraDeviceApproved = camera_device_approved;
	using cameraDeviceDenied = camera_device_denied;
	using renderTargetsReset = render_targets_reset;
	using renderDeviceReset = render_device_reset;
	using renderDeviceLost = render_device_lost;
	using pollSentinel = poll_sentinel;
	using userEvent = user_event;
	using Events = event_type;
	using Keycode = keycode;
	template <typename T>
	concept EventType = event_payload<T>;
	inline void pollEvents()
	{
		poll_events();
	}
} // namespace mngl

