#pragma once

#include <SDL3/SDL_error.h>
#include <charconv>
#include <cstdint>
#include <optional>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#if __has_include(<stacktrace>)
#include <stacktrace>
#if defined(__cpp_lib_stacktrace)
#define MNGL_DETAIL_HAS_STD_STACKTRACE
#endif
#endif

namespace mngl
{
	namespace detail
	{
		[[nodiscard]] inline std::string attach_error_trace(
		    std::string_view message, std::source_location location = std::source_location::current())
		{
			std::string result(message);
			result += "\n  at ";
			result += location.function_name();
			result += "\n  ";
			result += location.file_name();
			result += ':';
			result += std::to_string(location.line());
#if defined(MNGL_DETAIL_HAS_STD_STACKTRACE)
			const auto trace = std::stacktrace::current(2, 32);
			if (!trace.empty())
			{
				std::ostringstream stack;
				stack << trace;
				result += "\n  stack trace:\n";
				result += stack.str();
			}
#endif
			return result;
		}
	} // namespace detail

	class sdl_error : public std::runtime_error
	{
	public:
		explicit sdl_error(std::string_view context,
		                   std::source_location location = std::source_location::current())
		    : std::runtime_error(format(context, SDL_GetError(), location, true))
		{
		}

		// Use an already captured error; do not append SDL's mutable error state.
		[[nodiscard]] static sdl_error from_message(
		    std::string message, std::source_location location = std::source_location::current())
		{
			return sdl_error(std::move(message), location, captured_error{});
		}

	private:
		struct captured_error
		{
		};
		sdl_error(std::string message, std::source_location location, captured_error)
		    : std::runtime_error(format(message, {}, location, false))
		{
		}

		[[nodiscard]] static std::string format(std::string_view context, const char* detail,
		                                        std::source_location location, bool has_backend_detail)
		{
			std::string message(context);
			if (has_backend_detail)
			{
				if (message.empty() || message.back() != ' ')
				{
					message += message.find('\n') == std::string::npos ? ": " : "\n  SDL: ";
				}
				message += detail && *detail ? detail : "SDL provided no error detail";
			}

#if defined(_WIN32)
			if (const auto hresult = find_hresult(message))
			{
				if (const auto explanation = explain_hresult(*hresult); !explanation.empty())
				{
					message += "\n  Windows status: ";
					message += explanation;
				}
			}
#endif

			return detail::attach_error_trace(message, location);
		}

		[[nodiscard]] static std::optional<std::uint32_t> find_hresult(std::string_view message) noexcept
		{
			for (std::size_t index = 0; index + 10 <= message.size(); ++index)
			{
				if (message[index] != '0' || (message[index + 1] != 'x' && message[index + 1] != 'X'))
				{
					continue;
				}

				std::uint32_t value = 0;
				const auto first = message.data() + index + 2;
				const auto [end, error] = std::from_chars(first, first + 8, value, 16);
				if (error == std::errc{} && end == first + 8)
				{
					return value;
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] static std::string_view explain_hresult(std::uint32_t value) noexcept
		{
			switch (value)
			{
			case 0x80070057u:
				return "E_INVALIDARG (invalid parameter; Win32 error 87)";
			case 0x8007000Eu:
				return "E_OUTOFMEMORY (not enough memory)";
			case 0x887A0005u:
				return "DXGI_ERROR_DEVICE_REMOVED";
			case 0x887A0006u:
				return "DXGI_ERROR_DEVICE_HUNG";
			case 0x887A0007u:
				return "DXGI_ERROR_DEVICE_RESET";
			default:
				return {};
			}
		}
	};

	inline void check_sdl(bool success, std::string_view context = "SDL error: ",
	                      std::source_location location = std::source_location::current())
	{
		if (!success) [[unlikely]]
			throw sdl_error(context, location);
	}
} // namespace mngl

#if defined(MNGL_DETAIL_HAS_STD_STACKTRACE)
#undef MNGL_DETAIL_HAS_STD_STACKTRACE
#endif
