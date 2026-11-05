#pragma once

#include <SDL3/SDL_error.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace mngl
{
	class sdl_error : public std::runtime_error
	{
	public:
		explicit sdl_error(std::string_view context)
		    : std::runtime_error(std::string(context) + SDL_GetError())
		{
		}

		// Use an already captured error; do not append SDL's mutable error state.
		[[nodiscard]] static sdl_error from_message(std::string message)
		{
			return sdl_error(std::move(message), captured_error{});
		}

	private:
		struct captured_error
		{
		};
		sdl_error(std::string message, captured_error) : std::runtime_error(std::move(message)) {}
	};

	inline void check_sdl(bool success, std::string_view context = "SDL error: ")
	{
		if (!success)
			throw sdl_error(context);
	}
} // namespace mngl
