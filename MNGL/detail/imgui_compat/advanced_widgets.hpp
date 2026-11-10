#pragma once

#include "widgets.hpp"

namespace mngl::imgui_compat::detail
{
	[[nodiscard]] inline ImGuiKey to_imgui_key(key value)
	{
		switch (value)
		{
		case key::tab: return ImGuiKey_Tab;
		case key::left: return ImGuiKey_LeftArrow;
		case key::right: return ImGuiKey_RightArrow;
		case key::up: return ImGuiKey_UpArrow;
		case key::down: return ImGuiKey_DownArrow;
		case key::page_up: return ImGuiKey_PageUp;
		case key::page_down: return ImGuiKey_PageDown;
		case key::home: return ImGuiKey_Home;
		case key::end: return ImGuiKey_End;
		case key::insert: return ImGuiKey_Insert;
		case key::delete_key: return ImGuiKey_Delete;
		case key::backspace: return ImGuiKey_Backspace;
		case key::space: return ImGuiKey_Space;
		case key::enter: return ImGuiKey_Enter;
		case key::escape: return ImGuiKey_Escape;
		case key::left_control: return ImGuiKey_LeftCtrl;
		case key::left_shift: return ImGuiKey_LeftShift;
		case key::left_alt: return ImGuiKey_LeftAlt;
		case key::left_super: return ImGuiKey_LeftSuper;
		case key::right_control: return ImGuiKey_RightCtrl;
		case key::right_shift: return ImGuiKey_RightShift;
		case key::right_alt: return ImGuiKey_RightAlt;
		case key::right_super: return ImGuiKey_RightSuper;
		case key::menu: return ImGuiKey_Menu;
		case key::digit_0: return ImGuiKey_0;
		case key::digit_1: return ImGuiKey_1;
		case key::digit_2: return ImGuiKey_2;
		case key::digit_3: return ImGuiKey_3;
		case key::digit_4: return ImGuiKey_4;
		case key::digit_5: return ImGuiKey_5;
		case key::digit_6: return ImGuiKey_6;
		case key::digit_7: return ImGuiKey_7;
		case key::digit_8: return ImGuiKey_8;
		case key::digit_9: return ImGuiKey_9;
		case key::a: return ImGuiKey_A;
		case key::b: return ImGuiKey_B;
		case key::c: return ImGuiKey_C;
		case key::d: return ImGuiKey_D;
		case key::e: return ImGuiKey_E;
		case key::f: return ImGuiKey_F;
		case key::g: return ImGuiKey_G;
		case key::h: return ImGuiKey_H;
		case key::i: return ImGuiKey_I;
		case key::j: return ImGuiKey_J;
		case key::k: return ImGuiKey_K;
		case key::l: return ImGuiKey_L;
		case key::m: return ImGuiKey_M;
		case key::n: return ImGuiKey_N;
		case key::o: return ImGuiKey_O;
		case key::p: return ImGuiKey_P;
		case key::q: return ImGuiKey_Q;
		case key::r: return ImGuiKey_R;
		case key::s: return ImGuiKey_S;
		case key::t: return ImGuiKey_T;
		case key::u: return ImGuiKey_U;
		case key::v: return ImGuiKey_V;
		case key::w: return ImGuiKey_W;
		case key::x: return ImGuiKey_X;
		case key::y: return ImGuiKey_Y;
		case key::z: return ImGuiKey_Z;
		case key::f1: return ImGuiKey_F1;
		case key::f2: return ImGuiKey_F2;
		case key::f3: return ImGuiKey_F3;
		case key::f4: return ImGuiKey_F4;
		case key::f5: return ImGuiKey_F5;
		case key::f6: return ImGuiKey_F6;
		case key::f7: return ImGuiKey_F7;
		case key::f8: return ImGuiKey_F8;
		case key::f9: return ImGuiKey_F9;
		case key::f10: return ImGuiKey_F10;
		case key::f11: return ImGuiKey_F11;
		case key::f12: return ImGuiKey_F12;
		case key::f13: return ImGuiKey_F13;
		case key::f14: return ImGuiKey_F14;
		case key::f15: return ImGuiKey_F15;
		case key::f16: return ImGuiKey_F16;
		case key::f17: return ImGuiKey_F17;
		case key::f18: return ImGuiKey_F18;
		case key::f19: return ImGuiKey_F19;
		case key::f20: return ImGuiKey_F20;
		case key::f21: return ImGuiKey_F21;
		case key::f22: return ImGuiKey_F22;
		case key::f23: return ImGuiKey_F23;
		case key::f24: return ImGuiKey_F24;
		case key::apostrophe: return ImGuiKey_Apostrophe;
		case key::comma: return ImGuiKey_Comma;
		case key::minus: return ImGuiKey_Minus;
		case key::period: return ImGuiKey_Period;
		case key::slash: return ImGuiKey_Slash;
		case key::semicolon: return ImGuiKey_Semicolon;
		case key::equal: return ImGuiKey_Equal;
		case key::left_bracket: return ImGuiKey_LeftBracket;
		case key::backslash: return ImGuiKey_Backslash;
		case key::right_bracket: return ImGuiKey_RightBracket;
		case key::grave_accent: return ImGuiKey_GraveAccent;
		case key::caps_lock: return ImGuiKey_CapsLock;
		case key::scroll_lock: return ImGuiKey_ScrollLock;
		case key::num_lock: return ImGuiKey_NumLock;
		case key::print_screen: return ImGuiKey_PrintScreen;
		case key::pause: return ImGuiKey_Pause;
		case key::keypad_0: return ImGuiKey_Keypad0;
		case key::keypad_1: return ImGuiKey_Keypad1;
		case key::keypad_2: return ImGuiKey_Keypad2;
		case key::keypad_3: return ImGuiKey_Keypad3;
		case key::keypad_4: return ImGuiKey_Keypad4;
		case key::keypad_5: return ImGuiKey_Keypad5;
		case key::keypad_6: return ImGuiKey_Keypad6;
		case key::keypad_7: return ImGuiKey_Keypad7;
		case key::keypad_8: return ImGuiKey_Keypad8;
		case key::keypad_9: return ImGuiKey_Keypad9;
		case key::keypad_decimal: return ImGuiKey_KeypadDecimal;
		case key::keypad_divide: return ImGuiKey_KeypadDivide;
		case key::keypad_multiply: return ImGuiKey_KeypadMultiply;
		case key::keypad_subtract: return ImGuiKey_KeypadSubtract;
		case key::keypad_add: return ImGuiKey_KeypadAdd;
		case key::keypad_enter: return ImGuiKey_KeypadEnter;
		case key::keypad_equal: return ImGuiKey_KeypadEqual;
		case key::app_back: return ImGuiKey_AppBack;
		case key::app_forward: return ImGuiKey_AppForward;
		}
		throw std::invalid_argument("Unknown MNGL ImGui key");
	}

	[[nodiscard]] inline ImGuiMouseButton to_imgui_mouse_button(mouse_button value)
	{
		switch (value)
		{
		case mouse_button::left: return ImGuiMouseButton_Left;
		case mouse_button::middle: return ImGuiMouseButton_Middle;
		case mouse_button::right: return ImGuiMouseButton_Right;
		case mouse_button::x1: return ImGuiMouseButton(3);
		case mouse_button::x2: return ImGuiMouseButton(4);
		}
		throw std::invalid_argument("Unknown MNGL mouse button");
	}

	[[nodiscard]] inline ImGuiChildFlags to_imgui_child_flags(const child_options& options) noexcept
	{
		ImGuiChildFlags result = ImGuiChildFlags_None;
		if (options.border) result |= ImGuiChildFlags_Borders;
		if (options.resize_x) result |= ImGuiChildFlags_ResizeX;
		if (options.resize_y) result |= ImGuiChildFlags_ResizeY;
		if (options.auto_resize_x || options.always_auto_resize) result |= ImGuiChildFlags_AutoResizeX;
		if (options.auto_resize_y || options.always_auto_resize) result |= ImGuiChildFlags_AutoResizeY;
		if (options.always_auto_resize) result |= ImGuiChildFlags_AlwaysAutoResize;
		if (options.frame_style) result |= ImGuiChildFlags_FrameStyle;
		return result;
	}

	[[nodiscard]] inline ImGuiWindowFlags child_window_flags(const child_options& options) noexcept
	{
		ImGuiWindowFlags result = ImGuiWindowFlags_None;
		if (options.no_scrollbar) result |= ImGuiWindowFlags_NoScrollbar;
		if (options.no_scroll_with_mouse) result |= ImGuiWindowFlags_NoScrollWithMouse;
		return result;
	}

	[[nodiscard]] inline ImGuiTabBarFlags to_imgui_tab_bar_flags(const tab_bar_options& options) noexcept
	{
		ImGuiTabBarFlags result = ImGuiTabBarFlags_None;
		if (options.reorderable) result |= ImGuiTabBarFlags_Reorderable;
		if (options.auto_select_new_tabs) result |= ImGuiTabBarFlags_AutoSelectNewTabs;
		return result;
	}

	[[nodiscard]] inline ImGuiTabItemFlags to_imgui_tab_item_flags(const tab_item_options& options) noexcept
	{
		ImGuiTabItemFlags result = ImGuiTabItemFlags_None;
		if (options.unsaved_document) result |= ImGuiTabItemFlags_UnsavedDocument;
		if (options.set_selected) result |= ImGuiTabItemFlags_SetSelected;
		if (options.leading) result |= ImGuiTabItemFlags_Leading;
		if (options.trailing) result |= ImGuiTabItemFlags_Trailing;
		return result;
	}

	[[nodiscard]] inline ImGuiTableFlags to_imgui_table_flags(const table_options& options) noexcept
	{
		ImGuiTableFlags result = ImGuiTableFlags_None;
		if (options.resizable) result |= ImGuiTableFlags_Resizable;
		if (options.reorderable) result |= ImGuiTableFlags_Reorderable;
		if (options.hideable) result |= ImGuiTableFlags_Hideable;
		if (options.sortable) result |= ImGuiTableFlags_Sortable;
		if (options.row_background) result |= ImGuiTableFlags_RowBg;
		if (options.inner_horizontal_borders) result |= ImGuiTableFlags_BordersInnerH;
		if (options.inner_vertical_borders) result |= ImGuiTableFlags_BordersInnerV;
		if (options.stretch_columns) result |= ImGuiTableFlags_SizingStretchSame;
		if (options.no_saved_settings) result |= ImGuiTableFlags_NoSavedSettings;
		return result;
	}

	[[nodiscard]] inline ImGuiTreeNodeFlags to_imgui_tree_node_flags(const tree_node_options& options) noexcept
	{
		ImGuiTreeNodeFlags result = ImGuiTreeNodeFlags_None;
		if (options.selected) result |= ImGuiTreeNodeFlags_Selected;
		if (options.default_open) result |= ImGuiTreeNodeFlags_DefaultOpen;
		if (options.leaf) result |= ImGuiTreeNodeFlags_Leaf;
		if (options.span_available_width) result |= ImGuiTreeNodeFlags_SpanAvailWidth;
		if (options.open_on_arrow) result |= ImGuiTreeNodeFlags_OpenOnArrow;
		if (options.open_on_double_click) result |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if (options.framed) result |= ImGuiTreeNodeFlags_Framed;
		return result;
	}

	[[nodiscard]] inline ImGuiCond to_imgui_condition(condition value)
	{
		switch (value)
		{
		case condition::always: return ImGuiCond_Always;
		case condition::first_use: return ImGuiCond_FirstUseEver;
		case condition::once: return ImGuiCond_Once;
		case condition::appearing: return ImGuiCond_Appearing;
		}
		throw std::invalid_argument("Unknown MNGL ImGui condition");
	}

	[[nodiscard]] inline ImGuiCol to_imgui_color_role(color_role value)
	{
		switch (value)
		{
		case color_role::text: return ImGuiCol_Text;
		case color_role::disabled_text: return ImGuiCol_TextDisabled;
		case color_role::window_background: return ImGuiCol_WindowBg;
		case color_role::border: return ImGuiCol_Border;
		case color_role::button: return ImGuiCol_Button;
		case color_role::button_hovered: return ImGuiCol_ButtonHovered;
		case color_role::button_active: return ImGuiCol_ButtonActive;
		case color_role::header: return ImGuiCol_Header;
		case color_role::header_active: return ImGuiCol_HeaderActive;
		}
		throw std::invalid_argument("Unknown MNGL ImGui color role");
	}

	[[nodiscard]] inline ImGuiPopupFlags to_imgui_popup_mouse_button(mouse_button value)
	{
		switch (value)
		{
		case mouse_button::left: return ImGuiPopupFlags_MouseButtonLeft;
		case mouse_button::right: return ImGuiPopupFlags_MouseButtonRight;
		case mouse_button::middle: return ImGuiPopupFlags_MouseButtonMiddle;
		case mouse_button::x1:
		case mouse_button::x2:
			throw std::invalid_argument("Dear ImGui context menus support left, middle, or right mouse buttons");
		}
		throw std::invalid_argument("Unknown MNGL popup mouse button");
	}

	[[nodiscard]] inline ImVec4 to_imgui_color(const ::mngl::color& value) noexcept
	{
		const glm::vec4 rgba = value.rgba_glsl();
		return ImVec4(rgba.r, rgba.g, rgba.b, rgba.a);
	}

	[[nodiscard]] inline ImU32 to_imgui_color32(const ::mngl::color& value) noexcept
	{
		return ImGui::ColorConvertFloat4ToU32(to_imgui_color(value));
	}

	inline void validate_canvas_bounds(const ::mngl::rectangle& bounds)
	{
		if (!std::isfinite(bounds.pos.x) || !std::isfinite(bounds.pos.y) ||
			!std::isfinite(bounds.size.x) || !std::isfinite(bounds.size.y) ||
			bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("MNGL ImGui canvas bounds must be finite and positive");
		}
	}

	struct string_input_state
	{
		std::string* value;
	};

	inline int resize_string_input(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag != ImGuiInputTextFlags_CallbackResize)
		{
			return 0;
		}

		auto& value = *static_cast<string_input_state*>(data->UserData)->value;
		value.resize(static_cast<std::size_t>(data->BufTextLen));
		data->Buf = value.data();
		return 0;
	}
} // namespace mngl::imgui_compat::detail

namespace mngl
{
	inline void imgui_context::window_scope::text_wrapped(std::string_view value) const
	{
		if (!owner || !owner->context || !visible) return;
		owner->make_current();
		ImGui::PushTextWrapPos(0.0f);
		const char* begin = value.empty() ? "" : value.data();
		ImGui::TextUnformatted(begin, begin + value.size());
		ImGui::PopTextWrapPos();
	}

	inline void imgui_context::window_scope::text_disabled(std::string_view value) const
	{
		if (!owner || !owner->context || !visible) return;
		owner->make_current();
		ImGui::PushStyleColor(ImGuiCol_Text,
		                     ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		const char* begin = value.empty() ? "" : value.data();
		ImGui::TextUnformatted(begin, begin + value.size());
		ImGui::PopStyleColor();
	}

	inline void imgui_context::window_scope::text_colored(const ::mngl::color& tint,
	                                                      std::string_view value) const
	{
		if (!owner || !owner->context || !visible) return;
		owner->make_current();
		ImGui::PushStyleColor(ImGuiCol_Text, imgui_compat::detail::to_imgui_color(tint));
		const char* begin = value.empty() ? "" : value.data();
		ImGui::TextUnformatted(begin, begin + value.size());
		ImGui::PopStyleColor();
	}

	inline glm::vec2 imgui_context::window_scope::measure_text(std::string_view value,
	                                                           float wrap_width) const
	{
		if (!owner || !owner->context || !visible) return {};
		if (!std::isfinite(wrap_width) || wrap_width < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui text wrap width must be finite and non-negative");
		}
		owner->make_current();
		const char* begin = value.empty() ? "" : value.data();
		const ImVec2 size = ImGui::CalcTextSize(begin, begin + value.size(), false, wrap_width);
		return {size.x, size.y};
	}

	inline glm::vec2 imgui_context::window_scope::available_size() const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		const ImVec2 size = ImGui::GetContentRegionAvail();
		return {size.x, size.y};
	}

	inline glm::vec2 imgui_context::window_scope::cursor_screen_position() const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		const ImVec2 position = ImGui::GetCursorScreenPos();
		return {position.x, position.y};
	}

	inline imgui_compat::style_metrics imgui_context::window_scope::metrics() const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		const ImGuiStyle& style = ImGui::GetStyle();
		return {{style.ItemSpacing.x, style.ItemSpacing.y},
		        {style.FramePadding.x, style.FramePadding.y},
		        style.ScrollbarSize,
		        ImGui::GetTextLineHeight()};
	}

	inline ::mngl::color imgui_context::window_scope::style_color(imgui_compat::color_role role) const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		const ImVec4 rgba = ImGui::GetStyleColorVec4(imgui_compat::detail::to_imgui_color_role(role));
		return ::mngl::color(glm::vec4(rgba.x, rgba.y, rgba.z, rgba.w));
	}

	inline void imgui_context::window_scope::same_line(float offset_from_start, float spacing) const
	{
		if (!owner || !owner->context || !visible) return;
		if (!std::isfinite(offset_from_start) || !std::isfinite(spacing)) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui same-line offsets must be finite");
		}
		owner->make_current();
		ImGui::SameLine(offset_from_start, spacing);
	}

	inline void imgui_context::window_scope::new_line() const
	{
		if (!owner || !owner->context || !visible) return;
		owner->make_current();
		ImGui::NewLine();
	}

	inline void imgui_context::window_scope::separator() const
	{
		if (!owner || !owner->context || !visible) return;
		owner->make_current();
		ImGui::Separator();
	}

	inline void imgui_context::window_scope::set_next_item_width(float width) const
	{
		if (!owner || !owner->context || !visible) return;
		if (!std::isfinite(width)) [[unlikely]] throw std::invalid_argument("Dear ImGui item width must be finite");
		owner->make_current();
		ImGui::SetNextItemWidth(width);
	}

	inline void imgui_context::window_scope::set_keyboard_focus_here(int offset) const
	{
		if (!owner || !owner->context || !visible) return;
		owner->make_current();
		ImGui::SetKeyboardFocusHere(offset);
	}

	inline void imgui_context::window_scope::focus_last_item() const
	{
		if (!owner || !owner->context || !visible) return;
		owner->make_current();
		ImGui::SetItemDefaultFocus();
	}

	inline bool imgui_context::window_scope::small_button(std::string_view label) const
	{
		if (!owner || !owner->context || !visible) return false;
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(label);
		return ImGui::SmallButton(terminated.c_str());
	}

	inline bool imgui_context::window_scope::collapsing_header(std::string_view label,
	                                                            bool default_open) const
	{
		if (!owner || !owner->context || !visible) return false;
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(label);
		const ImGuiTreeNodeFlags flags = default_open ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
		return ImGui::CollapsingHeader(terminated.c_str(), flags);
	}

	inline imgui_compat::item_state imgui_context::window_scope::last_item() const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		return {
		    ImGui::IsItemVisible(),
		    ImGui::IsItemHovered(),
		    ImGui::IsItemActive(),
		    ImGui::IsItemFocused(),
		    ImGui::IsItemEdited(),
		    ImGui::IsItemActivated(),
		    ImGui::IsItemDeactivated(),
		    ImGui::IsItemDeactivatedAfterEdit(),
		    ImGui::IsItemClicked(ImGuiMouseButton_Left),
		    ImGui::IsItemClicked(ImGuiMouseButton_Middle),
		    ImGui::IsItemClicked(ImGuiMouseButton_Right)};
	}

	inline bool imgui_context::window_scope::focused(bool include_child_windows) const
	{
		if (!owner || !owner->context || !visible) return false;
		owner->make_current();
		return ImGui::IsWindowFocused(include_child_windows ? ImGuiFocusedFlags_ChildWindows : ImGuiFocusedFlags_None);
	}

	inline float imgui_context::window_scope::scroll_y() const
	{
		if (!owner || !owner->context || !visible) return 0.0f;
		owner->make_current();
		return ImGui::GetScrollY();
	}

	inline float imgui_context::window_scope::scroll_max_y() const
	{
		if (!owner || !owner->context || !visible) return 0.0f;
		owner->make_current();
		return ImGui::GetScrollMaxY();
	}

	inline void imgui_context::window_scope::scroll_to_y(float position) const
	{
		if (!owner || !owner->context || !visible) return;
		if (!std::isfinite(position) || position < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui scroll position must be finite and non-negative");
		}
		owner->make_current();
		ImGui::SetScrollY(position);
	}

	inline void imgui_context::window_scope::scroll_here_y(float center_ratio) const
	{
		if (!owner || !owner->context || !visible) return;
		if (!std::isfinite(center_ratio) || center_ratio < 0.0f || center_ratio > 1.0f) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui scroll center ratio must be between zero and one");
		}
		owner->make_current();
		ImGui::SetScrollHereY(center_ratio);
	}

	inline bool imgui_context::window_scope::popup_open(std::string_view id) const
	{
		if (!owner || !owner->context || !visible) return false;
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		return ImGui::IsPopupOpen(terminated.c_str());
	}

	inline void imgui_context::window_scope::open_popup(std::string_view id) const
	{
		if (!owner || !owner->context || !visible) return;
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		ImGui::OpenPopup(terminated.c_str());
	}

	inline void imgui_context::window_scope::close_popup() const
	{
		if (!owner || !owner->context || end != end_action::popup || !visible) [[unlikely]]
		{
			throw std::logic_error("Only an active MNGL popup scope can close its popup");
		}
		owner->make_current();
		ImGui::CloseCurrentPopup();
	}

	inline imgui_context::child_scope imgui_context::window_scope::child(
	    std::string_view id, const imgui_compat::child_options& options) const
	{
		if (!owner || !owner->context || !visible) return {};
		if (!std::isfinite(options.size.x) || !std::isfinite(options.size.y)) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui child size must be finite");
		}
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		const bool child_visible = ImGui::BeginChild(
		    terminated.c_str(), ImVec2(options.size.x, options.size.y),
		    imgui_compat::detail::to_imgui_child_flags(options),
		    imgui_compat::detail::child_window_flags(options));
		return window_scope(*owner, child_visible, end_action::child);
	}

	inline imgui_context::combo_scope imgui_context::window_scope::begin_combo(
	    std::string_view label, std::string_view preview) const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		const std::string terminated_label = imgui_compat::detail::widget_label(label);
		const std::string terminated_preview = imgui_compat::detail::widget_label(preview);
		if (!ImGui::BeginCombo(terminated_label.c_str(), terminated_preview.c_str())) return {};
		return window_scope(*owner, true, end_action::combo);
	}

	inline imgui_context::popup_scope imgui_context::window_scope::popup(
	    std::string_view id, const imgui_compat::window_options& options) const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		if (!ImGui::BeginPopup(terminated.c_str(), imgui_compat::detail::to_imgui_window_flags(options))) return {};
		return window_scope(*owner, true, end_action::popup);
	}

	inline imgui_context::popup_scope imgui_context::window_scope::modal(
	    std::string_view id, bool& open, const imgui_compat::window_options& options) const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		if (!ImGui::BeginPopupModal(terminated.c_str(), &open,
		                            imgui_compat::detail::to_imgui_window_flags(options))) return {};
		return window_scope(*owner, true, end_action::popup);
	}

	inline imgui_context::popup_scope imgui_context::window_scope::context_popup(
	    std::string_view id, mouse_button button) const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		const char* popup_id = terminated.empty() ? nullptr : terminated.c_str();
		if (!ImGui::BeginPopupContextItem(popup_id,
		                                 imgui_compat::detail::to_imgui_popup_mouse_button(button))) return {};
		return window_scope(*owner, true, end_action::popup);
	}

	inline imgui_context::tab_bar_scope imgui_context::window_scope::tab_bar(
	    std::string_view id, const imgui_compat::tab_bar_options& options) const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		if (!ImGui::BeginTabBar(terminated.c_str(), imgui_compat::detail::to_imgui_tab_bar_flags(options))) return {};
		return window_scope(*owner, true, end_action::tab_bar);
	}

	inline imgui_context::tab_item_scope imgui_context::window_scope::tab(
	    std::string_view label, bool* open, const imgui_compat::tab_item_options& options) const
	{
		if (!owner || !owner->context || !visible) return {};
		if (end != end_action::tab_bar) [[unlikely]]
		{
			throw std::logic_error("An ImGui tab item must be opened inside an MNGL tab bar scope");
		}
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(label);
		if (!ImGui::BeginTabItem(terminated.c_str(), open,
		                        imgui_compat::detail::to_imgui_tab_item_flags(options))) return {};
		return window_scope(*owner, true, end_action::tab_item);
	}

	inline imgui_context::table_scope imgui_context::window_scope::table(
	    std::string_view id, int columns, const imgui_compat::table_options& options) const
	{
		if (!owner || !owner->context || !visible) return {};
		if (columns <= 0) [[unlikely]] throw std::invalid_argument("Dear ImGui tables require at least one column");
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		if (!ImGui::BeginTable(terminated.c_str(), columns,
		                      imgui_compat::detail::to_imgui_table_flags(options))) return {};
		return window_scope(*owner, true, end_action::table);
	}

	inline imgui_context::tooltip_scope imgui_context::window_scope::tooltip() const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		if (!ImGui::BeginTooltip()) return {};
		return window_scope(*owner, true, end_action::tooltip);
	}

	inline imgui_context::disabled_scope imgui_context::window_scope::disabled(bool value) const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		ImGui::BeginDisabled(value);
		return window_scope(*owner, true, end_action::disabled);
	}

	inline imgui_context::tree_node_scope imgui_context::window_scope::tree_node(
	    std::string_view label, const imgui_compat::tree_node_options& options) const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(label);
		if (!ImGui::TreeNodeEx(terminated.c_str(), imgui_compat::detail::to_imgui_tree_node_flags(options))) return {};
		return window_scope(*owner, true, end_action::tree_node);
	}

	inline void imgui_context::window_scope::next_row(float minimum_height) const
	{
		if (!owner || !owner->context || !visible) return;
		if (end != end_action::table) [[unlikely]] throw std::logic_error("Rows can only be advanced inside an MNGL table scope");
		if (!std::isfinite(minimum_height) || minimum_height < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui table row height must be finite and non-negative");
		}
		owner->make_current();
		ImGui::TableNextRow(ImGuiTableRowFlags_None, minimum_height);
	}

	inline bool imgui_context::window_scope::next_column() const
	{
		if (!owner || !owner->context || !visible) return false;
		if (end != end_action::table) [[unlikely]] throw std::logic_error("Columns can only be advanced inside an MNGL table scope");
		owner->make_current();
		return ImGui::TableNextColumn();
	}

	inline void imgui_context::window_scope::setup_column(std::string_view label, float initial_width) const
	{
		if (!owner || !owner->context || !visible) return;
		if (end != end_action::table) [[unlikely]] throw std::logic_error("Columns can only be configured inside an MNGL table scope");
		if (!std::isfinite(initial_width) || initial_width < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui table column width must be finite and non-negative");
		}
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(label);
		ImGui::TableSetupColumn(terminated.c_str(), ImGuiTableColumnFlags_None, initial_width);
	}

	inline void imgui_context::window_scope::table_headers_row() const
	{
		if (!owner || !owner->context || !visible) return;
		if (end != end_action::table) [[unlikely]] throw std::logic_error("Table headers require an MNGL table scope");
		owner->make_current();
		ImGui::TableHeadersRow();
	}

	inline imgui_context::id_scope imgui_context::window_scope::push_id(std::string_view id) const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		ImGui::PushID(terminated.c_str());
		return id_scope(*owner, true);
	}

	inline imgui_context::id_scope imgui_context::window_scope::push_id(int id) const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		ImGui::PushID(id);
		return id_scope(*owner, true);
	}

	inline imgui_context::style_color_scope imgui_context::window_scope::push_style_color(
	    imgui_compat::color_role role, const ::mngl::color& value) const
	{
		if (!owner || !owner->context || !visible) return {};
		owner->make_current();
		ImGui::PushStyleColor(imgui_compat::detail::to_imgui_color_role(role),
		                     imgui_compat::detail::to_imgui_color(value));
		return style_color_scope(*owner, true);
	}

	inline bool imgui_context::window_scope::input_text(std::string_view label, std::string& value,
	                                                    std::string_view hint) const
	{
		if (!owner || !owner->context || !visible) return false;
		if (!owner->frame_active) [[unlikely]] throw std::logic_error("An ImGui input must be submitted inside an active frame");
		if (value.find('\0') != std::string::npos || hint.find('\0') != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui text input does not support embedded null characters");
		}
		if (value.capacity() >= static_cast<std::size_t>(std::numeric_limits<int>::max())) [[unlikely]]
		{
			throw std::length_error("Dear ImGui text input exceeds its buffer size limit");
		}
		owner->make_current();
		const std::string terminated_label = imgui_compat::detail::widget_label(label);
		const std::string terminated_hint = imgui_compat::detail::widget_label(hint);
		imgui_compat::detail::string_input_state state{&value};
		const char* hint_pointer = terminated_hint.empty() ? nullptr : terminated_hint.c_str();
		return ImGui::InputTextWithHint(terminated_label.c_str(), hint_pointer, value.data(), value.capacity() + 1,
		                                ImGuiInputTextFlags_CallbackResize,
		                                imgui_compat::detail::resize_string_input, &state);
	}

	inline bool imgui_context::window_scope::input_text_multiline(std::string_view label, std::string& value,
	                                                               glm::vec2 size) const
	{
		if (!owner || !owner->context || !visible) return false;
		if (!owner->frame_active) [[unlikely]] throw std::logic_error("An ImGui input must be submitted inside an active frame");
		if (value.find('\0') != std::string::npos || !std::isfinite(size.x) || !std::isfinite(size.y) ||
			size.x < 0.0f || size.y < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui multiline input has invalid text or size");
		}
		if (value.capacity() >= static_cast<std::size_t>(std::numeric_limits<int>::max())) [[unlikely]]
		{
			throw std::length_error("Dear ImGui text input exceeds its buffer size limit");
		}
		owner->make_current();
		const std::string terminated_label = imgui_compat::detail::widget_label(label);
		imgui_compat::detail::string_input_state state{&value};
		return ImGui::InputTextMultiline(terminated_label.c_str(), value.data(), value.capacity() + 1,
		                                 ImVec2(size.x, size.y), ImGuiInputTextFlags_CallbackResize,
		                                 imgui_compat::detail::resize_string_input, &state);
	}

	inline imgui_context::canvas_scope imgui_context::window_scope::canvas(
	    std::string_view id, glm::vec2 size, const imgui_compat::canvas_options& options) const
	{
		if (!owner || !owner->context || !visible) return {};
		if (!std::isfinite(size.x) || !std::isfinite(size.y) || size.x <= 0.0f || size.y <= 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui canvas size must be finite and positive");
		}
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		ImGuiButtonFlags flags = ImGuiButtonFlags_None;
		if (options.left_button) flags |= ImGuiButtonFlags_MouseButtonLeft;
		if (options.right_button) flags |= ImGuiButtonFlags_MouseButtonRight;
		if (options.middle_button) flags |= ImGuiButtonFlags_MouseButtonMiddle;
		if (flags == ImGuiButtonFlags_None) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui canvas must accept at least one mouse button");
		}
		const ImVec2 position = ImGui::GetCursorScreenPos();
		(void)ImGui::InvisibleButton(terminated.c_str(), ImVec2(size.x, size.y), flags);
		const imgui_compat::item_state item = last_item();
		const ImGuiIO& io = ImGui::GetIO();
		const bool has_mouse = std::isfinite(io.MousePos.x) && std::isfinite(io.MousePos.y) &&
		                      io.MousePos.x > -1.0e10f && io.MousePos.y > -1.0e10f;
		ImDrawList* list = ImGui::GetWindowDrawList();
		list->PushClipRect(position, ImVec2(position.x + size.x, position.y + size.y), true);
		return canvas_scope(*owner, {position.x, position.y}, size, list, item,
		                    {io.MousePos.x, io.MousePos.y}, has_mouse);
	}

	inline imgui_context::id_scope::~id_scope() noexcept
	{
		if (owner && owner->context && pushed)
		{
			owner->make_current();
			ImGui::PopID();
		}
	}

	inline imgui_context::style_color_scope::~style_color_scope() noexcept
	{
		if (owner && owner->context && pushed)
		{
			owner->make_current();
			ImGui::PopStyleColor();
		}
	}

	inline imgui_context::canvas_scope::canvas_scope(imgui_context& owner, glm::vec2 origin, glm::vec2 extent,
	                                                 void* draw_list, imgui_compat::item_state item,
	                                                 glm::vec2 mouse_position, bool has_mouse_position) noexcept
	    : owner(&owner), origin(origin), extent(extent), draw_list(draw_list), item(item),
	      mouse(mouse_position), has_mouse(has_mouse_position)
	{
	}

	inline imgui_context::canvas_scope::~canvas_scope() noexcept
	{
		if (owner && owner->context && draw_list)
		{
			owner->make_current();
			static_cast<ImDrawList*>(draw_list)->PopClipRect();
		}
	}

	inline bool imgui_context::canvas_scope::clicked(mouse_button button) const noexcept
	{
		switch (button)
		{
		case mouse_button::left: return item.clicked_left;
		case mouse_button::middle: return item.clicked_middle;
		case mouse_button::right: return item.clicked_right;
		case mouse_button::x1:
		case mouse_button::x2: return false;
		}
		return false;
	}

	inline bool imgui_context::canvas_scope::double_clicked(mouse_button button) const
	{
		if (!owner || !owner->context || !item.hovered) return false;
		owner->make_current();
		return ImGui::IsMouseDoubleClicked(imgui_compat::detail::to_imgui_mouse_button(button));
	}

	inline bool imgui_context::canvas_scope::dragging(mouse_button button, float threshold) const
	{
		if (!owner || !owner->context || !item.hovered) return false;
		if (!std::isfinite(threshold)) [[unlikely]] throw std::invalid_argument("Dear ImGui drag threshold must be finite");
		owner->make_current();
		return ImGui::IsMouseDragging(imgui_compat::detail::to_imgui_mouse_button(button), threshold);
	}

	inline std::optional<glm::vec2> imgui_context::canvas_scope::mouse_position() const noexcept
	{
		if (!has_mouse) return std::nullopt;
		return mouse - origin;
	}

	inline glm::vec2 imgui_context::canvas_scope::measure_text(std::string_view value) const
	{
		if (!owner || !owner->context) return {};
		owner->make_current();
		const char* begin = value.empty() ? "" : value.data();
		const ImVec2 size = ImGui::CalcTextSize(begin, begin + value.size());
		return {size.x, size.y};
	}

	inline void imgui_context::canvas_scope::fill_rectangle(const ::mngl::rectangle& bounds,
	                                                         const ::mngl::color& tint,
	                                                         float rounding) const
	{
		if (!owner || !owner->context || !draw_list) return;
		imgui_compat::detail::validate_canvas_bounds(bounds);
		if (!std::isfinite(rounding) || rounding < 0.0f) [[unlikely]] throw std::invalid_argument("Canvas rounding must be finite and non-negative");
		owner->make_current();
		const ImVec2 minimum(origin.x + bounds.pos.x, origin.y + bounds.pos.y);
		const ImVec2 maximum(minimum.x + bounds.size.x, minimum.y + bounds.size.y);
		static_cast<ImDrawList*>(draw_list)->AddRectFilled(
		    minimum, maximum, imgui_compat::detail::to_imgui_color32(tint), rounding);
	}

	inline void imgui_context::canvas_scope::draw_rectangle(const ::mngl::rectangle& bounds,
	                                                         const ::mngl::color& tint,
	                                                         float thickness, float rounding) const
	{
		if (!owner || !owner->context || !draw_list) return;
		imgui_compat::detail::validate_canvas_bounds(bounds);
		if (!std::isfinite(thickness) || thickness <= 0.0f || !std::isfinite(rounding) || rounding < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Canvas outline dimensions must be finite and positive");
		}
		owner->make_current();
		const ImVec2 minimum(origin.x + bounds.pos.x, origin.y + bounds.pos.y);
		const ImVec2 maximum(minimum.x + bounds.size.x, minimum.y + bounds.size.y);
		static_cast<ImDrawList*>(draw_list)->AddRect(
		    minimum, maximum, imgui_compat::detail::to_imgui_color32(tint), rounding, ImDrawFlags_None, thickness);
	}

	inline void imgui_context::canvas_scope::draw_line(glm::vec2 start, glm::vec2 end,
	                                                   const ::mngl::color& tint, float thickness) const
	{
		if (!owner || !owner->context || !draw_list) return;
		if (!std::isfinite(start.x) || !std::isfinite(start.y) || !std::isfinite(end.x) ||
			!std::isfinite(end.y) || !std::isfinite(thickness) || thickness <= 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Canvas line coordinates and thickness must be finite");
		}
		owner->make_current();
		static_cast<ImDrawList*>(draw_list)->AddLine(
		    ImVec2(origin.x + start.x, origin.y + start.y), ImVec2(origin.x + end.x, origin.y + end.y),
		    imgui_compat::detail::to_imgui_color32(tint), thickness);
	}

	inline void imgui_context::canvas_scope::text(glm::vec2 position, std::string_view value,
	                                              const ::mngl::color& tint) const
	{
		if (!owner || !owner->context || !draw_list) return;
		if (!std::isfinite(position.x) || !std::isfinite(position.y)) [[unlikely]]
		{
			throw std::invalid_argument("Canvas text position must be finite");
		}
		owner->make_current();
		const char* begin = value.empty() ? "" : value.data();
		static_cast<ImDrawList*>(draw_list)->AddText(
		    ImVec2(origin.x + position.x, origin.y + position.y),
		    imgui_compat::detail::to_imgui_color32(tint), begin, begin + value.size());
	}

	inline void imgui_context::canvas_scope::image(const ::mngl::texture& source,
	                                               const ::mngl::rectangle& bounds,
	                                               const ::mngl::color& tint,
	                                               glm::vec2 uv_min, glm::vec2 uv_max) const
	{
		if (!owner || !owner->context || !draw_list) return;
		imgui_compat::detail::validate_canvas_bounds(bounds);
		if (!source.valid()) [[unlikely]] throw std::invalid_argument("Cannot display an invalid MNGL texture in a canvas");
		const gpu::texture& gpu_texture = source.raw_gpu_texture();
		if (!gpu_texture.has_usage(gpu::texture::usage_flags::sampler) || source.rendering() ||
			gpu_texture.is_swapchain_texture()) [[unlikely]]
		{
			throw std::logic_error("MNGL canvas images must be sampleable and idle");
		}
		if (!owner->attached_window || gpu_texture.device_raw() != owner->attached_window->gpu_device().raw()) [[unlikely]]
		{
			throw std::invalid_argument("The MNGL canvas texture and ImGui context must share a GPU device");
		}
		if (!std::isfinite(uv_min.x) || !std::isfinite(uv_min.y) || !std::isfinite(uv_max.x) ||
			!std::isfinite(uv_max.y) || uv_min.x < 0.0f || uv_min.y < 0.0f || uv_max.x > 1.0f ||
			uv_max.y > 1.0f || uv_min.x > uv_max.x || uv_min.y > uv_max.y) [[unlikely]]
		{
			throw std::invalid_argument("MNGL canvas image UV bounds must be finite and normalized");
		}
		owner->make_current();
		const ImTextureID id = reinterpret_cast<ImTextureID>(gpu_texture.raw());
		static_cast<ImDrawList*>(draw_list)->AddImage(
		    ImTextureRef(id), ImVec2(origin.x + bounds.pos.x, origin.y + bounds.pos.y),
		    ImVec2(origin.x + bounds.pos.x + bounds.size.x, origin.y + bounds.pos.y + bounds.size.y),
		    ImVec2(uv_min.x, uv_min.y), ImVec2(uv_max.x, uv_max.y),
		    imgui_compat::detail::to_imgui_color32(tint));
	}

	inline bool imgui_context::menu_scope::menu_item(std::string_view label, std::string_view shortcut,
	                                                 bool selected, bool enabled) const
	{
		if (!owner || !owner->context || !visible) return false;
		owner->make_current();
		const std::string terminated_label = imgui_compat::detail::widget_label(label);
		const std::string terminated_shortcut = imgui_compat::detail::widget_label(shortcut);
		const char* shortcut_pointer = terminated_shortcut.empty() ? nullptr : terminated_shortcut.c_str();
		return ImGui::MenuItem(terminated_label.c_str(), shortcut_pointer, selected, enabled);
	}

	inline bool imgui_context::menu_scope::menu_item(std::string_view label, bool& selected,
	                                                 std::string_view shortcut, bool enabled) const
	{
		if (!owner || !owner->context || !visible) return false;
		owner->make_current();
		const std::string terminated_label = imgui_compat::detail::widget_label(label);
		const std::string terminated_shortcut = imgui_compat::detail::widget_label(shortcut);
		const char* shortcut_pointer = terminated_shortcut.empty() ? nullptr : terminated_shortcut.c_str();
		return ImGui::MenuItem(terminated_label.c_str(), shortcut_pointer, &selected, enabled);
	}

	inline imgui_context::menu_scope imgui_context::menu_scope::menu(std::string_view label) const
	{
		if (!owner || !owner->context || !visible) return {};
		return menu_scope(*owner, label);
	}

	inline bool imgui_context::main_menu_bar_scope::menu_item(std::string_view label,
	                                                          std::string_view shortcut,
	                                                          bool selected, bool enabled) const
	{
		if (!owner || !owner->context || !visible) return false;
		owner->make_current();
		const std::string terminated_label = imgui_compat::detail::widget_label(label);
		const std::string terminated_shortcut = imgui_compat::detail::widget_label(shortcut);
		const char* shortcut_pointer = terminated_shortcut.empty() ? nullptr : terminated_shortcut.c_str();
		return ImGui::MenuItem(terminated_label.c_str(), shortcut_pointer, selected, enabled);
	}

	inline bool imgui_context::main_menu_bar_scope::menu_item(std::string_view label, bool& selected,
	                                                          std::string_view shortcut,
	                                                          bool enabled) const
	{
		if (!owner || !owner->context || !visible) return false;
		owner->make_current();
		const std::string terminated_label = imgui_compat::detail::widget_label(label);
		const std::string terminated_shortcut = imgui_compat::detail::widget_label(shortcut);
		const char* shortcut_pointer = terminated_shortcut.empty() ? nullptr : terminated_shortcut.c_str();
		return ImGui::MenuItem(terminated_label.c_str(), shortcut_pointer, &selected, enabled);
	}

	inline imgui_compat::input_state imgui_context::frame_scope::input() const
	{
		if (!owner || !owner->context || !owner->frame_active) return {};
		owner->make_current();
		const ImGuiIO& io = ImGui::GetIO();
		imgui_compat::input_state state{};
		state.wants_keyboard = io.WantCaptureKeyboard;
		state.wants_mouse = io.WantCaptureMouse;
		state.wants_text = io.WantTextInput;
		state.control = io.KeyCtrl;
		state.shift = io.KeyShift;
		state.alt = io.KeyAlt;
		state.super = io.KeySuper;
		state.delta_time = io.DeltaTime;
		state.framebuffer_scale = {io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y};
		if (std::isfinite(io.MousePos.x) && std::isfinite(io.MousePos.y) &&
		    io.MousePos.x > -1.0e10f && io.MousePos.y > -1.0e10f)
		{
			state.mouse_position = glm::vec2(io.MousePos.x, io.MousePos.y);
		}
		state.mouse_delta = {io.MouseDelta.x, io.MouseDelta.y};
		state.mouse_wheel = {io.MouseWheelH, io.MouseWheel};
		return state;
	}

	inline bool imgui_context::frame_scope::key_pressed(imgui_compat::key value, bool repeat) const
	{
		if (!owner || !owner->context || !owner->frame_active) return false;
		owner->make_current();
		return ImGui::IsKeyPressed(imgui_compat::detail::to_imgui_key(value), repeat);
	}

	inline bool imgui_context::frame_scope::mouse_down(mouse_button button) const
	{
		if (!owner || !owner->context || !owner->frame_active) return false;
		owner->make_current();
		return ImGui::IsMouseDown(imgui_compat::detail::to_imgui_mouse_button(button));
	}

	inline bool imgui_context::frame_scope::mouse_clicked(mouse_button button) const
	{
		if (!owner || !owner->context || !owner->frame_active) return false;
		owner->make_current();
		return ImGui::IsMouseClicked(imgui_compat::detail::to_imgui_mouse_button(button));
	}

	inline bool imgui_context::frame_scope::mouse_double_clicked(mouse_button button) const
	{
		if (!owner || !owner->context || !owner->frame_active) return false;
		owner->make_current();
		return ImGui::IsMouseDoubleClicked(imgui_compat::detail::to_imgui_mouse_button(button));
	}

	inline bool imgui_context::frame_scope::mouse_dragging(mouse_button button, float threshold) const
	{
		if (!owner || !owner->context || !owner->frame_active) return false;
		if (!std::isfinite(threshold)) [[unlikely]] throw std::invalid_argument("Dear ImGui drag threshold must be finite");
		owner->make_current();
		return ImGui::IsMouseDragging(imgui_compat::detail::to_imgui_mouse_button(button), threshold);
	}

	inline bool imgui_context::frame_scope::any_item_active() const
	{
		if (!owner || !owner->context || !owner->frame_active) return false;
		owner->make_current();
		return ImGui::IsAnyItemActive();
	}

	inline imgui_compat::viewport_state imgui_context::frame_scope::main_viewport() const
	{
		if (!owner || !owner->context || !owner->frame_active) return {};
		owner->make_current();
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		if (!viewport) return {};
		return {{viewport->Pos.x, viewport->Pos.y},
		        {viewport->WorkPos.x, viewport->WorkPos.y},
		        {viewport->Size.x, viewport->Size.y},
		        {viewport->WorkSize.x, viewport->WorkSize.y}};
	}

	inline void imgui_context::frame_scope::dockspace_over_viewport() const
	{
		if (!owner || !owner->context || !owner->frame_active) return;
		owner->make_current();
		ImGui::DockSpaceOverViewport();
	}

	inline void imgui_context::frame_scope::set_next_window_position(glm::vec2 position,
	                                                                 imgui_compat::condition condition,
	                                                                 glm::vec2 pivot) const
	{
		if (!owner || !owner->context || !owner->frame_active) return;
		if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(pivot.x) ||
			!std::isfinite(pivot.y)) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui window position and pivot must be finite");
		}
		owner->make_current();
		ImGui::SetNextWindowPos(ImVec2(position.x, position.y),
		                        imgui_compat::detail::to_imgui_condition(condition),
		                        ImVec2(pivot.x, pivot.y));
	}

	inline void imgui_context::frame_scope::set_next_window_size(glm::vec2 size,
	                                                             imgui_compat::condition condition) const
	{
		if (!owner || !owner->context || !owner->frame_active) return;
		if (!std::isfinite(size.x) || !std::isfinite(size.y) || size.x < 0.0f || size.y < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui window size must be finite and non-negative");
		}
		owner->make_current();
		ImGui::SetNextWindowSize(ImVec2(size.x, size.y),
		                         imgui_compat::detail::to_imgui_condition(condition));
	}

	inline void imgui_context::frame_scope::focus_next_window() const
	{
		if (!owner || !owner->context || !owner->frame_active) return;
		owner->make_current();
		ImGui::SetNextWindowFocus();
	}

	inline void imgui_context::frame_scope::open_popup(std::string_view id) const
	{
		if (!owner || !owner->context || !owner->frame_active) return;
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		ImGui::OpenPopup(terminated.c_str());
	}

	inline bool imgui_context::frame_scope::popup_open(std::string_view id) const
	{
		if (!owner || !owner->context || !owner->frame_active) return false;
		owner->make_current();
		const std::string terminated = imgui_compat::detail::widget_label(id);
		return ImGui::IsPopupOpen(terminated.c_str());
	}
} // namespace mngl
