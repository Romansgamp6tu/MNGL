#pragma once

#include "batcher2d.hpp"
#include "hit_test.hpp"
#include "text.hpp"
#include "view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mngl::two_d
{
	struct texture_handle
	{
		constexpr static uint32_t invalid_value = std::numeric_limits<uint32_t>::max();

		uint32_t value = invalid_value;

		[[nodiscard]] constexpr bool valid() const noexcept { return value != invalid_value; }
		constexpr bool operator==(const texture_handle&) const noexcept = default;
	};

	struct texture_region
	{
		texture_handle page{};
		glm::vec4 uv_bounds{ 0.0f, 0.0f, 1.0f, 1.0f };
	};

	class texture_store
	{
	public:
		explicit texture_store(const gpu::device& device)
			: m_device(device)
		{
			if (!m_device.raw())
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
			if (!page.valid())
			{
				throw std::invalid_argument("Cannot register an invalid 2D texture");
			}
			if (page.raw_gpu_texture().device_raw() != m_device.raw())
			{
				throw std::invalid_argument("2D texture page belongs to a different GPU device");
			}
			if (m_pages.size() >= texture_handle::invalid_value)
			{
				throw std::length_error("2D texture store exhausted its 32-bit handle space");
			}

			m_pages.push_back(std::move(page));
			return texture_handle{ static_cast<uint32_t>(m_pages.size() - 1) };
		}

		[[nodiscard]] texture_region region(
			texture_handle page,
			glm::vec4 uv_bounds = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)
		) const
		{
			(void)resolve(page);
			if (!std::isfinite(uv_bounds.x) || !std::isfinite(uv_bounds.y) ||
				!std::isfinite(uv_bounds.z) || !std::isfinite(uv_bounds.w) ||
				uv_bounds.x < 0.0f || uv_bounds.x > 1.0f ||
				uv_bounds.y < 0.0f || uv_bounds.y > 1.0f ||
				uv_bounds.z < 0.0f || uv_bounds.z > 1.0f ||
				uv_bounds.w < 0.0f || uv_bounds.w > 1.0f)
			{
				throw std::invalid_argument("2D texture region UVs must be finite and normalized");
			}
			return texture_region{ page, uv_bounds };
		}

		[[nodiscard]] texture_region white_region() const noexcept
		{
			return texture_region{ m_white_page, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) };
		}

		[[nodiscard]] const ::mngl::texture& resolve(texture_handle page) const
		{
			if (!page.valid() || page.value >= m_pages.size())
			{
				throw std::out_of_range("Invalid 2D texture handle");
			}
			return m_pages[page.value];
		}

		[[nodiscard]] ::mngl::texture& resolve(texture_handle page)
		{
			if (!page.valid() || page.value >= m_pages.size())
			{
				throw std::out_of_range("Invalid 2D texture handle");
			}
			return m_pages[page.value];
		}

		[[nodiscard]] SDL_GPUDevice* device_raw() const noexcept
		{
			return m_device.raw();
		}

	private:
		static ::mngl::texture create_white_texture(const gpu::device& device)
		{
			std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
				SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32),
				&SDL_DestroySurface);
			if (!surface)
			{
				throw ::sdl_error("Failed to create the built-in white texture: ");
			}
			if (!surface->pixels || surface->pitch < 4)
			{
				throw std::runtime_error("SDL created an invalid surface for the built-in white texture");
			}

			const std::array<Uint8, 4> white_pixel{ 255, 255, 255, 255 };
			std::memcpy(surface->pixels, white_pixel.data(), white_pixel.size());

			gpu::command_buffer cmd(device);
			::mngl::texture texture(device, cmd, surface.get());
			cmd.submit();
			return texture;
		}

		const gpu::device& m_device;
		// V2 handles are append-only and stable until the store is destroyed.
		std::deque<::mngl::texture> m_pages;
		texture_handle m_white_page{};
	};

}

namespace mngl
{
	struct sprite
	{
		sprite() = default;

		sprite(
			const texture& image,
			const rectangle& destination,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f),
			gpu::quad_draw_state state = {}
		)
			: source_texture(&image)
			, destination(destination)
			, tint(tint)
			, state(std::move(state))
		{ }

		sprite(
			two_d::texture_region source,
			const rectangle& destination = {},
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f),
			gpu::quad_draw_state state = {}
		)
			: source(source)
			, destination(destination)
			, tint(tint)
			, state(std::move(state))
		{ }

		[[nodiscard]] glm::vec2 position() const noexcept
		{
			return destination.pos;
		}

		sprite& position(glm::vec2 value) noexcept
		{
			destination.pos = value;
			return *this;
		}

		[[nodiscard]] glm::vec2 size() const noexcept
		{
			return destination.size;
		}

		sprite& size(glm::vec2 value) noexcept
		{
			destination.size = value;
			return *this;
		}

		[[nodiscard]] float rotation() const noexcept
		{
			return rotation_radians;
		}

		sprite& rotation(float value) noexcept
		{
			rotation_radians = value;
			return *this;
		}

		void on_click(hit_test_callback callback)
		{
			detail::set_hit_test_callback(click_callback, std::move(callback));
		}

		void clear_on_click() noexcept
		{
			click_callback.reset();
		}

		two_d::texture_region source{};
		// Non-owning; keep this texture alive until each frame using the sprite is submitted.
		const texture* source_texture = nullptr;
		rectangle destination{};
		glm::vec2 pivot{ 0.5f, 0.5f };
		float rotation_radians = 0.0f;
		two_d::transform transform{};
		color tint = color(1.0f, 1.0f, 1.0f, 1.0f);
		gpu::quad_draw_state state{};
		std::shared_ptr<const hit_test_callback> click_callback{};
	};
}

namespace mngl::two_d
{
	struct circle
	{
		glm::vec2 center{ 0.0f, 0.0f };
		float radius = 0.0f;
		color tint = color(1.0f, 1.0f, 1.0f, 1.0f);
		gpu::quad_draw_state state{};
		::mngl::two_d::transform transform{};
		std::shared_ptr<const ::mngl::hit_test_callback> click_callback{};

		void on_click(::mngl::hit_test_callback callback)
		{
			::mngl::detail::set_hit_test_callback(click_callback, std::move(callback));
		}

		void clear_on_click() noexcept
		{
			click_callback.reset();
		}
	};

	struct ellipse
	{
		glm::vec2 center{ 0.0f, 0.0f };
		glm::vec2 radii{ 0.0f, 0.0f };
		color tint = color(1.0f, 1.0f, 1.0f, 1.0f);
		gpu::quad_draw_state state{};
		::mngl::two_d::transform transform{};
		std::shared_ptr<const ::mngl::hit_test_callback> click_callback{};

		void on_click(::mngl::hit_test_callback callback)
		{
			::mngl::detail::set_hit_test_callback(click_callback, std::move(callback));
		}

		void clear_on_click() noexcept
		{
			click_callback.reset();
		}
	};

	struct rounded_rectangle
	{
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

		void clear_on_click() noexcept
		{
			click_callback.reset();
		}
	};

	class renderer;

	namespace detail
	{
		struct renderer_frame_control
		{
			renderer* owner = nullptr;
		};
	}

	class Frame2D
	{
	public:
		Frame2D(const Frame2D&) = delete;
		Frame2D& operator=(const Frame2D&) = delete;

		Frame2D(Frame2D&& other) noexcept
			: m_control(std::move(other.m_control))
			, m_token(std::exchange(other.m_token, 0))
			, m_view_projection(std::move(other.m_view_projection))
			, m_view(std::move(other.m_view))
		{
			other.m_view_projection.reset();
			other.m_view.reset();
		}

		Frame2D& operator=(Frame2D&& other) noexcept;
		~Frame2D() noexcept;

		void draw(const ::mngl::sprite& value);
		void draw(const circle& value);
		void draw(const ellipse& value);
		void draw(const rounded_rectangle& value);
		void draw(
			const ::mngl::rectangle& bounds,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f)
		);
		void draw(
			const ::mngl::rectangle& bounds,
			const color& tint,
			const transform& object_transform
		);
		// The source is borrowed until finish() submits the frame's command buffer.
		void draw(
			const ::mngl::texture& source,
			const ::mngl::rectangle& bounds,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f)
		);
		void draw(
			const ::mngl::texture& source,
			const ::mngl::rectangle& bounds,
			const color& tint,
			const transform& object_transform
		);
		void draw_quad(
			const texture_region& source,
			const gpu::quad_transform& transform,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f),
			const gpu::quad_draw_state& state = {}
		);
		// The font must outlive submission of the command buffer that records this frame.
		void draw_text(
			font& typeface,
			std::string_view text,
			glm::vec2 top_left,
			const text_draw_options& options = {});
		void draw_text(
			font& typeface,
			const text_document& document,
			glm::vec2 top_left,
			const text_draw_options& options);
		template<typename DrawFunction>
			requires std::invocable<DrawFunction, Frame2D&>
		void draw(DrawFunction&& draw_function)
		{
			ensure_active();
			std::invoke(std::forward<DrawFunction>(draw_function), *this);
		}

		void set_view_projection(const glm::mat4& matrix);
		void set_view(const ::mngl::view& value);
		void finish();
		// Records into the supplied command buffer without submitting it.
		void finish(gpu::command_buffer& command_buffer);

	private:
		friend class renderer;

		Frame2D(
			std::shared_ptr<detail::renderer_frame_control> control,
			uint64_t token
		) noexcept
			: m_control(std::move(control)), m_token(token)
		{ }

		void ensure_active() const;
		void cancel() noexcept;
		void detach() noexcept
		{
			m_control.reset();
			m_token = 0;
			m_view_projection.reset();
			m_view.reset();
		}

		std::shared_ptr<detail::renderer_frame_control> m_control;
		uint64_t m_token = 0;
		std::optional<glm::mat4> m_view_projection;
		std::optional<::mngl::view> m_view;
	};

	class renderer
	{
	public:
		// Borrows the device, texture store, and optional swapchain window; they must outlive the renderer.
		renderer(
			const gpu::device& device,
			window& target_window,
			texture_store& textures
		)
			: renderer(
				device,
				textures,
				required_swapchain_format(device, target_window))
		{
			m_window = &target_window;
			m_custom_materials_allowed = true;
		}

		renderer(
			const gpu::device& device,
			texture_store& textures,
			gpu::texture_format target_format
		)
			: m_device(device)
			, m_textures(textures)
			, m_batcher(device, target_format)
			, m_frame_control(std::make_shared<detail::renderer_frame_control>())
		{
			if (m_textures.device_raw() != m_device.raw())
			{
				throw std::invalid_argument("2D renderer and texture store must use the same GPU device");
			}
			m_frame_control->owner = this;
		}

		renderer(const renderer&) = delete;
		renderer& operator=(const renderer&) = delete;
		renderer(renderer&&) = delete;
		renderer& operator=(renderer&&) = delete;

		~renderer()
		{
			if (m_frame_active)
			{
				m_batcher.cancel_frame();
			}
			m_frame_control->owner = nullptr;
		}

		[[nodiscard]] Frame2D begin_frame(
			const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f)
		)
		{
			if (!m_window)
			{
				throw std::logic_error("This 2D renderer requires a render target texture");
			}
			return begin_frame_impl(nullptr, clear_color);
		}

		[[nodiscard]] Frame2D begin_frame(
			::mngl::texture& target,
			const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f)
		)
		{
			if (!target.valid() || !target.can_render_to())
			{
				throw std::invalid_argument("2D render target must be a valid color-target texture");
			}
			if (target.raw_gpu_texture().device_raw() != m_device.raw())
			{
				throw std::invalid_argument("2D render target belongs to a different GPU device");
			}
			if (target.format() != target_format())
			{
				throw std::invalid_argument("2D render target format does not match its renderer");
			}
			return begin_frame_impl(&target, clear_color);
		}

		[[nodiscard]] gpu::texture_format target_format() const noexcept
		{
			return m_batcher.target_format();
		}

		void enable_custom_materials()
		{
			if (m_frame_active)
			{
				throw std::logic_error("Custom materials cannot be enabled during a 2D frame");
			}
			m_custom_materials_allowed = true;
		}

	private:
		[[nodiscard]] Frame2D begin_frame_impl(
			::mngl::texture* target,
			const color& clear_color
		)
		{
			if (m_frame_active)
			{
				throw std::logic_error("A 2D renderer frame is already active");
			}

			m_batcher.begin_frame(clear_color);
			m_active_target = target;
			m_hit_test_items.clear();
			m_used_fonts.clear();
			if (++m_next_frame_token == 0)
			{
				++m_next_frame_token;
			}
			m_active_frame_token = m_next_frame_token;
			m_frame_active = true;
			Frame2D frame(m_frame_control, m_active_frame_token);
			if (target)
			{
				frame.m_view = target->get_view();
			}
			return frame;
		}

	public:
		[[nodiscard]] gpu::material_handle create_material(
			const gpu::graphics_shader_asset& fragment_shader,
			gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha
		)
		{
			if (!m_custom_materials_allowed)
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			return m_batcher.create_material(fragment_shader, blend);
		}

		[[nodiscard]] gpu::material_instance_handle create_material_instance(
			gpu::material_handle material,
			gpu::fragment_resource_bindings bindings
		)
		{
			if (!m_custom_materials_allowed)
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			return m_batcher.create_material_instance(material, std::move(bindings));
		}

		[[nodiscard]] gpu::material_instance_handle create_material_instance_from_shader(
			gpu::material_handle material,
			const gpu::graphics_shader_asset& fragment_shader,
			gpu::fragment_resource_bindings bindings = {}
		)
		{
			bindings.add_uniform_blocks(fragment_shader);
			const gpu::sampler* automatic_sampler = fragment_shader.needs_default_sampler()
				? &default_sampler()
				: nullptr;
			bindings.add_shader_resources(fragment_shader, automatic_sampler);
			return create_material_instance(material, std::move(bindings));
		}

		void update_material_instance_from_shader(
			gpu::material_instance_handle instance,
			const gpu::graphics_shader_asset& fragment_shader,
			gpu::fragment_resource_bindings bindings = {}
		)
		{
			bindings.add_uniform_blocks(fragment_shader);
			const gpu::sampler* automatic_sampler = fragment_shader.needs_default_sampler()
				? &default_sampler()
				: nullptr;
			bindings.add_shader_resources(fragment_shader, automatic_sampler);
			update_material_instance(instance, std::move(bindings));
		}

		void update_material_instance(
			gpu::material_instance_handle instance,
			gpu::fragment_resource_bindings bindings
		)
		{
			if (!m_custom_materials_allowed)
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			m_batcher.update_material_instance(instance, std::move(bindings));
		}

		void destroy_material_instance(gpu::material_instance_handle instance)
		{
			if (!m_custom_materials_allowed)
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			m_batcher.destroy_material_instance(instance);
		}

		void destroy_material(gpu::material_handle material)
		{
			if (!m_custom_materials_allowed)
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			m_batcher.destroy_material(material);
		}

		[[nodiscard]] const gpu::batcher2d::frame_stats& last_frame_stats() const noexcept
		{
			return m_batcher.last_frame_stats();
		}

	private:
		friend class Frame2D;

		static gpu::texture_format required_swapchain_format(
			const gpu::device& device,
			const window& target_window
		)
		{
			if (!device.raw())
			{
				throw std::invalid_argument("Cannot create a 2D renderer with a null GPU device");
			}
			if (!target_window.raw())
			{
				throw std::invalid_argument("Cannot create a 2D renderer with a null window");
			}
			const auto format = device.swapchain_texture_format(target_window);
			if (format == gpu::texture_format::invalid)
			{
				throw std::runtime_error("The renderer window must be claimed by its GPU device");
			}
			return format;
		}

		[[nodiscard]] bool owns_frame(uint64_t token) const noexcept
		{
			return m_frame_active && token != 0 && token == m_active_frame_token;
		}

		[[nodiscard]] gpu::sampler& default_sampler()
		{
			if (!m_default_sampler)
			{
				m_default_sampler.emplace(gpu::sampler_builder::create_linear_clamp(m_device.raw()));
			}
			return *m_default_sampler;
		}

		void submit_quad(
			uint64_t token,
			const texture_region& source,
			const gpu::quad_transform& transform,
			const color& tint,
			const gpu::quad_draw_state& state,
			::mngl::detail::hit_test_shape shape = ::mngl::detail::hit_test_shape::quad,
			float corner_radius = 0.0f,
			const std::shared_ptr<const ::mngl::hit_test_callback>& click_callback = {}
		)
		{
			ensure_frame(token);
			if (state.material.valid() && !m_custom_materials_allowed)
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			const gpu::texture* page = nullptr;
			const ::mngl::texture* source_texture = nullptr;
			if (source.page.valid())
			{
				source_texture = &m_textures.resolve(source.page);
				page = &source_texture->raw_gpu_texture();
				if (m_active_target && page == &m_active_target->raw_gpu_texture())
				{
					throw std::invalid_argument("A render target texture cannot also be sampled in the same frame");
				}
			}
			else if (!state.material.valid())
			{
				throw std::invalid_argument("A textured quad requires a valid texture region");
			}
			m_batcher.draw_quad(
				gpu::texture_region_view{ page, source.uv_bounds },
				transform,
				tint,
				state);
			record_hit_item(
				transform,
				source.uv_bounds,
				source_texture,
				state,
				shape,
				corner_radius,
				click_callback);
		}

		void submit_rectangle(
			uint64_t token,
			const rectangle& bounds,
			const color& tint,
			const ::mngl::two_d::transform& object_transform = {}
		)
		{
			ensure_frame(token);
			gpu::quad_transform transform{};
			transform.destination = bounds;
			transform.matrix(object_transform.matrix());
			submit_quad(token, m_textures.white_region(), transform, tint, {});
		}

		void submit_texture(
			uint64_t token,
			const ::mngl::texture& source,
			const gpu::quad_transform& transform,
			const color& tint,
			const gpu::quad_draw_state& state,
			const std::shared_ptr<const ::mngl::hit_test_callback>& click_callback = {}
		)
		{
			ensure_frame(token);
			if (!source.valid())
			{
				throw std::invalid_argument("A 2D sprite source must be a valid texture");
			}
			if (m_active_target == &source)
			{
				throw std::invalid_argument("A render target texture cannot also be sampled in the same frame");
			}
			if (source.rendering())
			{
				throw std::logic_error("A texture being rendered to cannot be used as a 2D sprite source");
			}

			const auto& gpu_texture = source.raw_gpu_texture();
			if (gpu_texture.device_raw() != m_device.raw())
			{
				throw std::invalid_argument("2D sprite source texture belongs to a different GPU device");
			}
			if (!gpu_texture.has_usage(gpu::texture::usage_flags::sampler))
			{
				throw std::invalid_argument("2D sprite source texture must have sampler usage");
			}

			m_batcher.draw_quad(
				gpu::texture_region_view{ &gpu_texture, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },
				transform,
				tint,
				state);
			record_hit_item(
				transform,
				glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
				&source,
				state,
				::mngl::detail::hit_test_shape::quad,
				0.0f,
				click_callback);
		}

		void submit_circle(uint64_t token, const circle& value)
		{
			ensure_frame(token);
			if (!std::isfinite(value.center.x) || !std::isfinite(value.center.y) ||
				!std::isfinite(value.radius) || value.radius < 0.0f)
			{
				throw std::invalid_argument("Circle center and radius must be finite and radius non-negative");
			}
			if (value.radius == 0.0f)
			{
				return;
			}

			gpu::quad_transform transform{};
			transform.destination = rectangle{
				value.center.x - value.radius,
				value.center.y - value.radius,
				value.radius * 2.0f,
				value.radius * 2.0f
			};
			transform.matrix(value.transform.matrix());
			auto state = value.state;
			state.effect = gpu::quad_effect::circle;
			submit_quad(
				token,
				m_textures.white_region(),
				transform,
				value.tint,
				state,
				::mngl::detail::hit_test_shape::circle,
				0.0f,
				value.click_callback);
		}

		void submit_ellipse(uint64_t token, const ellipse& value)
		{
			ensure_frame(token);
			if (!std::isfinite(value.center.x) || !std::isfinite(value.center.y) ||
				!std::isfinite(value.radii.x) || !std::isfinite(value.radii.y) ||
				value.radii.x < 0.0f || value.radii.y < 0.0f)
			{
				throw std::invalid_argument("Ellipse center and radii must be finite and radii non-negative");
			}
			if (value.radii.x == 0.0f || value.radii.y == 0.0f)
			{
				return;
			}

			gpu::quad_transform transform{};
			transform.destination = rectangle{
				value.center.x - value.radii.x,
				value.center.y - value.radii.y,
				value.radii.x * 2.0f,
				value.radii.y * 2.0f
			};
			transform.matrix(value.transform.matrix());
			auto state = value.state;
			state.effect = gpu::quad_effect::ellipse;
			submit_quad(
				token,
				m_textures.white_region(),
				transform,
				value.tint,
				state,
				::mngl::detail::hit_test_shape::ellipse,
				0.0f,
				value.click_callback);
		}

		void submit_rounded_rectangle(uint64_t token, const rounded_rectangle& value)
		{
			ensure_frame(token);
			if (!std::isfinite(value.radius) || value.radius < 0.0f)
			{
				throw std::invalid_argument("Rounded rectangle radius must be finite and non-negative");
			}
			if (!std::isfinite(value.bounds.pos.x) || !std::isfinite(value.bounds.pos.y) ||
				!std::isfinite(value.bounds.size.x) || !std::isfinite(value.bounds.size.y) ||
				value.bounds.size.x < 0.0f || value.bounds.size.y < 0.0f)
			{
				throw std::invalid_argument("Rounded rectangle bounds must be finite and non-negative");
			}
			if (value.bounds.size.x == 0.0f || value.bounds.size.y == 0.0f)
			{
				return;
			}

			gpu::quad_transform transform{};
			transform.destination = value.bounds;
			transform.matrix(value.transform.matrix());
			auto state = value.state;
			state.effect = gpu::quad_effect::rounded_rectangle;
			const float half_min_dimension =
				std::min(value.bounds.size.x, value.bounds.size.y) * 0.5f;
			state.effect_parameters.x = std::clamp(value.radius / half_min_dimension, 0.0f, 1.0f);
			submit_quad(
				token,
				m_textures.white_region(),
				transform,
				value.tint,
				state,
				::mngl::detail::hit_test_shape::rounded_rectangle,
				value.radius,
				value.click_callback);
		}

		void record_hit_item(
			const gpu::quad_transform& transform,
			const glm::vec4& uv_bounds,
			const ::mngl::texture* source,
			const gpu::quad_draw_state& state,
			::mngl::detail::hit_test_shape shape,
			float corner_radius,
			const std::shared_ptr<const ::mngl::hit_test_callback>& click_callback
		)
		{
			if (!m_active_target || !m_active_target->m_hit_testing_enabled)
			{
				return;
			}

			std::shared_ptr<const ::mngl::detail::hit_test_frame> child_frame;
			if (source && source->m_hit_test_frame &&
				source->m_hit_test_frame->depth < ::mngl::detail::hit_test_max_depth)
			{
				child_frame = source->m_hit_test_frame;
			}

			if (shape == ::mngl::detail::hit_test_shape::quad)
			{
				switch (state.effect)
				{
				case gpu::quad_effect::circle:
					shape = ::mngl::detail::hit_test_shape::circle;
					break;
				case gpu::quad_effect::ellipse:
					shape = ::mngl::detail::hit_test_shape::ellipse;
					break;
				case gpu::quad_effect::rounded_rectangle:
					shape = ::mngl::detail::hit_test_shape::rounded_rectangle;
					corner_radius = std::clamp(
						state.effect_parameters.x,
						0.0f,
						1.0f) * std::min(
							transform.destination.size.x,
							transform.destination.size.y) * 0.5f;
					break;
				case gpu::quad_effect::textured:
					break;
				}
			}

			std::optional<glm::ivec4> scissor;
			if (state.scissor)
			{
				scissor = glm::ivec4{
					state.scissor->x,
					state.scissor->y,
					state.scissor->w,
					state.scissor->h
				};
			}
			const glm::mat4 local_to_world = transform.model_matrix();
			const float determinant = glm::determinant(local_to_world);
			glm::mat4 world_to_local(1.0f);
			bool inverse_valid =
				std::isfinite(determinant) &&
				std::abs(determinant) > std::numeric_limits<float>::min();
			if (inverse_valid)
			{
				world_to_local = glm::inverse(local_to_world);
				inverse_valid = ::mngl::detail::finite(world_to_local);
			}
			m_hit_test_items.push_back(::mngl::detail::hit_test_item{
				.draw_index = m_hit_test_items.size(),
				.destination = transform.destination,
				.world_to_local = world_to_local,
				.inverse_valid = inverse_valid,
				.shape = shape,
				.corner_radius = corner_radius,
				.uv_bounds = uv_bounds,
				.source_identity = source ? source->m_hit_test_identity : 0,
				.source_size = source ? source->size() : glm::uvec2{},
				.scissor = scissor,
				.child_frame = std::move(child_frame),
				.click_callback = click_callback
			});
		}

		void remember_font(font& typeface)
		{
			if (typeface.device_raw() != m_device.raw())
			{
				throw std::invalid_argument("Font and 2D renderer must use the same GPU device");
			}
			if (std::find(m_used_fonts.begin(), m_used_fonts.end(), &typeface) == m_used_fonts.end())
			{
				m_used_fonts.push_back(&typeface);
			}
		}

		static void validate_text_options(
			glm::vec2 top_left,
			const text_draw_options& options
		)
		{
			if (!::mngl::detail::finite(top_left) ||
				!std::isfinite(options.height) ||
				options.height <= 0.0f)
			{
				throw std::invalid_argument(
					"Text position must be finite and text height must be finite and positive");
			}

			const glm::vec4 tint = options.tint.rgba_glsl();
			if (!std::isfinite(tint.x) || !std::isfinite(tint.y) ||
				!std::isfinite(tint.z) || !std::isfinite(tint.w))
			{
				throw std::invalid_argument("Text tint channels must be finite");
			}

			if (options.clip_bounds)
			{
				const auto& clip = *options.clip_bounds;
				if (!::mngl::detail::finite(clip.pos) ||
					!::mngl::detail::finite(clip.size) ||
					clip.size.x < 0.0f || clip.size.y < 0.0f ||
					!std::isfinite(clip.pos.x + clip.size.x) ||
					!std::isfinite(clip.pos.y + clip.size.y))
				{
					throw std::invalid_argument(
						"Text clip bounds must be finite with non-negative dimensions");
				}
			}

			if (options.scissor && (options.scissor->w < 0 || options.scissor->h < 0))
			{
				throw std::invalid_argument("Text scissor dimensions must be non-negative");
			}
		}

		[[nodiscard]] float submit_text_line(
			font& typeface,
			std::string_view text,
			glm::vec2 baseline,
			float scale,
			const text_draw_options& options
		)
		{
			if (!::mngl::detail::finite(baseline))
			{
				throw std::length_error("Text line exceeds the renderer's coordinate range");
			}

			float pen_x = baseline.x;
			uint32_t previous_codepoint = 0;
			const auto space = typeface.glyph(' ', false);
			const float tab_width = space.advance * scale * 4.0f;
			gpu::quad_draw_state state{};
			state.scissor = options.scissor;
			const bool scissor_empty =
				options.scissor && (options.scissor->w == 0 || options.scissor->h == 0);

			for (std::size_t offset = 0; offset < text.size();)
			{
				uint32_t codepoint = 0;
				(void)::mngl::two_d::detail::next_utf8_codepoint(
					text,
					offset,
					codepoint);
				if (codepoint == '\t')
				{
					if (tab_width > 0.0f)
					{
						const float relative_x = pen_x - baseline.x;
						pen_x = baseline.x +
							(std::floor(relative_x / tab_width) + 1.0f) * tab_width;
					}
					if (!std::isfinite(pen_x))
					{
						throw std::length_error(
							"Text line exceeds the renderer's coordinate range");
					}
					previous_codepoint = 0;
					continue;
				}

				const auto metrics = typeface.glyph(codepoint, false);
				if (previous_codepoint != 0)
				{
					pen_x += typeface.kerning(
						previous_codepoint,
						metrics.codepoint) * scale;
				}
				if (!std::isfinite(pen_x))
				{
					throw std::length_error("Text line exceeds the renderer's coordinate range");
				}

				if (metrics.width > 0 && metrics.height > 0)
				{
					const rectangle glyph_bounds{
						pen_x + static_cast<float>(metrics.x_offset) * scale,
						baseline.y -
							(static_cast<float>(metrics.y_offset) +
								static_cast<float>(metrics.height)) * scale,
						static_cast<float>(metrics.width) * scale,
						static_cast<float>(metrics.height) * scale
					};
					if (!::mngl::detail::finite(glyph_bounds.pos) ||
						!::mngl::detail::finite(glyph_bounds.size) ||
						!std::isfinite(glyph_bounds.pos.x + glyph_bounds.size.x) ||
						!std::isfinite(glyph_bounds.pos.y + glyph_bounds.size.y))
					{
						throw std::length_error(
							"Text glyph exceeds the renderer's coordinate range");
					}
					const bool in_clip = !options.clip_bounds ||
						glyph_bounds.intersects(*options.clip_bounds);
					if (!scissor_empty && in_clip)
					{
						const auto glyph = typeface.glyph(codepoint, true);
						if (!glyph.atlas_page)
						{
							throw std::logic_error("Rasterized text glyph has no atlas page");
						}

						gpu::quad_transform transform{};
						transform.destination = glyph_bounds;
						m_batcher.draw_quad(
							gpu::texture_region_view{
								&glyph.atlas_page->raw_gpu_texture(),
								glyph.uv_bounds
							},
							transform,
							options.tint,
							state);
					}
				}

				pen_x += metrics.advance * scale;
				if (!std::isfinite(pen_x))
				{
					throw std::length_error("Text line exceeds the renderer's coordinate range");
				}
				previous_codepoint = metrics.codepoint;
			}

			const float width = pen_x - baseline.x;
			if (!std::isfinite(width))
			{
				throw std::length_error("Text line exceeds the renderer's coordinate range");
			}
			return width;
		}

		void record_text_hit(
			const rectangle& bounds,
			const text_draw_options& options
		)
		{
			if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f)
			{
				return;
			}

			gpu::quad_transform transform{};
			transform.destination = bounds;
			gpu::quad_draw_state state{};
			state.scissor = options.scissor;
			record_hit_item(
				transform,
				glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
				nullptr,
				state,
				::mngl::detail::hit_test_shape::quad,
				0.0f,
				{});
		}

		void submit_text(
			uint64_t token,
			font& typeface,
			std::string_view text,
			glm::vec2 top_left,
			const text_draw_options& options
		)
		{
			ensure_frame(token);
			validate_text_options(top_left, options);
			if (text.empty() ||
				(options.clip_bounds &&
					(options.clip_bounds->size.x == 0.0f ||
						options.clip_bounds->size.y == 0.0f)) ||
				(options.scissor &&
					(options.scissor->w == 0 || options.scissor->h == 0)))
			{
				return;
			}

			remember_font(typeface);
			const float scale = options.height / typeface.raster_height_pixels();
			const float line_height = typeface.line_height() * scale;
			const float ascent = typeface.ascent() * scale;
			if (!std::isfinite(scale) || scale <= 0.0f ||
				!std::isfinite(line_height) || line_height <= 0.0f ||
				!std::isfinite(ascent))
			{
				throw std::invalid_argument("Font metrics cannot be scaled to the requested text height");
			}

			std::size_t line_index = 0;
			std::size_t line_begin = 0;
			float max_width = 0.0f;
			while (line_begin <= text.size())
			{
				const std::size_t line_end = text.find('\n', line_begin);
				std::string_view line_text = text.substr(
					line_begin,
					line_end == std::string_view::npos
						? line_end
						: line_end - line_begin);
				if (!line_text.empty() && line_text.back() == '\r')
				{
					line_text.remove_suffix(1);
				}

				const float top = top_left.y -
					static_cast<float>(line_index) * line_height;
				if (!std::isfinite(top))
				{
					throw std::length_error("Text block exceeds the renderer's coordinate range");
				}
				const bool line_visible = !options.clip_bounds ||
					(top > options.clip_bounds->pos.y &&
						top - line_height <
							options.clip_bounds->pos.y + options.clip_bounds->size.y);
				if (line_visible)
				{
					const float width = submit_text_line(
						typeface,
						line_text,
						{ top_left.x, top - ascent },
						scale,
						options);
					max_width = std::max(max_width, width);
				}
				++line_index;
				if (line_end == std::string_view::npos)
				{
					break;
				}
				line_begin = line_end + 1;
			}

			const float block_height = static_cast<float>(line_index) * line_height;
			rectangle bounds{
				top_left.x,
				top_left.y - block_height,
				max_width,
				block_height
			};
			if (!::mngl::detail::finite(bounds.pos) ||
				!::mngl::detail::finite(bounds.size) ||
				!std::isfinite(bounds.pos.x + bounds.size.x) ||
				!std::isfinite(bounds.pos.y + bounds.size.y))
			{
				throw std::length_error("Text block exceeds the renderer's coordinate range");
			}
			if (options.clip_bounds)
			{
				bounds = bounds.intersection(*options.clip_bounds);
			}
			record_text_hit(bounds, options);
		}

		void submit_text_document(
			uint64_t token,
			font& typeface,
			const text_document& document,
			glm::vec2 top_left,
			const text_draw_options& options
		)
		{
			ensure_frame(token);
			validate_text_options(top_left, options);
			if (!options.clip_bounds)
			{
				throw std::invalid_argument(
					"Drawing a text document requires clip_bounds for line virtualization");
			}
			const auto& viewport = *options.clip_bounds;
			if (viewport.size.x == 0.0f || viewport.size.y == 0.0f ||
				document.line_count() == 0 ||
				(options.scissor &&
					(options.scissor->w == 0 || options.scissor->h == 0)))
			{
				return;
			}

			remember_font(typeface);
			const float scale = options.height / typeface.raster_height_pixels();
			const float line_height = typeface.line_height() * scale;
			const float ascent = typeface.ascent() * scale;
			if (!std::isfinite(scale) || scale <= 0.0f ||
				!std::isfinite(line_height) || line_height <= 0.0f ||
				!std::isfinite(ascent))
			{
				throw std::invalid_argument("Font metrics cannot be scaled to the requested text height");
			}

			const double first_offset =
				(static_cast<double>(top_left.y) -
					(static_cast<double>(viewport.pos.y) + viewport.size.y)) /
				line_height;
			const double end_offset =
				(static_cast<double>(top_left.y) - viewport.pos.y) / line_height;
			const std::size_t first_line = first_offset <= 0.0
				? 0
				: static_cast<std::size_t>(std::min(
					std::floor(first_offset),
					static_cast<double>(document.line_count())));
			const std::size_t end_line = end_offset <= 0.0
				? 0
				: static_cast<std::size_t>(std::min(
					std::ceil(end_offset),
					static_cast<double>(document.line_count())));
			if (first_line >= end_line)
			{
				return;
			}

			float max_width = 0.0f;
			for (std::size_t index = first_line; index < end_line; ++index)
			{
				const float line_top = top_left.y -
					static_cast<float>(index) * line_height;
				if (!std::isfinite(line_top))
				{
					throw std::length_error("Text document exceeds the renderer's coordinate range");
				}
				max_width = std::max(
					max_width,
					submit_text_line(
						typeface,
						document.line(index),
						{ top_left.x, line_top - ascent },
						scale,
						options));
			}

			const float block_height =
				static_cast<float>(end_line - first_line) * line_height;
			rectangle bounds{
				top_left.x,
				top_left.y - static_cast<float>(end_line) * line_height,
				max_width,
				block_height
			};
			if (!::mngl::detail::finite(bounds.pos) ||
				!::mngl::detail::finite(bounds.size) ||
				!std::isfinite(bounds.pos.x + bounds.size.x) ||
				!std::isfinite(bounds.pos.y + bounds.size.y))
			{
				throw std::length_error("Text document exceeds the renderer's coordinate range");
			}
			bounds = bounds.intersection(viewport);
			record_text_hit(bounds, options);
		}

		void finish_frame(Frame2D& frame, gpu::command_buffer* external_command_buffer = nullptr)
		{
			ensure_frame(frame.m_token);
			bool font_uploads_started = false;
			try
			{
				std::shared_ptr<::mngl::detail::hit_test_frame> hit_frame;
				if (m_active_target && m_active_target->m_hit_testing_enabled)
				{
					hit_frame = std::make_shared<::mngl::detail::hit_test_frame>();
					hit_frame->target_size = m_active_target->size();
					hit_frame->view_projection = projection_for_frame(frame, hit_frame->target_size);
					hit_frame->items = std::move(m_hit_test_items);
					for (const auto& item : hit_frame->items)
					{
						if (item.child_frame)
						{
							hit_frame->depth = std::max(
								hit_frame->depth,
								item.child_frame->depth + 1);
						}
					}
				}

				auto flush = [&](gpu::command_buffer& cmd)
				{
					if (!cmd.valid() || cmd.device_raw() != m_device.raw())
					{
						throw std::invalid_argument("2D frame command buffer must belong to the renderer's GPU device");
					}

					font_uploads_started = true;
					for (font* typeface : m_used_fonts)
					{
						if (!typeface)
						{
							throw std::logic_error("2D frame contains a null font reference");
						}
						typeface->upload_pending(cmd);
					}

					auto flush_target = [&](gpu::texture& target)
					{
						const auto projection = projection_for_frame(
							frame,
							target.get_size());
						m_batcher.flush(cmd, target, projection);
					};

					if (m_active_target)
					{
						flush_target(m_active_target->raw_gpu_texture());
					}
					else
					{
						if (!m_window)
						{
							throw std::logic_error("This 2D renderer requires a render target texture");
						}
						auto target = cmd.acquire_swapchain_texture(*m_window);
						flush_target(target);
					}
				};

				if (external_command_buffer)
				{
					flush(*external_command_buffer);
				}
				else
				{
					gpu::command_buffer cmd(m_device);
					flush(cmd);
					cmd.submit();
				}

				if (hit_frame)
				{
					m_active_target->m_hit_test_frame = std::move(hit_frame);
				}
			}
			catch (...)
			{
				if (font_uploads_started)
				{
					for (font* typeface : m_used_fonts)
					{
						if (typeface)
						{
							typeface->mark_uploads_dirty();
						}
					}
				}
				m_batcher.cancel_frame();
				m_frame_active = false;
				m_active_frame_token = 0;
				m_active_target = nullptr;
				m_hit_test_items.clear();
				m_used_fonts.clear();
				frame.detach();
				throw;
			}

			m_frame_active = false;
			m_active_frame_token = 0;
			m_active_target = nullptr;
			m_hit_test_items.clear();
			m_used_fonts.clear();
			frame.detach();
		}

		void cancel_frame(uint64_t token) noexcept
		{
			if (owns_frame(token))
			{
				m_batcher.cancel_frame();
				m_frame_active = false;
				m_active_frame_token = 0;
				m_active_target = nullptr;
				m_hit_test_items.clear();
				m_used_fonts.clear();
			}
		}

		static glm::mat4 coordinate_projection(glm::uvec2 target_size)
		{
			glm::mat4 projection(1.0f);
			if (target_size.x == 0 || target_size.y == 0)
			{
				return projection;
			}

			const float short_side = static_cast<float>(
				std::min(target_size.x, target_size.y));
			const float coordinate_width = static_cast<float>(target_size.x) / short_side;
			const float coordinate_height = static_cast<float>(target_size.y) / short_side;
			projection[0][0] = 2.0f / coordinate_width;
			projection[1][1] = 2.0f / coordinate_height;
			projection[3][0] = -1.0f;
			projection[3][1] = -1.0f;
			return projection;
		}

		static glm::mat4 projection_for_frame(
			const Frame2D& frame,
			glm::uvec2 target_size
		)
		{
			if (frame.m_view_projection)
			{
				return *frame.m_view_projection;
			}
			if (frame.m_view)
			{
				return frame.m_view->projection(target_size);
			}
			return coordinate_projection(target_size);
		}

		void ensure_frame(uint64_t token) const
		{
			if (!owns_frame(token))
			{
				throw std::logic_error("Frame2D is no longer active");
			}
		}

		const gpu::device& m_device;
		window* m_window = nullptr;
		texture_store& m_textures;
		std::optional<gpu::sampler> m_default_sampler;
		gpu::batcher2d m_batcher;
		std::shared_ptr<detail::renderer_frame_control> m_frame_control;
		::mngl::texture* m_active_target = nullptr;
		std::vector<::mngl::detail::hit_test_item> m_hit_test_items;
		std::vector<font*> m_used_fonts;
		bool m_custom_materials_allowed = false;
		uint64_t m_next_frame_token = 0;
		uint64_t m_active_frame_token = 0;
		bool m_frame_active = false;
	};

	class render_context
	{
	public:
		[[nodiscard]] static render_context& global()
		{
			static render_context context(gpu::device::global());
			return context;
		}

		render_context(const render_context&) = delete;
		render_context& operator=(const render_context&) = delete;

		[[nodiscard]] const gpu::device& gpu_device() const noexcept
		{
			return m_device;
		}

		[[nodiscard]] texture_store& textures() noexcept
		{
			return m_textures;
		}

		[[nodiscard]] renderer& renderer_for(
			gpu::texture_format target_format,
			bool allow_custom_materials = false
		)
		{
			if (target_format == gpu::texture_format::invalid)
			{
				throw std::invalid_argument("A render context requires a valid target format");
			}

			auto& target_renderer = m_renderers[target_format];
			if (!target_renderer)
			{
				target_renderer = std::make_unique<renderer>(
					m_device,
					m_textures,
					target_format);
			}
			if (allow_custom_materials)
			{
				target_renderer->enable_custom_materials();
			}
			return *target_renderer;
		}

	private:
		explicit render_context(const gpu::device& device)
			: m_device(device)
			, m_textures(device)
		{ }

		const gpu::device& m_device;
		texture_store m_textures;
		std::unordered_map<gpu::texture_format, std::unique_ptr<renderer>> m_renderers;
	};

}

namespace mngl
{
	namespace detail
	{
		class texture_render_session_impl final : public texture_render_session
		{
		public:
			texture_render_session_impl(::mngl::texture& target, const color& clear_color)
				: m_command_buffer(two_d::render_context::global().gpu_device())
				, m_frame(two_d::render_context::global().renderer_for(target.format()).begin_frame(target, clear_color))
			{ }

			void draw(const sprite& value) override { m_frame.draw(value); }
			void draw(const two_d::circle& value) override { m_frame.draw(value); }
			void draw(const two_d::ellipse& value) override { m_frame.draw(value); }
			void draw(const two_d::rounded_rectangle& value) override { m_frame.draw(value); }
			void draw(const rectangle& bounds, const color& tint) override
			{
				m_frame.draw(bounds, tint);
			}
			void draw(
				const rectangle& bounds,
				const color& tint,
				const two_d::transform& transform
			) override
			{
				m_frame.draw(bounds, tint, transform);
			}
			void draw(const texture& source, const rectangle& bounds, const color& tint) override
			{
				m_frame.draw(source, bounds, tint);
			}
			void draw(
				const texture& source,
				const rectangle& bounds,
				const color& tint,
				const two_d::transform& transform
			) override
			{
				m_frame.draw(source, bounds, tint, transform);
			}
			two_d::Frame2D& frame() noexcept override { return m_frame; }

			void display() override
			{
				m_frame.finish(m_command_buffer);
				m_command_buffer.submit();
			}

		private:
			gpu::command_buffer m_command_buffer;
			two_d::Frame2D m_frame;
		};
	}

	inline texture::texture(texture&& other)
	{
		if (other.rendering())
		{
			throw std::logic_error("Cannot move a texture while it is being rendered to");
		}
		m_gpu_tex = std::move(other.m_gpu_tex);
		m_view = std::move(other.m_view);
		m_hit_testing_enabled = std::exchange(other.m_hit_testing_enabled, false);
		m_hit_test_frame = std::move(other.m_hit_test_frame);
		other.m_view.reset();
		other.m_hit_test_frame.reset();
	}

	inline texture& texture::operator=(texture&& other)
	{
		if (this != &other)
		{
			if (rendering() || other.rendering())
			{
				throw std::logic_error("Cannot move-assign a texture while either texture is being rendered to");
			}
			m_gpu_tex = std::move(other.m_gpu_tex);
			m_view = std::move(other.m_view);
			m_hit_testing_enabled = std::exchange(other.m_hit_testing_enabled, false);
			m_hit_test_frame = std::move(other.m_hit_test_frame);
			other.m_view.reset();
			other.m_hit_test_frame.reset();
		}
		return *this;
	}

	inline void texture::clear(const color& clear_color)
	{
		if (!valid() || !can_render_to())
		{
			throw std::invalid_argument("Cannot render to an invalid or non-renderable texture");
		}
		if (m_gpu_tex.device_raw() != gpu::device::global().raw())
		{
			throw std::invalid_argument("Texture is not attached to the global render context");
		}
		if (rendering())
		{
			throw std::logic_error("Texture is already being rendered to");
		}
		m_render_session = std::make_unique<detail::texture_render_session_impl>(*this, clear_color);
	}

	inline void texture::draw(const sprite& value)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->draw(value);
	}

	inline void texture::draw(const two_d::circle& value)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->draw(value);
	}

	inline void texture::draw(const two_d::ellipse& value)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->draw(value);
	}

	inline void texture::draw(const two_d::rounded_rectangle& value)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->draw(value);
	}

	inline void texture::draw(const rectangle& bounds, const color& tint)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->draw(bounds, tint);
	}

	inline void texture::draw(
		const rectangle& bounds,
		const color& tint,
		const two_d::transform& object_transform
	)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->draw(bounds, tint, object_transform);
	}

	inline void texture::draw(const texture& source, const rectangle& bounds, const color& tint)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->draw(source, bounds, tint);
	}

	inline void texture::draw(
		const texture& source,
		const rectangle& bounds,
		const color& tint,
		const two_d::transform& object_transform
	)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->draw(source, bounds, tint, object_transform);
	}

	template<typename DrawFunction>
		requires std::invocable<DrawFunction, two_d::Frame2D&>
	inline void texture::draw(DrawFunction&& draw_function)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->frame().draw(std::forward<DrawFunction>(draw_function));
	}

	inline void texture::display()
	{
		if (!m_render_session)
		{
			throw std::logic_error("Cannot display a texture without an active rendering session");
		}

		try
		{
			m_render_session->display();
		}
		catch (...)
		{
			m_render_session.reset();
			m_hit_test_frame.reset();
			throw;
		}
		m_render_session.reset();
	}

}

namespace mngl::two_d
{
	inline Frame2D& Frame2D::operator=(Frame2D&& other) noexcept
	{
		if (this != &other)
		{
			cancel();
			m_control = std::move(other.m_control);
			m_token = std::exchange(other.m_token, 0);
			m_view_projection = std::move(other.m_view_projection);
			m_view = std::move(other.m_view);
			other.m_view_projection.reset();
			other.m_view.reset();
		}
		return *this;
	}

	inline Frame2D::~Frame2D() noexcept
	{
		cancel();
	}

	inline void Frame2D::draw(const ::mngl::sprite& value)
	{
		ensure_active();
		gpu::quad_transform gpu_transform{};
		gpu_transform.destination = value.destination;
		gpu_transform.pivot = value.pivot;
		gpu_transform.rotation_radians = value.rotation_radians;
		gpu_transform.object_transform = value.transform;
		if (value.source_texture)
		{
			m_control->owner->submit_texture(
				m_token,
				*value.source_texture,
				gpu_transform,
				value.tint,
				value.state,
				value.click_callback);
		}
		else
		{
			m_control->owner->submit_quad(
				m_token,
				value.source,
				gpu_transform,
				value.tint,
				value.state,
				::mngl::detail::hit_test_shape::quad,
				0.0f,
				value.click_callback);
		}
	}

	inline void Frame2D::draw(const circle& value)
	{
		ensure_active();
		m_control->owner->submit_circle(m_token, value);
	}

	inline void Frame2D::draw(const ellipse& value)
	{
		ensure_active();
		m_control->owner->submit_ellipse(m_token, value);
	}

	inline void Frame2D::draw(const rounded_rectangle& value)
	{
		ensure_active();
		m_control->owner->submit_rounded_rectangle(m_token, value);
	}

	inline void Frame2D::draw_text(
		font& typeface,
		std::string_view text,
		glm::vec2 top_left,
		const text_draw_options& options)
	{
		ensure_active();
		m_control->owner->submit_text(m_token, typeface, text, top_left, options);
	}

	inline void Frame2D::draw_text(
		font& typeface,
		const text_document& document,
		glm::vec2 top_left,
		const text_draw_options& options)
	{
		ensure_active();
		m_control->owner->submit_text_document(
			m_token,
			typeface,
			document,
			top_left,
			options);
	}

	inline void Frame2D::draw(const ::mngl::rectangle& bounds, const color& tint)
	{
		ensure_active();
		m_control->owner->submit_rectangle(m_token, bounds, tint);
	}

	inline void Frame2D::draw(
		const ::mngl::rectangle& bounds,
		const color& tint,
		const transform& object_transform
	)
	{
		ensure_active();
		m_control->owner->submit_rectangle(m_token, bounds, tint, object_transform);
	}

	inline void Frame2D::draw(
		const ::mngl::texture& source,
		const ::mngl::rectangle& bounds,
		const color& tint
	)
	{
		ensure_active();
		gpu::quad_transform transform{};
		transform.destination = bounds;
		m_control->owner->submit_texture(m_token, source, transform, tint, {});
	}

	inline void Frame2D::draw(
		const ::mngl::texture& source,
		const ::mngl::rectangle& bounds,
		const color& tint,
		const transform& object_transform
	)
	{
		ensure_active();
		gpu::quad_transform quad{};
		quad.destination = bounds;
		quad.matrix(object_transform.matrix());
		m_control->owner->submit_texture(m_token, source, quad, tint, {});
	}

	inline void Frame2D::draw_quad(
		const texture_region& source,
		const gpu::quad_transform& transform,
		const color& tint,
		const gpu::quad_draw_state& state
	)
	{
		ensure_active();
		m_control->owner->submit_quad(m_token, source, transform, tint, state);
	}

	inline void Frame2D::set_view_projection(const glm::mat4& matrix)
	{
		ensure_active();
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				if (!std::isfinite(matrix[column][row]))
				{
					throw std::invalid_argument("Frame2D view-projection matrix must be finite");
				}
			}
		}
		m_view_projection = matrix;
		m_view.reset();
	}

	inline void Frame2D::set_view(const ::mngl::view& value)
	{
		ensure_active();
		m_view = value;
		m_view_projection.reset();
	}

	inline void Frame2D::finish()
	{
		ensure_active();
		m_control->owner->finish_frame(*this);
	}

	inline void Frame2D::finish(gpu::command_buffer& command_buffer)
	{
		ensure_active();
		m_control->owner->finish_frame(*this, &command_buffer);
	}

	inline void Frame2D::ensure_active() const
	{
		if (!m_control || !m_control->owner || !m_control->owner->owns_frame(m_token))
		{
			throw std::logic_error("Frame2D is no longer active");
		}
	}

	inline void Frame2D::cancel() noexcept
	{
		if (m_control && m_control->owner)
		{
			m_control->owner->cancel_frame(m_token);
		}
		detach();
	}
}
