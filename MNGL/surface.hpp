#pragma once

#include <cstddef>
#include "iupdate.h"
#include "runtime.hpp"

#include <SDL3_image/SDL_image.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <fstream>
#include <functional>
#include <utility>

namespace mngl
{
	class surface : updatable
	{
	public:
		surface(const std::string& path)
		{
			if (IMG_Animation* a = IMG_LoadAnimation(path.c_str()))
			{
				anim = a;
			}
			else if (SDL_Surface* s = IMG_Load(path.c_str()))
			{
				single = s;
			}
			else
			{
				throw sdl_error("failed to load surface from " + path + ": ");
			}
		}

		~surface() { destroy(); }

		surface(surface&& other) noexcept
		    : updatable(), on_frame_changed(std::move(other.on_frame_changed)),
		      single(std::exchange(other.single, nullptr)), anim(std::exchange(other.anim, nullptr)),
		      current_frame(std::exchange(other.current_frame, 0)), accum(std::exchange(other.accum, 0.0))
		{
		}

		surface& operator=(surface&& other) noexcept
		{
			if (this != &other)
			{
				destroy();
				on_frame_changed = std::move(other.on_frame_changed);
				single = std::exchange(other.single, nullptr);
				anim = std::exchange(other.anim, nullptr);
				current_frame = std::exchange(other.current_frame, 0);
				accum = std::exchange(other.accum, 0.0);
			}
			return *this;
		}

		void update(double dt)
		{
			if (!std::isfinite(dt) || dt < 0.0)
			{
				throw std::invalid_argument("surface update time must be finite and non-negative");
			}
			const double elapsed_ms = dt * 1'000.0;
			if (!std::isfinite(elapsed_ms))
			{
				throw std::invalid_argument("surface update time is too large");
			}
			if (!animated())
				return;
			if (elapsed_ms > std::numeric_limits<double>::max() - accum)
			{
				throw std::invalid_argument("surface update time overflows the animation accumulator");
			}
			const double next_accum = accum + elapsed_ms;
			if (!std::isfinite(next_accum))
			{
				throw std::invalid_argument("surface update time overflows the animation accumulator");
			}

			constexpr std::size_t maximum_catch_up_frames = 4096;
			auto simulated_frame = current_frame;
			double simulated_accum = next_accum;
			std::size_t transitions = 0;
			for (;;)
			{
				int delay = anim->delays[simulated_frame];
				if (delay <= 0)
					delay = 100;
				if (simulated_accum < delay)
					break;
				if (transitions == maximum_catch_up_frames)
				{
					throw std::invalid_argument(
					    "surface update would require too many animation frame transitions");
				}

				const double remaining = simulated_accum - delay;
				if (remaining == simulated_accum)
				{
					throw std::invalid_argument(
					    "surface update time cannot be represented by the animation accumulator");
				}
				simulated_accum = remaining;
				simulated_frame = (simulated_frame + 1) % anim->count;
				++transitions;
			}

			accum = next_accum;
			int cur_delay = anim->delays[current_frame];
			if (cur_delay <= 0)
				cur_delay = 100;
			while (accum >= cur_delay)
			{
				accum -= cur_delay;
				current_frame = (current_frame + 1) % anim->count;
				if (on_frame_changed)
					on_frame_changed(anim->frames[current_frame]);
				cur_delay = anim->delays[current_frame];
				if (cur_delay <= 0)
					cur_delay = 100;
			}
		}

		SDL_Surface* raw() const noexcept
		{
			if (anim && anim->count > 0 && anim->frames)
				return anim->frames[current_frame];
			return single;
		}

		bool animated() const noexcept { return anim && anim->count > 1; }

		std::function<void(SDL_Surface*)> on_frame_changed;

	private:
		SDL_Surface* single = nullptr;
		IMG_Animation* anim = nullptr;

		std::size_t current_frame = 0;
		double accum = 0;

		void destroy() noexcept
		{
			if (anim)
				IMG_FreeAnimation(std::exchange(anim, nullptr));
			if (single)
				SDL_DestroySurface(std::exchange(single, nullptr));
			current_frame = 0;
			accum = 0.0;
		}
	};
} // namespace mngl
