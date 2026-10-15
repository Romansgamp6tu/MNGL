#pragma once

#include "utilities.hpp"
#include "device.hpp"

#include <stdexcept>
#include <utility>

namespace mngl::gpu
{
	class texture
	{
	public:
		texture() noexcept {}
		~texture()
		{
			destroy();
		}
		texture(const texture& other) = delete;
		texture& operator=(const texture& other) = delete;
		texture(texture&& other) noexcept
		{
			swap(*this, other);
		}
		texture& operator=(texture&& other) noexcept
		{
			if (this != &other)
			{
				destroy();
				swap(*this, other);
			}
			return *this;
		}

		texture(const device& device, glm::uvec2 texture_size, texture_format format, uint32_t flags)
			: dev(device.raw()), fmt(format), usage_flag(flags), size(texture_size), is_swapchain(false)
		{
			if (!dev)
			{
				throw std::invalid_argument("Cannot create a GPU texture with a null device");
			}
			if (size.x == 0 || size.y == 0)
			{
				throw std::invalid_argument("GPU texture dimensions must be non-zero");
			}
			if (!SDL_GPUTextureSupportsFormat(
				dev, static_cast<SDL_GPUTextureFormat>(format), SDL_GPU_TEXTURETYPE_2D, flags))
			{
				throw std::invalid_argument("GPU device does not support this texture format/usage combination");
			}

			SDL_GPUTextureCreateInfo create_info{};
			create_info.type = SDL_GPU_TEXTURETYPE_2D;
			create_info.format = static_cast<SDL_GPUTextureFormat>(format);
			create_info.usage = flags;
			create_info.width = size.x;
			create_info.height = size.y;
			create_info.layer_count_or_depth = 1;
			create_info.num_levels = 1; // Для начала без мип-мапов

			handle = SDL_CreateGPUTexture(dev, &create_info);
			if (!handle)
			{
				throw sdl_error("Failed to create GPU texture: ");
			}
		}

		struct usage_flags
		{
			constexpr static inline auto sampler			= SDL_GPU_TEXTUREUSAGE_SAMPLER;
			constexpr static inline auto color_target		= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
			constexpr static inline auto stencil_target	= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
			constexpr static inline auto graphics_read	= SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ;
			constexpr static inline auto compute_read		= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
			constexpr static inline auto compute_write	= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
			constexpr static inline auto compute_rw		= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE;
		};

		// Проверка на валидность
		[[nodiscard]] bool valid() const noexcept
		{
			return handle != nullptr &&
				dev != nullptr &&
				fmt != texture_format::invalid &&
				size.x != 0 &&
				size.y != 0;
		}
		explicit operator bool() const { return valid(); }

		// Доступ к сырым объектам SDL
		SDL_GPUTexture* raw() const { return handle; }
		SDL_GPUDevice* device_raw() const { return dev; }

		// Параметры текстуры
		glm::uvec2 get_size() const { return size; }
		uint32_t width() const { return size.x; }
		uint32_t height() const { return size.y; }
		texture_format format() const { return fmt; }

		// Проверка флагов использования (для ассертов)
		bool has_usage(uint32_t flag) const { return (usage_flag & flag) != 0; }
		bool is_swapchain_texture() const { return is_swapchain; }

		void destroy() noexcept
		{
			if (dev && handle && !is_swapchain)
			{
				SDL_ReleaseGPUTexture(dev, handle);
			}
			handle = nullptr;
			dev = nullptr;
			fmt = texture_format::invalid;
			usage_flag = 0;
			size = { 0, 0 };
			is_swapchain = false;
		}

	private:
		SDL_GPUTexture* handle	= nullptr;
		SDL_GPUDevice* dev		= nullptr;
		texture_format fmt		= texture_format::invalid;
		uint32_t usage_flag		= 0;
		glm::uvec2 size			= { 0, 0 };
		bool is_swapchain		= false;

		friend class command_buffer;

		texture(SDL_GPUDevice* device, SDL_GPUTexture* swapchain_handle, texture_format format, glm::uvec2 texture_size)
			: handle(swapchain_handle), dev(device), fmt(format),
			usage_flag(SDL_GPU_TEXTUREUSAGE_COLOR_TARGET), size(texture_size), is_swapchain(true)
		{}

		friend inline void swap(mngl::gpu::texture& l, mngl::gpu::texture& r) noexcept
		{
			using std::swap;
			swap(l.handle, r.handle);
			swap(l.dev, r.dev);
			swap(l.fmt, r.fmt);
			swap(l.usage_flag, r.usage_flag);
			swap(l.size, r.size);
			swap(l.is_swapchain, r.is_swapchain);
		}
	};
}