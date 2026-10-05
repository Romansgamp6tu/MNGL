#pragma once

#include "color.hpp"
#include "command_buffer.hpp"
#include "gpu_texture.hpp"
#include "rectangle.hpp"
#include "surface.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace mngl
{
	class texture;
	struct sprite;

	namespace two_d
	{
		struct circle;
		struct ellipse;
		struct rounded_rectangle;
	}

	namespace detail
	{
		class texture_render_session
		{
		public:
			virtual ~texture_render_session() = default;
			virtual void draw(const sprite& value) = 0;
			virtual void draw(const two_d::circle& value) = 0;
			virtual void draw(const two_d::ellipse& value) = 0;
			virtual void draw(const two_d::rounded_rectangle& value) = 0;
			virtual void draw(const rectangle& bounds, const color& tint) = 0;
			virtual void draw(const texture& source, const rectangle& bounds, const color& tint) = 0;
			virtual void display() = 0;
		};

		struct surface_deleter
		{
			void operator()(SDL_Surface* s) const noexcept { if (s) SDL_DestroySurface(s); }
		};
		using surface_ptr = std::unique_ptr<SDL_Surface, surface_deleter>;

		// Освобождает transfer buffer при любом выходе из области видимости
		class transfer_buffer_guard
		{
		public:
			transfer_buffer_guard(SDL_GPUDevice* device, SDL_GPUTransferBuffer* buffer) noexcept
				: dev(device), buf(buffer) {}
			~transfer_buffer_guard()
			{
				if (buf) SDL_ReleaseGPUTransferBuffer(dev, buf);
			}
			transfer_buffer_guard(const transfer_buffer_guard&) = delete;
			transfer_buffer_guard& operator=(const transfer_buffer_guard&) = delete;

		private:
			SDL_GPUDevice* dev;
			SDL_GPUTransferBuffer* buf;
		};
	}

	class texture
	{
	public:
		using pixel_format = gpu::texture_format;
		using usage = gpu::texture::usage_flags;

		// Uploaded textures can be sampled and used as color render targets by default.
		static constexpr uint32_t default_upload_usage = usage::sampler | usage::color_target;

		texture() = default;
		texture(const texture&) = delete;
		texture& operator=(const texture&) = delete;
		texture(texture&& other);
		texture& operator=(texture&& other);
		~texture() = default;

		// 1. Создание пустой текстуры (для Render Target или вычислительных шейдеров)
		texture(
			const gpu::device& dev,
			uint32_t width,
			uint32_t height,
			pixel_format fmt = pixel_format::r8g8b8a8_unorm,
			uint32_t usg = usage::sampler | usage::color_target
		)
			: m_gpu_tex(dev, glm::uvec2{ width, height }, fmt, usg)
		{ }

		texture(
			uint32_t width,
			uint32_t height,
			pixel_format fmt = pixel_format::r8g8b8a8_unorm,
			uint32_t usg = usage::sampler | usage::color_target
		)
			: texture(gpu::device::global(), width, height, fmt, usg)
		{ }

		// 2. Создание текстуры из готового SDL_Surface
		texture(
			const gpu::device& dev,
			gpu::command_buffer& cmd,
			SDL_Surface* surface,
			uint32_t usg = default_upload_usage
		)
		{
			load_from_surface(dev, cmd, surface, usg);
		}

		explicit texture(
			SDL_Surface* surface,
			uint32_t usg = default_upload_usage
		)
		{
			auto& device = gpu::device::global();
			gpu::command_buffer cmd(device);
			load_from_surface(device, cmd, surface, usg);
			cmd.submit();
		}

		// Uploads only the surface's current frame.
		texture(
			const gpu::device& dev,
			gpu::command_buffer& cmd,
			const mngl::surface& source,
			uint32_t usg = default_upload_usage
		)
		{
			load_from_surface(dev, cmd, source.raw(), usg);
		}

		explicit texture(
			const mngl::surface& source,
			uint32_t usg = default_upload_usage
		)
			: texture(source.raw(), usg)
		{ }

		// 3. Создание текстуры из массива float-данных (для HDR, симуляций, высотных карт)
		texture(
			const gpu::device& dev,
			gpu::command_buffer& cmd,
			uint32_t width,
			uint32_t height,
			std::span<const float> float_pixels,
			pixel_format fmt = pixel_format::r32g32b32a32_float,
			uint32_t usg = default_upload_usage
		)
		{
			gpu::texture tex(dev, glm::uvec2{ width, height }, fmt, usg);
			upload_raw(tex, cmd, float_pixels.data(), float_pixels.size_bytes());
			m_gpu_tex = std::move(tex);
		}

		texture(
			uint32_t width,
			uint32_t height,
			std::span<const float> float_pixels,
			pixel_format fmt = pixel_format::r32g32b32a32_float,
			uint32_t usg = default_upload_usage
		)
		{
			auto& device = gpu::device::global();
			gpu::command_buffer cmd(device);
			gpu::texture tex(device, glm::uvec2{ width, height }, fmt, usg);
			upload_raw(tex, cmd, float_pixels.data(), float_pixels.size_bytes());
			m_gpu_tex = std::move(tex);
			cmd.submit();
		}

		// Гарантия: при исключении текущая текстура остаётся нетронутой
		void load_from_surface(
			const gpu::device& dev,
			gpu::command_buffer& cmd,
			SDL_Surface* surface,
			uint32_t usg = default_upload_usage
		)
		{
			if (!surface)
			{
				throw std::invalid_argument("Cannot create texture from null SDL_Surface!");
			}

			// RGBA32 — байтовый порядок R,G,B,A независимо от endianness (в отличие от RGBA8888)
			detail::surface_ptr converted(SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32));
			if (!converted)
			{
				throw sdl_error("Failed to convert surface format: ");
			}
			if (converted->w <= 0 || converted->h <= 0 || !converted->pixels || converted->pitch < converted->w * 4)
			{
				throw std::invalid_argument("SDL_Surface has invalid dimensions or pixel data");
			}

			const uint32_t w = static_cast<uint32_t>(converted->w);
			const uint32_t h = static_cast<uint32_t>(converted->h);

			gpu::texture tex(dev, glm::uvec2{ w, h }, pixel_format::r8g8b8a8_unorm, usg);
			upload_rows(
				tex, cmd,
				converted->pixels,
				static_cast<std::size_t>(w) * 4,
				static_cast<std::size_t>(converted->pitch),
				h
			);
			m_gpu_tex = std::move(tex);
		}

		// Размер данных должен точно соответствовать размеру текстуры в её формате
		template<typename T>
		void update_pixels(gpu::command_buffer& cmd, std::span<const T> pixels)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Pixel data must be trivially copyable!");
			upload_raw(m_gpu_tex, cmd, pixels.data(), pixels.size_bytes());
		}

		[[nodiscard]] bool valid() const noexcept { return m_gpu_tex.valid(); }
		explicit operator bool() const noexcept { return valid(); }

		[[nodiscard]] glm::uvec2 size() const noexcept { return m_gpu_tex.get_size(); }
		[[nodiscard]] uint32_t width() const noexcept { return m_gpu_tex.width(); }
		[[nodiscard]] uint32_t height() const noexcept { return m_gpu_tex.height(); }
		// Drawing coordinates start at the bottom-left; the shorter side is 1 and the longer follows the texture aspect ratio.
		[[nodiscard]] glm::vec2 coordinate_size() const noexcept
		{
			const auto dimensions = size();
			const float short_side = static_cast<float>(
				dimensions.x < dimensions.y ? dimensions.x : dimensions.y);
			if (short_side == 0.0f)
			{
				return {};
			}
			return {
				static_cast<float>(dimensions.x) / short_side,
				static_cast<float>(dimensions.y) / short_side
			};
		}
		[[nodiscard]] gpu::texture_format format() const noexcept { return m_gpu_tex.format(); }
		[[nodiscard]] bool can_render_to() const noexcept
		{
			return valid() && m_gpu_tex.has_usage(usage::color_target);
		}
		[[nodiscard]] bool rendering() const noexcept
		{
			return m_render_session != nullptr || m_external_render_active;
		}

		[[nodiscard]] const gpu::texture& raw_gpu_texture() const noexcept { return m_gpu_tex; }
		[[nodiscard]] gpu::texture& raw_gpu_texture() noexcept { return m_gpu_tex; }

		void clear(const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f));
		void draw(const sprite& value);
		void draw(const two_d::circle& value);
		void draw(const two_d::ellipse& value);
		void draw(const two_d::rounded_rectangle& value);
		void draw(
			const rectangle& bounds,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f)
		);
		// The source is borrowed until display() submits the target texture's frame.
		void draw(
			const texture& source,
			const rectangle& bounds,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f)
		);
		void display();

	private:
		friend class render_window;

		gpu::texture m_gpu_tex;
		std::unique_ptr<detail::texture_render_session> m_render_session;
		bool m_external_render_active = false;

		explicit texture(gpu::texture&& texture) noexcept
			: m_gpu_tex(std::move(texture))
		{ }

		void begin_external_render()
		{
			if (rendering())
			{
				throw std::logic_error("Texture is already being rendered to");
			}
			m_external_render_active = true;
		}

		void end_external_render() noexcept
		{
			m_external_render_active = false;
		}

		static uint32_t checked_size(std::size_t size)
		{
			if (size > std::numeric_limits<uint32_t>::max())
			{
				throw std::length_error("Texture data exceeds SDL's 32-bit size limit");
			}
			return static_cast<uint32_t>(size);
		}

		// Плотно упакованные данные всей текстуры
		static void upload_raw(gpu::texture& tex, gpu::command_buffer& cmd, const void* data, std::size_t bytes)
		{
			upload_rows(tex, cmd, data, bytes, bytes, 1);
		}

		// Копирует rows строк по row_bytes байт (шаг источника src_stride) в плотный transfer buffer
		static void upload_rows(
			gpu::texture& tex,
			gpu::command_buffer& cmd,
			const void* data,
			std::size_t row_bytes,
			std::size_t src_stride,
			std::size_t rows)
		{
			if (!tex.valid())
			{
				throw std::logic_error("Cannot upload pixels to an invalid texture");
			}
			if (!cmd.valid())
			{
				throw std::invalid_argument("Cannot upload texture data with an invalid command buffer");
			}
			SDL_GPUDevice* device_raw = tex.device_raw();
			if (cmd.device_raw() != device_raw)
			{
				throw std::invalid_argument("Texture and command buffer belong to different GPU devices");
			}
			if (!data)
			{
				throw std::invalid_argument("Texture pixel data is null");
			}
			if (src_stride < row_bytes)
			{
				throw std::invalid_argument("Texture row stride is smaller than row size");
			}

			const uint32_t expected = SDL_CalculateGPUTextureFormatSize(
				static_cast<SDL_GPUTextureFormat>(tex.format()), tex.width(), tex.height(), 1);
			const std::size_t total = row_bytes * rows;
			if (expected == 0 || total != expected)
			{
				throw std::invalid_argument("Texture pixel data size does not match texture dimensions and format");
			}
			const uint32_t data_size = checked_size(total);

			SDL_GPUTransferBufferCreateInfo transfer_info{};
			transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			transfer_info.size = data_size;

			SDL_GPUTransferBuffer* transfer_buf = SDL_CreateGPUTransferBuffer(device_raw, &transfer_info);
			if (!transfer_buf)
			{
				throw sdl_error("Failed to create transfer buffer for texture: ");
			}
			detail::transfer_buffer_guard guard(device_raw, transfer_buf);

			void* mapped = SDL_MapGPUTransferBuffer(device_raw, transfer_buf, false);
			if (!mapped)
			{
				throw sdl_error("Failed to map transfer buffer: ");
			}

			if (src_stride == row_bytes)
			{
				std::memcpy(mapped, data, total);
			}
			else
			{
				const auto* src = static_cast<const std::byte*>(data);
				auto* dst = static_cast<std::byte*>(mapped);
				for (std::size_t y = 0; y < rows; ++y)
				{
					std::memcpy(dst + y * row_bytes, src + y * src_stride, row_bytes);
				}
			}
			SDL_UnmapGPUTransferBuffer(device_raw, transfer_buf);

			SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd.raw());
			if (!copy_pass)
			{
				throw sdl_error("Failed to begin copy pass for texture upload: ");
			}

			// Нули = плотная упаковка по размеру региона
			SDL_GPUTextureTransferInfo src_info{};
			src_info.transfer_buffer = transfer_buf;

			SDL_GPUTextureRegion dst_region{};
			dst_region.texture = tex.raw();
			dst_region.w = tex.width();
			dst_region.h = tex.height();
			dst_region.d = 1;

			SDL_UploadToGPUTexture(copy_pass, &src_info, &dst_region, false);
			SDL_EndGPUCopyPass(copy_pass);
		}
	};
}
