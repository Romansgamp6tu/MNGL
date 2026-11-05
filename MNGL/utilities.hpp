#pragma once

// Compatibility umbrella. Focused headers can include runtime.hpp/error.hpp directly.
#include "runtime.hpp"
#include "file_io.hpp"
#include "logger.h"
#include "proxy.hpp"
#include "iupdate.h"

#include <SDL3_image/SDL_image.h>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#if defined(_MSC_VER)
#pragma comment(lib, "SDL3_image.lib")
#pragma comment(lib, "freetype.lib")
#endif

using sdl_error = mngl::sdl_error;
using process_result = mngl::process_result;
using mngl::debug_mode;
using mngl::read_binary_file;

// Historical boolean convention: true means startup failed. New code calls runtime::ensure().
namespace mngl::detail
{
	struct legacy_runtime_status
	{
		static bool error() noexcept { return runtime::error(); }
		static std::string content() { return runtime::content(); }
		static bool running() noexcept { return runtime::running(); }
		static bool exit() noexcept { return runtime::exit(); }
		operator bool() const noexcept { return error(); }
		operator std::string() const { return runtime::content(); }
		friend std::ostream& operator<<(std::ostream& out, legacy_runtime_status)
		{
			return out << runtime::content();
		}
	};
} // namespace mngl::detail
inline mngl::detail::legacy_runtime_status SDL_Runtime;

#define MNGL_TRY(expr, msg) ::mngl::check_sdl(static_cast<bool>(expr), msg)
#define MNGL_TRY_SDL(expr) ::mngl::check_sdl(static_cast<bool>(expr))
// Legacy macros remain only in this compatibility umbrella.
#ifndef STR
#define STR(x) MNGL_STRINGIFY(x)
#endif
#ifndef TRY
#define TRY(expr, msg) MNGL_TRY(expr, msg)
#endif
#ifndef TRY_SDL
#define TRY_SDL(expr) MNGL_TRY_SDL(expr)
#endif
