#pragma once

#include "color.hpp"
#include "command_buffer.hpp"
#include "gpu_texture.hpp"
#include "gpu_buffer.hpp"
#include "hit_test.hpp"
#include "rectangle.hpp"
#include "surface.hpp"
#include "view.hpp"

#include <atomic>
#include <algorithm>
#include <cstddef>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace mngl
{
	class texture;
	class drawable;
	class imgui_context;

	namespace two_d
	{
		class frame_2d;
		class font;
		class recording_context;
		class renderer;
		class cached_layer;
	} // namespace two_d

	namespace detail
	{
		class texture_render_session
		{
		public:
			virtual ~texture_render_session() = default;
			virtual two_d::frame_2d& frame() noexcept = 0;
			virtual void display() = 0;
		};

		struct surface_deleter
		{
			void operator()(SDL_Surface* s) const noexcept
			{
				if (s)
					SDL_DestroySurface(s);
			}
		};
		using surface_ptr = std::unique_ptr<SDL_Surface, surface_deleter>;

		// Освобождает transfer buffer при любом выходе из области видимости
		class transfer_buffer_guard
		{
		public:
			transfer_buffer_guard(SDL_GPUDevice* device, SDL_GPUTransferBuffer* buffer) noexcept
			    : dev(device), buf(buffer)
			{
			}
			~transfer_buffer_guard()
			{
				if (buf)
					SDL_ReleaseGPUTransferBuffer(dev, buf);
			}
			transfer_buffer_guard(const transfer_buffer_guard&) = delete;
			transfer_buffer_guard& operator=(const transfer_buffer_guard&) = delete;

		private:
			SDL_GPUDevice* dev;
			SDL_GPUTransferBuffer* buf;
		};

		struct reusable_upload_buffer
		{
			reusable_upload_buffer(SDL_GPUDevice* owner, SDL_GPUTransferBuffer* value,
			                      std::uint32_t size) noexcept
			    : device(owner), handle(value), capacity(size)
			{
			}

			~reusable_upload_buffer()
			{
				if (handle)
				{
					SDL_ReleaseGPUTransferBuffer(device, handle);
				}
			}

			SDL_GPUDevice* device = nullptr;
			SDL_GPUTransferBuffer* handle = nullptr;
			std::uint32_t capacity = 0;
			bool in_flight = false;
		};
	} // namespace detail

	class texture
	{
	private:
		void reset_active_frame() noexcept;
		void cancel_active_frame(std::uint64_t generation) noexcept;

	public:
		using pixel_format = gpu::texture_format;
		using usage = gpu::texture::usage_flags;

		// Uploaded textures can be sampled and used as color render targets by default.
		static constexpr std::uint32_t default_upload_usage = usage::sampler | usage::color_target;

		texture() = default;
		texture(const texture&) = delete;
		texture& operator=(const texture&) = delete;
		texture(texture&& other);
		texture& operator=(texture&& other);
		~texture() = default;

		// 1. Создание пустой текстуры (для Render Target или вычислительных шейдеров)
		texture(const gpu::device& dev, std::uint32_t width, std::uint32_t height,
		        pixel_format fmt = pixel_format::r8g8b8a8_unorm,
		        std::uint32_t usg = usage::sampler | usage::color_target)
		    : m_gpu_tex(dev, glm::uvec2{width, height}, fmt, usg)
		{
		}

		texture(std::uint32_t width, std::uint32_t height, pixel_format fmt = pixel_format::r8g8b8a8_unorm,
		        std::uint32_t usg = usage::sampler | usage::color_target)
		    : texture(gpu::device::global(), width, height, fmt, usg)
		{
		}

		// 2. Создание текстуры из готового SDL_Surface
		texture(const gpu::device& dev, gpu::command_buffer& cmd, SDL_Surface* surface,
		        std::uint32_t usg = default_upload_usage)
		{
			load_from_surface(dev, cmd, surface, usg);
		}

		explicit texture(SDL_Surface* surface, std::uint32_t usg = default_upload_usage)
		{
			auto& device = gpu::device::global();
			gpu::command_buffer cmd(device);
			load_from_surface(device, cmd, surface, usg);
			cmd.submit();
		}

		// Uploads only the surface's current frame.
		texture(const gpu::device& dev, gpu::command_buffer& cmd, const mngl::surface& source,
		        std::uint32_t usg = default_upload_usage)
		{
			load_from_surface(dev, cmd, source.raw(), usg);
		}

		explicit texture(const mngl::surface& source, std::uint32_t usg = default_upload_usage)
		    : texture(source.raw(), usg)
		{
		}

		explicit texture(const std::string& path, std::uint32_t usg = default_upload_usage)
		{
			detail::surface_ptr image(IMG_Load(path.c_str()));
			if (!image) [[unlikely]]
			{
				throw sdl_error("Failed to load image from " + path + ": ");
			}

			auto& device = gpu::device::global();
			gpu::command_buffer cmd(device);
			load_from_surface(device, cmd, image.get(), usg);
			cmd.submit();
		}

		// 3. Создание текстуры из массива float-данных (для HDR, симуляций, высотных карт)
		texture(const gpu::device& dev, gpu::command_buffer& cmd, std::uint32_t width, std::uint32_t height,
		        std::span<const float> float_pixels, pixel_format fmt = pixel_format::r32g32b32a32_float,
		        std::uint32_t usg = default_upload_usage)
		{
			gpu::texture tex(dev, glm::uvec2{width, height}, fmt, usg);
			upload_raw(tex, cmd, float_pixels.data(), float_pixels.size_bytes());
			m_gpu_tex = std::move(tex);
		}

		texture(std::uint32_t width, std::uint32_t height, std::span<const float> float_pixels,
		        pixel_format fmt = pixel_format::r32g32b32a32_float, std::uint32_t usg = default_upload_usage)
		{
			auto& device = gpu::device::global();
			gpu::command_buffer cmd(device);
			gpu::texture tex(device, glm::uvec2{width, height}, fmt, usg);
			upload_raw(tex, cmd, float_pixels.data(), float_pixels.size_bytes());
			m_gpu_tex = std::move(tex);
			cmd.submit();
		}

		// Гарантия: при исключении текущая текстура остаётся нетронутой
		void load_from_surface(const gpu::device& dev, gpu::command_buffer& cmd, SDL_Surface* surface,
		                       std::uint32_t usg = default_upload_usage)
		{
			if (rendering()) [[unlikely]]
			{
				throw std::logic_error("Cannot replace a texture while it is being rendered to");
			}
			auto converted = convert_surface(surface);

			const std::uint32_t w = static_cast<std::uint32_t>(converted->w);
			const std::uint32_t h = static_cast<std::uint32_t>(converted->h);

			gpu::texture tex(dev, glm::uvec2{w, h}, pixel_format::r8g8b8a8_unorm, usg);
			upload_rows(tex, cmd, converted->pixels, static_cast<std::size_t>(w) * 4,
			            static_cast<std::size_t>(converted->pitch), h, 0, 0, 0, 0, this);
			m_gpu_tex = std::move(tex);
			invalidate_hit_test_frame();
		}

		// Updates an RGBA8 texture from a same-sized SDL surface.
		void update_from_surface(gpu::command_buffer& cmd, SDL_Surface* surface)
		{
			if (rendering()) [[unlikely]]
			{
				throw std::logic_error("Cannot update a texture while it is being rendered to");
			}
			if (!valid()) [[unlikely]]
			{
				throw std::logic_error("Cannot update an invalid texture from SDL_Surface");
			}

			auto converted = convert_surface(surface);
			if (m_gpu_tex.format() != pixel_format::r8g8b8a8_unorm) [[unlikely]]
			{
				throw std::invalid_argument("Updating from SDL_Surface requires an RGBA8 texture");
			}

			const std::uint32_t width = static_cast<std::uint32_t>(converted->w);
			const std::uint32_t height = static_cast<std::uint32_t>(converted->h);
			if (m_gpu_tex.width() != width || m_gpu_tex.height() != height) [[unlikely]]
			{
				throw std::invalid_argument("SDL_Surface dimensions must match the texture dimensions");
			}

			upload_rows(m_gpu_tex, cmd, converted->pixels, static_cast<std::size_t>(width) * 4,
			            static_cast<std::size_t>(converted->pitch), height, 0, 0, 0, 0, this);
			invalidate_hit_test_frame();
		}

		void update_from_surface(gpu::command_buffer& cmd, const mngl::surface& source)
		{
			update_from_surface(cmd, source.raw());
		}

		void update_from_surface(const gpu::device& dev, SDL_Surface* surface)
		{
			if (!valid()) [[unlikely]]
			{
				throw std::logic_error("Cannot update an invalid texture from SDL_Surface");
			}
			if (m_gpu_tex.device_raw() != dev.raw()) [[unlikely]]
			{
				throw std::invalid_argument("Texture and GPU device do not match");
			}

			gpu::command_buffer cmd(dev);
			update_from_surface(cmd, surface);
			cmd.submit();
		}

		void update_from_surface(const gpu::device& dev, const mngl::surface& source)
		{
			update_from_surface(dev, source.raw());
		}

		void update_from_surface(SDL_Surface* surface)
		{
			if (!valid()) [[unlikely]]
			{
				throw std::logic_error("Cannot update an invalid texture from SDL_Surface");
			}
			update_from_surface(gpu::device::global(), surface);
		}

		void update_from_surface(const mngl::surface& source) { update_from_surface(source.raw()); }

		// Размер данных должен точно соответствовать размеру текстуры в её формате
		template <typename T> void update_pixels(gpu::command_buffer& cmd, std::span<const T> pixels)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Pixel data must be trivially copyable!");
			upload_raw(m_gpu_tex, cmd, pixels.data(), pixels.size_bytes(), this);
			invalidate_hit_test_frame();
		}

		template <typename T> void update_pixels(const gpu::device& dev, std::span<const T> pixels)
		{
			if (!valid()) [[unlikely]]
			{
				throw std::logic_error("Cannot update an invalid texture");
			}
			if (m_gpu_tex.device_raw() != dev.raw()) [[unlikely]]
			{
				throw std::invalid_argument("Texture and GPU device do not match");
			}

			gpu::command_buffer cmd(dev);
			update_pixels(cmd, pixels);
			cmd.submit();
		}

		template <typename T> void update_pixels(std::span<const T> pixels)
		{
			if (!valid()) [[unlikely]]
			{
				throw std::logic_error("Cannot update an invalid texture");
			}
			update_pixels(gpu::device::global(), pixels);
		}

		[[nodiscard]] bool valid() const noexcept { return m_gpu_tex.valid(); }
		explicit operator bool() const noexcept { return valid(); }

		[[nodiscard]] glm::uvec2 size() const noexcept { return m_gpu_tex.get_size(); }
		[[nodiscard]] std::uint32_t width() const noexcept { return m_gpu_tex.width(); }
		[[nodiscard]] std::uint32_t height() const noexcept { return m_gpu_tex.height(); }
		// Without a custom view, drawing coordinates start at the bottom-left and the shorter texture side
		// is 1.
		[[nodiscard]] glm::vec2 coordinate_size() const
		{
			const auto dimensions = size();
			const float short_side =
			    static_cast<float>(dimensions.x < dimensions.y ? dimensions.x : dimensions.y);
			if (short_side == 0.0f)
			{
				return {};
			}
			if (m_view)
			{
				return m_view->visible_area(dimensions).size;
			}
			return {static_cast<float>(dimensions.x) / short_side,
			        static_cast<float>(dimensions.y) / short_side};
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

		void set_view(const view& value)
		{
			if (rendering()) [[unlikely]]
			{
				throw std::logic_error("Cannot change a texture view while the texture is being rendered to");
			}
			m_view = value;
		}

		void reset_view()
		{
			if (rendering()) [[unlikely]]
			{
				throw std::logic_error("Cannot reset a texture view while the texture is being rendered to");
			}
			m_view.reset();
		}

		[[nodiscard]] std::optional<view> current_view() const { return m_view; }
		[[nodiscard]] std::optional<::mngl::view> get_view() const { return current_view(); }

		void enable_hit_testing(bool enabled = true)
		{
			if (rendering()) [[unlikely]]
			{
				throw std::logic_error("Cannot change hit testing while the texture is being rendered to");
			}
			if (enabled && !can_render_to()) [[unlikely]]
			{
				throw std::invalid_argument("Hit testing requires a color-target texture");
			}
			m_hit_testing_enabled = enabled;
			if (enabled && !m_hit_test_frame_state)
			{
				m_hit_test_frame_state = std::make_shared<detail::hit_test_frame_state>();
			}
		else if (!enabled)
		{
			m_hit_test_frame_state.reset();
			}
		}

		[[nodiscard]] bool hit_testing_enabled() const noexcept { return m_hit_testing_enabled; }

		// The position is in target pixels, with the origin at the bottom-left.
		[[nodiscard]] std::optional<hit_test_result> hit_test(glm::vec2 pixel_position) const
		{
			if (!m_hit_testing_enabled) [[unlikely]]
			{
				throw std::logic_error("Enable hit testing before rendering to this texture");
			}
			if (!m_hit_test_frame_state || !m_hit_test_frame_state->completed) [[unlikely]]
			{
				throw std::logic_error("No completed hit-test frame is available for this texture");
			}
			return detail::hit_test_frame_at_pixel(*m_hit_test_frame_state->completed, pixel_position);
		}

		[[nodiscard]] const gpu::texture& raw_gpu_texture() const noexcept { return m_gpu_tex; }
		[[nodiscard]] gpu::texture& raw_gpu_texture() noexcept { return m_gpu_tex; }

		void clear(const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f));
		void draw(const drawable& value);
		template <typename DrawFunction>
		    requires std::invocable<DrawFunction, two_d::frame_2d&>
		void draw(DrawFunction&& draw_function);
		void prepare(const drawable& value);
		[[nodiscard]] two_d::recording_context create_recording_context(std::size_t expected_draw_count = 0);
		void merge(two_d::recording_context&& context);
		void display();

		class frame_scope
		{
		public:
			frame_scope(const frame_scope&) = delete;
			frame_scope& operator=(const frame_scope&) = delete;
			frame_scope(frame_scope&&) = delete;
			frame_scope& operator=(frame_scope&&) = delete;

			~frame_scope() noexcept { cancel(); }

			void draw(const drawable& value)
			{
				ensure_active();
				m_owner->draw(value);
			}

			template <typename DrawFunction>
			    requires std::invocable<DrawFunction, two_d::frame_2d&>
			void draw(DrawFunction&& draw_function)
			{
				ensure_active();
				m_owner->draw(std::forward<DrawFunction>(draw_function));
			}

			void prepare(const drawable& value)
			{
				ensure_active();
				m_owner->prepare(value);
			}

			[[nodiscard]] two_d::recording_context create_recording_context(
			    std::size_t expected_draw_count = 0);
			void merge(two_d::recording_context&& context);

			void display()
			{
				ensure_active();
				m_owner->display();
				m_owner = nullptr;
			}

		private:
			friend class texture;

			explicit frame_scope(texture& owner, const color& clear_color, bool preserve_contents)
			    : m_owner(&owner)
			{
				owner.begin_render(clear_color, preserve_contents);
				m_generation = owner.m_active_frame_generation;
			}

			void ensure_active() const
			{
				if (!m_owner || m_generation == 0 || m_owner->m_active_frame_generation != m_generation) [[unlikely]]
				{
					throw std::logic_error("Texture frame scope is no longer active");
				}
			}

			void cancel() noexcept
			{
				if (auto* owner = std::exchange(m_owner, nullptr))
				{
					owner->cancel_active_frame(m_generation);
				}
			}

			texture* m_owner;
			std::uint64_t m_generation = 0;
		};

		[[nodiscard]] frame_scope frame(const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f))
		{
			return frame_scope(*this, clear_color, false);
		}

	private:
		void invalidate_hit_test_frame() noexcept
		{
			if (m_hit_test_frame_state)
			{
				m_hit_test_frame_state->completed.reset();
				m_hit_test_frame_state->latest_recorded.reset();
			}
		}

		[[nodiscard]] std::shared_ptr<const detail::hit_test_frame> completed_hit_test_frame() const noexcept
		{
			return m_hit_test_frame_state ? m_hit_test_frame_state->completed : nullptr;
		}

		friend class render_window;
		friend class imgui_context;
		friend class two_d::renderer;
		friend class two_d::font;
		friend class two_d::cached_layer;
		// Preserves existing target pixels; cached layers redraw only their dirty regions.
		[[nodiscard]] frame_scope frame_preserving(
		    const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f))
		{
			return frame_scope(*this, clear_color, true);
		}
		void begin_render(const color& clear_color, bool preserve_contents);

		static std::uint64_t next_hit_test_identity() noexcept
		{
			static std::atomic_uint64_t next{1};
			std::uint64_t identity = next.fetch_add(1, std::memory_order_relaxed);
			while (identity == 0)
			{
				identity = next.fetch_add(1, std::memory_order_relaxed);
			}
			return identity;
		}

		std::uint64_t m_hit_test_identity = next_hit_test_identity();
		gpu::texture m_gpu_tex;
		std::optional<view> m_view;
		bool m_hit_testing_enabled = false;
		std::shared_ptr<detail::hit_test_frame_state> m_hit_test_frame_state;
		std::shared_ptr<detail::reusable_upload_buffer> m_upload_staging;
		std::unique_ptr<detail::texture_render_session> m_render_session;
		bool m_external_render_active = false;
		std::uint64_t m_next_frame_generation = 0;
		std::uint64_t m_active_frame_generation = 0;

		explicit texture(gpu::texture&& texture) noexcept : m_gpu_tex(std::move(texture)) {}

		static detail::surface_ptr convert_surface(SDL_Surface* surface)
		{
			if (!surface) [[unlikely]]
			{
				throw std::invalid_argument("Cannot create texture from null SDL_Surface!");
			}

			// RGBA32 keeps byte order R,G,B,A regardless of endianness.
			detail::surface_ptr converted(SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32));
			if (!converted) [[unlikely]]
			{
				throw sdl_error("Failed to convert surface format: ");
			}
			if (converted->w <= 0 || converted->h <= 0 || !converted->pixels ||
			    converted->pitch < converted->w * 4) [[unlikely]]
			{
				throw std::invalid_argument("SDL_Surface has invalid dimensions or pixel data");
			}
			return converted;
		}

		void begin_external_render()
		{
			if (rendering()) [[unlikely]]
			{
				throw std::logic_error("Texture is already being rendered to");
			}
			m_external_render_active = true;
		}

		void end_external_render() noexcept { m_external_render_active = false; }

		static std::uint32_t checked_size(std::size_t size)
		{
			if (size > std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
			{
				throw std::length_error("Texture data exceeds SDL's 32-bit size limit");
			}
			return static_cast<std::uint32_t>(size);
		}

		// Плотно упакованные данные всей текстуры
		static void upload_raw(gpu::texture& tex, gpu::command_buffer& cmd, const void* data,
		                       std::size_t bytes, texture* owner = nullptr)
		{
			upload_rows(tex, cmd, data, bytes, bytes, 1, 0, 0, tex.width(), tex.height(), owner);
		}

		static void upload_region(gpu::texture& tex, gpu::command_buffer& cmd, const void* data,
		                          std::size_t src_stride, std::uint32_t x, std::uint32_t y,
		                          std::uint32_t width, std::uint32_t height, texture* owner = nullptr)
		{
			if (width == 0 || height == 0) [[unlikely]]
			{
				throw std::invalid_argument("Texture upload region must have non-zero dimensions");
			}
			const auto expected = SDL_CalculateGPUTextureFormatSize(
			    static_cast<SDL_GPUTextureFormat>(tex.format()), width, height, 1);
			if (expected == 0 || expected % height != 0) [[unlikely]]
			{
				throw std::invalid_argument("Texture upload region has an unsupported pixel format or size");
			}
			const auto row_bytes = static_cast<std::size_t>(expected / height);
			upload_rows(tex, cmd, data, row_bytes, src_stride, height, x, y, width, height, owner);
		}

		// Копирует rows строк по row_bytes байт (шаг источника src_stride) в плотный transfer buffer
		static void upload_rows(gpu::texture& tex, gpu::command_buffer& cmd, const void* data,
		                        std::size_t row_bytes, std::size_t src_stride, std::size_t rows,
		                        std::uint32_t destination_x = 0, std::uint32_t destination_y = 0,
		                        std::uint32_t destination_width = 0, std::uint32_t destination_height = 0,
		                        texture* owner = nullptr)
		{
			if (!tex.valid()) [[unlikely]]
			{
				throw std::logic_error("Cannot upload pixels to an invalid texture");
			}
			if (!cmd.valid()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot upload texture data with an invalid command buffer");
			}
			SDL_GPUDevice* device_raw = tex.device_raw();
			if (cmd.device_raw() != device_raw) [[unlikely]]
			{
				throw std::invalid_argument("Texture and command buffer belong to different GPU devices");
			}
			if (!data) [[unlikely]]
			{
				throw std::invalid_argument("Texture pixel data is null");
			}
			if (src_stride < row_bytes) [[unlikely]]
			{
				throw std::invalid_argument("Texture row stride is smaller than row size");
			}
			if (destination_width == 0)
			{
				destination_width = tex.width();
			}
			if (destination_height == 0)
			{
				destination_height = tex.height();
			}
			const bool packed_whole_texture = destination_x == 0 && destination_y == 0 &&
			                                  destination_width == tex.width() &&
			                                  destination_height == tex.height() && rows == 1;
			if (destination_x > tex.width() || destination_width > tex.width() - destination_x ||
			    destination_y > tex.height() || destination_height > tex.height() - destination_y ||
			    (destination_height != rows && !packed_whole_texture)) [[unlikely]]
			{
				throw std::out_of_range("Texture upload region exceeds the destination texture");
			}

			const std::uint32_t expected = SDL_CalculateGPUTextureFormatSize(
			    static_cast<SDL_GPUTextureFormat>(tex.format()), destination_width, destination_height, 1);
			if (expected == 0) [[unlikely]]
			{
				throw std::invalid_argument("Texture upload region has an unsupported pixel format or size");
			}
			const std::uint64_t total_size = static_cast<std::uint64_t>(row_bytes) * rows;
			if (total_size > std::numeric_limits<std::size_t>::max()) [[unlikely]]
			{
				throw std::length_error("Texture upload region exceeds the addressable size");
			}
			const std::size_t total = static_cast<std::size_t>(total_size);
			if (total != expected) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Texture pixel data size does not match upload region dimensions and format");
			}
			const std::uint32_t data_size = checked_size(total);

			std::shared_ptr<detail::reusable_upload_buffer> reusable;
			std::unique_ptr<detail::transfer_buffer_guard> temporary_guard;
			SDL_GPUTransferBuffer* transfer_buf = nullptr;
			if (owner && owner->m_upload_staging && !owner->m_upload_staging->in_flight &&
			    owner->m_upload_staging->device == device_raw &&
			    owner->m_upload_staging->capacity >= data_size)
			{
				reusable = owner->m_upload_staging;
				transfer_buf = reusable->handle;
			}
			else
			{
				const std::uint32_t capacity = owner && owner->m_upload_staging &&
				                                       owner->m_upload_staging->device == device_raw
				                                   ? std::max(owner->m_upload_staging->capacity, data_size)
				                                   : data_size;
				SDL_GPUTransferBufferCreateInfo transfer_info{};
				transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
				transfer_info.size = capacity;
				auto* created = SDL_CreateGPUTransferBuffer(device_raw, &transfer_info);
				if (!created) [[unlikely]]
				{
					throw sdl_error("Failed to create transfer buffer for texture: ");
				}
				if (owner)
				{
					try
					{
						reusable = std::make_shared<detail::reusable_upload_buffer>(device_raw, created, capacity);
					}
					catch (...)
					{
						SDL_ReleaseGPUTransferBuffer(device_raw, created);
						throw;
					}
					owner->m_upload_staging = reusable;
					transfer_buf = reusable->handle;
				}
				else
				{
					transfer_buf = created;
					temporary_guard =
					    std::make_unique<detail::transfer_buffer_guard>(device_raw, transfer_buf);
				}
			}

			if (reusable)
			{
				cmd.retain_resource(reusable);
				cmd.on_completion([reusable](bool) { reusable->in_flight = false; });
				reusable->in_flight = true;
			}

			void* mapped = SDL_MapGPUTransferBuffer(device_raw, transfer_buf, true);
			if (!mapped) [[unlikely]]
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

			gpu::detail::gpu_copy_pass copy_pass(cmd);

			// Нули = плотная упаковка по размеру региона
			SDL_GPUTextureTransferInfo src_info{};
			src_info.transfer_buffer = transfer_buf;

			SDL_GPUTextureRegion dst_region{};
			dst_region.texture = tex.raw();
			dst_region.x = destination_x;
			dst_region.y = destination_y;
			dst_region.w = destination_width;
			dst_region.h = destination_height;
			dst_region.d = 1;

			SDL_UploadToGPUTexture(copy_pass.raw(), &src_info, &dst_region, false);
		}
	};

	inline void texture::reset_active_frame() noexcept
	{
		m_render_session.reset();
		m_active_frame_generation = 0;
	}

	inline void texture::cancel_active_frame(std::uint64_t generation) noexcept
	{
		if (generation != 0 && generation == m_active_frame_generation)
		{
			reset_active_frame();
		}
	}

	class animated_texture
	{
	public:
		explicit animated_texture(const std::string& path,
		                          std::uint32_t usage = texture::default_upload_usage);

		animated_texture(const animated_texture&) = delete;
		animated_texture& operator=(const animated_texture&) = delete;
		animated_texture(animated_texture&&) noexcept;
		animated_texture& operator=(animated_texture&&) noexcept;
		~animated_texture();

		[[nodiscard]] const texture& as_texture() const;

	private:
		struct state;
		std::unique_ptr<state> m_state;
	};

	struct animated_texture::state
	{
		struct update_hook final : updatable
		{
			explicit update_hook(state& owner) noexcept : m_owner(&owner) {}

			void update(double) override;

		private:
			state* m_owner;
		};

		state(const std::string& path, std::uint32_t texture_usage)
		    : m_source(path), m_texture(m_source, texture_usage), m_usage(texture_usage),
		      m_uploaded_frame(m_source.raw()), m_update_hook(*this)
		{
		}

		void synchronize()
		{
			if (!m_source.animated())
			{
				return;
			}

			SDL_Surface* current_frame = m_source.raw();
			if (!current_frame) [[unlikely]]
			{
				throw std::logic_error("Animated texture has no current frame");
			}
			if (current_frame == m_uploaded_frame)
			{
				return;
			}
			if (m_texture.rendering()) [[unlikely]]
			{
				throw std::logic_error("Cannot update an animated texture while it is being rendered to");
			}

			auto& device = gpu::device::global();
			gpu::command_buffer cmd(device);
			const bool same_size = current_frame->w > 0 && current_frame->h > 0 &&
			                       m_texture.width() == static_cast<std::uint32_t>(current_frame->w) &&
			                       m_texture.height() == static_cast<std::uint32_t>(current_frame->h);

			if (same_size)
			{
				m_texture.update_from_surface(cmd, current_frame);
			}
			else
			{
				m_texture.load_from_surface(device, cmd, current_frame, m_usage);
			}
			cmd.submit();
			m_uploaded_frame = current_frame;
		}

		surface m_source;
		texture m_texture;
		std::uint32_t m_usage;
		SDL_Surface* m_uploaded_frame;
		// Register after the source so a changed frame uploads in the same update tick.
		update_hook m_update_hook;
	};

	inline void animated_texture::state::update_hook::update(double)
	{
		m_owner->synchronize();
	}

	inline animated_texture::animated_texture(const std::string& path, std::uint32_t usage)
	    : m_state(std::make_unique<state>(path, usage))
	{
	}

	inline animated_texture::animated_texture(animated_texture&&) noexcept = default;

	inline animated_texture& animated_texture::operator=(animated_texture&&) noexcept = default;

	inline animated_texture::~animated_texture() = default;

	inline const texture& animated_texture::as_texture() const
	{
		if (!m_state) [[unlikely]]
		{
			throw std::logic_error("Cannot access a moved-from animated texture");
		}
		return m_state->m_texture;
	}
} // namespace mngl
