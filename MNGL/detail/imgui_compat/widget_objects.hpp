#pragma once

#include "advanced_widgets.hpp"

#include <concepts>
#include <cstddef>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mngl::imgui_compat::widgets
{
	using drawing_context = ::mngl::imgui_context::window_scope;
	using frame_context = ::mngl::imgui_context::frame_scope;
	using dockspace_context = ::mngl::imgui_context::dockspace_scope;
	using menu_bar_context = ::mngl::imgui_context::main_menu_bar_scope;
	using menu_context = ::mngl::imgui_context::menu_scope;

	namespace widget_detail
	{
		template <typename Begin, typename Draw>
		[[nodiscard]] bool draw_scope(Begin&& begin, Draw&& draw)
		{
			auto scope = std::forward<Begin>(begin)();
			if (!scope) return false;
			std::forward<Draw>(draw)(scope);
			return true;
		}
	} // namespace widget_detail

	// Retained MNGL-side state for a control. Dear ImGui still builds the frame,
	// while these objects keep labels, values, and settings between frames.
	// Shared state only; controls are intentionally not dispatched through a
	// virtual base, so applications can compose them without a retained UI tree.
	class widget_base
	{
	public:
		explicit widget_base(std::string_view label) : m_label(detail::widget_label(label)) {}

		[[nodiscard]] std::string_view label() const noexcept { return m_label; }
		void set_label(std::string_view value) { m_label = detail::widget_label(value); }

		[[nodiscard]] bool enabled() const noexcept { return m_enabled; }
		void set_enabled(bool value) noexcept { m_enabled = value; }

	protected:
		template <typename Draw>
		decltype(auto) with_enabled(const drawing_context& target, Draw&& draw) const
		{
			if (m_enabled)
			{
				return std::forward<Draw>(draw)();
			}

			auto disabled_scope = target.disabled();
			return std::forward<Draw>(draw)();
		}

	private:
		std::string m_label;
		bool m_enabled = true;
	};

	class text final : public widget_base
	{
	public:
		explicit text(std::string_view value) : widget_base(value) {}

		void set_text(std::string_view value) { set_label(value); }
		[[nodiscard]] bool wrapped() const noexcept { return m_wrapped; }
		void set_wrapped(bool value) noexcept { m_wrapped = value; }
		void set_color(const ::mngl::color& value) { m_color = value; }
		void clear_color() noexcept { m_color.reset(); }

		void draw(const drawing_context& target) const
		{
			if (!enabled())
			{
				target.text_disabled(label());
			}
			else if (m_color)
			{
				target.text_colored(*m_color, label());
			}
			else if (m_wrapped)
			{
				target.text_wrapped(label());
			}
			else
			{
				target.text(label());
			}
		}

	private:
		bool m_wrapped = false;
		std::optional<::mngl::color> m_color;
	};

	class button final : public widget_base
	{
	public:
		using widget_base::widget_base;

		[[nodiscard]] bool small() const noexcept { return m_small; }
		void set_small(bool value) noexcept { m_small = value; }
		[[nodiscard]] bool pressed() const noexcept { return m_pressed; }
		[[nodiscard]] explicit operator bool() const noexcept { return m_pressed; }

		[[nodiscard]] bool draw(const drawing_context& target)
		{
			m_pressed = with_enabled(target, [&]
			                         { return m_small ? target.small_button(label()) : target.button(label()); });
			return m_pressed;
		}

	private:
		bool m_small = false;
		bool m_pressed = false;
	};

	class checkbox final : public widget_base
	{
	public:
		explicit checkbox(std::string_view label, bool value = false) : widget_base(label), m_value(value) {}

		[[nodiscard]] bool value() const noexcept { return m_value; }
		void set_value(bool value) noexcept { m_value = value; }

		[[nodiscard]] bool draw(const drawing_context& target)
		{
			return with_enabled(target, [&] { return target.checkbox(label(), m_value); });
		}

	private:
		bool m_value = false;
	};

	class selectable final : public widget_base
	{
	public:
		explicit selectable(std::string_view label, bool selected = false)
		    : widget_base(label), m_selected(selected)
		{
		}

		[[nodiscard]] bool selected() const noexcept { return m_selected; }
		void set_selected(bool value) noexcept { m_selected = value; }

		[[nodiscard]] bool draw(const drawing_context& target)
		{
			const bool activated = with_enabled(target, [&] { return target.selectable(label(), m_selected); });
			if (activated) m_selected = !m_selected;
			return activated;
		}

	private:
		bool m_selected = false;
	};

	class bullet final : public widget_base
	{
	public:
		using widget_base::widget_base;

		void draw(const drawing_context& target) const
		{
			with_enabled(target, [&] { target.bullet(label()); });
		}
	};

	template <typename T>
	    requires std::copy_constructible<T> && std::equality_comparable<T> && std::assignable_from<T&, T>
	class radio_button final : public widget_base
	{
	public:
		radio_button(std::string_view label, T option) : widget_base(label), m_option(std::move(option)) {}

		[[nodiscard]] const T& option() const noexcept { return m_option; }

		[[nodiscard]] bool draw(const drawing_context& target, T& selected) const
		{
			return with_enabled(target,
			                    [&] { return target.radio_button(label(), selected, m_option); });
		}

	private:
		T m_option;
	};

	template <typename T>
	    requires detail::combo_range<std::vector<T>>
	class combo final : public widget_base
	{
	public:
		explicit combo(std::string_view label, std::vector<T> options = {})
		    : widget_base(label)
		{
			set_options(std::move(options));
		}

		[[nodiscard]] const std::vector<T>& options() const noexcept { return m_options; }
		[[nodiscard]] std::optional<std::size_t> selected_index() const noexcept { return m_selected; }
		[[nodiscard]] const T* selected() const noexcept
		{
			return m_selected ? &m_options[*m_selected] : nullptr;
		}

		void set_options(std::vector<T> options)
		{
			std::vector<std::string> labels;
			labels.reserve(options.size());
			for (const auto& option : options)
			{
				labels.push_back(detail::widget_label(detail::combo_item_to_string(option)));
			}
			m_options = std::move(options);
			m_labels = std::move(labels);
			m_selected.reset();
		}

		void add_option(T option)
		{
			std::string label = detail::widget_label(detail::combo_item_to_string(option));
			m_options.push_back(std::move(option));
			try
			{
				m_labels.push_back(std::move(label));
			}
			catch (...)
			{
				m_options.pop_back();
				throw;
			}
		}

		void set_selected(std::optional<std::size_t> index)
		{
			if (index && *index >= m_options.size())
			{
				throw std::out_of_range("MNGL combo selected index is outside its options");
			}
			m_selected = index;
		}

		void clear_selection() noexcept { m_selected.reset(); }

		[[nodiscard]] bool draw(const drawing_context& target)
		{
			return with_enabled(target, [&]
			{
				const std::string_view preview = m_selected ? std::string_view(m_labels[*m_selected])
				                                            : (m_options.empty() ? "(no options)" : "(none)");
				auto contents = target.begin_combo(label(), preview);
				if (!contents) return false;

				bool changed = false;
				if (m_options.empty())
				{
					contents.text_disabled("(no options)");
					return false;
				}

				{
					auto id = contents.push_id(-1);
					if (contents.selectable("(none)", !m_selected))
					{
						changed = m_selected.has_value();
						m_selected.reset();
					}
					if (!m_selected) contents.focus_last_item();
				}

				for (std::size_t index = 0; index < m_options.size(); ++index)
				{
					const bool is_selected = m_selected == index;
					auto id = contents.push_id(static_cast<int>(index));
					if (contents.selectable(m_labels[index], is_selected) && !is_selected)
					{
						m_selected = index;
						changed = true;
					}
					if (is_selected) contents.focus_last_item();
				}
				return changed;
			});
		}

	private:
		std::vector<T> m_options;
		std::vector<std::string> m_labels;
		std::optional<std::size_t> m_selected;
	};

	class context_menu final : public widget_base
	{
	public:
		explicit context_menu(std::string_view id, std::vector<std::string> options = {},
		                      mouse_button button = mouse_button::right)
		    : widget_base(id)
		{
			set_options(std::move(options));
			set_mouse_button(button);
		}

		[[nodiscard]] const std::vector<std::string>& options() const noexcept { return m_options; }
		void set_options(std::vector<std::string> options)
		{
			for (auto& option : options)
			{
				option = detail::widget_label(option);
			}
			m_options = std::move(options);
		}
		void add_option(std::string_view option) { m_options.push_back(detail::widget_label(option)); }
		void set_mouse_button(mouse_button value)
		{
			(void)detail::to_imgui_popup_mouse_button(value);
			m_button = value;
		}
		[[nodiscard]] mouse_button button() const noexcept { return m_button; }

		[[nodiscard]] std::optional<std::size_t> draw(const drawing_context& target) const
		{
			return with_enabled(target, [&]() -> std::optional<std::size_t>
			{
				auto contents = target.context_popup(label(), m_button);
				if (!contents) return std::nullopt;
				for (std::size_t index = 0; index < m_options.size(); ++index)
				{
					auto id = contents.push_id(static_cast<int>(index));
					if (contents.selectable(m_options[index])) return index;
				}
				return std::nullopt;
			});
		}

	private:
		std::vector<std::string> m_options;
		mouse_button m_button = mouse_button::right;
	};

	// Custom combo contents for cases where a fixed option list is not enough.
	// The callback receives an active window scope only while the combo is open.
	class combo_popup final : public widget_base
	{
	public:
		explicit combo_popup(std::string_view label, std::string_view preview = {})
		    : widget_base(label), m_preview(detail::widget_label(preview))
		{
		}

		[[nodiscard]] std::string_view preview() const noexcept { return m_preview; }
		void set_preview(std::string_view value) { m_preview = detail::widget_label(value); }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return parent.begin_combo(label(), m_preview); }, std::forward<Draw>(body));
		}

	private:
		std::string m_preview;
	};

	class image final
	{
	public:
		explicit image(glm::vec2 display_size) : m_display_size(display_size) {}

		[[nodiscard]] glm::vec2 display_size() const noexcept { return m_display_size; }
		void set_display_size(glm::vec2 value) noexcept { m_display_size = value; }
		[[nodiscard]] bool enabled() const noexcept { return m_enabled; }
		void set_enabled(bool value) noexcept { m_enabled = value; }

		void draw(const drawing_context& target, const ::mngl::texture& source) const
		{
			if (m_enabled) target.image(source, m_display_size);
		}

	private:
		glm::vec2 m_display_size{};
		bool m_enabled = true;
	};

	class text_input final : public widget_base
	{
	public:
		explicit text_input(std::string_view label, std::string_view value = {}, std::string_view hint = {})
		    : widget_base(label), m_value(value), m_hint(hint)
		{
		}

		[[nodiscard]] const std::string& value() const noexcept { return m_value; }
		[[nodiscard]] std::string& value() noexcept { return m_value; }
		void set_value(std::string value) { m_value = std::move(value); }
		void set_hint(std::string_view value) { m_hint = detail::widget_label(value); }
		void set_pattern(std::regex value)
		{
			if (m_multiline_size)
			{
				throw std::logic_error("Regex validation is currently available for single-line MNGL text input");
			}
			m_pattern = std::move(value);
		}
		void clear_pattern() noexcept { m_pattern.reset(); }
		[[nodiscard]] bool multiline() const noexcept { return m_multiline_size.has_value(); }
		void set_multiline_size(glm::vec2 size)
		{
			if (m_pattern)
			{
				throw std::logic_error("Clear the input regex before enabling multiline text input");
			}
			m_multiline_size = size;
		}
		void clear_multiline() noexcept { m_multiline_size.reset(); }

		[[nodiscard]] bool draw(const drawing_context& target)
		{
			return with_enabled(target, [&]
			{
				if (m_multiline_size)
				{
					return target.input_text_multiline(label(), m_value, *m_multiline_size);
				}
				if (m_pattern)
				{
					return target.input(label(), m_value, *m_pattern);
				}
				return target.input_text(label(), m_value, m_hint);
			});
		}

	private:
		std::string m_value;
		std::string m_hint;
		std::optional<std::regex> m_pattern;
		std::optional<glm::vec2> m_multiline_size;
	};

	template <detail::scalar_input_value T>
	class numeric_input final : public widget_base
	{
	public:
		explicit numeric_input(std::string_view label, T value = {}) : widget_base(label), m_value(value) {}

		[[nodiscard]] const T& value() const noexcept { return m_value; }
		[[nodiscard]] T& value() noexcept { return m_value; }
		void set_value(T value) noexcept { m_value = value; }

		[[nodiscard]] bool draw(const drawing_context& target)
		{
			return with_enabled(target, [&] { return target.input(label(), m_value); });
		}

	private:
		T m_value{};
	};

	namespace widget_detail
	{
		template <typename Value>
		struct slider_traits
		{
			using limit_type = Value;
		};

		template <detail::glm_slider_vector Vector>
		struct slider_traits<Vector>
		{
			using limit_type = typename detail::glm_slider_vector_traits<Vector>::scalar_type;
		};
	} // namespace widget_detail

	template <typename Value>
	concept slider_value = detail::scalar_input_value<Value> || detail::glm_slider_vector<Value>;

	template <slider_value Value>
	class slider final : public widget_base
	{
	public:
		using limit_type = typename widget_detail::slider_traits<Value>::limit_type;

		slider(std::string_view label, Value value, limit_type minimum, limit_type maximum)
		    : widget_base(label), m_value(value), m_minimum(minimum), m_maximum(maximum)
		{
		}

		[[nodiscard]] const Value& value() const noexcept { return m_value; }
		[[nodiscard]] Value& value() noexcept { return m_value; }
		void set_value(Value value) noexcept { m_value = value; }
		void set_range(limit_type minimum, limit_type maximum) noexcept
		{
			m_minimum = minimum;
			m_maximum = maximum;
		}

		[[nodiscard]] bool draw(const drawing_context& target)
		{
			return with_enabled(target,
			                    [&] { return target.slider(label(), m_value, m_minimum, m_maximum); });
		}

	private:
		Value m_value{};
		limit_type m_minimum{};
		limit_type m_maximum{};
	};

	class progress_bar final : public widget_base
	{
	public:
		explicit progress_bar(float fraction = 0.0f, std::string_view overlay = {})
		    : widget_base("##progress"), m_fraction(fraction), m_overlay(overlay)
		{
		}

		[[nodiscard]] float fraction() const noexcept { return m_fraction; }
		void set_fraction(float value) noexcept { m_fraction = value; }
		void set_overlay(std::string_view value) { m_overlay = detail::widget_label(value); }

		void draw(const drawing_context& target) const
		{
			with_enabled(target, [&]
			{
				if (m_overlay.empty())
				{
					target.progress_bar(m_fraction);
				}
				else
				{
					target.progress_bar(m_fraction, m_overlay);
				}
			});
		}

	private:
		float m_fraction = 0.0f;
		std::string m_overlay;
	};

	class window final : public widget_base
	{
	public:
		explicit window(std::string_view title, imgui_compat::window_options options = {})
		    : widget_base(title), m_options(options)
		{
		}

		[[nodiscard]] bool is_open() const noexcept { return m_open; }
		void set_open(bool value) noexcept { m_open = value; }
		[[nodiscard]] const imgui_compat::window_options& options() const noexcept { return m_options; }
		void set_options(imgui_compat::window_options value) noexcept { m_options = value; }

		template <typename Draw>
		[[nodiscard]] bool draw(const frame_context& frame, Draw&& body)
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return frame.window(label(), m_open, m_options); }, std::forward<Draw>(body));
		}

		template <typename Draw>
		[[nodiscard]] bool draw(const dockspace_context& dockspace, Draw&& body)
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return dockspace.window(label(), m_open, m_options); }, std::forward<Draw>(body));
		}

	private:
		imgui_compat::window_options m_options{};
		bool m_open = true;
	};

	class child final : public widget_base
	{
	public:
		explicit child(std::string_view id, imgui_compat::child_options options = {})
		    : widget_base(id), m_options(options)
		{
		}

		[[nodiscard]] const imgui_compat::child_options& options() const noexcept { return m_options; }
		void set_options(imgui_compat::child_options value) noexcept { m_options = value; }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return parent.child(label(), m_options); }, std::forward<Draw>(body));
		}

	private:
		imgui_compat::child_options m_options{};
	};

	class popup final : public widget_base
	{
	public:
		explicit popup(std::string_view id, imgui_compat::window_options options = {})
		    : widget_base(id), m_options(options)
		{
		}

		void open(const drawing_context& parent) const { parent.open_popup(label()); }
		void open(const frame_context& frame) const { frame.open_popup(label()); }
		[[nodiscard]] bool is_open(const drawing_context& parent) const { return parent.popup_open(label()); }
		[[nodiscard]] bool is_open(const frame_context& frame) const { return frame.popup_open(label()); }
		[[nodiscard]] const imgui_compat::window_options& options() const noexcept { return m_options; }
		void set_options(imgui_compat::window_options value) noexcept { m_options = value; }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return parent.popup(label(), m_options); }, std::forward<Draw>(body));
		}

	private:
		imgui_compat::window_options m_options{};
	};

	class context_popup final : public widget_base
	{
	public:
		explicit context_popup(std::string_view id = {}, mouse_button button = mouse_button::right)
		    : widget_base(id), m_button(button)
		{
			(void)detail::to_imgui_popup_mouse_button(button);
		}

		void set_mouse_button(mouse_button value)
		{
			(void)detail::to_imgui_popup_mouse_button(value);
			m_button = value;
		}

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return parent.context_popup(label(), m_button); }, std::forward<Draw>(body));
		}

	private:
		mouse_button m_button;
	};

	class modal final : public widget_base
	{
	public:
		explicit modal(std::string_view id, imgui_compat::window_options options = {})
		    : widget_base(id), m_options(options)
		{
		}

		[[nodiscard]] bool is_open() const noexcept { return m_open; }
		void set_open(bool value) noexcept { m_open = value; }
		[[nodiscard]] const imgui_compat::window_options& options() const noexcept { return m_options; }
		void set_options(imgui_compat::window_options value) noexcept { m_options = value; }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body)
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return parent.modal(label(), m_open, m_options); }, std::forward<Draw>(body));
		}

	private:
		imgui_compat::window_options m_options{};
		bool m_open = false;
	};

	class tab_bar final : public widget_base
	{
	public:
		explicit tab_bar(std::string_view id, imgui_compat::tab_bar_options options = {})
		    : widget_base(id), m_options(options)
		{
		}

		[[nodiscard]] const imgui_compat::tab_bar_options& options() const noexcept { return m_options; }
		void set_options(imgui_compat::tab_bar_options value) noexcept { m_options = value; }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return parent.tab_bar(label(), m_options); }, std::forward<Draw>(body));
		}

	private:
		imgui_compat::tab_bar_options m_options{};
	};

	class tab final : public widget_base
	{
	public:
		explicit tab(std::string_view label, imgui_compat::tab_item_options options = {})
		    : widget_base(label), m_options(options)
		{
		}

		[[nodiscard]] bool is_open() const noexcept { return m_open; }
		void set_open(bool value) noexcept { m_open = value; }
		[[nodiscard]] const imgui_compat::tab_item_options& options() const noexcept { return m_options; }
		void set_options(imgui_compat::tab_item_options value) noexcept { m_options = value; }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& bar, Draw&& body)
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return bar.tab(label(), &m_open, m_options); }, std::forward<Draw>(body));
		}

	private:
		imgui_compat::tab_item_options m_options{};
		bool m_open = true;
	};

	class table final : public widget_base
	{
	public:
		table(std::string_view id, int columns, imgui_compat::table_options options = {})
		    : widget_base(id), m_columns(columns), m_options(options)
		{
		}

		[[nodiscard]] int columns() const noexcept { return m_columns; }
		void set_columns(int value) noexcept { m_columns = value; }
		[[nodiscard]] const imgui_compat::table_options& options() const noexcept { return m_options; }
		void set_options(imgui_compat::table_options value) noexcept { m_options = value; }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return parent.table(label(), m_columns, m_options); }, std::forward<Draw>(body));
		}

	private:
		int m_columns;
		imgui_compat::table_options m_options{};
	};

	class tooltip final : public widget_base
	{
	public:
		tooltip() : widget_base("##tooltip") {}

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope([&] { return parent.tooltip(); }, std::forward<Draw>(body));
		}
	};

	class disabled final : public widget_base
	{
	public:
		explicit disabled(bool state = true) : widget_base("##disabled"), m_disabled(state) {}

		[[nodiscard]] bool value() const noexcept { return m_disabled; }
		void set_value(bool value) noexcept { m_disabled = value; }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return parent.disabled(m_disabled); }, std::forward<Draw>(body));
		}

	private:
		bool m_disabled = true;
	};

	class tree_node final : public widget_base
	{
	public:
		explicit tree_node(std::string_view label, imgui_compat::tree_node_options options = {})
		    : widget_base(label), m_options(options)
		{
		}

		void set_options(imgui_compat::tree_node_options value) noexcept { m_options = value; }
		[[nodiscard]] const imgui_compat::tree_node_options& options() const noexcept { return m_options; }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return parent.tree_node(label(), m_options); }, std::forward<Draw>(body));
		}

	private:
		imgui_compat::tree_node_options m_options{};
	};

	class collapsing_header final : public widget_base
	{
	public:
		explicit collapsing_header(std::string_view label, bool default_open = false)
		    : widget_base(label), m_default_open(default_open)
		{
		}

		[[nodiscard]] bool is_open() const noexcept { return m_open; }
		[[nodiscard]] bool default_open() const noexcept { return m_default_open; }
		void set_default_open(bool value) noexcept { m_default_open = value; }

		[[nodiscard]] bool draw(const drawing_context& parent)
		{
			m_open = with_enabled(parent, [&] { return parent.collapsing_header(label(), m_default_open); });
			return m_open;
		}

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body)
		{
			if (!draw(parent)) return false;
			std::forward<Draw>(body)(parent);
			return true;
		}

	private:
		bool m_default_open = false;
		bool m_open = false;
	};

	class dockspace final : public widget_base
	{
	public:
		explicit dockspace(std::string_view id) : widget_base(id) {}

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			auto scope = parent.dockspace(label());
			std::forward<Draw>(body)(scope);
			return true;
		}
	};

	class main_menu_bar final : public widget_base
	{
	public:
		main_menu_bar() : widget_base("##main-menu-bar") {}

		template <typename Draw>
		[[nodiscard]] bool draw(const frame_context& frame, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope([&] { return frame.main_menu_bar(); }, std::forward<Draw>(body));
		}
	};

	class menu final : public widget_base
	{
	public:
		using widget_base::widget_base;

		template <typename Draw>
		[[nodiscard]] bool draw(const menu_bar_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope([&] { return parent.menu(label()); }, std::forward<Draw>(body));
		}

		template <typename Draw>
		[[nodiscard]] bool draw(const menu_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope([&] { return parent.menu(label()); }, std::forward<Draw>(body));
		}
	};

	class menu_item final : public widget_base
	{
	public:
		explicit menu_item(std::string_view label, std::string_view shortcut = {})
		    : widget_base(label), m_shortcut(detail::widget_label(shortcut))
		{
		}

		[[nodiscard]] bool selected() const noexcept { return m_selected; }
		void set_selected(bool value) noexcept { m_selected = value; }
		void set_shortcut(std::string_view value) { m_shortcut = detail::widget_label(value); }

		[[nodiscard]] bool draw(const menu_context& parent)
		{
			return parent.menu_item(label(), m_selected, m_shortcut, enabled());
		}
		[[nodiscard]] bool draw(const menu_bar_context& parent)
		{
			return parent.menu_item(label(), m_selected, m_shortcut, enabled());
		}

	private:
		std::string m_shortcut;
		bool m_selected = false;
	};

	class font final
	{
	public:
		explicit font(imgui_compat::font_role role = imgui_compat::font_role::regular,
		              float size_pixels = 0.0f) noexcept
		    : m_role(role), m_size_pixels(size_pixels)
		{
		}

		[[nodiscard]] imgui_compat::font_role role() const noexcept { return m_role; }
		[[nodiscard]] float size() const noexcept { return m_size_pixels; }
		void set_role(imgui_compat::font_role value) noexcept { m_role = value; }
		void set_size(float value) noexcept { m_size_pixels = value; }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			auto scope = parent.use_font(m_role, m_size_pixels);
			std::forward<Draw>(body)(parent);
			return true;
		}

	private:
		imgui_compat::font_role m_role;
		float m_size_pixels;
	};

	class style_color final : public widget_base
	{
	public:
		style_color(imgui_compat::color_role role, const ::mngl::color& value)
		    : widget_base("##style-color"), m_role(role), m_value(value)
		{
		}

		void set_color(const ::mngl::color& value) { m_value = value; }
		[[nodiscard]] imgui_compat::color_role role() const noexcept { return m_role; }
		[[nodiscard]] const ::mngl::color& color() const noexcept { return m_value; }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			auto scope = parent.push_style_color(m_role, m_value);
			std::forward<Draw>(body)(parent);
			return true;
		}

	private:
		imgui_compat::color_role m_role;
		::mngl::color m_value;
	};

	class id_block final : public widget_base
	{
	public:
		explicit id_block(std::string_view id) : widget_base(id) {}
		explicit id_block(int id) : widget_base("##numeric-id"), m_numeric_id(id) {}

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			if (m_numeric_id)
			{
				auto scope = parent.push_id(*m_numeric_id);
				std::forward<Draw>(body)(parent);
			}
			else
			{
				auto scope = parent.push_id(label());
				std::forward<Draw>(body)(parent);
			}
			return true;
		}

	private:
		std::optional<int> m_numeric_id;
	};

	class canvas final : public widget_base
	{
	public:
		canvas(std::string_view id, glm::vec2 size, imgui_compat::canvas_options options = {})
		    : widget_base(id), m_size(size), m_options(options)
		{
		}

		[[nodiscard]] glm::vec2 size() const noexcept { return m_size; }
		void set_size(glm::vec2 value) noexcept { m_size = value; }
		[[nodiscard]] const imgui_compat::canvas_options& options() const noexcept { return m_options; }
		void set_options(imgui_compat::canvas_options value) noexcept { m_options = value; }

		template <typename Draw>
		[[nodiscard]] bool draw(const drawing_context& parent, Draw&& body) const
		{
			if (!enabled()) return false;
			return widget_detail::draw_scope(
			    [&] { return parent.canvas(label(), m_size, m_options); }, std::forward<Draw>(body));
		}

	private:
		glm::vec2 m_size{};
		imgui_compat::canvas_options m_options{};
	};
} // namespace mngl::imgui_compat::widgets
