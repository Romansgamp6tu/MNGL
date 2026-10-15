#pragma once
#include "utilities.hpp"
#include "device.hpp"
#include "gpu_texture.hpp"

#include <stdexcept>
#include <thread>
#include <utility>


namespace mngl::gpu
{
	class command_buffer
	{
	public:
		// A raw command buffer must be wrapped on the thread that acquired it.
		explicit command_buffer(SDL_GPUCommandBuffer* cb) noexcept
			: owner_thread(std::this_thread::get_id()), handle(cb)
		{}

		command_buffer(const device& dev, SDL_GPUCommandBuffer* cb) noexcept
			: owner_thread(std::this_thread::get_id()), handle(cb), dev(dev.raw())
		{}
		
		command_buffer(const device& dev)
			: owner_thread(std::this_thread::get_id()), dev(dev.raw())
		{
			TRY_SDL(handle = SDL_AcquireGPUCommandBuffer(dev.raw()));
		}

		~command_buffer() noexcept
		{
			submit_noexcept();
		}

		// Запрещаем копирование
		command_buffer(const command_buffer&) = delete;
		command_buffer& operator=(const command_buffer&) = delete;

		// Command buffer may only be moved on the thread that acquired it.
		command_buffer(command_buffer&& o)
			: owner_thread(require_owner_thread(o))
			, handle(std::exchange(o.handle, nullptr))
			, dev(std::exchange(o.dev, nullptr))
			, has_acquired_swapchain_texture(
				std::exchange(o.has_acquired_swapchain_texture, false))
		{}

		command_buffer& operator=(command_buffer&& o)
		{
			if (this != &o)
			{
				ensure_owner_thread();
				o.ensure_owner_thread();
				submit_noexcept();
				handle = std::exchange(o.handle, nullptr);
				dev = std::exchange(o.dev, nullptr);
				has_acquired_swapchain_texture =
					std::exchange(o.has_acquired_swapchain_texture, false);
			}
			return *this;
		}

		SDL_GPUCommandBuffer* raw()
		{
			ensure_owner_thread();
			return handle;
		}

		const SDL_GPUCommandBuffer* raw() const
		{
			ensure_owner_thread();
			return handle;
		}

		SDL_GPUDevice* device_raw() const
		{
			ensure_owner_thread();
			return dev;
		}

		bool valid() const
		{
			ensure_owner_thread();
			return handle != nullptr;
		}

		// Явный сабмит буфера в очередь GPU
		void submit()
		{
			ensure_owner_thread();
			if (handle)
			{
				auto* pending = std::exchange(handle, nullptr);
				dev = nullptr;
				has_acquired_swapchain_texture = false;
				TRY_SDL(SDL_SubmitGPUCommandBuffer(pending));
			}
		}

		void cancel()
		{
			ensure_owner_thread();
			if (has_acquired_swapchain_texture)
			{
				throw std::logic_error(
					"Cannot cancel a command buffer after acquiring its swapchain texture");
			}
			if (handle)
			{
				auto* pending = std::exchange(handle, nullptr);
				dev = nullptr;
				TRY_SDL(SDL_CancelGPUCommandBuffer(pending));
			}
		}

		texture acquire_swapchain_texture(window& win)
		{
			ensure_owner_thread();
			if (!SDL_IsMainThread())
			{
				throw std::logic_error(
					"Swapchain textures must be acquired on the thread that created the window");
			}
			if (!handle)
			{
				throw std::logic_error("Cannot acquire a swapchain texture without a valid command buffer");
			}
			if (!dev)
			{
				throw std::logic_error("Cannot acquire a swapchain texture without its GPU device");
			}

			SDL_GPUTexture* tex_handle = nullptr;
			glm::uvec2 size = { 0, 0 };
			// The non-blocking API can return a null texture while frames are in flight.
			TRY_SDL(SDL_WaitAndAcquireGPUSwapchainTexture(
				handle,
				win.raw(),
				&tex_handle,
				&size.x,
				&size.y));
			has_acquired_swapchain_texture = tex_handle != nullptr;
			auto fmt = static_cast<texture_format>(SDL_GetGPUSwapchainTextureFormat(dev, win.raw()));
			return texture(dev, tex_handle, fmt, size);
		}

	private:
		static std::thread::id require_owner_thread(const command_buffer& cmd)
		{
			cmd.ensure_owner_thread();
			return cmd.owner_thread;
		}

		[[nodiscard]] bool on_owner_thread() const noexcept
		{
			return owner_thread == std::this_thread::get_id();
		}

		void ensure_owner_thread() const
		{
			if (!on_owner_thread())
			{
				throw std::logic_error(
					"GPU command buffer may only be used on the thread that acquired it");
			}
		}

		void submit_noexcept() noexcept
		{
			if (!on_owner_thread())
			{
				SDL_LogError(
					SDL_LOG_CATEGORY_APPLICATION,
					"GPU command buffer was destroyed on a thread other than the one that acquired it; "
					"it was not submitted");
				return;
			}

			if (handle)
			{
				auto* pending = std::exchange(handle, nullptr);
				dev = nullptr;
				has_acquired_swapchain_texture = false;
				if (!SDL_SubmitGPUCommandBuffer(pending))
				{
					SDL_LogError(
						SDL_LOG_CATEGORY_APPLICATION,
						"Failed to submit GPU command buffer during cleanup: %s",
						SDL_GetError());
				}
			}
		}

		const std::thread::id owner_thread;
		SDL_GPUCommandBuffer* handle = nullptr;
		SDL_GPUDevice* dev = nullptr;
		bool has_acquired_swapchain_texture = false;
	};
}