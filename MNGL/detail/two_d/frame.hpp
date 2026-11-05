#pragma once

#include "geometry.hpp"

namespace mngl::two_d
{
	class renderer;
	class recording_context;

	namespace detail
	{
		struct renderer_frame_control
		{
			renderer* owner = nullptr;
		};

		struct renderer_frame_state
		{
			std::atomic<std::uint32_t> outstanding_recording_contexts{0};
		};
	} // namespace detail

	class frame_2d
	{
	public:
		frame_2d(const frame_2d&) = delete;
		frame_2d& operator=(const frame_2d&) = delete;

		frame_2d(frame_2d&& other) noexcept
		    : m_control(std::move(other.m_control)), m_token(std::exchange(other.m_token, 0)),
		      m_recording_state(std::move(other.m_recording_state)),
		      m_recording_context(std::exchange(other.m_recording_context, nullptr)),
		      m_view_projection(std::move(other.m_view_projection)), m_view(std::move(other.m_view))
		{
			other.m_view_projection.reset();
			other.m_view.reset();
		}

		frame_2d& operator=(frame_2d&& other) noexcept;
		~frame_2d() noexcept;

		void draw(const ::mngl::drawable& value);
		void draw_text(font& typeface, std::string_view text, glm::vec2 top_left,
		               const text_draw_options& options = {});
		template <typename DrawFunction>
		    requires std::invocable<DrawFunction, frame_2d&>
		void draw(DrawFunction&& draw_function)
		{
			ensure_active();
			ensure_recording_allowed();
			std::invoke(std::forward<DrawFunction>(draw_function), *this);
		}

		void prepare(const ::mngl::drawable& value);
		void prepare(font& typeface, std::string_view text);
		[[nodiscard]] recording_context create_recording_context(std::size_t expected_draw_count = 0);
		void merge(recording_context&& context);
		void set_view_projection(const glm::mat4& matrix);
		void set_view(const ::mngl::view& value);
		void finish();
		// Records into the supplied command buffer without submitting it.
		void finish(gpu::command_buffer& command_buffer);
		[[nodiscard]] bool active() const noexcept;

	private:
		friend class renderer;
		friend class recording_context;
		friend struct ::mngl::sprite;
		friend struct circle;
		friend struct ellipse;
		friend struct rounded_rectangle;
		friend class rectangle_shape;
		friend class vertex_array;
		friend class convex_polygon;
		friend class text;

		void submit_sprite(const ::mngl::sprite& value);
		void submit_circle(const circle& value);
		void submit_ellipse(const ellipse& value);
		void submit_rounded_rectangle(const rounded_rectangle& value);
		void submit_rectangle_shape(const rectangle_shape& value);
		void submit_vertex_array(const vertex_array& value);
		void submit_convex_polygon(const convex_polygon& value);
		void submit_text(const text& value);
		void prepare_shader_sprite(const ::mngl::sprite& value);
		void prepare_font(font& typeface, std::string_view text);

		frame_2d(std::shared_ptr<detail::renderer_frame_control> control, std::uint64_t token,
		         std::shared_ptr<detail::renderer_frame_state> recording_state) noexcept
		    : m_control(std::move(control)), m_token(token), m_recording_state(std::move(recording_state))
		{
		}

		frame_2d(std::shared_ptr<detail::renderer_frame_control> control, std::uint64_t token,
		         recording_context* context) noexcept
		    : m_control(std::move(control)), m_token(token), m_recording_context(context)
		{
		}

		void ensure_active() const;
		void ensure_recording_allowed() const;
		void cancel() noexcept;
		void detach() noexcept
		{
			m_control.reset();
			m_token = 0;
			m_recording_state.reset();
			m_recording_context = nullptr;
			m_view_projection.reset();
			m_view.reset();
		}

		std::shared_ptr<detail::renderer_frame_control> m_control;
		std::uint64_t m_token = 0;
		std::shared_ptr<detail::renderer_frame_state> m_recording_state;
		recording_context* m_recording_context = nullptr;
		std::optional<glm::mat4> m_view_projection;
		std::optional<::mngl::view> m_view;
	};

	class recording_context
	{
	public:
		recording_context() noexcept = default;
		recording_context(const recording_context&) = delete;
		recording_context& operator=(const recording_context&) = delete;
		recording_context(recording_context&& other) noexcept
		    : m_control(std::move(other.m_control)), m_recording_state(std::move(other.m_recording_state)),
		      m_token(std::exchange(other.m_token, 0)), m_batcher_context(std::move(other.m_batcher_context)),
		      m_geometry_scratch(std::move(other.m_geometry_scratch)),
		      m_hit_test_items(std::move(other.m_hit_test_items)),
		      m_used_fonts(std::move(other.m_used_fonts)),
		      m_view_projection(std::move(other.m_view_projection)), m_view(std::move(other.m_view)),
		      m_registered(std::exchange(other.m_registered, false)),
		      m_finished(std::exchange(other.m_finished, false))
		{
		}

		recording_context& operator=(recording_context&& other) noexcept
		{
			if (this != &other)
			{
				release_registration();
				m_control = std::move(other.m_control);
				m_recording_state = std::move(other.m_recording_state);
				m_token = std::exchange(other.m_token, 0);
				m_batcher_context = std::move(other.m_batcher_context);
				m_geometry_scratch = std::move(other.m_geometry_scratch);
				m_hit_test_items = std::move(other.m_hit_test_items);
				m_used_fonts = std::move(other.m_used_fonts);
				m_view_projection = std::move(other.m_view_projection);
				m_view = std::move(other.m_view);
				m_registered = std::exchange(other.m_registered, false);
				m_finished = std::exchange(other.m_finished, false);
			}
			return *this;
		}

		~recording_context() noexcept { release_registration(); }

		[[nodiscard]] bool valid() const noexcept
		{
			return m_control && m_token != 0 && m_batcher_context.valid();
		}

		[[nodiscard]] bool finished() const noexcept { return m_finished; }

		void draw(const ::mngl::drawable& value);

		template <typename DrawFunction>
		    requires std::invocable<DrawFunction, frame_2d&>
		void draw(DrawFunction&& draw_function)
		{
			ensure_active();
			auto frame = frame_view();
			frame.draw(std::forward<DrawFunction>(draw_function));
		}

		void finish();

	private:
		friend class frame_2d;
		friend class renderer;

		recording_context(std::shared_ptr<detail::renderer_frame_control> control,
		                  std::shared_ptr<detail::renderer_frame_state> recording_state, std::uint64_t token,
		                  gpu::batcher2d::recording_context batcher_context,
		                  std::optional<glm::mat4> view_projection, std::optional<::mngl::view> view) noexcept
		    : m_control(std::move(control)), m_recording_state(std::move(recording_state)), m_token(token),
		      m_batcher_context(std::move(batcher_context)), m_view_projection(std::move(view_projection)),
		      m_view(std::move(view))
		{
		}

		void ensure_active() const;
		[[nodiscard]] frame_2d frame_view();
		void register_with_frame();
		void release_registration() noexcept;
		void detach_after_merge() noexcept;

		std::shared_ptr<detail::renderer_frame_control> m_control;
		std::shared_ptr<detail::renderer_frame_state> m_recording_state;
		std::uint64_t m_token = 0;
		gpu::batcher2d::recording_context m_batcher_context;
		detail::tessellation_scratch m_geometry_scratch;
		std::vector<::mngl::detail::hit_test_item> m_hit_test_items;
		std::vector<font*> m_used_fonts;
		std::optional<glm::mat4> m_view_projection;
		std::optional<::mngl::view> m_view;
		bool m_registered = false;
		bool m_finished = false;
	};
} // namespace mngl::two_d
