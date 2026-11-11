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
		class window_scope;
		using child_scope = window_scope;
		using combo_scope = window_scope;
		using popup_scope = window_scope;
		using tab_bar_scope = window_scope;
		using tab_item_scope = window_scope;
		using table_scope = window_scope;
		using tooltip_scope = window_scope;
		using disabled_scope = window_scope;
		using tree_node_scope = window_scope;
		class id_scope;
		class style_color_scope;
		class canvas_scope;

		class window_scope
		{
		public:
			window_scope() noexcept = default;
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
			[[nodiscard]] bool input_text(std::string_view label, std::string& value,
			                              std::string_view hint = {}) const;
			[[nodiscard]] bool input_text_multiline(std::string_view label, std::string& value,
			                                       glm::vec2 size) const;
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
			void text_wrapped(std::string_view value) const;
			void text_disabled(std::string_view value) const;
			void text_colored(const ::mngl::color& tint, std::string_view value) const;
			[[nodiscard]] glm::vec2 measure_text(std::string_view value, float wrap_width = 0.0f) const;
			[[nodiscard]] glm::vec2 available_size() const;
			[[nodiscard]] glm::vec2 cursor_screen_position() const;
			[[nodiscard]] imgui_compat::style_metrics metrics() const;
			[[nodiscard]] ::mngl::color style_color(imgui_compat::color_role role) const;
			void same_line(float offset_from_start = 0.0f, float spacing = -1.0f) const;
			void new_line() const;
			void separator() const;
			void set_next_item_width(float width) const;
			void set_keyboard_focus_here(int offset = 0) const;
			void focus_last_item() const;
			[[nodiscard]] bool small_button(std::string_view label) const;
			[[nodiscard]] bool collapsing_header(std::string_view label, bool default_open = false) const;
			[[nodiscard]] imgui_compat::item_state last_item() const;
			[[nodiscard]] bool focused(bool include_child_windows = true) const;
			[[nodiscard]] float scroll_y() const;
			[[nodiscard]] float scroll_max_y() const;
			void scroll_to_y(float position) const;
			void scroll_here_y(float center_ratio = 0.5f) const;
			[[nodiscard]] bool popup_open(std::string_view id) const;
			void open_popup(std::string_view id) const;
			void close_popup() const;

			[[nodiscard]] child_scope child(std::string_view id,
			                               const imgui_compat::child_options& options = {}) const;
			[[nodiscard]] combo_scope begin_combo(std::string_view label, std::string_view preview) const;
			[[nodiscard]] popup_scope popup(std::string_view id,
			                               const imgui_compat::window_options& options = {}) const;
			[[nodiscard]] popup_scope modal(std::string_view id, bool& open,
			                               const imgui_compat::window_options& options = {}) const;
			[[nodiscard]] popup_scope context_popup(
			    std::string_view id = {}, mouse_button button = mouse_button::right) const;
			[[nodiscard]] tab_bar_scope tab_bar(
			    std::string_view id, const imgui_compat::tab_bar_options& options = {}) const;
			[[nodiscard]] tab_item_scope tab(
			    std::string_view label, bool* open = nullptr,
			    const imgui_compat::tab_item_options& options = {}) const;
			[[nodiscard]] table_scope table(
			    std::string_view id, int columns,
			    const imgui_compat::table_options& options = {}) const;
			[[nodiscard]] tooltip_scope tooltip() const;
			[[nodiscard]] disabled_scope disabled(bool value = true) const;
			[[nodiscard]] tree_node_scope tree_node(
			    std::string_view label, const imgui_compat::tree_node_options& options = {}) const;
			[[nodiscard]] canvas_scope canvas(std::string_view id, glm::vec2 size,
			                                const imgui_compat::canvas_options& options = {}) const;
			[[nodiscard]] id_scope push_id(std::string_view id) const;
			[[nodiscard]] id_scope push_id(int id) const;
			[[nodiscard]] style_color_scope push_style_color(
			    imgui_compat::color_role role, const ::mngl::color& value) const;

			void next_row(float minimum_height = 0.0f) const;
			[[nodiscard]] bool next_column() const;
			void setup_column(std::string_view label, float initial_width = 0.0f) const;
			void table_headers_row() const;

			[[nodiscard]] dockspace_scope dockspace(std::string_view id) const;
			[[nodiscard]] font_scope use_font(imgui_compat::font_role role, float size_pixels = 0.0f) const;

		private:
			enum class end_action : std::uint8_t
			{
				none,
				window,
				child,
				combo,
				popup,
				tab_bar,
				tab_item,
				table,
				tooltip,
				disabled,
				tree_node
			};

			friend class frame_scope;
			friend class dockspace_scope;
			friend class canvas_scope;
			window_scope(imgui_context& owner, std::string_view title,
			             const imgui_compat::window_options& options = {},
			             std::optional<ImGuiID> dockspace_id = std::nullopt, bool* open = nullptr);
			window_scope(imgui_context& owner, bool visible, end_action end) noexcept;

			imgui_context* owner = nullptr;
			bool visible = false;
			end_action end = end_action::none;
		};

		class dockspace_scope
		{
		public:
			dockspace_scope(const dockspace_scope&) = delete;
			dockspace_scope& operator=(const dockspace_scope&) = delete;
			dockspace_scope(dockspace_scope&&) = delete;
			dockspace_scope& operator=(dockspace_scope&&) = delete;
			~dockspace_scope() noexcept = default;

			[[nodiscard]] window_scope window(
			    std::string_view title, const imgui_compat::window_options& options = {}) const;
			[[nodiscard]] window_scope window(std::string_view title, bool& open,
			                                  const imgui_compat::window_options& options = {}) const;

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
			[[nodiscard]] bool menu_item(std::string_view label, std::string_view shortcut = {},
			                             bool selected = false, bool enabled = true) const;
			[[nodiscard]] bool menu_item(std::string_view label, bool& selected,
			                             std::string_view shortcut = {}, bool enabled = true) const;
			[[nodiscard]] menu_scope menu(std::string_view label) const;

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
			[[nodiscard]] bool menu_item(std::string_view label, std::string_view shortcut = {},
			                             bool selected = false, bool enabled = true) const;
			[[nodiscard]] bool menu_item(std::string_view label, bool& selected,
			                             std::string_view shortcut = {}, bool enabled = true) const;

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
			[[nodiscard]] window_scope window(
			    std::string_view title, const imgui_compat::window_options& options = {}) const;
			[[nodiscard]] window_scope window(std::string_view title, bool& open,
			                                  const imgui_compat::window_options& options = {}) const;
			[[nodiscard]] imgui_compat::input_state input() const;
			[[nodiscard]] bool key_pressed(imgui_compat::key value, bool repeat = false) const;
			[[nodiscard]] bool mouse_down(mouse_button button) const;
			[[nodiscard]] bool mouse_clicked(mouse_button button) const;
			[[nodiscard]] bool mouse_double_clicked(mouse_button button) const;
			[[nodiscard]] bool mouse_dragging(mouse_button button, float threshold = -1.0f) const;
			[[nodiscard]] bool any_item_active() const;
			[[nodiscard]] imgui_compat::viewport_state main_viewport() const;
			void dockspace_over_viewport() const;
			void set_next_window_position(glm::vec2 position,
			                             imgui_compat::condition condition = imgui_compat::condition::always,
			                             glm::vec2 pivot = {}) const;
			void set_next_window_size(glm::vec2 size,
			                         imgui_compat::condition condition = imgui_compat::condition::always) const;
			void focus_next_window() const;
			void open_popup(std::string_view id) const;
			[[nodiscard]] bool popup_open(std::string_view id) const;

		private:
			friend class imgui_context;
			explicit frame_scope(imgui_context& owner) noexcept : owner(&owner) {}

			imgui_context* owner;
		};

		class id_scope
		{
		public:
			id_scope() noexcept = default;
			id_scope(const id_scope&) = delete;
			id_scope& operator=(const id_scope&) = delete;
			id_scope(id_scope&&) = delete;
			id_scope& operator=(id_scope&&) = delete;
			~id_scope() noexcept;

		private:
			friend class window_scope;
			id_scope(imgui_context& owner, bool pushed) noexcept : owner(&owner), pushed(pushed) {}
			imgui_context* owner = nullptr;
			bool pushed = false;
		};

		class style_color_scope
		{
		public:
			style_color_scope() noexcept = default;
			style_color_scope(const style_color_scope&) = delete;
			style_color_scope& operator=(const style_color_scope&) = delete;
			style_color_scope(style_color_scope&&) = delete;
			style_color_scope& operator=(style_color_scope&&) = delete;
			~style_color_scope() noexcept;

		private:
			friend class window_scope;
			style_color_scope(imgui_context& owner, bool pushed) noexcept : owner(&owner), pushed(pushed) {}
			imgui_context* owner = nullptr;
			bool pushed = false;
		};

		class canvas_scope
		{
		public:
			canvas_scope() noexcept = default;
			canvas_scope(const canvas_scope&) = delete;
			canvas_scope& operator=(const canvas_scope&) = delete;
			canvas_scope(canvas_scope&&) = delete;
			canvas_scope& operator=(canvas_scope&&) = delete;
			~canvas_scope() noexcept;

			[[nodiscard]] explicit operator bool() const noexcept { return owner != nullptr; }
			[[nodiscard]] glm::vec2 position() const noexcept { return origin; }
			[[nodiscard]] glm::vec2 size() const noexcept { return extent; }
			[[nodiscard]] bool hovered() const noexcept { return item.hovered; }
			[[nodiscard]] bool active() const noexcept { return item.active; }
			[[nodiscard]] bool clicked(mouse_button button) const noexcept;
			[[nodiscard]] bool double_clicked(mouse_button button) const;
			[[nodiscard]] bool dragging(mouse_button button, float threshold = -1.0f) const;
			[[nodiscard]] std::optional<glm::vec2> mouse_position() const noexcept;
			[[nodiscard]] glm::vec2 measure_text(std::string_view text) const;
			void fill_rectangle(const ::mngl::rectangle& bounds, const ::mngl::color& tint,
			                    float rounding = 0.0f) const;
			void draw_rectangle(const ::mngl::rectangle& bounds, const ::mngl::color& tint,
			                    float thickness = 1.0f, float rounding = 0.0f) const;
			void draw_line(glm::vec2 start, glm::vec2 end, const ::mngl::color& tint,
			               float thickness = 1.0f) const;
			void text(glm::vec2 position, std::string_view value, const ::mngl::color& tint) const;
			void image(const ::mngl::texture& source, const ::mngl::rectangle& bounds,
			           const ::mngl::color& tint = ::mngl::color(1.0f, 1.0f, 1.0f, 1.0f),
			           glm::vec2 uv_min = {0.0f, 0.0f}, glm::vec2 uv_max = {1.0f, 1.0f}) const;

		private:
			friend class window_scope;
			canvas_scope(imgui_context& owner, glm::vec2 origin, glm::vec2 extent,
			             void* draw_list, imgui_compat::item_state item,
			             glm::vec2 mouse_position, bool has_mouse_position) noexcept;
			imgui_context* owner = nullptr;
			glm::vec2 origin{};
			glm::vec2 extent{};
			void* draw_list = nullptr;
			imgui_compat::item_state item{};
			glm::vec2 mouse{};
			bool has_mouse = false;
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
