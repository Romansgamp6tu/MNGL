#pragma once
#pragma comment(lib, "SDL3.lib")
#pragma comment(lib, "SDL3_image.lib")
#pragma comment(lib, "freetype.lib")

// определим информацию о приложении, если не была задана пользователем

#define MNGL_STRINGIFY_IMPL(x) #x
#define STR(x) MNGL_STRINGIFY_IMPL(x)

#ifndef APP_NAME
#define APP_NAME "SDL3 app"
#endif

#ifndef APP_VERSION
#define APP_VERSION 09072026
#endif

#ifndef APP_ID
#define APP_ID "com.sdl3.mngl." STR(APP_VERSION)
#endif

#ifndef APP_FLAGS
#define APP_FLAGS \
		SDL_INIT_AUDIO   |\
		SDL_INIT_VIDEO   |\
		SDL_INIT_GAMEPAD |\
		SDL_INIT_HAPTIC  |\
		SDL_INIT_SENSOR  |\
		SDL_INIT_CAMERA
#endif


// для хешей
#define GLM_ENABLE_EXPERIMENTAL

// для ui на базе IMGUI
#ifdef IMGUI_VERSION
#define MNGL_IMGUI
#endif

// включаем utf-8 и виртуализацию терминала на уровне винды
#ifdef _WIN32
#define NOMINMAX
#ifndef _WINDOWS_
#include <Windows.h>
#endif

class __onBegin
{
public:
	__onBegin()
	{
		SetConsoleCP(CP_UTF8);
		SetConsoleOutputCP(CP_UTF8);

		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

		DWORD mode = 0;
		GetConsoleMode(hOut, &mode);
		SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}
};

inline __onBegin __onBeginInstance;

#endif

// включаем SDL3 и инициализируем его
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdexcept>
#include <string>

class __SDL_While_Runtime__
{
	__SDL_While_Runtime__()
	{
		if (!SDL_SetAppMetadata(APP_NAME, STR(APP_VERSION), APP_ID))
		{
			has_err = true;
			err = "SDL metadata setup error: " + std::string(SDL_GetError());
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

	inline static constexpr uint32_t init_flags = APP_FLAGS; // флаги инициализации

	__SDL_While_Runtime__(const __SDL_While_Runtime__& other) = delete;
	__SDL_While_Runtime__(__SDL_While_Runtime__&& other) = delete;

	~__SDL_While_Runtime__()
	{
		is_running = false;
		SDL_Quit();
	}

	friend std::ostream& operator << (std::ostream& out, const __SDL_While_Runtime__& self)
	{
		out << std::string(self);
		return out;
	}

	operator std::string() const
	{
		return err;
	}

	operator bool() const
	{
		return has_err;
	}

	static bool error()
	{
		return has_err;
	}

	static std::string content()
	{
		return err;
	}

	static bool running()
	{
		return is_running;
	}

	static bool exit()
	{
		return is_exit;
	}

	static __SDL_While_Runtime__& instance()
	{
		static __SDL_While_Runtime__ inst;
		return inst;
	}

private:
	inline static bool is_running = false;
	inline static bool has_err = false;
	inline static bool is_exit = false;
	inline static std::string err;
};

inline auto& SDL_Runtime = __SDL_While_Runtime__::instance();

// ошибки с информацией из SDL3
class [[nodiscard]] sdl_error : public std::runtime_error
{
public:
	using _Mybase = runtime_error;
	explicit sdl_error(const std::string& _Message) : _Mybase((_Message + SDL_GetError()).c_str()) {}
};

#include "logger.h"
#include "proxy.hpp"
#include "iupdate.h"

// макросы для чеканья ошибок SDL3

#define TRY(expr, msg) do{ if(!(expr)) throw sdl_error(msg); } while(0)
#define TRY_SDL(expr) TRY(expr, "SDL error in " #expr ": ")

#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/string_cast.hpp>

#ifdef _DEBUG
constexpr inline bool debug_mode = true;
#else
constexpr inline bool debug_mode = false;
#endif


#include <filesystem>
#include <fstream>
#include <vector>
#include <format>
#include <cstdint>
#include <limits>

struct process_result
{
	int exit_code = 0;
	std::string output;
};

inline std::vector<uint8_t> read_binary_file(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		throw std::runtime_error(std::format("Failed to open file: {}", path.string()));
	}

	const auto size = static_cast<std::streamoff>(file.tellg());
	if (size < 0)
	{
		throw std::runtime_error(std::format("Failed to determine file size: {}", path.string()));
	}
	const auto unsigned_size = static_cast<uintmax_t>(size);
	if (unsigned_size > std::numeric_limits<size_t>::max() ||
		unsigned_size > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max()))
	{
		throw std::length_error(std::format("File is too large to read: {}", path.string()));
	}
	if (!file.seekg(0, std::ios::beg))
	{
		throw std::runtime_error(std::format("Failed to seek in file: {}", path.string()));
	}

	std::vector<uint8_t> buffer(static_cast<size_t>(unsigned_size));
	if (!buffer.empty() &&
		!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size())))
	{
		throw std::runtime_error(std::format("Failed to read binary file: {}", path.string()));
	}

	return buffer;
}
