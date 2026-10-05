#pragma once

#include "renderer2d.hpp"
#include "view.hpp"
#include "window.hpp"

#include <optional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace mngl
{
	class render_window : public window
	{
	public:
		render_window(
			const std::string& title,
			glm::uvec2 size,
			window::flags flags = window::flags()
		)
			: window(title, size, flags)
			, m_context(two_d::render_context::global())
		{
			m_context.gpu_device().claim_window(*this);
		}

		render_window(const render_window&) = delete;
		render_window& operator=(const render_window&) = delete;
		render_window(render_window&&) = delete;
		render_window& operator=(render_window&&) = delete;

		~render_window()
		{
			m_frame.reset();
			release_active_texture_target();
			m_swapchain_texture.reset();
			m_command_buffer.reset();
			m_context.gpu_device().release_window(*this);
		}

		void clear(const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f))
		{
			begin_target_frame(nullptr, clear_color);
		}

		void set_view(const ::mngl::view& value)
		{
			if (m_frame || m_command_buffer)
			{
				throw std::logic_error("Cannot change the render_window view during an active frame");
			}
			m_view = value;
		}

		void reset_view()
		{
			if (m_frame || m_command_buffer)
			{
				throw std::logic_error("Cannot reset the render_window view during an active frame");
			}
			m_view.reset();
		}

		[[nodiscard]] std::optional<::mngl::view> get_view() const
		{
			return m_view;
		}

		// The target must remain alive until display() finishes this frame.
		void clear(
			::mngl::texture& target,
			const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f)
		)
		{
			begin_target_frame(&target, clear_color);
		}

		void draw(const sprite& value)
		{
			active_frame().draw(value);
		}

		void draw(const two_d::circle& value)
		{
			active_frame().draw(value);
		}

		void draw(const two_d::ellipse& value)
		{
			active_frame().draw(value);
		}

		void draw(const two_d::rounded_rectangle& value)
		{
			active_frame().draw(value);
		}

		void draw(
			const rectangle& bounds,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f)
		)
		{
			active_frame().draw(bounds, tint);
		}

		// The source is borrowed until display() submits the window frame.
		void draw(
			const ::mngl::texture& source,
			const rectangle& bounds,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f)
		)
		{
			active_frame().draw(source, bounds, tint);
		}

		void draw_quad(
			const two_d::texture_region& source,
			const gpu::quad_transform& transform,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f),
			const gpu::quad_draw_state& state = {}
		)
		{
			active_frame().draw_quad(source, transform, tint, state);
		}

		void display()
		{
			if (!m_frame || !m_command_buffer)
			{
				throw std::logic_error("Cannot display a render_window without an active frame");
			}

			try
			{
				m_frame->finish(*m_command_buffer);
				m_command_buffer->submit();
			}
			catch (...)
			{
				m_frame.reset();
				release_active_texture_target();
				m_swapchain_texture.reset();
				m_command_buffer.reset();
				throw;
			}
			m_frame.reset();
			release_active_texture_target();
			m_swapchain_texture.reset();
			m_command_buffer.reset();
		}

		// The returned texture is borrowed and remains valid only until display().
		[[nodiscard]] const ::mngl::texture& swapchain_texture() const
		{
			if (!m_frame || !m_swapchain_texture)
			{
				throw std::logic_error("The swapchain texture is only available during an active window frame");
			}
			return *m_swapchain_texture;
		}

		[[nodiscard]] const gpu::device& gpu_device() const noexcept
		{
			return m_context.gpu_device();
		}

		// Without a custom view, coordinates start at the bottom-left and the shorter target side is 1.
		[[nodiscard]] glm::vec2 coordinate_size() const
		{
			if (m_active_texture_target)
			{
				return m_active_texture_target->coordinate_size();
			}
			if (m_swapchain_texture)
			{
				if (m_view)
				{
					return m_view->visible_area(m_swapchain_texture->size()).size;
				}
				return m_swapchain_texture->coordinate_size();
			}

			const auto dimensions = pixel_size();
			const float short_side = static_cast<float>(
				dimensions.x < dimensions.y ? dimensions.x : dimensions.y);
			if (short_side == 0.0f)
			{
				return {};
			}
			if (m_view)
			{
				return m_view->visible_area(dimensions).size;
			}
			return {
				static_cast<float>(dimensions.x) / short_side,
				static_cast<float>(dimensions.y) / short_side
			};
		}

		[[nodiscard]] two_d::texture_store& textures() noexcept
		{
			return m_context.textures();
		}

		[[nodiscard]] two_d::renderer& renderer()
		{
			return renderer_for(swapchain_format(), true);
		}

		[[nodiscard]] ::mngl::texture create_texture(
			glm::uvec2 size,
			gpu::texture_format format = gpu::texture_format::invalid,
			uint32_t usage = ::mngl::texture::usage::sampler | ::mngl::texture::usage::color_target
		) const
		{
			if (format == gpu::texture_format::invalid)
			{
				format = swapchain_format();
			}
			return ::mngl::texture(gpu_device(), size.x, size.y, format, usage);
		}

	private:
		[[nodiscard]] gpu::texture_format swapchain_format() const
		{
			const auto format = gpu_device().swapchain_texture_format(*this);
			if (format == gpu::texture_format::invalid)
			{
				throw std::runtime_error("The render_window must be claimed by its GPU device");
			}
			return format;
		}

		void begin_target_frame(::mngl::texture* target, const color& clear_color)
		{
			if (m_frame || m_command_buffer)
			{
				throw std::logic_error("A render_window frame is already active");
			}

			// Target textures are borrowed through display().
			two_d::renderer* active_renderer = target
				? &renderer_for(*target)
				: &renderer_for(swapchain_format(), true);
			if (target)
			{
				target->begin_external_render();
				m_active_texture_target = target;
			}
			try
			{
				m_command_buffer.emplace(gpu_device());
				if (target)
				{
					m_frame.emplace(active_renderer->begin_frame(*target, clear_color));
				}
				else
				{
					auto swapchain_target = ::mngl::texture(
						m_command_buffer->acquire_swapchain_texture(*this));
					m_swapchain_texture.emplace(std::move(swapchain_target));
					m_frame.emplace(active_renderer->begin_frame(*m_swapchain_texture, clear_color));
					if (m_view)
					{
						m_frame->set_view(*m_view);
					}
				}
			}
			catch (...)
			{
				m_frame.reset();
				release_active_texture_target();
				m_swapchain_texture.reset();
				m_command_buffer.reset();
				throw;
			}
		}

		[[nodiscard]] two_d::renderer& renderer_for(::mngl::texture& target)
		{
			if (!target.valid() || !target.can_render_to() || target.rendering())
			{
				throw std::invalid_argument("render_window target must be a valid, idle color-target texture");
			}
			if (target.raw_gpu_texture().device_raw() != gpu_device().raw())
			{
				throw std::invalid_argument("render_window target belongs to a different GPU device");
			}
			return renderer_for(target.format());
		}

		[[nodiscard]] two_d::renderer& renderer_for(
			gpu::texture_format format,
			bool allow_custom_materials = false
		)
		{
			auto& target_renderer = m_renderers[format];
			if (!target_renderer)
			{
				target_renderer = std::make_unique<two_d::renderer>(
					gpu_device(),
					m_context.textures(),
					format);
			}
			if (allow_custom_materials)
			{
				target_renderer->enable_custom_materials();
			}
			return *target_renderer;
		}

		void release_active_texture_target() noexcept
		{
			if (m_active_texture_target)
			{
				m_active_texture_target->end_external_render();
				m_active_texture_target = nullptr;
			}
		}

		[[nodiscard]] two_d::Frame2D& active_frame()
		{
			if (!m_frame)
			{
				throw std::logic_error("Call clear() before drawing to a render_window");
			}
			return *m_frame;
		}

		two_d::render_context& m_context;
		std::unordered_map<gpu::texture_format, std::unique_ptr<two_d::renderer>> m_renderers;
		std::optional<::mngl::view> m_view;
		::mngl::texture* m_active_texture_target = nullptr;
		std::optional<gpu::command_buffer> m_command_buffer;
		std::optional<::mngl::texture> m_swapchain_texture;
		std::optional<two_d::Frame2D> m_frame;
	};
}
