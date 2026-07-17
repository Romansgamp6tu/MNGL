#pragma once
#include "utilities.hpp"
#include "device.hpp"
#include "texture.hpp"


namespace mngl::gpu
{
	class command_buffer
	{
	public:
		explicit command_buffer(SDL_GPUCommandBuffer* cb) : handle(cb) {}
		
		command_buffer(const device& dev)
		{
			TRY_SDL(handle = SDL_AcquireGPUCommandBuffer(dev.raw()));
			this->dev = dev.raw();
		}

		~command_buffer()
		{
			if (handle) submit();
		}

		// Запрещаем копирование
		command_buffer(const command_buffer&) = delete;
		command_buffer& operator=(const command_buffer&) = delete;

		// Разрешаем перемещение
		command_buffer(command_buffer&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
		command_buffer& operator=(command_buffer&& o) noexcept
		{
			if (this != &o)
			{
				handle = o.handle;
				o.handle = nullptr;
			}
			return *this;
		}

		SDL_GPUCommandBuffer* raw() { return handle; }
		const SDL_GPUCommandBuffer* raw() const { return handle; }
		bool valid() const { return handle != nullptr; }

		// Явный сабмит буфера в очередь GPU
		void submit()
		{
			if (handle)
			{
				SDL_SubmitGPUCommandBuffer(handle);
				handle = nullptr; // обнуляем, так как буфер улетел и больше нам не принадлежит
			}
		}

		texture acquire_swapchain_texture(window& win)
		{
			SDL_GPUTexture* tex_handle = nullptr;
			glm::uvec2 size = { 0, 0 };
			TRY_SDL(SDL_AcquireGPUSwapchainTexture(handle, win.raw(), &tex_handle, &size.x, &size.y));
			auto fmt = static_cast<texture_format>(SDL_GetGPUSwapchainTextureFormat(dev, win.raw()));
			return texture(dev, tex_handle, fmt, size);
		}

	private:
		SDL_GPUCommandBuffer* handle = nullptr;
		SDL_GPUDevice* dev;
	};
}