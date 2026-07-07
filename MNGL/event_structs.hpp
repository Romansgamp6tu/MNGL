#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "event_enums.hpp" // Твой файл с enum class Events, scancode, keymod и т.д.

namespace mngl
{
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
    struct appQuit : eventBase {};
    struct appTerminating : eventBase {};
    struct appLowMemory : eventBase {};
    struct appWillEnterBackground : eventBase {};
    struct appDidEnterBackground : eventBase {};
    struct appWillEnterForeground : eventBase {};
    struct appDidEnterForeground : eventBase {};
    struct appLocaleChanged : eventBase {};
    struct appSystemThemeChanged : eventBase {};

    // --- Display ---
    struct displayAdded : displayEvent {};
    struct displayRemoved : displayEvent {};
    struct displayDesktopModeChanged : displayEvent { int width; int height; };
    struct displayCurrentModeChanged : displayEvent { int width; int height; };

    // --- Window ---
    struct windowShown : windowEvent {};
    struct windowHidden : windowEvent {};
    struct windowMinimized : windowEvent {};
    struct windowMaximized : windowEvent {};
    struct windowRestored : windowEvent {};
    struct windowOccluded : windowEvent {};
    struct windowEnterFullscreen : windowEvent {};
    struct windowLeaveFullscreen : windowEvent {};
    struct windowDestroyed : windowEvent {};
    struct windowMouseEnter : windowEvent {};
    struct windowMouseLeave : windowEvent {};
    struct windowFocusGained : windowEvent {};
    struct windowFocusLost : windowEvent {};
    struct windowPixelSizeChanged : windowEvent { glm::ivec2 size; };
    struct windowMetalViewResized : windowEvent { glm::ivec2 size; };
    struct windowCloseRequest : windowEvent {};
    struct windowIccProfChanged : windowEvent {};

    // --- Keyboard ---
    struct keyboardDevice : eventBase { uint32_t keyboardID; };
    struct keymapChanged : eventBase {};
    struct keyboardAdded : eventBase { uint32_t keyboardID; };
    struct keyboardRemoved : eventBase { uint32_t keyboardID; };
    struct screenKeyboardShown : eventBase {};
    struct screenKeyboardHidden : eventBase {};

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
    struct joystickDevice : joystickEvent {};
    struct joystickAxisMoved : joystickEvent { uint8_t axis; int16_t value; };
    struct joystickBallMoved : joystickEvent { uint8_t ball; glm::ivec2 rel; };
    struct joystickHatPressed : joystickEvent { uint8_t hat; hat_position position; };
    struct joystickButtonDown : joystickEvent { uint8_t button; bool pressed; };
    struct joystickButtonUp : joystickEvent { uint8_t button; bool pressed; };
    struct joystickAdded : joystickEvent {};
    struct joystickRemoved : joystickEvent {};
    struct joystickUpdateComplete : joystickEvent {};
    struct joystickBattery : joystickEvent { power_state state; float percent; };

    // --- Gamepad ---
    struct gamepadEvent : eventBase { uint32_t joystickID; };
    struct gamepadDevice : gamepadEvent {};
    struct gamepadAdded : gamepadEvent {};
    struct gamepadRemoved : gamepadEvent {};
    struct gamepadRemapped : gamepadEvent {};
    struct gamepadUpdateComplete : gamepadEvent {};
    struct gamepadSteamHandleUpdated : gamepadEvent {};
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
    struct renderTargetsReset : windowEvent {};
    struct renderDeviceReset : windowEvent {};
    struct renderDeviceLost : windowEvent {};

    // --- Internal ---
    struct pollSentinel : eventBase {};
    struct userEvent : eventBase
    {
        uint32_t windowID; int32_t code; void* data1; void* data2;
    };

    // =========================================================================
    // ДОБАВЛЕННЫЕ И ИСПРАВЛЕННЫЕ СТРУКТУРЫ ДЛЯ EVENT_STRUCTS.HPP
    // =========================================================================

    // --- Дополнительные оконные и дисплейные события, которых не хватало ---
    struct displayOrientation : displayEvent { display_orientation orientation; };
    struct displayAdded : displayEvent {};
    struct displayRemoved : displayEvent {};
    struct displayMoved : displayEvent {};
    struct displayDesktopModeChanged : displayEvent {};
    struct displayCurrentModeChanged : displayEvent {};
    struct displayContentScaleChanged : displayEvent { float contentScale; };

    struct windowShown : windowEvent {};
    struct windowHidden : windowEvent {};
    struct windowExposed : windowEvent {};
    struct windowMoved : windowEvent { glm::ivec2 position; };
    struct windowResized : windowEvent { glm::ivec2 size; };
    struct windowHitTest : windowEvent {};
    struct windowDisplayChanged : windowEvent { uint32_t displayID; };
    struct windowDisplayScaleChanged : windowEvent {};
    struct windowSafeAreaChanged : windowEvent {};

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
}