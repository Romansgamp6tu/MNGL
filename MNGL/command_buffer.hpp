#pragma once
#include "utilities.hpp"
#include "device.hpp"
#include "gpu_texture.hpp"

#include <stdexcept>
#include <optional>
#include <thread>
#include <utility>

namespace mngl::gpu
{
	class command_buffer
	{
	public:
		// Adopts a raw command buffer without a device lease; the caller keeps the device alive.
		// It must be wrapped on the thread that acquired it.
		explicit command_buffer(SDL_GPUCommandBuffer* cb) noexcept
		    : owner_thread(std::this_thread::get_id()), handle(cb)
		{
		}

		command_buffer(const device& dev, SDL_GPUCommandBuffer* cb) noexcept
		    : owner_thread(std::this_thread::get_id()), m_device_owner(dev), handle(cb), dev(dev.raw())
		{
		}

		command_buffer(const device& dev)
		    : owner_thread(std::this_thread::get_id()), m_device_owner(dev), dev(dev.raw())
		{
			::mngl::check_sdl(handle = SDL_AcquireGPUCommandBuffer(dev.raw()));
		}

		// Unsubmitted work is canceled on destruction. Call submit() to execute it.
		~command_buffer() noexcept { cancel_noexcept(); }

		// Запрещаем копирование
		command_buffer(const command_buffer&) = delete;
		command_buffer& operator=(const command_buffer&) = delete;

		// Command buffer may only be moved on the thread that acquired it.
		command_buffer(command_buffer&& o)
		    : owner_thread(require_owner_thread(o)), m_device_owner(std::move(o.m_device_owner)),
		      handle(std::exchange(o.handle, nullptr)), dev(std::exchange(o.dev, nullptr))
		{
			o.m_device_owner.reset();
		}

		command_buffer& operator=(command_buffer&& o)
		{
			if (this != &o)
			{
				ensure_owner_thread();
				o.ensure_owner_thread();
				cancel_noexcept();
				m_device_owner = std::move(o.m_device_owner);
				o.m_device_owner.reset();
				handle = std::exchange(o.handle, nullptr);
				dev = std::exchange(o.dev, nullptr);
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

		// Explicitly submit recorded work to the GPU queue.
		void submit()
		{
			ensure_owner_thread();
			if (handle)
			{
				auto* pending = std::exchange(handle, nullptr);
				dev = nullptr;
				::mngl::check_sdl(SDL_SubmitGPUCommandBuffer(pending));
				m_device_owner.reset();
			}
		}

		void cancel()
		{
			ensure_owner_thread();
			if (handle)
			{
				auto* pending = std::exchange(handle, nullptr);
				dev = nullptr;
				::mngl::check_sdl(SDL_CancelGPUCommandBuffer(pending));
				m_device_owner.reset();
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
			if (!m_device_owner)
			{
				throw std::logic_error(
				    "Cannot create an owned swapchain texture from a borrowed command buffer");
			}

			SDL_GPUTexture* tex_handle = nullptr;
			glm::uvec2 size = {0, 0};
			::mngl::check_sdl(
			    SDL_AcquireGPUSwapchainTexture(handle, win.raw(), &tex_handle, &size.x, &size.y));
			auto fmt = static_cast<texture_format>(SDL_GetGPUSwapchainTextureFormat(dev, win.raw()));
			return texture(*m_device_owner, tex_handle, fmt, size);
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
				throw std::logic_error("GPU command buffer may only be used on the thread that acquired it");
			}
		}

		void cancel_noexcept() noexcept
		{
			if (!on_owner_thread())
			{
				SDL_LogError(
				    SDL_LOG_CATEGORY_APPLICATION,
				    "GPU command buffer was destroyed on a thread other than the one that acquired it; "
				    "it was not canceled");
				return;
			}

			if (handle)
			{
				auto* pending = std::exchange(handle, nullptr);
				dev = nullptr;
				if (!SDL_CancelGPUCommandBuffer(pending))
				{
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
					             "Failed to cancel GPU command buffer during cleanup: %s", SDL_GetError());
				}
				m_device_owner.reset();
			}
		}

		const std::thread::id owner_thread;
		std::optional<device> m_device_owner;
		SDL_GPUCommandBuffer* handle = nullptr;
		SDL_GPUDevice* dev = nullptr;
	};
} // namespace mngl::gpu
