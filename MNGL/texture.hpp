#pragma once

#include "utilities.hpp"
#include "device.hpp"

namespace mngl::gpu
{
	class texture
	{
	public:
		texture() {}
		~texture()
		{
			if (handle && !is_swapchain) SDL_ReleaseGPUTexture(dev, handle);
		}
		texture(const texture& other) = delete;
		texture& operator=(const texture& other) = delete;
		texture(texture&& other)
		{
			swap(*this, other);
		}
		texture& operator=(texture&& other)
		{
			swap(*this, other);
			return *this;
		}

		texture(const device& device, glm::uvec2 texture_size, texture_format format, uint32_t flags)
			: dev(device.raw()), fmt(format), usage_flag(flags), size(texture_size), is_swapchain(false)
		{
			SDL_GPUTextureCreateInfo create_info{};
			create_info.type = SDL_GPU_TEXTURETYPE_2D;
			create_info.format = static_cast<SDL_GPUTextureFormat>(format);
			create_info.usage = flags;
			create_info.width = size.x;
			create_info.height = size.y;
			create_info.layer_count_or_depth = 1;
			create_info.num_levels = 1; // Для начала без мип-мапов

			TRY_SDL(handle = SDL_CreateGPUTexture(dev, &create_info));
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
		bool valid() const { return handle != nullptr; }
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

		friend inline void swap(mngl::gpu::texture& l, mngl::gpu::texture& r)
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