#pragma once

#include "utilities.hpp"
#include <bitset>

#include "event_structs.hpp"

namespace mngl
{
	enum class MouseButton : uint8_t
	{
		left = 1, // SDL_BUTTON_LEFT
		middle = 2, // SDL_BUTTON_MIDDLE
		right = 3, // SDL_BUTTON_RIGHT
		x1 = 4, // SDL_BUTTON_X1
		x2 = 5  // SDL_BUTTON_X2
	};
}