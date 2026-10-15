#pragma once

#include "renderer2d.hpp"
#include "view.hpp"
#include "window.hpp"

#include <cmath>
#include <concepts>
#include <optional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace mngl
{
	namespace detail
	{
		class render_window_overlay
		{
		public:
			virtual ~render_window_overlay() = default;
			virtual void render(gpu::command_buffer& command_buffer, ::mngl::texture& target) = 0;
			virtual void detach_from_window() noexcept = 0;
		};
	}

	class imgui_context;

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
			if (m_overlay)
			{
				auto* overlay = std::exchange(m_overlay, nullptr);
				overlay->detach_from_window();
			}
			m_context.gpu_device().release_window(*this);
		}

		void clear(const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f))
		{
			if (!try_clear(clear_color))
			{
				throw std::runtime_error(
					"No swapchain texture is available; the window may be minimized or occluded");
			}
		}

		// Returns false when SDL cannot provide a swapchain texture for this frame.
		[[nodiscard]] bool try_clear(
			const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f))
		{
			return begin_target_frame(nullptr, clear_color);
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

		void enable_hit_testing(bool enabled = true)
		{
			if (m_frame || m_command_buffer)
			{
				throw std::logic_error("Cannot change hit testing during an active frame");
			}
			m_hit_testing_enabled = enabled;
			if (!enabled)
			{
				m_hit_test_frame.reset();
			}
		}

		[[nodiscard]] bool hit_testing_enabled() const noexcept
		{
			return m_hit_testing_enabled;
		}

		// The position is in logical window coordinates, with the origin at the top-left.
		[[nodiscard]] std::optional<hit_test_result> hit_test(glm::vec2 window_position) const
		{
			if (!m_hit_testing_enabled)
			{
				throw std::logic_error("Enable hit testing before drawing to the render window");
			}
			if (!m_hit_test_frame)
			{
				throw std::logic_error("No completed hit-test frame is available for this window");
			}
			if (!std::isfinite(window_position.x) || !std::isfinite(window_position.y))
			{
				return std::nullopt;
			}

			const auto logical_size = size();
			if (logical_size.x == 0 || logical_size.y == 0 ||
				window_position.x < 0.0f || window_position.y < 0.0f ||
				window_position.x > static_cast<float>(logical_size.x) ||
				window_position.y > static_cast<float>(logical_size.y))
			{
				return std::nullopt;
			}

			const glm::vec2 pixel_position{
				window_position.x * static_cast<float>(m_hit_test_frame->target_size.x) /
					static_cast<float>(logical_size.x),
				(static_cast<float>(logical_size.y) - window_position.y) *
					static_cast<float>(m_hit_test_frame->target_size.y) /
					static_cast<float>(logical_size.y)
			};
			return detail::hit_test_frame_at_pixel(*m_hit_test_frame, pixel_position);
		}

		// Dispatches to the topmost hit object or its nearest clickable ancestor.
		[[nodiscard]] bool dispatch_click(glm::vec2 window_position) const
		{
			if (auto hit = hit_test(window_position))
			{
				return hit->invoke_click_callback();
			}
			return false;
		}

		// The target must remain alive until display() finishes this frame.
		void clear(
			::mngl::texture& target,
			const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f)
		)
		{
			(void)begin_target_frame(&target, clear_color);
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
		void draw(
			const rectangle& bounds,
			const color& tint,
			const two_d::transform& object_transform
		)
		{
			active_frame().draw(bounds, tint, object_transform);
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
		void draw(
			const ::mngl::texture& source,
			const rectangle& bounds,
			const color& tint,
			const two_d::transform& object_transform
		)
		{
			active_frame().draw(source, bounds, tint, object_transform);
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

		template<typename DrawFunction>
			requires std::invocable<DrawFunction, two_d::Frame2D&>
		void draw(DrawFunction&& draw_function)
		{
			active_frame().draw(std::forward<DrawFunction>(draw_function));
		}

		void display()
		{
			if (!display_frame())
			{
				throw std::runtime_error(
					"Window render target became unavailable before presentation");
			}
		}

		// Returns false if the window target became unavailable before presentation.
		[[nodiscard]] bool try_display()
		{
			return display_frame();
		}

	private:
		bool display_frame()
		{
			if (!m_frame || !m_command_buffer)
			{
				throw std::logic_error("Cannot display a render_window without an active frame");
			}
			if ((m_swapchain_texture &&
					!swapchain_texture_is_usable(*m_swapchain_texture)) ||
				(m_active_texture_target &&
					!offscreen_target_is_usable(*m_active_texture_target)))
			{
				discard_active_frame();
				return false;
			}

			try
			{
				m_frame->finish(*m_command_buffer);
				if (m_overlay)
				{
					if (m_active_texture_target)
					{
						m_overlay->render(*m_command_buffer, *m_active_texture_target);
					}
					else if (m_swapchain_texture)
					{
						m_overlay->render(*m_command_buffer, *m_swapchain_texture);
					}
				}
				m_command_buffer->submit();
				if (m_swapchain_texture)
				{
					m_hit_test_frame = m_hit_testing_enabled
						? m_swapchain_texture->m_hit_test_frame
						: nullptr;
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
			m_frame.reset();
			release_active_texture_target();
			m_swapchain_texture.reset();
			m_command_buffer.reset();
			return true;
		}

	public:
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
		friend class imgui_context;

		void attach_overlay(detail::render_window_overlay& overlay)
		{
			if (m_overlay)
			{
				throw std::logic_error("A render_window can only have one active overlay");
			}
			m_overlay = &overlay;
		}

		void detach_overlay(detail::render_window_overlay& overlay) noexcept
		{
			if (m_overlay == &overlay)
			{
				m_overlay = nullptr;
			}
		}

		[[nodiscard]] gpu::texture_format swapchain_format() const
		{
			const auto format = gpu_device().swapchain_texture_format(*this);
			if (format == gpu::texture_format::invalid)
			{
				throw std::runtime_error("The render_window must be claimed by its GPU device");
			}
			return format;
		}

		bool begin_target_frame(
			::mngl::texture* target,
			const color& clear_color)
		{
			if (m_frame || m_command_buffer)
			{
				throw std::logic_error("A render_window frame is already active");
			}

			// Target textures are borrowed through display().
			gpu::texture_format acquired_swapchain_format =
				gpu::texture_format::invalid;
			two_d::renderer* active_renderer = nullptr;
			if (target)
			{
				active_renderer = &renderer_for(*target);
			}
			else
			{
				if (!window_is_drawable())
				{
					return false;
				}
				acquired_swapchain_format =
					gpu_device().swapchain_texture_format(*this);
				if (acquired_swapchain_format ==
					gpu::texture_format::invalid)
				{
					return false;
				}
				active_renderer = &renderer_for(
					acquired_swapchain_format,
					true);
			}
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
					if (!swapchain_target.raw_gpu_texture().raw())
					{
						m_command_buffer->cancel();
						m_command_buffer.reset();
						m_hit_test_frame.reset();
						return false;
					}
					if (!swapchain_texture_is_usable(
						swapchain_target,
						acquired_swapchain_format))
					{
						m_command_buffer->submit();
						m_command_buffer.reset();
						m_hit_test_frame.reset();
						return false;
					}
					m_swapchain_texture.emplace(std::move(swapchain_target));
					m_swapchain_texture->enable_hit_testing(m_hit_testing_enabled);
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
			return true;
		}

		[[nodiscard]] bool window_is_drawable() const
		{
			constexpr SDL_WindowFlags unavailable_flags =
				SDL_WINDOW_HIDDEN |
				SDL_WINDOW_MINIMIZED |
				SDL_WINDOW_OCCLUDED;
			if ((SDL_GetWindowFlags(raw()) & unavailable_flags) != 0)
			{
				return false;
			}
			const auto dimensions = pixel_size();
			return dimensions.x != 0 && dimensions.y != 0;
		}

		[[nodiscard]] bool swapchain_texture_is_usable(
			const ::mngl::texture& target) const
		{
			return swapchain_texture_is_usable(
				target,
				gpu_device().swapchain_texture_format(*this));
		}

		[[nodiscard]] bool swapchain_texture_is_usable(
			const ::mngl::texture& target,
			gpu::texture_format expected_format) const
		{
			if (!window_is_drawable() ||
				expected_format == gpu::texture_format::invalid ||
				gpu_device().swapchain_texture_format(*this) !=
					expected_format ||
				!target.valid() ||
				!target.can_render_to() ||
				target.format() != expected_format ||
				target.raw_gpu_texture().device_raw() != gpu_device().raw())
			{
				return false;
			}
			const auto window_size = pixel_size();
			const auto target_size = target.size();
			return target_size.x == window_size.x &&
				target_size.y == window_size.y;
		}

		[[nodiscard]] bool offscreen_target_is_usable(
			const ::mngl::texture& target) const noexcept
		{
			return target.valid() &&
				target.can_render_to() &&
				target.raw_gpu_texture().device_raw() == gpu_device().raw();
		}

		void discard_active_frame()
		{
			m_frame.reset();
			release_active_texture_target();
			try
			{
				if (m_command_buffer)
				{
					if (m_swapchain_texture)
					{
						m_command_buffer->submit();
					}
					else
					{
						m_command_buffer->cancel();
					}
				}
			}
			catch (...)
			{
				m_swapchain_texture.reset();
				m_command_buffer.reset();
				m_hit_test_frame.reset();
				throw;
			}
			m_swapchain_texture.reset();
			m_command_buffer.reset();
			m_hit_test_frame.reset();
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
		bool m_hit_testing_enabled = false;
		std::shared_ptr<const detail::hit_test_frame> m_hit_test_frame;
		std::optional<gpu::command_buffer> m_command_buffer;
		std::optional<::mngl::texture> m_swapchain_texture;
		std::optional<two_d::Frame2D> m_frame;
		detail::render_window_overlay* m_overlay = nullptr;
	};
}
