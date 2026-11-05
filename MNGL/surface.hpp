#pragma once

#include <cstddef>
#include "utilities.hpp"
#include "iupdate.h"
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
			if (!animated())
				return;
			accum += dt * 1'000;
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