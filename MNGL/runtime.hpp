#pragma once

#include "config.hpp"
#include "error.hpp"
#include <SDL3/SDL.h>
#include <cstdint>
#include <ostream>
#include <string>

#if defined(_MSC_VER)
#pragma comment(lib, "SDL3.lib")
#endif

// включаем utf-8 и виртуализацию терминала на уровне винды
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WINDOWS_
#include <Windows.h>
#endif

namespace mngl::detail
{
	class console_setup
	{
	public:
		console_setup()
		{
			SetConsoleCP(CP_UTF8);
			SetConsoleOutputCP(CP_UTF8);

			HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

			DWORD mode = 0;
			GetConsoleMode(hOut, &mode);
			SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
		}
	};

	inline console_setup console_runtime;
} // namespace mngl::detail

#endif

namespace mngl
{
	class runtime
	{
		runtime()
		{
			if (!SDL_SetAppMetadata(APP_NAME, MNGL_STRINGIFY(APP_VERSION), APP_ID))
			{
				has_err = true;
				err = "SDL metadata setup error: " + std::string(SDL_GetError());
				return;
			}
			if (!set_optional_metadata())
			{
				return;
			}
			if (!SDL_Init(init_flags))
			{
				has_err = true;
				err = ("SDL Init setup error: " + std::string(SDL_GetError()));
				return;
			}
			is_running = true;
		}

	public:
		static void ensure()
		{
			auto& current = instance();
			if (!current.running())
				throw sdl_error::from_message(current.content());
		}

		inline static constexpr std::uint32_t init_flags = APP_FLAGS; // флаги инициализации

		runtime(const runtime& other) = delete;
		runtime(runtime&& other) = delete;

		~runtime()
		{
			is_running = false;
			SDL_Quit();
		}

		friend std::ostream& operator<<(std::ostream& out, const runtime& self)
		{
			out << std::string(self);
			return out;
		}

		operator std::string() const { return err; }

		explicit operator bool() const noexcept { return is_running; }

		static bool error() { return has_err; }

		static std::string content() { return err; }

		static bool running() { return is_running; }

		static bool exit() { return is_exit; }

		static runtime& instance()
		{
			static runtime inst;
			return inst;
		}

	private:
		static bool set_optional_metadata()
		{
#if defined(APP_CREATOR) || defined(APP_COPYRIGHT) || defined(APP_URL) || defined(APP_TYPE)
			const auto set_property = [](const char* property, const char* value)
			{
				if (SDL_SetAppMetadataProperty(property, value))
				{
					return true;
				}
				runtime::has_err = true;
				runtime::err = "SDL metadata setup error: " + std::string(SDL_GetError());
				return false;
			};

#ifdef APP_CREATOR
			if (!set_property(SDL_PROP_APP_METADATA_CREATOR_STRING, APP_CREATOR))
				return false;
#endif
#ifdef APP_COPYRIGHT
			if (!set_property(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, APP_COPYRIGHT))
				return false;
#endif
#ifdef APP_URL
			if (!set_property(SDL_PROP_APP_METADATA_URL_STRING, APP_URL))
				return false;
#endif
#ifdef APP_TYPE
			if (!set_property(SDL_PROP_APP_METADATA_TYPE_STRING, APP_TYPE))
				return false;
#endif
#endif
			return true;
		}

		inline static bool is_running = false;
		inline static bool has_err = false;
		inline static bool is_exit = false;
		inline static std::string err;
	};

	inline auto& app_runtime = runtime::instance();
#ifdef _DEBUG
	inline constexpr bool debug_mode = true;
#else
	inline constexpr bool debug_mode = false;
#endif
} // namespace mngl
