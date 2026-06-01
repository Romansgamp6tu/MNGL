#pragma once

#include "utilities.hpp"

#ifdef SDL_PLATFORM_WINDOWS
using native_handle = HWND;
#elifdef SDL_PLATFORM_LINUX
using native_handle = Window;
#elifdef SDL_PLATFORM_MACOS
using native_handle = NSWindow*;
#elifdef SDL_PLATFORM_IOS
using native_handle = UIWindow*;
#elifdef SDL_PLATFORM_ANDROID
using native_handle = ANativeWindow*;
#elifdef SDL_PLATFORM_EMSCRIPTEN
using native_handle = HTMLCanvasElement*;
#else
#error "Unsupported platform!"
#endif // SDL_PLATFORM_WINDOWS