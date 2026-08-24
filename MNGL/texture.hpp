#pragma once

#include "command_buffer.hpp"
#include "gpu_texture.hpp"

namespace mngl
{
	class texture
	{
	public:
		using format = gpu::texture_format;
		using usage = gpu::texture::usage_flags;

		texture() = default;

		// 1. Создание пустой текстуры (для Render Target или вычислительных шейдеров)
		texture(
			gpu::device& dev,
			uint32_t width,
			uint32_t height,
			format fmt = format::r8g8b8a8_unorm,
			uint32_t usg = usage::sampler | usage::color_target
		)
			: m_gpu_tex(dev, glm::uvec2{width, height}, fmt, usg)
		{ }

		// 2. Создание текстуры из готового SDL_Surface
		texture(gpu::device& dev, gpu::command_buffer& cmd, SDL_Surface* surface)
		{
			load_from_surface(dev, cmd, surface);
		}

		// 3. Создание текстуры из массива float-данных (для HDR, симуляций, высотных карт)
		texture(
			gpu::device& dev,
			gpu::command_buffer& cmd,
			uint32_t width,
			uint32_t height,
			std::span<const float> float_pixels,
			format fmt = format::r32g32b32a32_float
		)
		{
			m_gpu_tex = gpu::texture(dev, glm::uvec2{width, height}, fmt, usage::sampler | usage::color_target);
			upload_bytes(cmd, float_pixels.data(), static_cast<uint32_t>(float_pixels.size_bytes()));
		}

		void load_from_surface(const gpu::device& dev, gpu::command_buffer& cmd, SDL_Surface* surface)
		{
			if (!surface)
			{
				throw std::invalid_argument("Cannot create texture from null SDL_Surface!");
			}

			// Конвертируем в стандартизированный RGBA32
			SDL_Surface* formatted_surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
			if (!formatted_surface)
			{
				throw sdl_error("Failed to convert surface format: ");
			}

			const glm::uvec2 img_size{
				static_cast<uint32_t>(formatted_surface->w),
				static_cast<uint32_t>(formatted_surface->h)
			};

			// Создаем / Пересоздаем GPU ресурс
			m_gpu_tex = gpu::texture(
				dev,
				img_size,
				gpu::texture_format::r8g8b8a8_unorm,
				gpu::texture::usage_flags::sampler | gpu::texture::usage_flags::color_target
			);

			upload_bytes(
				cmd,
				formatted_surface->pixels,
				static_cast<uint32_t>(img_size.y * formatted_surface->pitch)
			);

			SDL_DestroySurface(formatted_surface);
		}

		template<typename T>
		void update_pixels(gpu::command_buffer& cmd, std::span<const T> pixels)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Pixel data must be trivially copyable!");
			upload_bytes(cmd, pixels.data(), static_cast<uint32_t>(pixels.size_bytes()));
		}

		[[nodiscard]] glm::uvec2 size() const noexcept { return m_gpu_tex.get_size(); }
		[[nodiscard]] uint32_t width() const noexcept { return m_gpu_tex.width(); }
		[[nodiscard]] uint32_t height() const noexcept { return m_gpu_tex.height(); }
		[[nodiscard]] gpu::texture_format format() const noexcept { return m_gpu_tex.format(); }

		[[nodiscard]] const gpu::texture& raw_gpu_texture() const noexcept { return m_gpu_tex; }
		[[nodiscard]] gpu::texture& raw_gpu_texture() noexcept { return m_gpu_tex; }





	private:
		gpu::texture m_gpu_tex;

		void upload_bytes(gpu::command_buffer& cmd, const void* data, uint32_t data_size)
		{
			if (!data || data_size == 0 || !m_gpu_tex.valid()) return;

			SDL_GPUDevice* device_raw = m_gpu_tex.device_raw();

			// 1. Создаем трансферный буфер
			SDL_GPUTransferBufferCreateInfo transfer_info{};
			transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			transfer_info.size = data_size;

			SDL_GPUTransferBuffer* transfer_buf = SDL_CreateGPUTransferBuffer(device_raw, &transfer_info);
			if (!transfer_buf)
			{
				throw sdl_error("Failed to create transfer buffer for texture: ");
			}

			// 2. Копируем в RAM/VRAM видимую область
			void* mapped = SDL_MapGPUTransferBuffer(device_raw, transfer_buf, false);
			if (!mapped)
			{
				SDL_ReleaseGPUTransferBuffer(device_raw, transfer_buf);
				throw sdl_error("Failed to map transfer buffer: ");
			}

			std::memcpy(mapped, data, data_size);
			SDL_UnmapGPUTransferBuffer(device_raw, transfer_buf);

			// 3. Передаем в texture через CopyPass
			SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd.raw());
			if (!copy_pass)
			{
				SDL_ReleaseGPUTransferBuffer(device_raw, transfer_buf);
				throw sdl_error("Failed to begin copy pass for texture upload: ");
			}

			SDL_GPUTextureTransferInfo src_info{};
			src_info.transfer_buffer = transfer_buf;
			src_info.offset = 0;
			src_info.pixels_per_row = m_gpu_tex.width();
			src_info.rows_per_layer = m_gpu_tex.height();

			SDL_GPUTextureRegion dst_region{};
			dst_region.texture = m_gpu_tex.raw();
			dst_region.w = m_gpu_tex.width();
			dst_region.h = m_gpu_tex.height();
			dst_region.d = 1;

			SDL_UploadToGPUTexture(copy_pass, &src_info, &dst_region, false);
			SDL_EndGPUCopyPass(copy_pass);

			SDL_ReleaseGPUTransferBuffer(device_raw, transfer_buf);
		}
	};
}