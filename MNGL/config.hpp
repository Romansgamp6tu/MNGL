#pragma once

#define MNGL_STRINGIFY_IMPL(x) #x
#define MNGL_STRINGIFY(x) MNGL_STRINGIFY_IMPL(x)

#ifndef APP_NAME
#define APP_NAME "SDL3 app"
#endif

#ifndef APP_VERSION
#define APP_VERSION 09072026
#endif

#ifndef APP_ID
#define APP_ID "com.example.mngl"
#endif

#ifndef APP_ORGANIZATION
#define APP_ORGANIZATION "MNGL"
#endif

#ifndef APP_STORAGE_NAME
#define APP_STORAGE_NAME APP_NAME
#endif

#if defined(MNGL_CUSTOM_AUDIO_BACKEND)
#define MNGL_DETAIL_DEFAULT_AUDIO_INIT_FLAG 0
#else
#define MNGL_DETAIL_DEFAULT_AUDIO_INIT_FLAG SDL_INIT_AUDIO
#endif

#ifndef APP_FLAGS
#define APP_FLAGS                                                                                            \
	MNGL_DETAIL_DEFAULT_AUDIO_INIT_FLAG | SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC |              \
	    SDL_INIT_SENSOR | SDL_INIT_CAMERA
#endif
