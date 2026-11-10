#pragma once

#include "renderer.hpp"

#include <unordered_set>

namespace mngl
{
	namespace detail
	{
		class texture_render_session_impl final : public texture_render_session
		{
		public:
		texture_render_session_impl(::mngl::texture& target, const color& clear_color,
		                            bool preserve_target_contents)
			    : m_device(two_d::render_context::global().gpu_device()),
			      m_started_frame(
			          two_d::render_context::global().begin_frame(target, clear_color, false,
			                                                     preserve_target_contents))
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
		if (other.rendering()) [[unlikely]]
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
			if (rendering() || other.rendering()) [[unlikely]]
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
		if (!m_render_session) [[unlikely]]
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->frame().draw(value);
	}

	inline void texture::prepare(const drawable& value)
	{
		if (!m_render_session) [[unlikely]]
		{
			throw std::logic_error("Call clear() before preparing resources for a texture frame");
		}
		m_render_session->frame().prepare(value);
	}

	inline two_d::recording_context texture::create_recording_context(std::size_t expected_draw_count)
	{
		if (!m_render_session) [[unlikely]]
		{
			throw std::logic_error("Call clear() before creating a texture recording context");
		}
		return m_render_session->frame().create_recording_context(expected_draw_count);
	}

	inline void texture::merge(two_d::recording_context&& context)
	{
		if (!m_render_session) [[unlikely]]
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
		begin_render(clear_color, false);
	}

	inline void texture::begin_render(const color& clear_color, bool preserve_contents)
	{
		if (!valid() || !can_render_to()) [[unlikely]]
		{
			throw std::invalid_argument("Cannot render to an invalid or non-renderable texture");
		}
		if (m_gpu_tex.device_raw() != gpu::device::global().raw()) [[unlikely]]
		{
			throw std::invalid_argument("Texture is not attached to the global render context");
		}
		if (rendering()) [[unlikely]]
		{
			throw std::logic_error("Texture is already being rendered to");
		}
		m_render_session = std::make_unique<detail::texture_render_session_impl>(
		    *this, clear_color, preserve_contents);
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
		if (!m_render_session) [[unlikely]]
		{
			throw std::logic_error("Call clear() before drawing to a texture");
		}
		m_render_session->frame().draw(std::forward<DrawFunction>(draw_function));
	}

	inline void texture::display()
	{
		if (!m_render_session) [[unlikely]]
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
		ensure_recording_allowed();
		value.draw(*this);
	}

	inline void frame_2d::draw_text(font& typeface, std::string_view text, glm::vec2 top_left,
	                                const text_draw_options& options)
	{
		ensure_recording_allowed();
		m_control->owner->submit_text(m_recording_context, typeface, text, top_left, options);
	}

	inline void recording_context::draw(const ::mngl::drawable& value)
	{
		auto frame = frame_view();
		frame.draw(value);
	}

	template <typename DrawFunction>
	    requires std::invocable<DrawFunction, frame_2d&>
	inline void frame_2d::draw_order_independent(DrawFunction&& draw_function)
	{
		ensure_active();
		if (m_recording_context) [[unlikely]]
		{
			throw std::logic_error("Order-independent draw groups can only be created on the parent frame");
		}

		auto context = create_recording_context();
		{
			auto group = context.frame_view();
			std::invoke(std::forward<DrawFunction>(draw_function), group);
		}
		if (!context.finished())
		{
			context.finish();
		}
		context.m_order_independent = true;
		merge(std::move(context));
	}

	template <typename DrawFunction>
	    requires std::invocable<DrawFunction, frame_2d&>
	inline bool frame_2d::draw_if_visible(const ::mngl::rectangle& bounds, DrawFunction&& draw_function)
	{
		ensure_recording_allowed();
		if (scene_culling_enabled() && !m_control->owner->bounds_visible(*this, bounds))
		{
			return false;
		}

		std::invoke(std::forward<DrawFunction>(draw_function), *this);
		return true;
	}

	template <typename DrawFunction>
	    requires std::invocable<DrawFunction, frame_2d&>
	inline void frame_2d::draw_scissored(SDL_Rect pixel_bounds, DrawFunction&& draw_function)
	{
		ensure_recording_allowed();
		auto* owner = m_control->owner;
		const auto previous_scissor = owner->exchange_frame_scissor(*this, pixel_bounds);
		try
		{
			std::invoke(std::forward<DrawFunction>(draw_function), *this);
		}
		catch (...)
		{
			owner->restore_frame_scissor(previous_scissor);
			throw;
		}
		owner->restore_frame_scissor(previous_scissor);
	}

	inline void recording_context::finish()
	{
		ensure_active();
		m_batcher_context.finish();
		m_finished = true;
	}

	inline void recording_context::ensure_active() const
	{
		if (!valid() || !m_control->owner || !m_control->owner->owns_frame(m_token) || m_finished) [[unlikely]]
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
		if (!m_recording_state || !m_control || !m_control->owner || !m_control->owner->owns_frame(m_token)) [[unlikely]]
		{
			throw std::logic_error("Cannot register a recording context for an inactive frame");
		}

		auto current = m_recording_state->outstanding_recording_contexts.load(std::memory_order_relaxed);
		for (;;)
		{
			if (current == std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
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
		m_scissor.reset();
		m_order_independent = false;
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

	inline cached_layer::cached_layer(glm::uvec2 pixel_size, gpu::texture_format format, color clear_color)
	    : m_texture(pixel_size.x, pixel_size.y, format, ::mngl::texture::default_upload_usage),
	      m_clear_color(clear_color)
	{
		if (pixel_size.x > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
		    pixel_size.y > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) [[unlikely]]
		{
			throw std::length_error("cached_layer dimensions exceed SDL's scissor rectangle range");
		}
		destination = ::mngl::rectangle{glm::vec2(0.0f), m_texture.coordinate_size()};
	}

	inline void cached_layer::invalidate_region(const ::mngl::rectangle& bounds)
	{
		if (!std::isfinite(bounds.pos.x) || !std::isfinite(bounds.pos.y) ||
		    !std::isfinite(bounds.size.x) || !std::isfinite(bounds.size.y) || bounds.size.x < 0.0f ||
		    bounds.size.y < 0.0f || !std::isfinite(bounds.pos.x + bounds.size.x) ||
		    !std::isfinite(bounds.pos.y + bounds.size.y)) [[unlikely]]
		{
			throw std::invalid_argument("cached_layer dirty bounds must be finite and non-negative");
		}
		if (m_full_dirty || bounds.size.x == 0.0f || bounds.size.y == 0.0f)
		{
			return;
		}

		const auto pixel_size = m_texture.size();
		const auto coordinate_size = m_texture.coordinate_size();
		const rectangle clipped = bounds.intersection(
		    rectangle{0.0f, 0.0f, coordinate_size.x, coordinate_size.y});
		if (clipped.size.x <= 0.0f || clipped.size.y <= 0.0f)
		{
			return;
		}

		const double scale = static_cast<double>(std::min(pixel_size.x, pixel_size.y));
		const auto clamp_pixel = [](double value, std::uint32_t limit)
		{
			return static_cast<int>(std::clamp(value, 0.0, static_cast<double>(limit)));
		};
		const int x0 = clamp_pixel(std::floor(static_cast<double>(clipped.pos.x) * scale), pixel_size.x);
		const int x1 = clamp_pixel(
		    std::ceil(static_cast<double>(clipped.pos.x + clipped.size.x) * scale), pixel_size.x);
		const int y0 = clamp_pixel(
		    std::floor(static_cast<double>(pixel_size.y) -
	               static_cast<double>(clipped.pos.y + clipped.size.y) * scale),
		    pixel_size.y);
		const int y1 = clamp_pixel(
		    std::ceil(static_cast<double>(pixel_size.y) - static_cast<double>(clipped.pos.y) * scale),
		    pixel_size.y);
		if (x1 <= x0 || y1 <= y0)
		{
			return;
		}

		SDL_Rect region{x0, y0, x1 - x0, y1 - y0};
		for (std::size_t index = 0; index < m_dirty_regions.size();)
		{
			const auto& existing = m_dirty_regions[index];
			const bool overlap = region.x < existing.x + existing.w && existing.x < region.x + region.w &&
			                     region.y < existing.y + existing.h && existing.y < region.y + region.h;
			if (!overlap)
			{
				++index;
				continue;
			}

			const int merged_left = std::min(region.x, existing.x);
			const int merged_top = std::min(region.y, existing.y);
			const int merged_right = std::max(region.x + region.w, existing.x + existing.w);
			const int merged_bottom = std::max(region.y + region.h, existing.y + existing.h);
			region = {merged_left, merged_top, merged_right - merged_left, merged_bottom - merged_top};
			m_dirty_regions.erase(m_dirty_regions.begin() + static_cast<std::ptrdiff_t>(index));
			index = 0;
		}
		m_dirty_regions.push_back(region);
	}

	template <typename DrawFunction>
	    requires std::invocable<DrawFunction&, frame_2d&, const ::mngl::rectangle&>
	inline bool cached_layer::update_dirty_regions(DrawFunction&& draw_region)
	{
		if (!dirty_region_updates_enabled())
		{
			const rectangle full_bounds{glm::vec2(0.0f), m_texture.coordinate_size()};
			return update_if_dirty(
			    [&](frame_2d& frame) { std::invoke(draw_region, frame, full_bounds); });
		}

		if (!dirty())
		{
			return false;
		}

		const bool rebuild_all = m_full_dirty;
		auto pending_regions = std::move(m_dirty_regions);
		m_dirty_regions.clear();
		m_full_dirty = false;
		try
		{
			if (rebuild_all)
			{
				const rectangle full_bounds{glm::vec2(0.0f), m_texture.coordinate_size()};
				auto target_frame = m_texture.frame(m_clear_color);
				target_frame.draw([&](frame_2d& frame) { std::invoke(draw_region, frame, full_bounds); });
				target_frame.display();
			}
			else
			{
				const auto pixel_size = m_texture.size();
				const float scale = static_cast<float>(std::min(pixel_size.x, pixel_size.y));
				auto target_frame = m_texture.frame_preserving(m_clear_color);
				target_frame.draw(
				    [&](frame_2d& frame)
				    {
					    for (const auto& pixel_region : pending_regions)
					    {
						    const rectangle region{
						        static_cast<float>(pixel_region.x) / scale,
						        static_cast<float>(pixel_size.y - pixel_region.y - pixel_region.h) / scale,
						        static_cast<float>(pixel_region.w) / scale,
						        static_cast<float>(pixel_region.h) / scale};
						    frame.draw_scissored(
						        pixel_region,
						        [&](frame_2d& clipped_frame)
						        {
							        rectangle_shape clear_region{
							            rectangle{glm::vec2(0.0f), m_texture.coordinate_size()}, m_clear_color};
							        clear_region.blend = gpu::sprite_blend_mode::opaque;
							        clipped_frame.draw(clear_region);
							        std::invoke(draw_region, clipped_frame, region);
					        });
					    }
			    });
				target_frame.display();
			}
		}
		catch (...)
		{
			m_full_dirty = true;
			m_dirty_regions.clear();
			throw;
		}
		return true;
	}

	inline void cached_layer::draw(frame_2d& frame) const
	{
		if (dirty()) [[unlikely]]
		{
			throw std::logic_error("Update a dirty cached_layer before drawing it");
		}

		::mngl::sprite image{m_texture, destination, tint};
		image.transform = transform;
		image.state.blend = blend;
		frame.draw(image);
	}

	inline void vertex_array::draw(frame_2d& frame) const
	{
		frame.submit_vertex_array(*this);
	}

	inline prepared_vertex_array::prepared_vertex_array(const vertex_array& source)
	    : tint(source.tint), blend(source.blend), transform(source.transform)
	{
		detail::tessellation_scratch scratch;
		const auto triangles = detail::tessellate(source, scratch);
		detail::validate_points(triangles, "Geometry vertices must be finite");
		if (source.primitive == primitive_type::triangles)
		{
			m_triangles.assign(triangles.begin(), triangles.end());
		}
		else
		{
			m_triangles = std::move(scratch.triangles);
		}
	}

	inline mesh::mesh(const gpu::device& device, gpu::command_buffer& command_buffer,
	                   std::span<const glm::vec2> triangle_vertices)
	{
		if (triangle_vertices.empty()) [[unlikely]]
		{
			throw std::invalid_argument("A 2D mesh requires at least one triangle");
		}
		if (triangle_vertices.size() % 3 != 0) [[unlikely]]
		{
			throw std::invalid_argument("A 2D mesh requires a triangle-list vertex count");
		}
		if (!device.raw()) [[unlikely]]
		{
			throw std::invalid_argument("Cannot create a 2D mesh with a null GPU device");
		}
		if (!command_buffer.valid()) [[unlikely]]
		{
			throw std::invalid_argument("Cannot create a 2D mesh with an invalid command buffer");
		}
		if (command_buffer.device_raw() && command_buffer.device_raw() != device.raw()) [[unlikely]]
		{
			throw std::invalid_argument("2D mesh and command buffer must use the same GPU device");
		}
		if (triangle_vertices.size() > std::numeric_limits<std::uint32_t>::max() /
		                                   sizeof(detail::resident_mesh_vertex)) [[unlikely]]
		{
			throw std::length_error("2D mesh exceeds SDL's 32-bit GPU buffer size limit");
		}

		auto storage = std::make_shared<detail::resident_mesh_storage>(device);
		storage->vertex_count = static_cast<std::uint32_t>(triangle_vertices.size());
		const std::size_t block_count =
		    (triangle_vertices.size() - 1) / detail::resident_mesh_block_vertex_capacity + 1;
		storage->blocks.reserve(block_count);

		std::vector<detail::resident_mesh_vertex> packed_vertices;
		packed_vertices.reserve(triangle_vertices.size());
		storage->bounds_min = glm::vec2(std::numeric_limits<float>::max());
		storage->bounds_max = glm::vec2(std::numeric_limits<float>::lowest());
		for (std::size_t block_offset = 0; block_offset < triangle_vertices.size();
		     block_offset += detail::resident_mesh_block_vertex_capacity)
		{
			const auto block_vertex_count = std::min(detail::resident_mesh_block_vertex_capacity,
			                                         triangle_vertices.size() - block_offset);
			detail::resident_mesh_block block;
			block.first_vertex = static_cast<std::uint32_t>(block_offset);
			block.vertex_count = static_cast<std::uint32_t>(block_vertex_count);
			block.bounds_min = glm::vec2(std::numeric_limits<float>::max());
			block.bounds_max = glm::vec2(std::numeric_limits<float>::lowest());
			for (std::size_t vertex_index = block_offset; vertex_index < block_offset + block_vertex_count;
			     ++vertex_index)
			{
				const auto vertex = triangle_vertices[vertex_index];
				if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)) [[unlikely]]
				{
					throw std::invalid_argument("2D mesh vertices must be finite");
				}
				packed_vertices.push_back({vertex.x, vertex.y});
				block.bounds_min.x = std::min(block.bounds_min.x, vertex.x);
				block.bounds_min.y = std::min(block.bounds_min.y, vertex.y);
				block.bounds_max.x = std::max(block.bounds_max.x, vertex.x);
				block.bounds_max.y = std::max(block.bounds_max.y, vertex.y);
			}
			storage->bounds_min = glm::min(storage->bounds_min, block.bounds_min);
			storage->bounds_max = glm::max(storage->bounds_max, block.bounds_max);
			storage->blocks.push_back(block);
		}

		command_buffer.retain_resource(storage);
		storage->vertex_buffer = gpu::buffer::create_with_data(
		    device, command_buffer, std::span<const detail::resident_mesh_vertex>(packed_vertices),
		    gpu::buffer_usage::vertex);
		m_storage = std::move(storage);
	}

	inline mesh::mesh(const gpu::device& device, gpu::command_buffer& command_buffer,
	                   const vertex_array& source)
	    : mesh(device, command_buffer, prepared_vertex_array(source))
	{
	}

	inline mesh::mesh(const gpu::device& device, gpu::command_buffer& command_buffer,
	                   const prepared_vertex_array& source)
	    : mesh(device, command_buffer, source.triangle_vertices())
	{
		tint = source.tint;
		blend = source.blend;
		transform = source.transform;
	}

	inline std::uint32_t mesh::vertex_count() const noexcept
	{
		return m_storage ? m_storage->vertex_count : 0;
	}

	inline std::size_t mesh::block_count() const noexcept
	{
		return m_storage ? m_storage->blocks.size() : 0;
	}

	inline std::uint32_t mesh::block_first_vertex(std::size_t index) const
	{
		if (!m_storage) [[unlikely]]
		{
			throw std::logic_error("Cannot inspect a moved-from 2D mesh");
		}
		if (index >= m_storage->blocks.size()) [[unlikely]]
		{
			throw std::out_of_range("2D mesh block index is out of range");
		}
		return m_storage->blocks[index].first_vertex;
	}

	inline std::uint32_t mesh::block_vertex_count(std::size_t index) const
	{
		if (!m_storage) [[unlikely]]
		{
			throw std::logic_error("Cannot inspect a moved-from 2D mesh");
		}
		if (index >= m_storage->blocks.size()) [[unlikely]]
		{
			throw std::out_of_range("2D mesh block index is out of range");
		}
		return m_storage->blocks[index].vertex_count;
	}

	inline void mesh::update_block(gpu::command_buffer& command_buffer, std::size_t index,
	                               std::span<const glm::vec2> vertices)
	{
		const std::array updates{block_update{.index = index, .vertices = vertices}};
		update_blocks(command_buffer, std::span<const block_update>(updates));
	}

	inline void mesh::update_blocks(gpu::command_buffer& command_buffer,
	                                std::span<const block_update> updates)
	{
		if (updates.empty())
		{
			return;
		}
		if (!m_storage) [[unlikely]]
		{
			throw std::logic_error("Cannot update a moved-from 2D mesh");
		}
		if (!command_buffer.valid()) [[unlikely]]
		{
			throw std::invalid_argument("Cannot update a 2D mesh with an invalid command buffer");
		}
		if (command_buffer.device_raw() && command_buffer.device_raw() != m_storage->device.raw()) [[unlikely]]
		{
			throw std::invalid_argument("2D mesh and command buffer must use the same GPU device");
		}
		std::lock_guard storage_lock(m_storage->mutex);

		struct pending_upload
		{
			std::size_t block_index = 0;
			std::uint32_t destination_offset = 0;
			std::uint32_t source_offset = 0;
			std::uint32_t size = 0;
			glm::vec2 bounds_min{};
			glm::vec2 bounds_max{};
		};
		std::vector<pending_upload> pending;
		pending.reserve(updates.size());
		std::unordered_set<std::size_t> seen_blocks;
		seen_blocks.reserve(updates.size());
		std::vector<detail::resident_mesh_vertex> packed_vertices;
		constexpr std::size_t max_upload_vertices =
		    std::numeric_limits<std::uint32_t>::max() / sizeof(detail::resident_mesh_vertex);
		for (const auto& update : updates)
		{
			if (update.index >= m_storage->blocks.size()) [[unlikely]]
			{
				throw std::out_of_range("2D mesh block index is out of range");
			}
			if (!seen_blocks.insert(update.index).second) [[unlikely]]
			{
				throw std::invalid_argument("A 2D mesh block can only be updated once per update batch");
			}
			const auto& block = m_storage->blocks[update.index];
			if (update.vertices.size() != block.vertex_count) [[unlikely]]
			{
				throw std::invalid_argument("A 2D mesh block update must preserve its vertex count");
			}
			if (update.vertices.size() > max_upload_vertices - packed_vertices.size()) [[unlikely]]
			{
				throw std::length_error("2D mesh update exceeds SDL's 32-bit transfer buffer size limit");
			}

			pending_upload upload;
			upload.block_index = update.index;
			upload.destination_offset = static_cast<std::uint32_t>(
			    block.first_vertex * sizeof(detail::resident_mesh_vertex));
			upload.source_offset = static_cast<std::uint32_t>(packed_vertices.size() *
			                                                 sizeof(detail::resident_mesh_vertex));
			upload.size = static_cast<std::uint32_t>(update.vertices.size() *
			                                        sizeof(detail::resident_mesh_vertex));
			upload.bounds_min = glm::vec2(std::numeric_limits<float>::max());
			upload.bounds_max = glm::vec2(std::numeric_limits<float>::lowest());
			for (const auto vertex : update.vertices)
			{
				if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)) [[unlikely]]
				{
					throw std::invalid_argument("2D mesh vertices must be finite");
				}
				packed_vertices.push_back({vertex.x, vertex.y});
				upload.bounds_min.x = std::min(upload.bounds_min.x, vertex.x);
				upload.bounds_min.y = std::min(upload.bounds_min.y, vertex.y);
				upload.bounds_max.x = std::max(upload.bounds_max.x, vertex.x);
				upload.bounds_max.y = std::max(upload.bounds_max.y, vertex.y);
			}
			pending.push_back(upload);
		}

		command_buffer.retain_resource(m_storage);
		const auto staged_size = m_storage->vertex_buffer.stage_upload(
		    m_storage->device.raw(), std::as_bytes(std::span<const detail::resident_mesh_vertex>(packed_vertices)));
		gpu::detail::gpu_copy_pass copy_pass(command_buffer);
		bool cycle_vertex_buffer = true;
		for (const auto& upload : pending)
		{
			m_storage->vertex_buffer.record_upload(copy_pass, upload.size, cycle_vertex_buffer,
			                                      upload.destination_offset,
			                                      upload.source_offset);
			cycle_vertex_buffer = false;
		}
		(void)staged_size;

		for (const auto& upload : pending)
		{
			auto& block = m_storage->blocks[upload.block_index];
			// Keep both old and new positions covered if the caller later cancels this command buffer.
			block.bounds_min = glm::min(block.bounds_min, upload.bounds_min);
			block.bounds_max = glm::max(block.bounds_max, upload.bounds_max);
		}
		m_storage->bounds_min = glm::vec2(std::numeric_limits<float>::max());
		m_storage->bounds_max = glm::vec2(std::numeric_limits<float>::lowest());
		for (const auto& block : m_storage->blocks)
		{
			m_storage->bounds_min = glm::min(m_storage->bounds_min, block.bounds_min);
			m_storage->bounds_max = glm::max(m_storage->bounds_max, block.bounds_max);
		}
	}

	inline void prepared_vertex_array::draw(frame_2d& frame) const
	{
		frame.submit_prepared_vertex_array(*this);
	}

	inline void mesh::draw(frame_2d& frame) const
	{
		frame.submit_mesh(*this);
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
			m_control->owner->submit_texture(m_recording_context, *value.source_texture,
			                                 gpu_transform, value.tint, value.state, value.click_callback);
		}
		else
		{
			m_control->owner->submit_quad(m_recording_context, value.source, gpu_transform,
			                              value.tint, value.state, ::mngl::detail::hit_test_shape::quad, 0.0f,
			                              value.click_callback);
		}
	}

	inline void frame_2d::submit_circle(const circle& value)
	{
		m_control->owner->submit_circle(m_recording_context, value);
	}

	inline void frame_2d::submit_ellipse(const ellipse& value)
	{
		m_control->owner->submit_ellipse(m_recording_context, value);
	}

	inline void frame_2d::submit_rounded_rectangle(const rounded_rectangle& value)
	{
		m_control->owner->submit_rounded_rectangle(m_recording_context, value);
	}

	inline void frame_2d::submit_rectangle_shape(const rectangle_shape& value)
	{
		m_control->owner->submit_rectangle(m_recording_context, value.bounds, value.tint,
		                                   value.transform, value.blend);
	}

	inline void frame_2d::submit_vertex_array(const vertex_array& value)
	{
		m_control->owner->submit_vertex_array(m_recording_context, value);
	}

	inline void frame_2d::submit_prepared_vertex_array(const prepared_vertex_array& value)
	{
		m_control->owner->submit_prepared_vertex_array(m_recording_context, value);
	}

	inline void frame_2d::submit_mesh(const mesh& value)
	{
		m_control->owner->submit_mesh(m_recording_context, value);
	}

	inline void frame_2d::submit_convex_polygon(const convex_polygon& value)
	{
		m_control->owner->submit_convex_polygon(m_recording_context, value);
	}

	inline void frame_2d::submit_text(const text& value)
	{
		if (value.m_document)
		{
			m_control->owner->submit_text_document(m_recording_context, *value.m_typeface,
			                                       *value.m_document, value.m_top_left, value.m_options);
		}
		else
		{
			m_control->owner->submit_text(m_recording_context, *value.m_typeface, value.m_text,
			                              value.m_top_left, value.m_options);
		}
	}

	inline void frame_2d::prepare(const ::mngl::drawable& value)
	{
		ensure_active();
		if (m_recording_context) [[unlikely]]
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
		if (m_recording_context) [[unlikely]]
		{
			throw std::logic_error("Worker recording contexts can only be created from the parent frame_2d");
		}
		return m_control->owner->create_recording_context(*this, expected_draw_count);
	}

	inline void frame_2d::merge(recording_context&& context)
	{
		ensure_active();
		if (m_recording_context) [[unlikely]]
		{
			throw std::logic_error("Worker recording contexts can only be merged into the parent frame_2d");
		}
		m_control->owner->merge_recording_context(*this, context);
	}

	inline void frame_2d::prepare_shader_sprite(const ::mngl::sprite& value)
	{
		ensure_active();
		if (m_recording_context) [[unlikely]]
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
		if (m_recording_context) [[unlikely]]
		{
			throw std::logic_error("Fonts must be prepared on the parent frame_2d");
		}
		m_control->owner->prepare_font(m_token, typeface, text);
	}

	inline void frame_2d::set_view_projection(const glm::mat4& matrix)
	{
		ensure_active();
		if (m_recording_context) [[unlikely]]
		{
			throw std::logic_error("Worker recording contexts cannot change the frame projection");
		}
		m_control->owner->ensure_preparation_allowed(m_token);
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				if (!std::isfinite(matrix[column][row])) [[unlikely]]
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
		if (m_recording_context) [[unlikely]]
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
		if (m_recording_context) [[unlikely]]
		{
			throw std::logic_error("A worker recording context cannot finish its parent frame");
		}
		m_control->owner->finish_frame(*this);
	}

	inline void frame_2d::finish(gpu::command_buffer& command_buffer)
	{
		ensure_active();
		if (m_recording_context) [[unlikely]]
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
		if (!active()) [[unlikely]]
		{
			throw std::logic_error("frame_2d is no longer active");
		}
	}

	inline void frame_2d::ensure_recording_allowed() const
	{
		if (!m_control || !m_control->owner) [[unlikely]]
		{
			throw std::logic_error("frame_2d is no longer active");
		}
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
