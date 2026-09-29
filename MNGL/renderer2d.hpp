#pragma once

#include "batcher2d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
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

			m_pages.reserve(16);
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
		std::vector<::mngl::texture> m_pages;
		texture_handle m_white_page{};
	};

	struct sprite
	{
		texture_region source{};
		gpu::quad_transform transform{};
		color tint = color(1.0f, 1.0f, 1.0f, 1.0f);
		gpu::quad_draw_state state{};
	};

	struct circle
	{
		glm::vec2 center{ 0.0f, 0.0f };
		float radius = 0.0f;
		color tint = color(1.0f, 1.0f, 1.0f, 1.0f);
		gpu::quad_draw_state state{};
	};

	struct ellipse
	{
		glm::vec2 center{ 0.0f, 0.0f };
		glm::vec2 radii{ 0.0f, 0.0f };
		color tint = color(1.0f, 1.0f, 1.0f, 1.0f);
		gpu::quad_draw_state state{};
	};

	struct rounded_rectangle
	{
		rectangle bounds{};
		float radius = 0.0f;
		color tint = color(1.0f, 1.0f, 1.0f, 1.0f);
		gpu::quad_draw_state state{};
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
		{ }

		Frame2D& operator=(Frame2D&& other) noexcept;
		~Frame2D() noexcept;

		void draw(const sprite& value);
		void draw(const circle& value);
		void draw(const ellipse& value);
		void draw(const rounded_rectangle& value);
		void draw_quad(
			const texture_region& source,
			const gpu::quad_transform& transform,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f),
			const gpu::quad_draw_state& state = {}
		);

		void set_view_projection(const glm::mat4& matrix);
		void finish();

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
		}

		std::shared_ptr<detail::renderer_frame_control> m_control;
		uint64_t m_token = 0;
		std::optional<glm::mat4> m_view_projection;
	};

	class renderer
	{
	public:
		// Borrows the device, window, and texture store; they must outlive the renderer.
		renderer(
			const gpu::device& device,
			window& target_window,
			texture_store& textures
		)
			: m_device(device)
			, m_window(target_window)
			, m_textures(textures)
			, m_batcher(device, required_swapchain_format(device, target_window))
			, m_frame_control(std::make_shared<detail::renderer_frame_control>())
		{
			if (!m_window.raw())
			{
				throw std::invalid_argument("Cannot create a 2D renderer with a null window");
			}
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
			if (m_frame_active)
			{
				throw std::logic_error("A 2D renderer frame is already active");
			}

			m_batcher.begin_frame(clear_color);
			if (++m_next_frame_token == 0)
			{
				++m_next_frame_token;
			}
			m_active_frame_token = m_next_frame_token;
			m_frame_active = true;
			return Frame2D(m_frame_control, m_active_frame_token);
		}

		[[nodiscard]] gpu::material_handle create_material(
			const gpu::graphics_shader_asset& fragment_shader,
			gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha
		)
		{
			return m_batcher.create_material(fragment_shader, blend);
		}

		[[nodiscard]] gpu::material_instance_handle create_material_instance(
			gpu::material_handle material,
			gpu::fragment_resource_bindings bindings
		)
		{
			return m_batcher.create_material_instance(material, std::move(bindings));
		}

		void update_material_instance(
			gpu::material_instance_handle instance,
			gpu::fragment_resource_bindings bindings
		)
		{
			m_batcher.update_material_instance(instance, std::move(bindings));
		}

		void destroy_material_instance(gpu::material_instance_handle instance)
		{
			m_batcher.destroy_material_instance(instance);
		}

		void destroy_material(gpu::material_handle material)
		{
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

		void submit_quad(
			uint64_t token,
			const texture_region& source,
			const gpu::quad_transform& transform,
			const color& tint,
			const gpu::quad_draw_state& state
		)
		{
			ensure_frame(token);
			const gpu::texture* page = nullptr;
			if (source.page.valid())
			{
				page = &m_textures.resolve(source.page).raw_gpu_texture();
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
			auto state = value.state;
			state.effect = gpu::quad_effect::circle;
			submit_quad(token, m_textures.white_region(), transform, value.tint, state);
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
			auto state = value.state;
			state.effect = gpu::quad_effect::ellipse;
			submit_quad(token, m_textures.white_region(), transform, value.tint, state);
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
			auto state = value.state;
			state.effect = gpu::quad_effect::rounded_rectangle;
			const float half_min_dimension =
				std::min(value.bounds.size.x, value.bounds.size.y) * 0.5f;
			state.effect_parameters.x = std::clamp(value.radius / half_min_dimension, 0.0f, 1.0f);
			submit_quad(token, m_textures.white_region(), transform, value.tint, state);
		}

		void finish_frame(Frame2D& frame)
		{
			ensure_frame(frame.m_token);
			try
			{
				gpu::command_buffer cmd(m_device);
				auto target = cmd.acquire_swapchain_texture(m_window);
				if (frame.m_view_projection)
				{
					m_batcher.flush(cmd, target, *frame.m_view_projection);
				}
				else
				{
					m_batcher.flush(cmd, target);
				}
				cmd.submit();
			}
			catch (...)
			{
				m_batcher.cancel_frame();
				m_frame_active = false;
				m_active_frame_token = 0;
				frame.detach();
				throw;
			}

			m_frame_active = false;
			m_active_frame_token = 0;
			frame.detach();
		}

		void cancel_frame(uint64_t token) noexcept
		{
			if (owns_frame(token))
			{
				m_batcher.cancel_frame();
				m_frame_active = false;
				m_active_frame_token = 0;
			}
		}

		void ensure_frame(uint64_t token) const
		{
			if (!owns_frame(token))
			{
				throw std::logic_error("Frame2D is no longer active");
			}
		}

		const gpu::device& m_device;
		window& m_window;
		texture_store& m_textures;
		gpu::batcher2d m_batcher;
		std::shared_ptr<detail::renderer_frame_control> m_frame_control;
		uint64_t m_next_frame_token = 0;
		uint64_t m_active_frame_token = 0;
		bool m_frame_active = false;
	};

	inline Frame2D& Frame2D::operator=(Frame2D&& other) noexcept
	{
		if (this != &other)
		{
			cancel();
			m_control = std::move(other.m_control);
			m_token = std::exchange(other.m_token, 0);
			m_view_projection = std::move(other.m_view_projection);
		}
		return *this;
	}

	inline Frame2D::~Frame2D() noexcept
	{
		cancel();
	}

	inline void Frame2D::draw(const sprite& value)
	{
		ensure_active();
		m_control->owner->submit_quad(
			m_token,
			value.source,
			value.transform,
			value.tint,
			value.state);
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
	}

	inline void Frame2D::finish()
	{
		ensure_active();
		m_control->owner->finish_frame(*this);
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
