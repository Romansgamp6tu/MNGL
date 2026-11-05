#pragma once

#include "lifecycle.hpp"

namespace mngl
{
	template <imgui_compat::detail::scalar_numeric_value T>
	inline void imgui_context::window_scope::text(const T& value) const
	{
		text(imgui_compat::detail::format_scalar_value(value));
	}

	inline void imgui_context::window_scope::text(bool value) const
	{
		text(imgui_compat::detail::format_scalar_value(value));
	}

	inline bool imgui_context::window_scope::button(std::string_view label) const
	{
		if (!owner || !owner->context || !visible)
		{
			return false;
		}

		owner->make_current();
		const std::string terminated_label(label);
		return ImGui::Button(terminated_label.c_str());
	}

	inline bool imgui_context::window_scope::button(const char* label) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui button label cannot be null");
		}
		return button(std::string_view(label));
	}

	template <imgui_compat::detail::scalar_numeric_value T>
	inline bool imgui_context::window_scope::button(const T& label) const
	{
		return button(imgui_compat::detail::format_scalar_value(label));
	}

	inline bool imgui_context::window_scope::button(bool label) const
	{
		return button(imgui_compat::detail::format_scalar_value(label));
	}

	inline void imgui_context::window_scope::progress_bar(float fraction) const
	{
		if (!owner || !owner->context || !visible)
		{
			return;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui progress bar must be submitted inside an active frame");
		}
		imgui_compat::detail::validate_progress_fraction(fraction);

		owner->make_current();
		ImGui::ProgressBar(fraction);
	}

	inline void imgui_context::window_scope::progress_bar(float fraction, std::string_view overlay) const
	{
		if (!owner || !owner->context || !visible)
		{
			return;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui progress bar must be submitted inside an active frame");
		}
		imgui_compat::detail::validate_progress_fraction(fraction);
		if (overlay.find('\0') != std::string_view::npos)
		{
			throw std::invalid_argument(
			    "Dear ImGui progress overlays cannot contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_overlay(overlay);
		ImGui::ProgressBar(fraction, ImVec2(-std::numeric_limits<float>::min(), 0.0f),
		                   terminated_overlay.c_str());
	}

	inline bool imgui_context::window_scope::selectable(std::string_view label, bool selected) const
	{
		if (!owner || !owner->context || !visible)
		{
			return false;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui selectable must be submitted inside an active frame");
		}
		if (label.find('\0') != std::string_view::npos)
		{
			throw std::invalid_argument(
			    "Dear ImGui selectable labels cannot contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_label(label);
		return ImGui::Selectable(terminated_label.c_str(), selected);
	}

	inline bool imgui_context::window_scope::selectable(const char* label, bool selected) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui selectable labels cannot be null");
		}
		return selectable(std::string_view(label), selected);
	}

	inline void imgui_context::window_scope::bullet() const
	{
		if (!owner || !owner->context || !visible)
		{
			return;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui bullet must be submitted inside an active frame");
		}

		owner->make_current();
		ImGui::Bullet();
	}

	inline void imgui_context::window_scope::bullet(std::string_view label) const
	{
		if (!owner || !owner->context || !visible)
		{
			return;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui bullet must be submitted inside an active frame");
		}

		owner->make_current();
		ImGui::Bullet();
		const char* begin = label.empty() ? "" : label.data();
		ImGui::TextUnformatted(begin, begin + label.size());
	}

	inline void imgui_context::window_scope::bullet(const char* label) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui bullet text cannot be null");
		}
		bullet(std::string_view(label));
	}

	inline bool imgui_context::window_scope::radio_button(std::string_view label, bool active) const
	{
		if (!owner || !owner->context || !visible)
		{
			return false;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui radio button must be submitted inside an active frame");
		}
		if (label.find('\0') != std::string_view::npos)
		{
			throw std::invalid_argument(
			    "Dear ImGui radio button labels cannot contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_label(label);
		return ImGui::RadioButton(terminated_label.c_str(), active);
	}

	inline bool imgui_context::window_scope::radio_button(const char* label, bool active) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui radio button labels cannot be null");
		}
		return radio_button(std::string_view(label), active);
	}

	template <typename T>
	    requires std::equality_comparable<T> && std::assignable_from<T&, std::type_identity_t<T>>
	inline bool imgui_context::window_scope::radio_button(std::string_view label, T& selected,
	                                                      std::type_identity_t<T> value) const
	{
		const bool activated = radio_button(label, selected == value);
		if (activated)
		{
			selected = value;
		}
		return activated;
	}

	template <imgui_compat::detail::combo_range Range>
	inline imgui_compat::detail::combo_result_t<Range> imgui_context::window_scope::combo(
	    std::string_view label, Range&& values) const
	{
		using result_type = imgui_compat::detail::combo_result_t<Range>;
		using value_type = std::ranges::range_value_t<Range>;

		if (!owner || !owner->context || !visible)
		{
			return {};
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui combo must be submitted inside an active frame");
		}

		std::vector<std::string> item_labels;
		if constexpr (std::ranges::sized_range<Range>)
		{
			const auto item_count = std::ranges::size(values);
			if (std::cmp_greater(item_count, std::numeric_limits<int>::max()))
			{
				throw std::length_error("Dear ImGui combo supports at most INT_MAX items");
			}
			item_labels.reserve(static_cast<std::size_t>(item_count));
		}

		for (auto iterator = std::ranges::begin(values); iterator != std::ranges::end(values); ++iterator)
		{
			if (item_labels.size() == static_cast<std::size_t>(std::numeric_limits<int>::max()))
			{
				throw std::length_error("Dear ImGui combo supports at most INT_MAX items");
			}
			item_labels.push_back(imgui_compat::detail::combo_item_to_string(*iterator));
		}

		owner->make_current();
		if (label.find('\0') != std::string_view::npos)
		{
			throw std::invalid_argument("Dear ImGui combo labels cannot contain embedded null characters");
		}
		const std::string terminated_label(label);
		const ImGuiID storage_id = ImGui::GetID(terminated_label.c_str());
		ImGuiStorage* const storage = ImGui::GetStateStorage();
		if (!storage)
		{
			throw std::logic_error("Dear ImGui did not provide state storage for a combo");
		}

		int selected_index = storage->GetInt(storage_id, -1);
		if (selected_index < -1 || selected_index >= static_cast<int>(item_labels.size()))
		{
			selected_index = -1;
			storage->SetInt(storage_id, selected_index);
		}

		const char* preview = selected_index < 0
		                          ? (item_labels.empty() ? "(no options)" : "(none)")
		                          : item_labels[static_cast<std::size_t>(selected_index)].c_str();

		if (ImGui::BeginCombo(terminated_label.c_str(), preview))
		{
			if (item_labels.empty())
			{
				ImGui::TextUnformatted("(no options)");
			}
			else
			{
				ImGui::PushID(-1);
				if (ImGui::Selectable("(none)", selected_index < 0))
				{
					selected_index = -1;
					storage->SetInt(storage_id, selected_index);
				}
				if (selected_index < 0)
				{
					ImGui::SetItemDefaultFocus();
				}
				ImGui::PopID();
				for (std::size_t index = 0; index < item_labels.size(); ++index)
				{
					const bool is_selected = selected_index == static_cast<int>(index);
					const std::string& item_label = item_labels[index];
					const ImVec2 text_position = ImGui::GetCursorScreenPos();
					ImGui::PushID(static_cast<int>(index));
					if (ImGui::Selectable("##mngl_combo_option", is_selected, ImGuiSelectableFlags_None,
					                      ImVec2(0.0f, ImGui::GetTextLineHeight())))
					{
						selected_index = static_cast<int>(index);
						storage->SetInt(storage_id, selected_index);
					}
					ImGui::GetWindowDrawList()->AddText(text_position, ImGui::GetColorU32(ImGuiCol_Text),
					                                    item_label.data(),
					                                    item_label.data() + item_label.size());
					if (is_selected)
					{
						ImGui::SetItemDefaultFocus();
					}
					ImGui::PopID();
				}
			}
			ImGui::EndCombo();
		}

		if (selected_index < 0)
		{
			return {};
		}

		auto selected_iterator = std::ranges::begin(values);
		std::ranges::advance(selected_iterator, selected_index);
		if constexpr (std::ranges::borrowed_range<Range>)
		{
			return result_type{std::move(selected_iterator)};
		}
		else if constexpr (imgui_compat::detail::combo_copyable_result<Range>)
		{
			return result_type{std::in_place, *selected_iterator};
		}
		else
		{
			return result_type{std::in_place, static_cast<std::size_t>(selected_index)};
		}
	}

	template <typename T>
	    requires imgui_compat::detail::combo_range<std::initializer_list<T>>
	inline imgui_compat::detail::combo_result_t<std::initializer_list<T>> imgui_context::window_scope::combo(
	    std::string_view label, std::initializer_list<T>&& values) const
	{
		return combo<std::initializer_list<T>>(label, std::move(values));
	}

	template <imgui_compat::detail::scalar_numeric_value Label, typename T>
	    requires imgui_compat::detail::combo_range<std::initializer_list<T>>
	inline imgui_compat::detail::combo_result_t<std::initializer_list<T>> imgui_context::window_scope::combo(
	    const Label& label, std::initializer_list<T>&& values) const
	{
		const std::string formatted_label = imgui_compat::detail::format_scalar_value(label);
		return combo(std::string_view(formatted_label), std::move(values));
	}

	template <typename T>
	    requires imgui_compat::detail::combo_range<std::initializer_list<T>>
	inline imgui_compat::detail::combo_result_t<std::initializer_list<T>> imgui_context::window_scope::combo(
	    bool label, std::initializer_list<T>&& values) const
	{
		const std::string formatted_label = imgui_compat::detail::format_scalar_value(label);
		return combo(std::string_view(formatted_label), std::move(values));
	}

	template <imgui_compat::detail::combo_range Range>
	inline imgui_compat::detail::combo_result_t<Range> imgui_context::window_scope::combo(
	    const char* label, Range&& values) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui combo labels cannot be null");
		}
		return combo(std::string_view(label), std::forward<Range>(values));
	}

	template <imgui_compat::detail::scalar_numeric_value Label, imgui_compat::detail::combo_range Range>
	inline imgui_compat::detail::combo_result_t<Range> imgui_context::window_scope::combo(
	    const Label& label, Range&& values) const
	{
		return combo(imgui_compat::detail::format_scalar_value(label), std::forward<Range>(values));
	}

	template <imgui_compat::detail::combo_range Range>
	inline imgui_compat::detail::combo_result_t<Range> imgui_context::window_scope::combo(
	    bool label, Range&& values) const
	{
		return combo(imgui_compat::detail::format_scalar_value(label), std::forward<Range>(values));
	}

	template <imgui_compat::detail::combo_range Range>
	inline imgui_compat::detail::context_menu_result_t<Range> imgui_context::window_scope::context_menu(
	    Range&& values) const
	{
		using result_type = imgui_compat::detail::context_menu_result_t<Range>;

		if (!owner || !owner->context || !visible)
		{
			return {};
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui context menu must be submitted inside an active frame");
		}

		owner->make_current();
		if (!ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
		{
			return {};
		}

		struct popup_end_guard
		{
			~popup_end_guard()
			{
				if (active)
				{
					ImGui::EndPopup();
				}
			}

			void close() noexcept
			{
				if (active)
				{
					ImGui::EndPopup();
					active = false;
				}
			}

			bool active = true;
		} popup_guard;

		std::vector<std::string> item_labels;
		if constexpr (std::ranges::sized_range<Range>)
		{
			const auto item_count = std::ranges::size(values);
			if (std::cmp_greater(item_count, std::numeric_limits<int>::max()))
			{
				throw std::length_error("Dear ImGui context menu supports at most INT_MAX items");
			}
			item_labels.reserve(static_cast<std::size_t>(item_count));
		}

		for (auto iterator = std::ranges::begin(values); iterator != std::ranges::end(values); ++iterator)
		{
			if (item_labels.size() == static_cast<std::size_t>(std::numeric_limits<int>::max()))
			{
				throw std::length_error("Dear ImGui context menu supports at most INT_MAX items");
			}
			std::string item_label = imgui_compat::detail::combo_item_to_string(*iterator);
			if (item_label.find('\0') != std::string::npos)
			{
				throw std::invalid_argument(
				    "Dear ImGui context menu item labels cannot contain embedded null characters");
			}
			item_labels.push_back(std::move(item_label));
		}

		int selected_index = -1;
		if (item_labels.empty())
		{
			ImGui::TextUnformatted("(no actions)");
		}
		else
		{
			for (std::size_t index = 0; index < item_labels.size(); ++index)
			{
				ImGui::PushID(static_cast<int>(index));
				if (ImGui::MenuItem(item_labels[index].c_str()))
				{
					selected_index = static_cast<int>(index);
				}
				ImGui::PopID();
			}
		}
		popup_guard.close();

		if (selected_index < 0)
		{
			return {};
		}

		auto selected_iterator = std::ranges::begin(values);
		std::ranges::advance(selected_iterator, selected_index);
		if constexpr (std::ranges::borrowed_range<Range>)
		{
			return result_type{std::move(selected_iterator)};
		}
		else if constexpr (imgui_compat::detail::combo_copyable_result<Range>)
		{
			return result_type{std::in_place, *selected_iterator};
		}
		else
		{
			return result_type{std::in_place, static_cast<std::size_t>(selected_index)};
		}
	}

	template <typename T>
	    requires imgui_compat::detail::combo_range<std::initializer_list<T>>
	inline imgui_compat::detail::context_menu_result_t<std::initializer_list<T>> imgui_context::window_scope::
	    context_menu(std::initializer_list<T>&& values) const
	{
		return context_menu<std::initializer_list<T>>(std::move(values));
	}

	template <imgui_compat::detail::scalar_input_value T>
	inline bool imgui_context::window_scope::slider(std::string_view label, T& value,
	                                                std::type_identity_t<T> min,
	                                                std::type_identity_t<T> max) const
	{
		if (!owner || !owner->context || !visible)
		{
			return false;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui slider must be submitted inside an active frame");
		}

		owner->make_current();
		const std::string terminated_label(label);
		return imgui_compat::detail::slider_scalar(terminated_label.c_str(), value, min, max);
	}

	template <imgui_compat::detail::scalar_input_value T>
	inline bool imgui_context::window_scope::slider(const char* label, T& value, std::type_identity_t<T> min,
	                                                std::type_identity_t<T> max) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui slider label cannot be null");
		}
		return slider(std::string_view(label), value, min, max);
	}

	template <imgui_compat::detail::scalar_numeric_value Label, imgui_compat::detail::scalar_input_value T>
	inline bool imgui_context::window_scope::slider(const Label& label, T& value, std::type_identity_t<T> min,
	                                                std::type_identity_t<T> max) const
	{
		return slider(imgui_compat::detail::format_scalar_value(label), value, min, max);
	}

	template <imgui_compat::detail::scalar_input_value T>
	inline bool imgui_context::window_scope::slider(bool label, T& value, std::type_identity_t<T> min,
	                                                std::type_identity_t<T> max) const
	{
		return slider(imgui_compat::detail::format_scalar_value(label), value, min, max);
	}

	template <imgui_compat::detail::glm_slider_vector Vector>
	inline bool imgui_context::window_scope::slider(
	    std::string_view label, Vector& value,
	    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type min,
	    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type max)
	    const
	{
		if (!owner || !owner->context || !visible)
		{
			return false;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui vector slider must be submitted inside an active frame");
		}
		if (label.find('\0') != std::string_view::npos)
		{
			throw std::invalid_argument(
			    "Dear ImGui vector slider labels cannot contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_label(label);
		return imgui_compat::detail::slider_vector(terminated_label.c_str(), value, min, max);
	}

	template <imgui_compat::detail::glm_slider_vector Vector>
	inline bool imgui_context::window_scope::slider(
	    const char* label, Vector& value,
	    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type min,
	    typename imgui_compat::detail::glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type max)
	    const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui vector slider labels cannot be null");
		}
		return slider(std::string_view(label), value, min, max);
	}

	inline bool imgui_context::window_scope::input(std::string_view label, std::string& value,
	                                               const std::regex& pattern) const
	{
		if (!owner || !owner->context || !visible)
		{
			return false;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui input must be submitted inside an active frame");
		}
		if (value.find('\0') != std::string::npos)
		{
			throw std::invalid_argument("Dear ImGui text input does not support embedded null characters");
		}
		if (!std::regex_match(value, pattern))
		{
			throw std::invalid_argument(
			    "The current Dear ImGui input value does not match its regular expression");
		}
		if (value.capacity() >= static_cast<std::size_t>(std::numeric_limits<int>::max()))
		{
			throw std::length_error("Dear ImGui text input exceeds its buffer size limit");
		}

		const std::string previous_value = value;
		imgui_compat::detail::regex_input_state state{&value, &pattern, value};
		const std::string terminated_label(label);
		const bool edited =
		    ImGui::InputText(terminated_label.c_str(), value.data(), value.capacity() + 1,
			                 ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackEdit,
			                 imgui_compat::detail::regex_input_callback, &state);
		return edited && value != previous_value;
	}

	template <imgui_compat::detail::scalar_input_value T>
	inline bool imgui_context::window_scope::input(std::string_view label, T& value) const
	{
		if (!owner || !owner->context || !visible)
		{
			return false;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui input must be submitted inside an active frame");
		}

		owner->make_current();
		const std::string terminated_label(label);
		return imgui_compat::detail::input_scalar(terminated_label.c_str(), value);
	}

	inline bool imgui_context::window_scope::input(std::string_view label, bool& value) const
	{
		if (!owner || !owner->context || !visible)
		{
			return false;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui input must be submitted inside an active frame");
		}

		owner->make_current();
		const std::string terminated_label(label);
		return ImGui::Checkbox(terminated_label.c_str(), &value);
	}

	inline bool imgui_context::window_scope::checkbox(std::string_view label, bool& value) const
	{
		return input(label, value);
	}

	inline bool imgui_context::window_scope::checkbox(const char* label, bool& value) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui checkbox labels cannot be null");
		}
		return checkbox(std::string_view(label), value);
	}

	inline void imgui_context::window_scope::image(const ::mngl::texture& source,
	                                               glm::vec2 display_size) const
	{
		if (!owner || !owner->context || !visible)
		{
			return;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui image must be submitted inside an active frame");
		}
		if (!source.valid())
		{
			throw std::invalid_argument("Cannot display an invalid MNGL texture in Dear ImGui");
		}

		const gpu::texture& gpu_texture = source.raw_gpu_texture();
		if (!gpu_texture.has_usage(gpu::texture::usage_flags::sampler))
		{
			throw std::invalid_argument("Dear ImGui requires a sampleable MNGL texture");
		}
		if (source.rendering() || gpu_texture.is_swapchain_texture())
		{
			throw std::logic_error("Cannot display a texture while it is being rendered to");
		}
		if (!owner->attached_window || gpu_texture.device_raw() != owner->attached_window->gpu_device().raw())
		{
			throw std::invalid_argument(
			    "The MNGL texture and Dear ImGui context must use the same GPU device");
		}
		if (!std::isfinite(display_size.x) || !std::isfinite(display_size.y) || display_size.x <= 0.0f ||
		    display_size.y <= 0.0f)
		{
			throw std::invalid_argument("Dear ImGui image dimensions must be finite and positive");
		}

		owner->make_current();
		const ImTextureID texture_id = reinterpret_cast<ImTextureID>(gpu_texture.raw());
		ImGui::Image(ImTextureRef(texture_id), ImVec2(display_size.x, display_size.y));
	}

	inline imgui_context::font_scope imgui_context::window_scope::use_font(imgui_compat::font_role role,
	                                                                       float size_pixels) const
	{
		if (!owner || !owner->context || !owner->frame_active)
		{
			throw std::logic_error("An ImGui font must be selected inside an active frame");
		}
		if (!visible)
		{
			return {};
		}

		const loaded_font& selected_font = owner->get_font(role);
		if (size_pixels == 0.0f)
		{
			size_pixels = selected_font.size_pixels;
		}
		return font_scope(*owner, selected_font.value, size_pixels);
	}

	inline imgui_context::font_scope::font_scope(imgui_context& owner, ImFont* font, float size_pixels)
	    : owner(&owner)
	{
		if (!font)
		{
			throw std::logic_error("Cannot select an uninitialized Dear ImGui font");
		}
		if (!std::isfinite(size_pixels) || size_pixels <= 0.0f)
		{
			throw std::invalid_argument("Dear ImGui font size must be finite and positive");
		}

		owner.make_current();
		ImGui::PushFont(font, size_pixels);
	}

	inline imgui_context::font_scope::~font_scope() noexcept
	{
		if (owner && owner->context)
		{
			owner->make_current();
			ImGui::PopFont();
		}
	}

	inline imgui_context::frame_scope::~frame_scope() noexcept
	{
		if (owner)
		{
			owner->finish_frame();
		}
	}

	inline imgui_context::main_menu_bar_scope imgui_context::frame_scope::main_menu_bar() const
	{
		if (!owner)
		{
			throw std::logic_error("Cannot create a menu bar from an inactive frame scope");
		}
		return main_menu_bar_scope(*owner);
	}

	inline imgui_context::window_scope imgui_context::frame_scope::window(std::string_view title) const
	{
		if (!owner)
		{
			throw std::logic_error("Cannot create an ImGui window from an inactive frame scope");
		}
		return window_scope(*owner, title);
	}

	inline imgui_context::menu_scope imgui_context::main_menu_bar_scope::menu(std::string_view label) const
	{
		if (!owner || !visible)
		{
			return {};
		}
		return menu_scope(*owner, label);
	}
} // namespace mngl
