#pragma once

#include "helpers.hpp"

namespace mngl
{
	class imgui_context final : private detail::render_window_overlay
	{
	private:
		struct loaded_font
		{
			ImFont* value = nullptr;
			float size_pixels = 18.0f;
		};

	public:
		class frame_scope;
		class font_scope;
		class dockspace_scope;
		class main_menu_bar_scope;
		class menu_scope;

		class window_scope
		{
		public:
			window_scope(const window_scope&) = delete;
			window_scope& operator=(const window_scope&) = delete;
			window_scope(window_scope&&) = delete;
			window_scope& operator=(window_scope&&) = delete;
			~window_scope() noexcept;

			[[nodiscard]] explicit operator bool() const noexcept { return visible; }
			void text(std::string_view value) const;
			void text(const char* value) const;
			void text(std::nullptr_t) const = delete;
			template <imgui_compat::detail::scalar_numeric_value T> void text(const T& value) const;
			void text(bool value) const;
			[[nodiscard]] bool button(std::string_view label) const;
			[[nodiscard]] bool button(const char* label) const;
			[[nodiscard]] bool button(std::nullptr_t) const = delete;
			template <imgui_compat::detail::scalar_numeric_value T>
			[[nodiscard]] bool button(const T& label) const;
			[[nodiscard]] bool button(bool label) const;
			void progress_bar(float fraction) const;
			void progress_bar(float fraction, std::string_view overlay) const;
			[[nodiscard]] bool selectable(std::string_view label, bool selected = false) const;
			[[nodiscard]] bool selectable(const char* label, bool selected = false) const;
			[[nodiscard]] bool selectable(std::nullptr_t, bool = false) const = delete;
			template <imgui_compat::detail::widget_label_value Label>
			[[nodiscard]] bool selectable(const Label& label, bool selected = false) const
			{
				return selectable(imgui_compat::detail::format_widget_label(label), selected);
			}
			void bullet() const;
			void bullet(std::string_view label) const;
			void bullet(const char* label) const;
			void bullet(std::nullptr_t) const = delete;
			template <imgui_compat::detail::widget_label_value Label> void bullet(const Label& label) const
			{
				bullet(imgui_compat::detail::format_widget_label(label));
			}
			[[nodiscard]] bool radio_button(std::string_view label, bool active) const;
			[[nodiscard]] bool radio_button(const char* label, bool active) const;
			[[nodiscard]] bool radio_button(std::nullptr_t, bool) const = delete;
			template <imgui_compat::detail::widget_label_value Label>
			[[nodiscard]] bool radio_button(const Label& label, bool active) const
			{
				return radio_button(imgui_compat::detail::format_widget_label(label), active);
			}
			template <typename T>
			    requires std::equality_comparable<T> && std::assignable_from<T&, std::type_identity_t<T>>
			[[nodiscard]] bool radio_button(std::string_view label, T& selected,
			                                std::type_identity_t<T> value) const;
			template <imgui_compat::detail::widget_label_value Label, typename T>
			    requires std::equality_comparable<T> && std::assignable_from<T&, std::type_identity_t<T>>
			[[nodiscard]] bool radio_button(const Label& label, T& selected,
			                                std::type_identity_t<T> value) const
			{
				return radio_button(imgui_compat::detail::format_widget_label(label), selected, value);
			}
			template <imgui_compat::detail::combo_range Range>
			[[nodiscard]] imgui_compat::detail::combo_result_t<Range> combo(std::string_view label,
			                                                                Range&& values) const;
			template <typename T>
			    requires imgui_compat::detail::combo_range<std::initializer_list<T>>
			[[nodiscard]] imgui_compat::detail::combo_result_t<std::initializer_list<T>> combo(
			    std::string_view label, std::initializer_list<T>&& values) const;
			template <imgui_compat::detail::scalar_numeric_value Label, typename T>
			    requires imgui_compat::detail::combo_range<std::initializer_list<T>>
			[[nodiscard]] imgui_compat::detail::combo_result_t<std::initializer_list<T>> combo(
			    const Label& label, std::initializer_list<T>&& values) const;
			template <typename T>
			    requires imgui_compat::detail::combo_range<std::initializer_list<T>>
			[[nodiscard]] imgui_compat::detail::combo_result_t<std::initializer_list<T>> combo(
			    bool label, std::initializer_list<T>&& values) const;
			template <imgui_compat::detail::combo_range Range>
			[[nodiscard]] imgui_compat::detail::combo_result_t<Range> combo(const char* label,
			                                                                Range&& values) const;
			template <imgui_compat::detail::scalar_numeric_value Label,
			          imgui_compat::detail::combo_range Range>
			[[nodiscard]] imgui_compat::detail::combo_result_t<Range> combo(const Label& label,
			                                                                Range&& values) const;
			template <imgui_compat::detail::combo_range Range>
			[[nodiscard]] imgui_compat::detail::combo_result_t<Range> combo(bool label, Range&& values) const;
			template <imgui_compat::detail::combo_range Range>
			imgui_compat::detail::combo_result_t<Range> combo(std::nullptr_t, Range&& values) const = delete;
			template <imgui_compat::detail::combo_range Range>
			[[nodiscard]] imgui_compat::detail::context_menu_result_t<Range> context_menu(
			    Range&& values) const;
			template <typename T>
			    requires imgui_compat::detail::combo_range<std::initializer_list<T>>
			[[nodiscard]] imgui_compat::detail::context_menu_result_t<std::initializer_list<T>> context_menu(
			    std::initializer_list<T>&& values) const;
			template <imgui_compat::detail::glm_slider_vector Vector>
			[[nodiscard]] bool slider(
			    std::string_view label, Vector& value,
			    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type
			        min,
			    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type
			        max) const;
			template <imgui_compat::detail::glm_slider_vector Vector>
			[[nodiscard]] bool slider(
			    const char* label, Vector& value,
			    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type
			        min,
			    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type
			        max) const;
			template <imgui_compat::detail::scalar_numeric_value Label,
			          imgui_compat::detail::glm_slider_vector Vector>
			[[nodiscard]] bool slider(
			    const Label& label, Vector& value,
			    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type
			        min,
			    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type
			        max) const
			{
				return slider(imgui_compat::detail::format_scalar_value(label), value, min, max);
			}
			template <imgui_compat::detail::glm_slider_vector Vector>
			[[nodiscard]] bool slider(
			    bool label, Vector& value,
			    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type
			        min,
			    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type
			        max) const
			{
				return slider(imgui_compat::detail::format_scalar_value(label), value, min, max);
			}
			template <imgui_compat::detail::glm_slider_vector Vector>
			[[nodiscard]] bool slider(
			    std::nullptr_t, Vector& value,
			    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type
			        min,
			    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type
			        max) const = delete;
			template <imgui_compat::detail::scalar_input_value T>
			[[nodiscard]] bool slider(std::string_view label, T& value, std::type_identity_t<T> min,
			                          std::type_identity_t<T> max) const;
			template <imgui_compat::detail::scalar_input_value T>
			[[nodiscard]] bool slider(const char* label, T& value, std::type_identity_t<T> min,
			                          std::type_identity_t<T> max) const;
			template <imgui_compat::detail::scalar_numeric_value Label,
			          imgui_compat::detail::scalar_input_value T>
			[[nodiscard]] bool slider(const Label& label, T& value, std::type_identity_t<T> min,
			                          std::type_identity_t<T> max) const;
			template <imgui_compat::detail::scalar_input_value T>
			[[nodiscard]] bool slider(bool label, T& value, std::type_identity_t<T> min,
			                          std::type_identity_t<T> max) const;
			template <imgui_compat::detail::scalar_input_value T>
			[[nodiscard]] bool slider(std::nullptr_t, T& value, std::type_identity_t<T> min,
			                          std::type_identity_t<T> max) const = delete;
			[[nodiscard]] bool input(std::string_view label, std::string& value,
			                         const std::regex& pattern) const;
			template <imgui_compat::detail::scalar_input_value T>
			[[nodiscard]] bool input(std::string_view label, T& value) const;
			[[nodiscard]] bool input(std::string_view label, bool& value) const;
			[[nodiscard]] bool checkbox(std::string_view label, bool& value) const;
			[[nodiscard]] bool checkbox(const char* label, bool& value) const;
			[[nodiscard]] bool checkbox(std::nullptr_t, bool&) const = delete;
			template <imgui_compat::detail::widget_label_value Label>
			[[nodiscard]] bool checkbox(const Label& label, bool& value) const
			{
				return checkbox(imgui_compat::detail::format_widget_label(label), value);
			}
			void image(const ::mngl::texture& source, glm::vec2 display_size) const;
			[[nodiscard]] dockspace_scope dockspace(std::string_view id) const;
			[[nodiscard]] font_scope use_font(imgui_compat::font_role role, float size_pixels = 0.0f) const;

		private:
			friend class frame_scope;
			friend class dockspace_scope;
			window_scope(imgui_context& owner, std::string_view title,
			             std::optional<ImGuiID> dockspace_id = std::nullopt, bool* open = nullptr);

			imgui_context* owner;
			bool visible;
		};

		class dockspace_scope
		{
		public:
			dockspace_scope(const dockspace_scope&) = delete;
			dockspace_scope& operator=(const dockspace_scope&) = delete;
			dockspace_scope(dockspace_scope&&) = delete;
			dockspace_scope& operator=(dockspace_scope&&) = delete;
			~dockspace_scope() noexcept = default;

			[[nodiscard]] window_scope window(std::string_view title) const;
			[[nodiscard]] window_scope window(std::string_view title, bool& open) const;

		private:
			friend class window_scope;
			dockspace_scope(imgui_context& owner, ImGuiID id) noexcept : owner(&owner), id(id) {}

			imgui_context* owner;
			ImGuiID id;
		};

		class menu_scope
		{
		public:
			menu_scope() noexcept = default;
			menu_scope(const menu_scope&) = delete;
			menu_scope& operator=(const menu_scope&) = delete;
			menu_scope(menu_scope&&) = delete;
			menu_scope& operator=(menu_scope&&) = delete;
			~menu_scope() noexcept;

			[[nodiscard]] explicit operator bool() const noexcept { return visible; }
			[[nodiscard]] bool toggle(std::string_view label, bool& value) const;

		private:
			friend class main_menu_bar_scope;
			menu_scope(imgui_context& owner, std::string_view label);

			imgui_context* owner = nullptr;
			bool visible = false;
		};

		class main_menu_bar_scope
		{
		public:
			main_menu_bar_scope(const main_menu_bar_scope&) = delete;
			main_menu_bar_scope& operator=(const main_menu_bar_scope&) = delete;
			main_menu_bar_scope(main_menu_bar_scope&&) = delete;
			main_menu_bar_scope& operator=(main_menu_bar_scope&&) = delete;
			~main_menu_bar_scope() noexcept;

			[[nodiscard]] explicit operator bool() const noexcept { return visible; }
			[[nodiscard]] menu_scope menu(std::string_view label) const;

		private:
			friend class frame_scope;
			explicit main_menu_bar_scope(imgui_context& owner);

			imgui_context* owner = nullptr;
			bool visible = false;
		};

		class font_scope
		{
		public:
			font_scope() noexcept = default;
			font_scope(const font_scope&) = delete;
			font_scope& operator=(const font_scope&) = delete;
			font_scope(font_scope&&) = delete;
			font_scope& operator=(font_scope&&) = delete;
			~font_scope() noexcept;

		private:
			friend class window_scope;
			font_scope(imgui_context& owner, ImFont* font, float size_pixels);

			imgui_context* owner = nullptr;
		};

		class frame_scope
		{
		public:
			frame_scope(const frame_scope&) = delete;
			frame_scope& operator=(const frame_scope&) = delete;
			frame_scope(frame_scope&&) = delete;
			frame_scope& operator=(frame_scope&&) = delete;
			~frame_scope() noexcept;

			[[nodiscard]] main_menu_bar_scope main_menu_bar() const;
			[[nodiscard]] window_scope window(std::string_view title) const;

		private:
			friend class imgui_context;
			explicit frame_scope(imgui_context& owner) noexcept : owner(&owner) {}

			imgui_context* owner;
		};

		explicit imgui_context(render_window& window, const imgui_compat::font_config& fonts = {});
		imgui_context(const imgui_context&) = delete;
		imgui_context& operator=(const imgui_context&) = delete;
		imgui_context(imgui_context&&) = delete;
		imgui_context& operator=(imgui_context&&) = delete;
		~imgui_context() noexcept override;

		[[nodiscard]] frame_scope frame();
		[[nodiscard]] frame_scope frame(::mngl::texture& target);

		void set_user_scale(float scale)
		{
			if (std::isfinite(scale) && scale > 0.05f && scale < 10.0f)
			{
				m_user_scale = scale;
			}
		}
		[[nodiscard]] float user_scale() const noexcept { return m_user_scale; }
		[[nodiscard]] float display_scale() const noexcept
		{
			return attached_window ? attached_window->display_scale() : 1.0f;
		}
		[[nodiscard]] float effective_scale() const noexcept
		{
			const float base_scale = display_scale();
			const float scale = base_scale * m_user_scale;
			return (scale > 0.05f && std::isfinite(scale)) ? scale : 1.0f;
		}

	private:
		void initialize_fonts(const imgui_compat::font_config& config);
		[[nodiscard]] const loaded_font& get_font(imgui_compat::font_role role) const;
		void begin_frame(::mngl::texture* target);
		void finish_frame() noexcept;
		void render(gpu::command_buffer& command_buffer, ::mngl::texture& target) override;
		void detach_from_window() noexcept override;
		void shutdown() noexcept;
		void make_current() const noexcept;

		render_window* attached_window = nullptr;
		ImGuiContext* context = nullptr;
		std::array<loaded_font, 5> fonts{};
		gpu::texture_format color_target_format = gpu::texture_format::invalid;
		::mngl::texture* requested_render_target = nullptr;
		std::vector<hit_test_path_entry> captured_hit_test_path;
		glm::vec2 last_offscreen_mouse_position{};
		std::uint64_t captured_texture_identity = 0;
		bool frame_active = false;
		bool draw_data_ready = false;
		bool offscreen_mouse_captured = false;
		bool has_last_offscreen_mouse_position = false;
		float m_applied_scale = 1.0f;
		float m_user_scale = 1.0f;
	};
} // namespace mngl
