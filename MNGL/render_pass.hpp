#pragma once

#include "utilities.hpp"
#include "color.hpp"
#include "command_buffer.hpp"
#include "texture.hpp"

namespace mngl::gpu
{
	class render_pass
	{
	public:

		enum class loader
		{
			clear =		SDL_GPU_LOADOP_CLEAR,
			load =		SDL_GPU_LOADOP_LOAD,
			dont_care =	SDL_GPU_LOADOP_DONT_CARE
		};

		enum class storer
		{
			store =		SDL_GPU_STOREOP_STORE,
			dont_care =	SDL_GPU_STOREOP_DONT_CARE
		};

		struct color_attachment
		{
			texture& target;
			color col = color(0);
			loader load = loader::clear;
			storer store = storer::store;
		};

		struct depth_attachment
		{
			texture& target;
			float depth = 1;
			uint8_t stencil = 0;
			loader load = loader::clear;
			storer store = storer::store;
		};

		//ez constructor
		render_pass(command_buffer& cmd, texture& target, const color& col = color(0), loader ld = loader::clear, storer st = storer::store, const depth_attachment* depth = nullptr) :
			render_pass(cmd, {{target, col, ld, st}}, depth)
		{		}

		render_pass(command_buffer& cmd, std::initializer_list<color_attachment> color_targets, const depth_attachment* depth = nullptr)
		{
			if (color_targets.empty()) throw std::invalid_argument("render pass must have to have at least one target!");

			std::vector<SDL_GPUColorTargetInfo> sdl_color_targets;
			sdl_color_targets.reserve(color_targets.size());

			for (auto& attachment : color_targets)
			{
				if (!attachment.target.has_usage(texture::usage_flags::color_target)) throw std::invalid_argument("target texture must have to have color_target usage flag!");
				SDL_GPUColorTargetInfo info{};
				info.texture = attachment.target.raw();
				info.clear_color = SDL_FColor{ attachment.col.r, attachment.col.g, attachment.col.b, attachment.col.a };
				info.load_op = static_cast<SDL_GPULoadOp>(attachment.load);
				info.store_op = static_cast<SDL_GPUStoreOp>(attachment.store);

				sdl_color_targets.push_back(info);
			}

			SDL_GPUDepthStencilTargetInfo sdl_depth_info{};
			SDL_GPUDepthStencilTargetInfo* depth_ptr = nullptr;

			if (depth)
			{
				auto& attachment = *depth;
				if (!attachment.target.has_usage(texture::usage_flags::stencil_target)) throw std::invalid_argument("target texture must have to have color_target usage flag!");
				sdl_depth_info.texture = depth->target.raw();
				sdl_depth_info.clear_depth = depth->depth;
				sdl_depth_info.clear_stencil = depth->stencil;
				sdl_depth_info.load_op = static_cast<SDL_GPULoadOp>(depth->load);
				sdl_depth_info.store_op = static_cast<SDL_GPUStoreOp>(depth->store);
				sdl_depth_info.stencil_load_op = static_cast<SDL_GPULoadOp>(depth->load);
				sdl_depth_info.stencil_store_op = static_cast<SDL_GPUStoreOp>(depth->store);

				depth_ptr = &sdl_depth_info;
			}

			handle = SDL_BeginGPURenderPass(
				cmd.raw(),
				sdl_color_targets.data(),
				static_cast<uint32_t>(sdl_color_targets.size()),
				depth_ptr
			);
		}

		~render_pass()
		{
			if (handle)
			{
				SDL_EndGPURenderPass(handle);
			}
		}

		render_pass(const render_pass&) = delete;
		render_pass& operator=(const render_pass&) = delete;

		// Разрешаем перемещение
		render_pass(render_pass&& o) noexcept { swap(*this, o); }
		render_pass& operator=(render_pass&& o) noexcept
		{
			swap(*this, o);
			return *this;
		}

		SDL_GPURenderPass* raw() const { return handle; }
		bool valid() const { return handle != nullptr; }

		void end_pass()
		{
			if (handle)
			{
				SDL_EndGPURenderPass(handle);
			}
			handle = nullptr;
		}

	private:
		SDL_GPURenderPass* handle = nullptr;

		friend inline void swap(render_pass& l, render_pass& r) noexcept
		{
			using std::swap;
			swap(l.handle, r.handle);
		}
	};
}