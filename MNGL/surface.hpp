#pragma once
#include "utilities.hpp"
#include "iupdate.h"
#include <string>
#include <fstream>
#include <functional>

namespace mngl
{
	class surface : Iupdatable
	{
	public:
		surface(const std::string& path)
		{
			if (IMG_Animation* a = IMG_LoadAnimation(path.c_str()))
			{
				if (anim->count > 1) anim = a;
				else
				{
					single = a->frames[0];
					IMG_FreeAnimation(a);
				}
			}
			else if(SDL_Surface* s = IMG_Load(path.c_str()))
			{
				single = s;
			}
			else
			{
				throw sdl_error("failed to load surface from " + path + ": ");
			}
		}

		~surface()
		{
			if (anim) IMG_FreeAnimation(anim);
			if (single) SDL_DestroySurface(single);
		}

		surface(surface&& other) noexcept = default;
		surface& operator=(surface&& other) noexcept = default;

		void update(double dt)
		{
			if (!anim || anim->count <= 1) return;
			accum += dt * 1'000;
			int cur_delay = anim->delays[current_frame];
			if (cur_delay <= 0) cur_delay = 100;
			while (accum >= cur_delay)
			{
				accum -= cur_delay;
				int cur_delay = anim->delays[current_frame];
				if (cur_delay <= 0) cur_delay = 100;

				current_frame = (current_frame + 1) % anim->count;
				if (on_frame_changed) on_frame_changed(anim->frames[current_frame]);
			}
		}

		SDL_Surface* raw() const noexcept
		{
			if (animated()) return anim->frames[current_frame];
			return single;
		}

		bool animated() const
		{
			return anim;
		}

		std::function<void(SDL_Surface*)> on_frame_changed;

	private:
		
		SDL_Surface* single;
		IMG_Animation* anim;

		size_t current_frame = 0;
		float accum = 0;
	};
}