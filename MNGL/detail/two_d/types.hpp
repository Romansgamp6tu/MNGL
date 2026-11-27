#pragma once

#include "common.hpp"
#include "../../gpu_optimizations.hpp"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mngl::detail
{
	class texture_render_session_impl;
}

namespace mngl::two_d
{
	namespace detail
	{
		struct resident_mesh_vertex;

		inline std::uint64_t allocate_texture_store_id()
		{
			static std::atomic<std::uint64_t> next_id{1};
			auto id = next_id.load(std::memory_order_relaxed);
			while (id != 0 && id != std::numeric_limits<std::uint64_t>::max())
			{
				if (next_id.compare_exchange_weak(id, id + 1, std::memory_order_relaxed,
				                                  std::memory_order_relaxed))
				{
					return id;
				}
			}
			throw std::overflow_error("2D texture store identity space exhausted");
		}
	}

	inline void enable_scene_culling() noexcept
	{
		::mngl::gpu::enable_scene_culling();
	}
	inline void disable_scene_culling() noexcept
	{
		::mngl::gpu::disable_scene_culling();
	}

	[[nodiscard]] inline bool scene_culling_enabled() noexcept
	{
		return ::mngl::gpu::scene_culling_enabled();
	}

	inline void enable_dirty_region_updates() noexcept
	{
		::mngl::gpu::enable_dirty_region_updates();
	}
	inline void disable_dirty_region_updates() noexcept
	{
		::mngl::gpu::disable_dirty_region_updates();
	}

	[[nodiscard]] inline bool dirty_region_updates_enabled() noexcept
	{
		return ::mngl::gpu::dirty_region_updates_enabled();
	}

	struct texture_handle
	{
		constexpr static std::uint32_t invalid_value = std::numeric_limits<std::uint32_t>::max();

		constexpr texture_handle() noexcept = default;
		[[nodiscard]] constexpr bool valid() const noexcept
		{
			return m_value != invalid_value && m_store_id != 0;
		}
		constexpr bool operator==(const texture_handle&) const noexcept = default;

	private:
		friend class texture_store;

		constexpr texture_handle(std::uint32_t value, std::uint64_t store_id) noexcept
		    : m_value(value), m_store_id(store_id)
		{
		}

		std::uint32_t m_value = invalid_value;
		std::uint64_t m_store_id = 0;
	};

	struct texture_region
	{
		texture_handle page{};
		glm::vec4 uv_bounds{0.0f, 0.0f, 1.0f, 1.0f};
	};

	class texture_store
	{
	public:
		explicit texture_store(const gpu::device& device)
		    : m_device(device), m_store_id(detail::allocate_texture_store_id())
		{
			if (!m_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot create a 2D texture store with a null GPU device");
			}

			m_white_page = add(create_white_texture(m_device));
		}

		texture_store(const texture_store&) = delete;
		texture_store& operator=(const texture_store&) = delete;
		texture_store(texture_store&&) = delete;
		texture_store& operator=(texture_store&&) = delete;

		[[nodiscard]] texture_handle add(::mngl::texture&& page)
		{
			if (!page.valid()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot register an invalid 2D texture");
			}
			if (page.raw_gpu_texture().device_raw() != m_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("2D texture page belongs to a different GPU device");
			}
			if (m_pages.size() >= texture_handle::invalid_value) [[unlikely]]
			{
				throw std::length_error("2D texture store exhausted its 32-bit handle space");
			}

			m_pages.push_back(std::move(page));
			return texture_handle{static_cast<std::uint32_t>(m_pages.size() - 1), m_store_id};
		}

		[[nodiscard]] texture_region region(texture_handle page,
		                                    glm::vec4 uv_bounds = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)) const
		{
			(void)resolve(page);
			if (!std::isfinite(uv_bounds.x) || !std::isfinite(uv_bounds.y) || !std::isfinite(uv_bounds.z) ||
			    !std::isfinite(uv_bounds.w) || uv_bounds.x < 0.0f || uv_bounds.x > 1.0f ||
			    uv_bounds.y < 0.0f || uv_bounds.y > 1.0f || uv_bounds.z < 0.0f || uv_bounds.z > 1.0f ||
			    uv_bounds.w < 0.0f || uv_bounds.w > 1.0f) [[unlikely]]
			{
				throw std::invalid_argument("2D texture region UVs must be finite and normalized");
			}
			return texture_region{page, uv_bounds};
		}

		[[nodiscard]] texture_region white_region() const noexcept
		{
			return texture_region{m_white_page, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)};
		}

		[[nodiscard]] const ::mngl::texture& resolve(texture_handle page) const
		{
			if (!page.valid() || page.m_store_id != m_store_id || page.m_value >= m_pages.size()) [[unlikely]]
			{
				throw std::out_of_range("Invalid 2D texture handle for this store");
			}
			return m_pages[page.m_value];
		}

		[[nodiscard]] ::mngl::texture& resolve(texture_handle page)
		{
			if (!page.valid() || page.m_store_id != m_store_id || page.m_value >= m_pages.size()) [[unlikely]]
			{
				throw std::out_of_range("Invalid 2D texture handle for this store");
			}
			return m_pages[page.m_value];
		}

		[[nodiscard]] SDL_GPUDevice* device_raw() const noexcept { return m_device.raw(); }

	private:
		static ::mngl::texture create_white_texture(const gpu::device& device)
		{
			std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
			    SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32), &SDL_DestroySurface);
			if (!surface) [[unlikely]]
			{
				throw ::mngl::sdl_error("Failed to create the built-in white texture: ");
			}
			if (!surface->pixels || surface->pitch < 4) [[unlikely]]
			{
				throw std::runtime_error("SDL created an invalid surface for the built-in white texture");
			}

			const std::array<Uint8, 4> white_pixel{255, 255, 255, 255};
			std::memcpy(surface->pixels, white_pixel.data(), white_pixel.size());

			gpu::command_buffer cmd(device);
			::mngl::texture texture(device, cmd, surface.get());
			cmd.submit();
			return texture;
		}

		gpu::device m_device;
		const std::uint64_t m_store_id;
		// Handles are append-only and remain stable until this store is destroyed.
		std::deque<::mngl::texture> m_pages;
		texture_handle m_white_page{};
	};

} // namespace mngl::two_d

namespace mngl::two_d
{
	class frame_2d;
	class renderer;
	class svg_sprite;
} // namespace mngl::two_d

namespace mngl
{
	class drawable
	{
	public:
		virtual ~drawable() = default;

	protected:
		virtual void draw(two_d::frame_2d& frame) const = 0;
		virtual void prepare(two_d::frame_2d&) const {}

	private:
		friend class two_d::frame_2d;
	};

	struct sprite : drawable
	{
		sprite() = default;

		explicit sprite(const two_d::texture_store& textures, two_d::texture_handle image)
		    : sprite(textures.region(image),
		             rectangle{glm::vec2(0.0f), textures.resolve(image).coordinate_size()})
		{
		}

		sprite(const two_d::texture_store& textures, two_d::texture_handle image,
		       const rectangle& destination, const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f),
		       gpu::quad_draw_state state = {})
		    : sprite(textures.region(image), destination, tint, std::move(state))
		{
		}

		sprite(two_d::texture_region source, const rectangle& destination = {},
		       const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f), gpu::quad_draw_state state = {})
		    : source(source), destination(destination), tint(tint), state(std::move(state))
		{
		}

		// Use only for render targets or other textures that intentionally remain externally owned.
		[[nodiscard]] static sprite borrowed(const texture& image)
		{
			return borrowed(image, rectangle{glm::vec2(0.0f), image.coordinate_size()});
		}

		[[nodiscard]] static sprite borrowed(
		    const texture& image, const rectangle& destination,
		    const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f), gpu::quad_draw_state state = {})
		{
			sprite result{};
			result.m_borrowed_source = &image;
			result.destination = destination;
			result.tint = tint;
			result.state = std::move(state);
			return result;
		}

		sprite(glm::uvec2 target_size, const gpu::graphics_shader_asset& fragment_shader)
		    : sprite(glm::vec2(target_size), fragment_shader)
		{
		}

		sprite(glm::vec2 target_size, const gpu::graphics_shader_asset& fragment_shader)
		    : m_fragment_shader(&fragment_shader)
		{
			const float short_side = std::min(target_size.x, target_size.y);
			if (short_side > 0.0f)
			{
				destination = rectangle{glm::vec2(0.0f), target_size / short_side};
			}
		}

		sprite& shader(const gpu::graphics_shader_asset& value) noexcept
		{
			m_fragment_shader = &value;
			return *this;
		}

		[[nodiscard]] glm::vec2 position() const noexcept { return destination.pos; }

		sprite& position(glm::vec2 value) noexcept
		{
			destination.pos = value;
			return *this;
		}

		[[nodiscard]] glm::vec2 size() const noexcept { return destination.size; }

		sprite& size(glm::vec2 value) noexcept
		{
			destination.size = value;
			return *this;
		}

		[[nodiscard]] float rotation() const noexcept { return rotation_radians; }

		sprite& rotation(float value) noexcept
		{
			rotation_radians = value;
			return *this;
		}

		void on_click(hit_test_callback callback)
		{
			detail::set_hit_test_callback(click_callback, std::move(callback));
		}

		void clear_on_click() noexcept { click_callback.reset(); }

		two_d::texture_region source{};

	private:
		const texture* m_borrowed_source = nullptr;

	public:
		rectangle destination{};
		glm::vec2 pivot{0.5f, 0.5f};
		float rotation_radians = 0.0f;
		two_d::transform transform{};
		color tint = color(1.0f, 1.0f, 1.0f, 1.0f);
		gpu::quad_draw_state state{};
		std::shared_ptr<const hit_test_callback> click_callback{};

	private:
		friend class two_d::frame_2d;
		friend class two_d::renderer;
		friend class render_window;
		friend class resizable_texture;
		friend class sprite_animation;
		void draw(two_d::frame_2d& frame) const override;
		void prepare(two_d::frame_2d& frame) const override;
		const gpu::graphics_shader_asset* m_fragment_shader = nullptr;
	};

	class sprite_animation
	{
	public:
		struct frame
		{
			two_d::texture_region source{};
			double duration_seconds{};
		};

		enum class playback_mode : std::uint8_t
		{
			loop,
			once
		};

		explicit sprite_animation(std::vector<frame> frames, playback_mode mode = playback_mode::loop)
		    : m_frames(std::move(frames)), m_mode(mode)
		{
			if (m_frames.empty()) [[unlikely]]
			{
				throw std::invalid_argument("Sprite animation must contain at least one frame");
			}
			if (m_mode != playback_mode::loop && m_mode != playback_mode::once) [[unlikely]]
			{
				throw std::invalid_argument("Invalid sprite animation playback mode");
			}

			for (const auto& value : m_frames)
			{
				if (!valid_region(value.source)) [[unlikely]]
				{
					throw std::invalid_argument(
					    "Sprite animation frame must use a valid normalized texture region");
				}
				if (!std::isfinite(value.duration_seconds) || value.duration_seconds <= 0.0) [[unlikely]]
				{
					throw std::invalid_argument("Sprite animation frame duration must be finite and positive");
				}
				m_total_duration += value.duration_seconds;
				if (!std::isfinite(m_total_duration)) [[unlikely]]
				{
					throw std::length_error("Sprite animation duration is too large");
				}
			}
		}

		explicit sprite_animation(std::initializer_list<frame> frames,
		                         playback_mode mode = playback_mode::loop)
		    : sprite_animation(std::vector<frame>(frames), mode)
		{
		}

		void update(double delta_seconds)
		{
			if (!std::isfinite(delta_seconds) || delta_seconds < 0.0) [[unlikely]]
			{
				throw std::invalid_argument("Sprite animation delta time must be finite and non-negative");
			}
			if (!m_playing || delta_seconds == 0.0)
			{
				return;
			}

			if (m_mode == playback_mode::once)
			{
				advance_once(delta_seconds);
				return;
			}
			advance_loop(delta_seconds);
		}

		void play() noexcept { m_playing = !m_finished; }
		void pause() noexcept { m_playing = false; }

		void restart() noexcept
		{
			m_frame_index = 0;
			m_frame_elapsed = 0.0;
			m_finished = false;
			m_playing = true;
		}

		[[nodiscard]] bool playing() const noexcept { return m_playing; }
		[[nodiscard]] bool finished() const noexcept { return m_finished; }
		[[nodiscard]] std::size_t frame_count() const noexcept { return m_frames.size(); }
		[[nodiscard]] std::size_t frame_index() const noexcept { return m_frame_index; }
		[[nodiscard]] playback_mode mode() const noexcept { return m_mode; }

		[[nodiscard]] const two_d::texture_region& current_region() const noexcept
		{
			return m_frames[m_frame_index].source;
		}

		void apply_to(sprite& target) const noexcept
		{
			target.source = current_region();
			target.m_borrowed_source = nullptr;
		}

	private:
		[[nodiscard]] static bool valid_region(const two_d::texture_region& value) noexcept
		{
			const auto& uv = value.uv_bounds;
			return value.page.valid() && std::isfinite(uv.x) && std::isfinite(uv.y) && std::isfinite(uv.z) &&
			       std::isfinite(uv.w) && uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f &&
			       uv.z >= 0.0f && uv.z <= 1.0f && uv.w >= 0.0f && uv.w <= 1.0f;
		}

		void advance_once(double remaining) noexcept
		{
			while (remaining > 0.0)
			{
				const double until_next = m_frames[m_frame_index].duration_seconds - m_frame_elapsed;
				if (remaining < until_next)
				{
					m_frame_elapsed += remaining;
					return;
				}

				remaining -= until_next;
				if (m_frame_index + 1 == m_frames.size())
				{
					m_frame_elapsed = m_frames[m_frame_index].duration_seconds;
					m_finished = true;
					m_playing = false;
					return;
				}

				++m_frame_index;
				m_frame_elapsed = 0.0;
			}
		}

		void advance_loop(double delta_seconds) noexcept
		{
			double remaining = std::fmod(delta_seconds, m_total_duration);
			const double until_next = m_frames[m_frame_index].duration_seconds - m_frame_elapsed;
			if (remaining < until_next)
			{
				m_frame_elapsed += remaining;
				return;
			}

			remaining -= until_next;
			m_frame_index = (m_frame_index + 1) % m_frames.size();
			m_frame_elapsed = 0.0;
			remaining = std::fmod(remaining, m_total_duration);
			while (remaining >= m_frames[m_frame_index].duration_seconds)
			{
				remaining -= m_frames[m_frame_index].duration_seconds;
				m_frame_index = (m_frame_index + 1) % m_frames.size();
			}
			m_frame_elapsed = remaining;
		}

		std::vector<frame> m_frames;
		playback_mode m_mode;
		std::size_t m_frame_index{};
		double m_frame_elapsed{};
		double m_total_duration{};
		bool m_playing{true};
		bool m_finished{};
	};
} // namespace mngl

namespace mngl::two_d
{
	struct circle : ::mngl::drawable
	{
		circle() = default;
		circle(glm::vec2 center, float radius, color tint = color(1.0f, 1.0f, 1.0f, 1.0f),
		       gpu::quad_draw_state state = {})
		    : center(center), radius(radius), tint(tint), state(std::move(state))
		{
		}

		glm::vec2 center{0.0f, 0.0f};
		float radius = 0.0f;
		color tint = color(1.0f, 1.0f, 1.0f, 1.0f);
		gpu::quad_draw_state state{};
		::mngl::two_d::transform transform{};
		std::shared_ptr<const ::mngl::hit_test_callback> click_callback{};

		void on_click(::mngl::hit_test_callback callback)
		{
			::mngl::detail::set_hit_test_callback(click_callback, std::move(callback));
		}

		void clear_on_click() noexcept { click_callback.reset(); }

	private:
		friend class frame_2d;
		void draw(frame_2d& frame) const override;
	};

	struct ellipse : ::mngl::drawable
	{
		ellipse() = default;
		ellipse(glm::vec2 center, glm::vec2 radii, color tint = color(1.0f, 1.0f, 1.0f, 1.0f),
		        gpu::quad_draw_state state = {})
		    : center(center), radii(radii), tint(tint), state(std::move(state))
		{
		}

		glm::vec2 center{0.0f, 0.0f};
		glm::vec2 radii{0.0f, 0.0f};
		color tint = color(1.0f, 1.0f, 1.0f, 1.0f);
		gpu::quad_draw_state state{};
		::mngl::two_d::transform transform{};
		std::shared_ptr<const ::mngl::hit_test_callback> click_callback{};

		void on_click(::mngl::hit_test_callback callback)
		{
			::mngl::detail::set_hit_test_callback(click_callback, std::move(callback));
		}

		void clear_on_click() noexcept { click_callback.reset(); }

	private:
		friend class frame_2d;
		void draw(frame_2d& frame) const override;
	};

	struct rounded_rectangle : ::mngl::drawable
	{
		rounded_rectangle() = default;
		rounded_rectangle(::mngl::rectangle bounds, float radius, color tint = color(1.0f, 1.0f, 1.0f, 1.0f),
		                  gpu::quad_draw_state state = {})
		    : bounds(bounds), radius(radius), tint(tint), state(std::move(state))
		{
		}

		rectangle bounds{};
		float radius = 0.0f;
		color tint = color(1.0f, 1.0f, 1.0f, 1.0f);
		gpu::quad_draw_state state{};
		::mngl::two_d::transform transform{};
		std::shared_ptr<const ::mngl::hit_test_callback> click_callback{};

		void on_click(::mngl::hit_test_callback callback)
		{
			::mngl::detail::set_hit_test_callback(click_callback, std::move(callback));
		}

		void clear_on_click() noexcept { click_callback.reset(); }

	private:
		friend class frame_2d;
		void draw(frame_2d& frame) const override;
	};

	class rectangle_shape : public ::mngl::drawable
	{
	public:
		rectangle_shape() = default;
		explicit rectangle_shape(::mngl::rectangle bounds, color tint = color(1.0f, 1.0f, 1.0f, 1.0f),
		                         transform object_transform = {})
		    : bounds(bounds), tint(tint), transform(std::move(object_transform))
		{
		}

		::mngl::rectangle bounds{};
		color tint{1.0f, 1.0f, 1.0f, 1.0f};
		::mngl::two_d::transform transform{};
		gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha;

	private:
		friend class frame_2d;
		void draw(frame_2d& frame) const override;
	};

	enum class primitive_type : std::uint8_t
	{
		points,
		lines,
		line_strip,
		triangles,
		triangle_strip,
		triangle_fan
	};

	class vertex_array : public ::mngl::drawable
	{
	public:
		vertex_array() = default;
		explicit vertex_array(primitive_type primitive) : primitive(primitive) {}
		vertex_array(primitive_type primitive, std::span<const glm::vec2> vertices)
		    : primitive(primitive), vertices(vertices.begin(), vertices.end())
		{
		}
		vertex_array(primitive_type primitive, std::initializer_list<glm::vec2> vertices)
		    : vertex_array(primitive, std::span<const glm::vec2>(vertices.begin(), vertices.size()))
		{
		}

		vertex_array& add_vertex(glm::vec2 value)
		{
			vertices.push_back(value);
			return *this;
		}

		vertex_array& clear() noexcept
		{
			vertices.clear();
			return *this;
		}

		primitive_type primitive = primitive_type::triangles;
		std::vector<glm::vec2> vertices;
		color tint{1.0f, 1.0f, 1.0f, 1.0f};
		float line_width = 0.002f;
		float point_size = 0.01f;
		bool closed = false;
		gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha;
		::mngl::two_d::transform transform{};

	private:
		friend class frame_2d;
		void draw(frame_2d& frame) const override;
	};

	class prepared_vertex_array : public ::mngl::drawable
	{
	public:
		// Stores a validated, immutable triangle-list snapshot of the source geometry.
		explicit prepared_vertex_array(const vertex_array& source);

		[[nodiscard]] std::span<const glm::vec2> triangle_vertices() const noexcept
		{
			return std::span<const glm::vec2>(m_triangles);
		}

		color tint{1.0f, 1.0f, 1.0f, 1.0f};
		gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha;
		::mngl::two_d::transform transform{};

	private:
		friend class frame_2d;
		void draw(frame_2d& frame) const override;

		std::vector<glm::vec2> m_triangles;
	};

	class mesh : public ::mngl::drawable
	{
	public:
		struct block_update
		{
			std::size_t index = 0;
			std::span<const glm::vec2> vertices;
		};

		// Uploads a fixed triangle-list mesh into a persistent GPU vertex buffer.
		mesh(const gpu::device& device, gpu::command_buffer& command_buffer,
		     std::span<const glm::vec2> triangle_vertices);
		mesh(const gpu::device& device, gpu::command_buffer& command_buffer, const vertex_array& source);
		mesh(const gpu::device& device, gpu::command_buffer& command_buffer,
		     const prepared_vertex_array& source);
		mesh(const mesh&) = delete;
		mesh& operator=(const mesh&) = delete;
		mesh(mesh&&) noexcept = default;
		mesh& operator=(mesh&&) noexcept = default;
		~mesh() override = default;

		[[nodiscard]] std::uint32_t vertex_count() const noexcept;
		[[nodiscard]] std::size_t block_count() const noexcept;
		[[nodiscard]] std::uint32_t block_first_vertex(std::size_t index) const;
		[[nodiscard]] std::uint32_t block_vertex_count(std::size_t index) const;

		// Each update replaces one existing block; topology and vertex count stay fixed.
		void update_block(gpu::command_buffer& command_buffer, std::size_t index,
		                  std::span<const glm::vec2> vertices);
		void update_blocks(gpu::command_buffer& command_buffer, std::span<const block_update> updates);

		color tint{1.0f, 1.0f, 1.0f, 1.0f};
		gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha;
		::mngl::two_d::transform transform{};

	private:
		friend class frame_2d;
		friend class renderer;
		friend class svg_object;
		friend class svg_sprite;
		mesh() = default;
		mesh(const gpu::device& device, gpu::command_buffer& command_buffer,
		     std::span<const detail::resident_mesh_vertex> triangle_vertices);
		[[nodiscard]] static std::shared_ptr<detail::resident_mesh_storage> create_storage(
		    const gpu::device& device, gpu::command_buffer& command_buffer,
		    std::span<const detail::resident_mesh_vertex> triangle_vertices);
		void draw(frame_2d& frame) const override;

		std::shared_ptr<detail::resident_mesh_storage> m_storage;
	};

	class cached_layer : public ::mngl::drawable
	{
	public:
		explicit cached_layer(glm::uvec2 pixel_size,
		                      gpu::texture_format format = gpu::texture_format::r8g8b8a8_unorm,
		                      color clear_color = color(0.0f, 0.0f, 0.0f, 0.0f));
		cached_layer(const cached_layer&) = delete;
		cached_layer& operator=(const cached_layer&) = delete;
		cached_layer(cached_layer&&) = default;
		cached_layer& operator=(cached_layer&&) = default;
		~cached_layer() override = default;

		template <typename DrawFunction>
		    requires std::invocable<DrawFunction, frame_2d&>
		[[nodiscard]] bool update_if_dirty(DrawFunction&& draw_contents)
		{
			if (!dirty())
			{
				return false;
			}

			m_full_dirty = false;
			m_dirty_regions.clear();
			try
			{
				auto target_frame = m_texture.frame(m_clear_color);
				target_frame.draw(std::forward<DrawFunction>(draw_contents));
				target_frame.display();
			}
			catch (...)
			{
				m_full_dirty = true;
				m_dirty_regions.clear();
				throw;
			}
			return true;
		}

		template <typename DrawFunction>
		    requires std::invocable<DrawFunction&, frame_2d&, const ::mngl::rectangle&>
		// Runs once per merged dirty region; the callback must redraw every intersecting item in order.
		[[nodiscard]] bool update_dirty_regions(DrawFunction&& draw_region);

		void invalidate() noexcept
		{
			m_full_dirty = true;
			m_dirty_regions.clear();
		}
		// Bounds use the layer's bottom-left-origin content coordinates.
		void invalidate_region(const ::mngl::rectangle& bounds);
		[[nodiscard]] bool dirty() const noexcept { return m_full_dirty || !m_dirty_regions.empty(); }
		[[nodiscard]] glm::uvec2 size() const noexcept { return m_texture.size(); }
		[[nodiscard]] const ::mngl::texture& image() const noexcept { return m_texture; }

		cached_layer& clear_color(color value) noexcept
		{
			m_clear_color = value;
			invalidate();
			return *this;
		}

		::mngl::rectangle destination{};
		color tint{1.0f, 1.0f, 1.0f, 1.0f};
		gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha;
		::mngl::two_d::transform transform{};

	private:
		friend class frame_2d;
		void draw(frame_2d& frame) const override;

		::mngl::texture m_texture;
		color m_clear_color;
		bool m_full_dirty = true;
		std::vector<SDL_Rect> m_dirty_regions;
	};

	class convex_polygon : public ::mngl::drawable
	{
	public:
		convex_polygon() = default;
		explicit convex_polygon(std::span<const glm::vec2> points) : points(points.begin(), points.end()) {}
		convex_polygon(std::initializer_list<glm::vec2> points)
		    : convex_polygon(std::span<const glm::vec2>(points.begin(), points.size()))
		{
		}

		convex_polygon& add_point(glm::vec2 value)
		{
			points.push_back(value);
			return *this;
		}

		std::vector<glm::vec2> points;
		color fill_color{1.0f, 1.0f, 1.0f, 1.0f};
		color outline_color{0.0f, 0.0f, 0.0f, 0.0f};
		float outline_thickness = 0.0f;
		gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha;
		::mngl::two_d::transform transform{};

	private:
		friend class frame_2d;
		void draw(frame_2d& frame) const override;
	};

	class text : public ::mngl::drawable
	{
	public:
		// Owns string text, but borrows its font and, for document text, the document.
		text(font& typeface, std::string_view value, glm::vec2 top_left, text_draw_options options = {})
		    : m_typeface(&typeface), m_text(value), m_top_left(top_left), m_options(std::move(options))
		{
		}

		text(font& typeface, const text_document& document, glm::vec2 top_left,
		     text_draw_options options = {})
		    : m_typeface(&typeface), m_document(&document), m_top_left(top_left),
		      m_options(std::move(options))
		{
		}

		text& set_text(std::string_view value)
		{
			m_text.assign(value);
			m_document = nullptr;
			return *this;
		}

		text& set_document(const text_document& value) noexcept
		{
			m_document = &value;
			return *this;
		}

		text& position(glm::vec2 value) noexcept
		{
			m_top_left = value;
			return *this;
		}

		text& options(text_draw_options value)
		{
			m_options = std::move(value);
			return *this;
		}

	private:
		friend class frame_2d;
		void draw(frame_2d& frame) const override;
		void prepare(frame_2d& frame) const override;

		font* m_typeface;
		std::string m_text;
		const text_document* m_document = nullptr;
		glm::vec2 m_top_left{};
		text_draw_options m_options{};
	};
} // namespace mngl::two_d
