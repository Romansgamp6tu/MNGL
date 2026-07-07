#pragma once

#include "utilities.hpp"
#include "event_enums.hpp"
#include "event_structs.hpp"

namespace mngl
{
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
    class eventManager
    {
    public:
        // Используем move_only_function для максимальной скорости и поддержки лямбд с захватом move-only объектов
        template<EventType T>
        using callback = std::move_only_function<void(const T&)>;

        eventManager(const eventManager&) = delete;
        eventManager& operator=(const eventManager&) = delete;

        static eventManager& instance()
        {
            static eventManager inst;
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
        eventManager()
        {
            // Регистрируем низкоуровневый фильтр SDL3, который перехватывает абсолютно все события на лету
            SDL_AddEventWatch(sdlEventWatcher, this);
        }

        ~eventManager()
        {
            SDL_RemoveEventWatch(sdlEventWatcher, this);
        }

        static bool SDLCALL sdlEventWatcher(void* userData, SDL_Event* event)
        {
            auto* manager = static_cast<eventManager*>(userData);
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