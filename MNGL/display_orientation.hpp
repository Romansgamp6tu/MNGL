#pragma once

#include "utilities.hpp"

namespace mngl
{
	enum class display_orientation
	{
		unknown				= SDL_ORIENTATION_UNKNOWN,
		landscape			= SDL_ORIENTATION_LANDSCAPE,
		landscape_flipped	= SDL_ORIENTATION_LANDSCAPE_FLIPPED,
		portrait			= SDL_ORIENTATION_PORTRAIT,
		portrait_flipped	= SDL_ORIENTATION_PORTRAIT_FLIPPED
	};
}