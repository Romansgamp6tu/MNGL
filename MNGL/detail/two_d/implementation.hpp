#pragma once

#include "renderer.hpp"

namespace mngl
{
	namespace detail
	{
		class texture_render_session_impl final : public texture_render_session
		{
		public:
			texture_render_session_impl(::mngl::texture& target, const color& clear_color)
			    : m_device(two_d::render_context::global().gpu_device()),
			      m_started_frame(two_d::render_context::global().begin_frame(target, clear_color))
			{
			}

			two_d::frame_2d& frame() noexcept override { return m_started_frame.frame; }

			void display() override
			{
				m_started_frame.owner->begin_command_submission();
				try
				{
					// Font atlases are shared by renderers; keep their uploads ordered through submit.
					auto font_uploads = m_started_frame.owner->lock_frame_fonts(m_started_frame.frame);
					gpu::command_buffer command_buffer(m_device);
					try
					{
						m_started_frame.frame.finish(command_buffer);
					}
					catch (...)
					{
						command_buffer.cancel();
						throw;
					}
					try
					{
						command_buffer.submit();
					}
					catch (...)
					{
						m_started_frame.owner->mark_frame_fonts_dirty(font_uploads);
						throw;
					}
				}
				catch (...)
				{
					m_started_frame.owner->end_command_submission();
					throw;
				}
				m_started_frame.owner->end_command_submission();
			}

		private:
			gpu::device m_device;
			two_d::render_context::frame_start m_started_frame;
		};
	} // namespace detail

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
				throw std::logic_error(
				    "Cannot move-assign a texture while either texture is being rendered to");
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

	inline void texture::draw(const drawable& value)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->frame().draw(value);
	}

	inline void texture::prepare(const drawable& value)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before preparing resources for a texture frame");
		}
		m_render_session->frame().prepare(value);
	}

	inline two_d::recording_context texture::create_recording_context(std::size_t expected_draw_count)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before creating a texture recording context");
		}
		return m_render_session->frame().create_recording_context(expected_draw_count);
	}

	inline void texture::merge(two_d::recording_context&& context)
	{
		if (!m_render_session)
		{
			throw std::logic_error("Call clear() before merging a texture recording context");
		}
		m_render_session->frame().merge(std::move(context));
	}

	inline two_d::recording_context texture::frame_scope::create_recording_context(
	    std::size_t expected_draw_count)
	{
		ensure_active();
		return m_owner->create_recording_context(expected_draw_count);
	}

	inline void texture::frame_scope::merge(two_d::recording_context&& context)
	{
		ensure_active();
		m_owner->merge(std::move(context));
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
		if (++m_next_frame_generation == 0)
		{
			++m_next_frame_generation;
		}
		m_active_frame_generation = m_next_frame_generation;
	}

	template <typename DrawFunction>
	    requires std::invocable<DrawFunction, two_d::frame_2d&>
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
			if (!m_render_session || !m_render_session->frame().active())
			{
				reset_active_frame();
				m_hit_test_frame.reset();
			}
			throw;
		}
		reset_active_frame();
	}

} // namespace mngl

namespace mngl::two_d
{
	inline frame_2d& frame_2d::operator=(frame_2d&& other) noexcept
	{
		if (this != &other)
		{
			cancel();
			m_control = std::move(other.m_control);
			m_token = std::exchange(other.m_token, 0);
			m_recording_state = std::move(other.m_recording_state);
			m_recording_context = std::exchange(other.m_recording_context, nullptr);
			m_view_projection = std::move(other.m_view_projection);
			m_view = std::move(other.m_view);
			other.m_view_projection.reset();
			other.m_view.reset();
		}
		return *this;
	}

	inline frame_2d::~frame_2d() noexcept
	{
		cancel();
	}

	inline void frame_2d::draw(const ::mngl::drawable& value)
	{
		ensure_active();
		ensure_recording_allowed();
		value.draw(*this);
	}

	inline void frame_2d::draw_text(font& typeface, std::string_view text, glm::vec2 top_left,
	                                const text_draw_options& options)
	{
		ensure_active();
		ensure_recording_allowed();
		m_control->owner->submit_text(m_token, m_recording_context, typeface, text, top_left, options);
	}

	inline void recording_context::draw(const ::mngl::drawable& value)
	{
		ensure_active();
		auto frame = frame_view();
		frame.draw(value);
	}

	inline void recording_context::finish()
	{
		ensure_active();
		m_batcher_context.finish();
		m_finished = true;
	}

	inline void recording_context::ensure_active() const
	{
		if (!valid() || !m_control->owner || !m_control->owner->owns_frame(m_token) || m_finished)
		{
			throw std::logic_error("Recording context is no longer active");
		}
	}

	inline frame_2d recording_context::frame_view()
	{
		frame_2d frame(m_control, m_token, this);
		frame.m_recording_state = m_recording_state;
		frame.m_view_projection = m_view_projection;
		frame.m_view = m_view;
		return frame;
	}

	inline void recording_context::register_with_frame()
	{
		if (!m_recording_state || !m_control || !m_control->owner || !m_control->owner->owns_frame(m_token))
		{
			throw std::logic_error("Cannot register a recording context for an inactive frame");
		}

		auto current = m_recording_state->outstanding_recording_contexts.load(std::memory_order_relaxed);
		for (;;)
		{
			if (current == std::numeric_limits<std::uint32_t>::max())
			{
				throw std::length_error("2D frame exhausted its recording-context count");
			}
			if (m_recording_state->outstanding_recording_contexts.compare_exchange_weak(
			        current, current + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
			{
				m_registered = true;
				return;
			}
		}
	}

	inline void recording_context::release_registration() noexcept
	{
		if (m_registered && m_recording_state)
		{
			m_recording_state->outstanding_recording_contexts.fetch_sub(1, std::memory_order_acq_rel);
		}
		m_registered = false;
	}

	inline void recording_context::detach_after_merge() noexcept
	{
		release_registration();
		m_control.reset();
		m_recording_state.reset();
		m_token = 0;
		m_hit_test_items.clear();
		m_used_fonts.clear();
		m_view_projection.reset();
		m_view.reset();
	}
} // namespace mngl::two_d

namespace mngl
{
	inline void sprite::draw(two_d::frame_2d& frame) const
	{
		frame.submit_sprite(*this);
	}

	inline void sprite::prepare(two_d::frame_2d& frame) const
	{
		if (m_fragment_shader)
		{
			frame.prepare_shader_sprite(*this);
		}
	}
} // namespace mngl

namespace mngl::two_d
{
	inline void circle::draw(frame_2d& frame) const
	{
		frame.submit_circle(*this);
	}

	inline void ellipse::draw(frame_2d& frame) const
	{
		frame.submit_ellipse(*this);
	}

	inline void rounded_rectangle::draw(frame_2d& frame) const
	{
		frame.submit_rounded_rectangle(*this);
	}

	inline void rectangle_shape::draw(frame_2d& frame) const
	{
		frame.submit_rectangle_shape(*this);
	}

	inline void vertex_array::draw(frame_2d& frame) const
	{
		frame.submit_vertex_array(*this);
	}

	inline void convex_polygon::draw(frame_2d& frame) const
	{
		frame.submit_convex_polygon(*this);
	}

	inline void text::draw(frame_2d& frame) const
	{
		frame.submit_text(*this);
	}

	inline void text::prepare(frame_2d& frame) const
	{
		if (m_document)
		{
			for (std::size_t line = 0; line < m_document->line_count(); ++line)
			{
				frame.prepare_font(*m_typeface, m_document->line(line));
			}
		}
		else
		{
			frame.prepare_font(*m_typeface, m_text);
		}
	}

	inline void frame_2d::submit_sprite(const ::mngl::sprite& value)
	{
		ensure_active();
		if (value.m_fragment_shader)
		{
			m_control->owner->submit_shader_sprite(m_token, m_recording_context, value,
			                                       *value.m_fragment_shader);
			return;
		}

		gpu::quad_transform gpu_transform{};
		gpu_transform.destination = value.destination;
		gpu_transform.pivot = value.pivot;
		gpu_transform.rotation_radians = value.rotation_radians;
		gpu_transform.object_transform = value.transform;
		if (value.source_texture)
		{
			m_control->owner->submit_texture(m_token, m_recording_context, *value.source_texture,
			                                 gpu_transform, value.tint, value.state, value.click_callback);
		}
		else
		{
			m_control->owner->submit_quad(m_token, m_recording_context, value.source, gpu_transform,
			                              value.tint, value.state, ::mngl::detail::hit_test_shape::quad, 0.0f,
			                              value.click_callback);
		}
	}

	inline void frame_2d::submit_circle(const circle& value)
	{
		ensure_active();
		m_control->owner->submit_circle(m_token, m_recording_context, value);
	}

	inline void frame_2d::submit_ellipse(const ellipse& value)
	{
		ensure_active();
		m_control->owner->submit_ellipse(m_token, m_recording_context, value);
	}

	inline void frame_2d::submit_rounded_rectangle(const rounded_rectangle& value)
	{
		ensure_active();
		m_control->owner->submit_rounded_rectangle(m_token, m_recording_context, value);
	}

	inline void frame_2d::submit_rectangle_shape(const rectangle_shape& value)
	{
		ensure_active();
		m_control->owner->submit_rectangle(m_token, m_recording_context, value.bounds, value.tint,
		                                   value.transform);
	}

	inline void frame_2d::submit_vertex_array(const vertex_array& value)
	{
		ensure_active();
		m_control->owner->submit_vertex_array(m_token, m_recording_context, value);
	}

	inline void frame_2d::submit_convex_polygon(const convex_polygon& value)
	{
		ensure_active();
		m_control->owner->submit_convex_polygon(m_token, m_recording_context, value);
	}

	inline void frame_2d::submit_text(const text& value)
	{
		ensure_active();
		if (value.m_document)
		{
			m_control->owner->submit_text_document(m_token, m_recording_context, *value.m_typeface,
			                                       *value.m_document, value.m_top_left, value.m_options);
		}
		else
		{
			m_control->owner->submit_text(m_token, m_recording_context, *value.m_typeface, value.m_text,
			                              value.m_top_left, value.m_options);
		}
	}

	inline void frame_2d::prepare(const ::mngl::drawable& value)
	{
		ensure_active();
		if (m_recording_context)
		{
			throw std::logic_error("Resources can only be prepared on the parent frame_2d");
		}
		m_control->owner->ensure_preparation_allowed(m_token);
		value.prepare(*this);
	}

	inline void frame_2d::prepare(font& typeface, std::string_view text)
	{
		prepare_font(typeface, text);
	}

	inline recording_context frame_2d::create_recording_context(std::size_t expected_draw_count)
	{
		ensure_active();
		if (m_recording_context)
		{
			throw std::logic_error("Worker recording contexts can only be created from the parent frame_2d");
		}
		return m_control->owner->create_recording_context(*this, expected_draw_count);
	}

	inline void frame_2d::merge(recording_context&& context)
	{
		ensure_active();
		if (m_recording_context)
		{
			throw std::logic_error("Worker recording contexts can only be merged into the parent frame_2d");
		}
		m_control->owner->merge_recording_context(*this, context);
	}

	inline void frame_2d::prepare_shader_sprite(const ::mngl::sprite& value)
	{
		ensure_active();
		if (m_recording_context)
		{
			throw std::logic_error("Shader sprites must be prepared on the parent frame_2d");
		}
		if (value.m_fragment_shader)
		{
			m_control->owner->prepare_shader_sprite(m_token, value, *value.m_fragment_shader);
		}
	}

	inline void frame_2d::prepare_font(font& typeface, std::string_view text)
	{
		ensure_active();
		if (m_recording_context)
		{
			throw std::logic_error("Fonts must be prepared on the parent frame_2d");
		}
		m_control->owner->prepare_font(m_token, typeface, text);
	}

	inline void frame_2d::set_view_projection(const glm::mat4& matrix)
	{
		ensure_active();
		if (m_recording_context)
		{
			throw std::logic_error("Worker recording contexts cannot change the frame projection");
		}
		m_control->owner->ensure_preparation_allowed(m_token);
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				if (!std::isfinite(matrix[column][row]))
				{
					throw std::invalid_argument("frame_2d view-projection matrix must be finite");
				}
			}
		}
		m_view_projection = matrix;
		m_view.reset();
	}

	inline void frame_2d::set_view(const ::mngl::view& value)
	{
		ensure_active();
		if (m_recording_context)
		{
			throw std::logic_error("Worker recording contexts cannot change the frame view");
		}
		m_control->owner->ensure_preparation_allowed(m_token);
		m_view = value;
		m_view_projection.reset();
	}

	inline void frame_2d::finish()
	{
		ensure_active();
		if (m_recording_context)
		{
			throw std::logic_error("A worker recording context cannot finish its parent frame");
		}
		m_control->owner->finish_frame(*this);
	}

	inline void frame_2d::finish(gpu::command_buffer& command_buffer)
	{
		ensure_active();
		if (m_recording_context)
		{
			throw std::logic_error("A worker recording context cannot finish its parent frame");
		}
		m_control->owner->finish_frame(*this, &command_buffer);
	}

	inline bool frame_2d::active() const noexcept
	{
		if (!m_control || !m_control->owner || !m_control->owner->owns_frame(m_token))
		{
			return false;
		}
		return !m_recording_context || (m_recording_context->valid() && !m_recording_context->finished());
	}

	inline void frame_2d::ensure_active() const
	{
		if (!active())
		{
			throw std::logic_error("frame_2d is no longer active");
		}
	}

	inline void frame_2d::ensure_recording_allowed() const
	{
		m_control->owner->ensure_frame(m_token, m_recording_context);
	}

	inline void frame_2d::cancel() noexcept
	{
		if (!m_recording_context && m_control && m_control->owner)
		{
			m_control->owner->cancel_frame(m_token);
		}
		detach();
	}
} // namespace mngl::two_d

// Compatibility names; new code uses snake_case.
