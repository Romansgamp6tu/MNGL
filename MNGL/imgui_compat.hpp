#pragma once

#include "input.hpp"
#include "render_window.hpp"

#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_sdlgpu3.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <optional>
#include <regex>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mngl::imgui_compat
{
	enum class font_role : std::uint8_t
	{
		regular,
		bold,
		italic,
		bold_italic,
		monospace
	};

	struct font_source
	{
		std::filesystem::path file;
		float size_pixels = 18.0f;
	};

	struct font_config
	{
		font_source regular{};
		font_source bold{};
		font_source italic{};
		font_source bold_italic{};
		font_source monospace{};
	};
}

namespace mngl::imgui_compat::detail
{
	namespace combo_string_adl
	{
		using std::to_string;

		template<typename T>
		[[nodiscard]] auto convert(const T& value)
			-> decltype(to_string(value))
		{
			return to_string(value);
		}
	}

	template<typename T>
	concept combo_stringifiable =
		std::convertible_to<T, std::string_view> ||
		requires(const std::remove_reference_t<T>& value)
		{
			std::string{ combo_string_adl::convert(value) };
		};

	template<typename T>
	[[nodiscard]] std::string combo_item_to_string(T&& value)
	{
		if constexpr (std::convertible_to<T, std::string_view>)
		{
			if constexpr (
				std::is_pointer_v<std::remove_reference_t<T>> &&
				std::same_as<
					std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>,
					char
				>)
			{
				if (!value)
				{
					throw std::invalid_argument("Dear ImGui option text cannot be null");
				}
			}
			return std::string(std::string_view(std::forward<T>(value)));
		}
		else
		{
			return std::string(combo_string_adl::convert(value));
		}
	}

	template<typename Range>
	concept combo_range =
		std::ranges::forward_range<Range> &&
		combo_stringifiable<std::ranges::range_reference_t<Range>>;

	template<typename Range>
	concept combo_copyable_result =
		std::copy_constructible<std::ranges::range_value_t<Range>> &&
		std::constructible_from<
			std::ranges::range_value_t<Range>,
			std::ranges::range_reference_t<Range>
		>;

	template<typename Range>
	struct combo_result
	{
		using iterator_type = std::ranges::iterator_t<Range>;
		using value_type = std::ranges::range_value_t<Range>;
		using type = std::conditional_t<
			std::ranges::borrowed_range<Range>,
			std::optional<iterator_type>,
			std::conditional_t<
				combo_copyable_result<Range>,
				std::optional<value_type>,
				std::optional<std::size_t>
			>
		>;
	};

	template<typename Range>
	using combo_result_t = typename combo_result<Range>::type;

	template<typename Range>
	using context_menu_result_t = combo_result_t<Range>;

	template<typename T>
	concept scalar_numeric_value =
		!std::is_volatile_v<T> &&
		(
			(
				std::integral<std::remove_cv_t<T>> &&
				!std::same_as<std::remove_cv_t<T>, bool> &&
				(
					sizeof(std::remove_cv_t<T>) == 1 ||
					sizeof(std::remove_cv_t<T>) == 2 ||
					sizeof(std::remove_cv_t<T>) == 4 ||
					sizeof(std::remove_cv_t<T>) == 8
				)
			) ||
			std::same_as<std::remove_cv_t<T>, float> ||
			std::same_as<std::remove_cv_t<T>, double>
		);

	template<typename T>
	concept scalar_input_value =
		scalar_numeric_value<T> &&
		!std::is_const_v<T> &&
		!std::is_volatile_v<T>;

	template<typename T>
	struct glm_slider_vector_traits
	{
		static constexpr bool valid = false;
	};

	template<glm::length_t Length, typename Scalar, glm::qualifier Qualifier>
	struct glm_slider_vector_traits<glm::vec<Length, Scalar, Qualifier>>
	{
		using scalar_type = Scalar;
		static constexpr glm::length_t length = Length;
		static constexpr bool valid =
			Length >= 2 &&
			Length <= 4 &&
			scalar_input_value<Scalar>;
	};

	template<typename T>
	concept glm_slider_vector =
		!std::is_const_v<T> &&
		!std::is_volatile_v<T> &&
		glm_slider_vector_traits<std::remove_cv_t<T>>::valid;

	template<scalar_numeric_value T>
	[[nodiscard]] inline std::string format_scalar_value(const T& value)
	{
		if constexpr (std::integral<std::remove_cv_t<T>>)
		{
			if constexpr (std::is_signed_v<std::remove_cv_t<T>>)
			{
				return std::format("{}", static_cast<std::intmax_t>(value));
			}
			else
			{
				return std::format("{}", static_cast<std::uintmax_t>(value));
			}
		}
		else
		{
			return std::format("{}", value);
		}
	}

	[[nodiscard]] inline std::string format_scalar_value(bool value)
	{
		return value ? "1" : "0";
	}

	template<typename T>
	concept widget_label_value =
		!std::is_volatile_v<T> &&
		(
			scalar_numeric_value<T> ||
			std::same_as<std::remove_cv_t<T>, bool>
		);

	template<widget_label_value T>
	[[nodiscard]] inline std::string format_widget_label(const T& value)
	{
		return format_scalar_value(value);
	}

	inline void validate_progress_fraction(float fraction)
	{
		if (!std::isfinite(fraction) || fraction < 0.0f || fraction > 1.0f)
		{
			throw std::invalid_argument("Dear ImGui progress fraction must be finite and between 0 and 1");
		}
	}

	template<scalar_input_value T>
	[[nodiscard]] constexpr ImGuiDataType scalar_input_data_type() noexcept
	{
		using value_type = std::remove_cv_t<T>;
		if constexpr (std::same_as<value_type, float>)
		{
			return ImGuiDataType_Float;
		}
		else if constexpr (std::same_as<value_type, double>)
		{
			return ImGuiDataType_Double;
		}
		else if constexpr (sizeof(value_type) == 1)
		{
			return std::is_signed_v<value_type> ? ImGuiDataType_S8 : ImGuiDataType_U8;
		}
		else if constexpr (sizeof(value_type) == 2)
		{
			return std::is_signed_v<value_type> ? ImGuiDataType_S16 : ImGuiDataType_U16;
		}
		else if constexpr (sizeof(value_type) == 4)
		{
			return std::is_signed_v<value_type> ? ImGuiDataType_S32 : ImGuiDataType_U32;
		}
		else
		{
			return std::is_signed_v<value_type> ? ImGuiDataType_S64 : ImGuiDataType_U64;
		}
	}

	template<typename T>
	struct scalar_input_storage
	{
		using value_type = std::remove_cv_t<T>;
		using type = std::conditional_t<
			std::same_as<value_type, float>,
			float,
			std::conditional_t<
				std::same_as<value_type, double>,
				double,
				std::conditional_t<
					std::is_signed_v<value_type>,
					std::conditional_t<
						sizeof(value_type) == 1,
						ImS8,
						std::conditional_t<
							sizeof(value_type) == 2,
							ImS16,
							std::conditional_t<sizeof(value_type) == 4, ImS32, ImS64>
						>
					>,
					std::conditional_t<
						sizeof(value_type) == 1,
						ImU8,
						std::conditional_t<
							sizeof(value_type) == 2,
							ImU16,
							std::conditional_t<sizeof(value_type) == 4, ImU32, ImU64>
						>
					>
				>
			>
		>;
	};

	template<glm_slider_vector Vector>
	[[nodiscard]] bool slider_vector(
		const char* label,
		Vector& value,
		typename glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type min,
		typename glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type max
	)
	{
		using traits = glm_slider_vector_traits<std::remove_cv_t<Vector>>;
		using scalar_type = typename traits::scalar_type;
		using storage_type = typename scalar_input_storage<scalar_type>::type;

		if constexpr (std::floating_point<scalar_type>)
		{
			if (!std::isfinite(min) || !std::isfinite(max))
			{
				throw std::invalid_argument("Dear ImGui vector slider bounds must be finite");
			}
		}
		if (!(min < max))
		{
			throw std::invalid_argument("Dear ImGui vector slider minimum must be less than its maximum");
		}

		std::array<storage_type, static_cast<std::size_t>(traits::length)> components{};
		for (glm::length_t index = 0; index < traits::length; ++index)
		{
			const scalar_type component = value[index];
			if constexpr (std::floating_point<scalar_type>)
			{
				if (!std::isfinite(component))
				{
					throw std::invalid_argument("Dear ImGui vector slider values must be finite");
				}
			}
			if (component < min || component > max)
			{
				throw std::invalid_argument("Dear ImGui vector slider values must be within their range");
			}
			components[static_cast<std::size_t>(index)] =
				static_cast<storage_type>(component);
		}

		const storage_type scalar_min = static_cast<storage_type>(min);
		const storage_type scalar_max = static_cast<storage_type>(max);
		bool changed = false;
		if constexpr (std::same_as<scalar_type, float>)
		{
			if constexpr (traits::length == 2)
			{
				changed = ImGui::SliderFloat2(label, components.data(), min, max);
			}
			else if constexpr (traits::length == 3)
			{
				changed = ImGui::SliderFloat3(label, components.data(), min, max);
			}
			else
			{
				changed = ImGui::SliderFloat4(label, components.data(), min, max);
			}
		}
		else
		{
			changed = ImGui::SliderScalarN(
				label,
				scalar_input_data_type<scalar_type>(),
				components.data(),
				static_cast<int>(traits::length),
				&scalar_min,
				&scalar_max);
		}
		if (!changed)
		{
			return false;
		}

		for (glm::length_t index = 0; index < traits::length; ++index)
		{
			value[index] = static_cast<scalar_type>(
				components[static_cast<std::size_t>(index)]);
		}
		return true;
	}

	template<scalar_input_value T>
	[[nodiscard]] bool input_scalar(const char* label, T& value)
	{
		using storage_type = typename scalar_input_storage<T>::type;
		storage_type scalar = static_cast<storage_type>(value);
		if (!ImGui::InputScalar(label, scalar_input_data_type<T>(), &scalar))
		{
			return false;
		}
		value = static_cast<T>(scalar);
		return true;
	}

	template<scalar_input_value T>
	[[nodiscard]] bool slider_scalar(
		const char* label,
		T& value,
		std::type_identity_t<T> min,
		std::type_identity_t<T> max
	)
	{
		if constexpr (std::floating_point<std::remove_cv_t<T>>)
		{
			if (!std::isfinite(value) || !std::isfinite(min) || !std::isfinite(max))
			{
				throw std::invalid_argument("Dear ImGui slider values must be finite");
			}
		}
		if (!(min < max))
		{
			throw std::invalid_argument("Dear ImGui slider minimum must be less than its maximum");
		}
		if (value < min || value > max)
		{
			throw std::invalid_argument("Dear ImGui slider value must be within its range");
		}

		using storage_type = typename scalar_input_storage<T>::type;
		storage_type scalar = static_cast<storage_type>(value);
		const storage_type scalar_min = static_cast<storage_type>(min);
		const storage_type scalar_max = static_cast<storage_type>(max);
		if (!ImGui::SliderScalar(
			label,
			scalar_input_data_type<T>(),
			&scalar,
			&scalar_min,
			&scalar_max))
		{
			return false;
		}
		value = static_cast<T>(scalar);
		return true;
	}

	struct regex_input_state
	{
		std::string* value;
		const std::regex* pattern;
		std::string accepted_value;
	};

	inline int regex_input_callback(ImGuiInputTextCallbackData* data)
	{
		auto& state = *static_cast<regex_input_state*>(data->UserData);
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			state.value->resize(static_cast<std::size_t>(data->BufTextLen));
			data->Buf = state.value->data();
			return 0;
		}
		if (data->EventFlag != ImGuiInputTextFlags_CallbackEdit)
		{
			return 0;
		}

		const std::string_view candidate(
			data->Buf,
			static_cast<std::size_t>(data->BufTextLen));
		if (std::regex_match(candidate.begin(), candidate.end(), *state.pattern))
		{
			state.accepted_value.assign(candidate);
			return 0;
		}

		if (state.accepted_value.size() >= static_cast<std::size_t>(data->BufSize))
		{
			throw std::logic_error("Dear ImGui input buffer cannot restore its last matching value");
		}

		const auto accepted_size = static_cast<int>(state.accepted_value.size());
		std::memcpy(data->Buf, state.accepted_value.data(), state.accepted_value.size());
		data->Buf[accepted_size] = '\0';
		data->BufTextLen = accepted_size;
		data->CursorPos = std::clamp(data->CursorPos, 0, accepted_size);
		data->SelectionStart = data->CursorPos;
		data->SelectionEnd = data->CursorPos;
		data->BufDirty = true;
		return 0;
	}

	inline std::string path_to_utf8(const std::filesystem::path& path)
	{
		const auto encoded_path = path.u8string();
		return std::string(
			reinterpret_cast<const char*>(encoded_path.data()),
			encoded_path.size());
	}

	inline ImFont* load_font(
		ImFontAtlas& atlas,
		const font_source& source,
		ImFont* fallback = nullptr
	)
	{
		if (!std::isfinite(source.size_pixels) || source.size_pixels <= 0.0f)
		{
			throw std::invalid_argument("ImGui font size must be finite and positive");
		}
		if (source.file.empty())
		{
			if (fallback)
			{
				return fallback;
			}

			ImFontConfig default_config{};
			default_config.SizePixels = source.size_pixels;
			ImFont* font = atlas.AddFontDefaultVector(&default_config);
			if (!font)
			{
				throw std::runtime_error("Failed to load Dear ImGui's embedded vector font");
			}
			return font;
		}

		const std::string filename = path_to_utf8(source.file);
		ImFontConfig file_config{};
		file_config.Flags |= ImFontFlags_NoLoadError;
		static const ImWchar latin_cyrillic_ranges[] = {
			0x0020, 0x00FF,
			0x0400, 0x052F,
			0
		};
		file_config.GlyphRanges = latin_cyrillic_ranges;
		ImFont* font = atlas.AddFontFromFileTTF(
			filename.c_str(),
			source.size_pixels,
			&file_config);
		if (!font)
		{
			throw std::runtime_error("Failed to load Dear ImGui font: " + filename);
		}
		return font;
	}

	inline void apply_vs_dark_style(float scale = 1.0f)
	{
		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowPadding = ImVec2(10.0f, 10.0f);
		style.FramePadding = ImVec2(8.0f, 5.0f);
		style.ItemSpacing = ImVec2(8.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
		style.CellPadding = ImVec2(6.0f, 4.0f);
		style.WindowRounding = 4.0f;
		style.ChildRounding = 3.0f;
		style.FrameRounding = 3.0f;
		style.PopupRounding = 4.0f;
		style.ScrollbarRounding = 4.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 4.0f;
		style.WindowBorderSize = 1.0f;

		if (scale > 0.0f && std::isfinite(scale) && std::abs(scale - 1.0f) > 0.001f)
		{
			style.ScaleAllSizes(scale);
		}
		style.FontScaleDpi = (scale > 0.0f && std::isfinite(scale)) ? scale : 1.0f;

		const auto color = [](ImU32 value, float alpha = 1.0f)
		{
			return ImVec4(
				static_cast<float>((value >> 16) & 0xFF) / 255.0f,
				static_cast<float>((value >> 8) & 0xFF) / 255.0f,
				static_cast<float>(value & 0xFF) / 255.0f,
				alpha);
		};
		const ImVec4 accent = color(0x007ACC);
		auto& colors = style.Colors;
		colors[ImGuiCol_Text] = color(0xF1F1F1);
		colors[ImGuiCol_TextDisabled] = color(0x9D9D9D);
		colors[ImGuiCol_WindowBg] = color(0x1E1E1E, 0.98f);
		colors[ImGuiCol_ChildBg] = color(0x1E1E1E, 0.0f);
		colors[ImGuiCol_PopupBg] = color(0x252526, 0.98f);
		colors[ImGuiCol_Border] = color(0x454545);
		colors[ImGuiCol_BorderShadow] = color(0x000000, 0.0f);
		colors[ImGuiCol_FrameBg] = color(0x333337);
		colors[ImGuiCol_FrameBgHovered] = color(0x3E3E42);
		colors[ImGuiCol_FrameBgActive] = color(0x45454A);
		colors[ImGuiCol_TitleBg] = color(0x252526);
		colors[ImGuiCol_TitleBgActive] = color(0x2D2D30);
		colors[ImGuiCol_TitleBgCollapsed] = color(0x252526);
		colors[ImGuiCol_MenuBarBg] = color(0x2D2D30);
		colors[ImGuiCol_ScrollbarBg] = color(0x1E1E1E);
		colors[ImGuiCol_ScrollbarGrab] = color(0x3E3E42);
		colors[ImGuiCol_ScrollbarGrabHovered] = color(0x56565A);
		colors[ImGuiCol_ScrollbarGrabActive] = color(0x68686E);
		colors[ImGuiCol_CheckMark] = color(0x4FC1FF);
		colors[ImGuiCol_CheckboxSelectedBg] = color(0x007ACC);
		colors[ImGuiCol_SliderGrab] = accent;
		colors[ImGuiCol_SliderGrabActive] = color(0x1C97EA);
		colors[ImGuiCol_Button] = color(0x333337);
		colors[ImGuiCol_ButtonHovered] = color(0x3E3E42);
		colors[ImGuiCol_ButtonActive] = color(0x005A9E);
		colors[ImGuiCol_Header] = color(0x2D2D30);
		colors[ImGuiCol_HeaderHovered] = color(0x264F78);
		colors[ImGuiCol_HeaderActive] = accent;
		colors[ImGuiCol_Separator] = color(0x454545);
		colors[ImGuiCol_SeparatorHovered] = color(0x4FC1FF);
		colors[ImGuiCol_SeparatorActive] = accent;
		colors[ImGuiCol_ResizeGrip] = color(0x007ACC, 0.20f);
		colors[ImGuiCol_ResizeGripHovered] = color(0x007ACC, 0.65f);
		colors[ImGuiCol_ResizeGripActive] = color(0x007ACC, 0.95f);
		colors[ImGuiCol_InputTextCursor] = color(0x4FC1FF);
		colors[ImGuiCol_TabHovered] = color(0x3E3E42);
		colors[ImGuiCol_Tab] = color(0x252526);
		colors[ImGuiCol_TabSelected] = color(0x1E1E1E);
		colors[ImGuiCol_TabSelectedOverline] = accent;
		colors[ImGuiCol_TabDimmed] = color(0x2D2D30);
		colors[ImGuiCol_TabDimmedSelected] = color(0x252526);
		colors[ImGuiCol_TabDimmedSelectedOverline] = color(0x007ACC, 0.55f);
		colors[ImGuiCol_DockingPreview] = color(0x007ACC, 0.35f);
		colors[ImGuiCol_DockingEmptyBg] = color(0x1E1E1E);
		colors[ImGuiCol_PlotLines] = color(0x75BEFF);
		colors[ImGuiCol_PlotLinesHovered] = color(0x4FC1FF);
		colors[ImGuiCol_PlotHistogram] = color(0xB180D7);
		colors[ImGuiCol_PlotHistogramHovered] = color(0xD7BA7D);
		colors[ImGuiCol_TableHeaderBg] = color(0x2D2D30);
		colors[ImGuiCol_TableBorderStrong] = color(0x454545);
		colors[ImGuiCol_TableBorderLight] = color(0x383838);
		colors[ImGuiCol_TableRowBg] = color(0x000000, 0.0f);
		colors[ImGuiCol_TableRowBgAlt] = color(0xFFFFFF, 0.025f);
		colors[ImGuiCol_TextLink] = color(0x4FC1FF);
		colors[ImGuiCol_TextSelectedBg] = color(0x007ACC, 0.40f);
		colors[ImGuiCol_TreeLines] = color(0x454545);
		colors[ImGuiCol_DragDropTarget] = color(0x4FC1FF);
		colors[ImGuiCol_DragDropTargetBg] = color(0x007ACC, 0.20f);
		colors[ImGuiCol_UnsavedMarker] = color(0xDCDCAA);
		colors[ImGuiCol_NavCursor] = color(0x4FC1FF);
		colors[ImGuiCol_NavWindowingHighlight] = color(0xFFFFFF, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = color(0x1E1E1E, 0.70f);
		colors[ImGuiCol_ModalWindowDimBg] = color(0x1E1E1E, 0.75f);
	}
}

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
			template<imgui_compat::detail::scalar_numeric_value T>
			void text(const T& value) const;
			void text(bool value) const;
			[[nodiscard]] bool button(std::string_view label) const;
			[[nodiscard]] bool button(const char* label) const;
			[[nodiscard]] bool button(std::nullptr_t) const = delete;
			template<imgui_compat::detail::scalar_numeric_value T>
			[[nodiscard]] bool button(const T& label) const;
			[[nodiscard]] bool button(bool label) const;
			void progress_bar(float fraction) const;
			void progress_bar(float fraction, std::string_view overlay) const;
			[[nodiscard]] bool selectable(std::string_view label, bool selected = false) const;
			[[nodiscard]] bool selectable(const char* label, bool selected = false) const;
			[[nodiscard]] bool selectable(std::nullptr_t, bool = false) const = delete;
			template<imgui_compat::detail::widget_label_value Label>
			[[nodiscard]] bool selectable(const Label& label, bool selected = false) const
			{
				return selectable(
					imgui_compat::detail::format_widget_label(label),
					selected);
			}
			void bullet() const;
			void bullet(std::string_view label) const;
			void bullet(const char* label) const;
			void bullet(std::nullptr_t) const = delete;
			template<imgui_compat::detail::widget_label_value Label>
			void bullet(const Label& label) const
			{
				bullet(imgui_compat::detail::format_widget_label(label));
			}
			[[nodiscard]] bool radio_button(std::string_view label, bool active) const;
			[[nodiscard]] bool radio_button(const char* label, bool active) const;
			[[nodiscard]] bool radio_button(std::nullptr_t, bool) const = delete;
			template<imgui_compat::detail::widget_label_value Label>
			[[nodiscard]] bool radio_button(const Label& label, bool active) const
			{
				return radio_button(
					imgui_compat::detail::format_widget_label(label),
					active);
			}
			template<typename T>
				requires std::equality_comparable<T> &&
					std::assignable_from<T&, std::type_identity_t<T>>
			[[nodiscard]] bool radio_button(
				std::string_view label,
				T& selected,
				std::type_identity_t<T> value
			) const;
			template<
				imgui_compat::detail::widget_label_value Label,
				typename T
			>
				requires std::equality_comparable<T> &&
					std::assignable_from<T&, std::type_identity_t<T>>
			[[nodiscard]] bool radio_button(
				const Label& label,
				T& selected,
				std::type_identity_t<T> value
			) const
			{
				return radio_button(
					imgui_compat::detail::format_widget_label(label),
					selected,
					value);
			}
			template<imgui_compat::detail::combo_range Range>
			[[nodiscard]] imgui_compat::detail::combo_result_t<Range> combo(
				std::string_view label,
				Range&& values
			) const;
			template<typename T>
				requires imgui_compat::detail::combo_range<std::initializer_list<T>>
			[[nodiscard]] imgui_compat::detail::combo_result_t<std::initializer_list<T>> combo(
				std::string_view label,
				std::initializer_list<T>&& values
			) const;
			template<
				imgui_compat::detail::scalar_numeric_value Label,
				typename T
			>
				requires imgui_compat::detail::combo_range<std::initializer_list<T>>
			[[nodiscard]] imgui_compat::detail::combo_result_t<std::initializer_list<T>> combo(
				const Label& label,
				std::initializer_list<T>&& values
			) const;
			template<typename T>
				requires imgui_compat::detail::combo_range<std::initializer_list<T>>
			[[nodiscard]] imgui_compat::detail::combo_result_t<std::initializer_list<T>> combo(
				bool label,
				std::initializer_list<T>&& values
			) const;
			template<imgui_compat::detail::combo_range Range>
			[[nodiscard]] imgui_compat::detail::combo_result_t<Range> combo(
				const char* label,
				Range&& values
			) const;
			template<imgui_compat::detail::scalar_numeric_value Label, imgui_compat::detail::combo_range Range>
			[[nodiscard]] imgui_compat::detail::combo_result_t<Range> combo(
				const Label& label,
				Range&& values
			) const;
			template<imgui_compat::detail::combo_range Range>
			[[nodiscard]] imgui_compat::detail::combo_result_t<Range> combo(
				bool label,
				Range&& values
			) const;
			template<imgui_compat::detail::combo_range Range>
			imgui_compat::detail::combo_result_t<Range> combo(
				std::nullptr_t,
				Range&& values
			) const = delete;
			template<imgui_compat::detail::combo_range Range>
			[[nodiscard]] imgui_compat::detail::context_menu_result_t<Range> context_menu(
				Range&& values
			) const;
			template<typename T>
				requires imgui_compat::detail::combo_range<std::initializer_list<T>>
			[[nodiscard]] imgui_compat::detail::context_menu_result_t<
				std::initializer_list<T>> context_menu(
					std::initializer_list<T>&& values
				) const;
			template<imgui_compat::detail::glm_slider_vector Vector>
			[[nodiscard]] bool slider(
				std::string_view label,
				Vector& value,
				typename imgui_compat::detail::glm_slider_vector_traits<
					std::remove_cv_t<Vector>>::scalar_type min,
				typename imgui_compat::detail::glm_slider_vector_traits<
					std::remove_cv_t<Vector>>::scalar_type max
			) const;
			template<imgui_compat::detail::glm_slider_vector Vector>
			[[nodiscard]] bool slider(
				const char* label,
				Vector& value,
				typename imgui_compat::detail::glm_slider_vector_traits<
					std::remove_cv_t<Vector>>::scalar_type min,
				typename imgui_compat::detail::glm_slider_vector_traits<
					std::remove_cv_t<Vector>>::scalar_type max
			) const;
			template<
				imgui_compat::detail::scalar_numeric_value Label,
				imgui_compat::detail::glm_slider_vector Vector
			>
			[[nodiscard]] bool slider(
				const Label& label,
				Vector& value,
				typename imgui_compat::detail::glm_slider_vector_traits<
					std::remove_cv_t<Vector>>::scalar_type min,
				typename imgui_compat::detail::glm_slider_vector_traits<
					std::remove_cv_t<Vector>>::scalar_type max
			) const
			{
				return slider(
					imgui_compat::detail::format_scalar_value(label),
					value,
					min,
					max);
			}
			template<imgui_compat::detail::glm_slider_vector Vector>
			[[nodiscard]] bool slider(
				bool label,
				Vector& value,
				typename imgui_compat::detail::glm_slider_vector_traits<
					std::remove_cv_t<Vector>>::scalar_type min,
				typename imgui_compat::detail::glm_slider_vector_traits<
					std::remove_cv_t<Vector>>::scalar_type max
			) const
			{
				return slider(
					imgui_compat::detail::format_scalar_value(label),
					value,
					min,
					max);
			}
			template<imgui_compat::detail::glm_slider_vector Vector>
			[[nodiscard]] bool slider(
				std::nullptr_t,
				Vector& value,
				typename imgui_compat::detail::glm_slider_vector_traits<
					std::remove_cv_t<Vector>>::scalar_type min,
				typename imgui_compat::detail::glm_slider_vector_traits<
					std::remove_cv_t<Vector>>::scalar_type max
			) const = delete;
			template<imgui_compat::detail::scalar_input_value T>
			[[nodiscard]] bool slider(
				std::string_view label,
				T& value,
				std::type_identity_t<T> min,
				std::type_identity_t<T> max
			) const;
			template<imgui_compat::detail::scalar_input_value T>
			[[nodiscard]] bool slider(
				const char* label,
				T& value,
				std::type_identity_t<T> min,
				std::type_identity_t<T> max
			) const;
			template<
				imgui_compat::detail::scalar_numeric_value Label,
				imgui_compat::detail::scalar_input_value T
			>
			[[nodiscard]] bool slider(
				const Label& label,
				T& value,
				std::type_identity_t<T> min,
				std::type_identity_t<T> max
			) const;
			template<imgui_compat::detail::scalar_input_value T>
			[[nodiscard]] bool slider(
				bool label,
				T& value,
				std::type_identity_t<T> min,
				std::type_identity_t<T> max
			) const;
			template<imgui_compat::detail::scalar_input_value T>
			[[nodiscard]] bool slider(
				std::nullptr_t,
				T& value,
				std::type_identity_t<T> min,
				std::type_identity_t<T> max
			) const = delete;
			[[nodiscard]] bool input(
				std::string_view label,
				std::string& value,
				const std::regex& pattern
			) const;
			template<imgui_compat::detail::scalar_input_value T>
			[[nodiscard]] bool input(std::string_view label, T& value) const;
			[[nodiscard]] bool input(std::string_view label, bool& value) const;
			[[nodiscard]] bool checkbox(std::string_view label, bool& value) const;
			[[nodiscard]] bool checkbox(const char* label, bool& value) const;
			[[nodiscard]] bool checkbox(std::nullptr_t, bool&) const = delete;
			template<imgui_compat::detail::widget_label_value Label>
			[[nodiscard]] bool checkbox(const Label& label, bool& value) const
			{
				return checkbox(
					imgui_compat::detail::format_widget_label(label),
					value);
			}
			void image(const ::mngl::texture& source, glm::vec2 display_size) const;
			[[nodiscard]] dockspace_scope dockspace(std::string_view id) const;
			[[nodiscard]] font_scope use_font(
				imgui_compat::font_role role,
				float size_pixels = 0.0f
			) const;

		private:
			friend class frame_scope;
			friend class dockspace_scope;
			window_scope(
				imgui_context& owner,
				std::string_view title,
				std::optional<ImGuiID> dockspace_id = std::nullopt,
				bool* open = nullptr,
				ImGuiWindowFlags flags = 0);

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
			[[nodiscard]] window_scope window(
				std::string_view title,
				bool& open
			) const;

		private:
			friend class window_scope;
			dockspace_scope(imgui_context& owner, ImGuiID id) noexcept
				: owner(&owner)
				, id(id)
			{
			}

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
			[[nodiscard]] window_scope window(
				std::string_view title,
				ImGuiWindowFlags flags) const;

		private:
			friend class imgui_context;
			explicit frame_scope(imgui_context& owner) noexcept : owner(&owner) {}

			imgui_context* owner;
		};

		explicit imgui_context(
			render_window& window,
			const imgui_compat::font_config& fonts = {}
		);
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

	inline imgui_context::imgui_context(
		render_window& window,
		const imgui_compat::font_config& font_config
	)
	{
		if (!SDL_IsMainThread())
		{
			throw std::logic_error("Dear ImGui must be initialized on the SDL main thread");
		}
		if (ImGui::GetCurrentContext())
		{
			throw std::logic_error("MNGL currently supports one active Dear ImGui context");
		}

		color_target_format = window.gpu_device().swapchain_texture_format(window);
		if (color_target_format == gpu::texture_format::invalid)
		{
			throw std::runtime_error("Cannot initialize Dear ImGui for an invalid swapchain format");
		}

		context = ImGui::CreateContext();
		if (!context)
		{
			throw std::runtime_error("Failed to create a Dear ImGui context");
		}

		make_current();
		try
		{
			attached_window = &window;
			ImGuiIO& io = ImGui::GetIO();
			io.IniFilename = "MNGL_imgui.ini";
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			m_applied_scale = effective_scale();
			imgui_compat::detail::apply_vs_dark_style(m_applied_scale);
			initialize_fonts(font_config);

			if (!ImGui_ImplSDL3_InitForSDLGPU(window.raw()))
			{
				throw sdl_error("Failed to initialize the Dear ImGui SDL3 backend: ");
			}
			ImGui_ImplSDL3_SetMouseCaptureMode(
				ImGui_ImplSDL3_MouseCaptureMode_EnabledAfterDrag);

			ImGui_ImplSDLGPU3_InitInfo init_info{};
			init_info.Device = window.gpu_device().raw();
			init_info.ColorTargetFormat = static_cast<SDL_GPUTextureFormat>(color_target_format);
			init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
			if (!ImGui_ImplSDLGPU3_Init(&init_info))
			{
				throw sdl_error("Failed to initialize the Dear ImGui SDL_GPU backend: ");
			}

			event_manager::instance();
			event_manager::set_imgui_input_enabled(true);
			window.attach_overlay(*this);
			attached_window = &window;
		}
		catch (...)
		{
			shutdown();
			throw;
		}
	}

	inline void imgui_context::initialize_fonts(const imgui_compat::font_config& config)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImFontAtlas& atlas = *io.Fonts;
		fonts[0] = {
			imgui_compat::detail::load_font(atlas, config.regular),
			config.regular.size_pixels
		};
		fonts[1] = {
			imgui_compat::detail::load_font(atlas, config.bold, fonts[0].value),
			config.bold.size_pixels
		};
		fonts[2] = {
			imgui_compat::detail::load_font(atlas, config.italic, fonts[0].value),
			config.italic.size_pixels
		};
		fonts[3] = {
			imgui_compat::detail::load_font(atlas, config.bold_italic, fonts[0].value),
			config.bold_italic.size_pixels
		};
		fonts[4] = {
			imgui_compat::detail::load_font(atlas, config.monospace, fonts[0].value),
			config.monospace.size_pixels
		};

		io.FontDefault = fonts[0].value;
		ImGui::GetStyle().FontSizeBase = config.regular.size_pixels;
	}

	inline const imgui_context::loaded_font& imgui_context::get_font(
		imgui_compat::font_role role
	) const
	{
		const auto index = static_cast<std::size_t>(role);
		if (index >= fonts.size())
		{
			throw std::invalid_argument("Invalid Dear ImGui font role");
		}
		return fonts[index];
	}

	inline imgui_context::~imgui_context() noexcept
	{
		if (attached_window)
		{
			attached_window->detach_overlay(*this);
			attached_window = nullptr;
		}
		shutdown();
	}

	inline imgui_context::frame_scope imgui_context::frame()
	{
		begin_frame(nullptr);
		return frame_scope(*this);
	}

	inline imgui_context::frame_scope imgui_context::frame(
		::mngl::texture& target
	)
	{
		begin_frame(&target);
		return frame_scope(*this);
	}

	inline void imgui_context::begin_frame(::mngl::texture* target)
	{
		if (!context || !attached_window)
		{
			throw std::logic_error("Cannot begin a frame without an attached render_window");
		}
		if (frame_active)
		{
			throw std::logic_error("A Dear ImGui frame is already active");
		}
		if (!SDL_IsMainThread())
		{
			throw std::logic_error("Dear ImGui frames must run on the SDL main thread");
		}
		if (target)
		{
			if (!attached_window->hit_testing_enabled())
			{
				throw std::logic_error(
					"Enable render_window hit testing before drawing an ImGui surface sprite");
			}

			const auto& gpu_texture = target->raw_gpu_texture();
			if (!target->valid() || !target->can_render_to() ||
				!gpu_texture.has_usage(gpu::texture::usage_flags::sampler) ||
				target->rendering() || gpu_texture.is_swapchain_texture())
			{
				throw std::invalid_argument(
					"Dear ImGui offscreen output must be an idle, sampleable color-target texture");
			}
			if (gpu_texture.device_raw() != attached_window->gpu_device().raw())
			{
				throw std::invalid_argument(
					"Dear ImGui offscreen output and render_window must use the same GPU device");
			}
			if (target->format() != color_target_format)
			{
				throw std::invalid_argument(
					"Dear ImGui offscreen output must use the render_window swapchain format");
			}
		}

		make_current();
		if (target)
		{
			if (captured_texture_identity != target->m_hit_test_identity)
			{
				captured_texture_identity = target->m_hit_test_identity;
				captured_hit_test_path.clear();
				offscreen_mouse_captured = false;
				has_last_offscreen_mouse_position = false;
			}
		}
		else
		{
			captured_texture_identity = 0;
			captured_hit_test_path.clear();
			offscreen_mouse_captured = false;
			has_last_offscreen_mouse_position = false;
		}

		ImGui_ImplSDL3_NewFrame();
		ImGui_ImplSDLGPU3_NewFrame();

		const float needed_scale = effective_scale();
		if (std::abs(needed_scale - m_applied_scale) > 0.001f)
		{
			m_applied_scale = needed_scale;
			imgui_compat::detail::apply_vs_dark_style(m_applied_scale);
		}

		if (target)
		{
			const auto target_size = target->size();
			ImGuiIO& io = ImGui::GetIO();
			io.DisplaySize = ImVec2(
				static_cast<float>(target_size.x),
				static_cast<float>(target_size.y));
			io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
			const float offscreen_cursor = -std::numeric_limits<float>::max();
			io.AddMousePosEvent(offscreen_cursor, offscreen_cursor);
			bool cursor_routed = false;

			if (attached_window->m_hit_test_frame)
			{
				const auto& hit_frame = *attached_window->m_hit_test_frame;
				const auto logical_size = attached_window->size();
				if (logical_size.x != 0 && logical_size.y != 0)
				{
					const glm::vec2 mouse_position = Input::get_mouse_position();
					const glm::vec2 window_pixel_position{
						mouse_position.x * static_cast<float>(hit_frame.target_size.x) /
							static_cast<float>(logical_size.x),
						(static_cast<float>(logical_size.y) - mouse_position.y) *
							static_cast<float>(hit_frame.target_size.y) /
							static_cast<float>(logical_size.y)
					};
					auto add_routed_position = [&](const hit_test_path_entry& entry)
					{
						if (entry.source_size.x == 0 || entry.source_size.y == 0)
						{
							return false;
						}

						const glm::vec2 routed_position =
							entry.source_pixel_coords *
							glm::vec2(target_size) /
							glm::vec2(entry.source_size);
						last_offscreen_mouse_position = {
							routed_position.x,
							static_cast<float>(target_size.y) - routed_position.y
						};
						has_last_offscreen_mouse_position = true;
						io.AddMousePosEvent(
							last_offscreen_mouse_position.x,
							last_offscreen_mouse_position.y);
						return true;
					};

					if (offscreen_mouse_captured &&
						captured_texture_identity == target->m_hit_test_identity &&
						!captured_hit_test_path.empty())
					{
						if (const auto surface_path =
							detail::map_hit_test_path_to_source_unbounded(
								hit_frame,
								captured_hit_test_path,
								window_pixel_position,
								target->m_hit_test_identity))
						{
							cursor_routed = add_routed_position(*surface_path);
						}
					}
					else if (const auto hit = attached_window->hit_test(mouse_position))
					{
						const auto surface_path = std::find_if(
							hit->path.begin(),
							hit->path.end(),
							[target](const hit_test_path_entry& entry)
							{
								return entry.source_identity == target->m_hit_test_identity;
							});
						if (surface_path != hit->path.end() &&
							add_routed_position(*surface_path))
						{
							captured_hit_test_path = hit->path;
							cursor_routed = true;
						}
					}
				}
			}

			if (!cursor_routed && offscreen_mouse_captured &&
				has_last_offscreen_mouse_position)
			{
				io.AddMousePosEvent(
					last_offscreen_mouse_position.x,
					last_offscreen_mouse_position.y);
				cursor_routed = true;
			}
			if (!cursor_routed && !offscreen_mouse_captured)
			{
				captured_hit_test_path.clear();
				has_last_offscreen_mouse_position = false;
			}
		}
		ImGui::NewFrame();
		frame_active = true;
		draw_data_ready = false;
		requested_render_target = target;
	}

	inline void imgui_context::finish_frame() noexcept
	{
		if (!context || !frame_active)
		{
			return;
		}

		make_current();
		ImGui::Render();
		frame_active = false;
		draw_data_ready = true;

		const ImGuiIO& io = ImGui::GetIO();
		Input::set_gui_capture(io.WantCaptureMouse, io.WantCaptureKeyboard);
		offscreen_mouse_captured = false;
		if (requested_render_target && !captured_hit_test_path.empty())
		{
			for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
			{
				if (io.MouseDown[button])
				{
					offscreen_mouse_captured = true;
					break;
				}
			}
		}
		if (!offscreen_mouse_captured)
		{
			captured_hit_test_path.clear();
			has_last_offscreen_mouse_position = false;
		}
	}

	inline void imgui_context::render(
		gpu::command_buffer& command_buffer,
		::mngl::texture& target
	)
	{
		if (!context || !draw_data_ready)
		{
			return;
		}
		if (requested_render_target)
		{
			if (requested_render_target != &target)
			{
				throw std::logic_error(
					"Dear ImGui frame must be displayed to the texture passed to frame()");
			}
		}
		else if (!target.raw_gpu_texture().is_swapchain_texture())
		{
			return;
		}
		if (!SDL_IsMainThread())
		{
			throw std::logic_error("Dear ImGui rendering must run on the SDL main thread");
		}

		draw_data_ready = false;
		requested_render_target = nullptr;
		make_current();
		ImDrawData* data = ImGui::GetDrawData();
		if (!data || data->TotalVtxCount == 0 ||
			data->DisplaySize.x <= 0.0f || data->DisplaySize.y <= 0.0f)
		{
			return;
		}
		if (target.format() != color_target_format)
		{
			throw std::logic_error("Dear ImGui render target format changed; recreate its context");
		}

		SDL_GPUCommandBuffer* raw_command_buffer = command_buffer.raw();
		ImGui_ImplSDLGPU3_PrepareDrawData(data, raw_command_buffer);

		SDL_GPUColorTargetInfo color_target{};
		color_target.texture = target.raw_gpu_texture().raw();
		color_target.load_op = SDL_GPU_LOADOP_LOAD;
		color_target.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(
			raw_command_buffer,
			&color_target,
			1,
			nullptr);
		if (!render_pass)
		{
			throw sdl_error("Failed to begin the Dear ImGui GPU render pass: ");
		}

		ImGui_ImplSDLGPU3_RenderDrawData(data, raw_command_buffer, render_pass);
		SDL_EndGPURenderPass(render_pass);
	}

	inline void imgui_context::detach_from_window() noexcept
	{
		attached_window = nullptr;
		shutdown();
	}

	inline void imgui_context::shutdown() noexcept
	{
		event_manager::set_imgui_input_enabled(false);
		Input::set_gui_capture(false, false);
		requested_render_target = nullptr;
		captured_hit_test_path.clear();
		captured_texture_identity = 0;
		offscreen_mouse_captured = false;
		has_last_offscreen_mouse_position = false;
		if (!context)
		{
			return;
		}

		make_current();
		if (frame_active)
		{
			ImGui::EndFrame();
			frame_active = false;
		}

		ImGuiIO& io = ImGui::GetIO();
		if (io.IniFilename)
		{
			ImGui::SaveIniSettingsToDisk(io.IniFilename);
		}
		if (io.BackendRendererUserData)
		{
			ImGui_ImplSDLGPU3_Shutdown();
		}
		if (io.BackendPlatformUserData)
		{
			ImGui_ImplSDL3_SetMouseCaptureMode(
				ImGui_ImplSDL3_MouseCaptureMode_Disabled);
			ImGui_ImplSDL3_Shutdown();
		}
		ImGui::DestroyContext(context);
		context = nullptr;
		fonts = {};
		draw_data_ready = false;
	}

	inline void imgui_context::make_current() const noexcept
	{
		if (context)
		{
			ImGui::SetCurrentContext(context);
		}
	}

	inline imgui_context::window_scope::window_scope(
		imgui_context& owner,
		std::string_view title,
		std::optional<ImGuiID> dockspace_id,
		bool* open,
		ImGuiWindowFlags flags
	)
		: owner(&owner)
		, visible(false)
	{
		if (!owner.context || !owner.frame_active)
		{
			throw std::logic_error("An ImGui window must be created inside an active frame");
		}

		owner.make_current();
		if (dockspace_id)
		{
			ImGui::SetNextWindowDockID(
				*dockspace_id,
				ImGuiCond_FirstUseEver);
		}
		const std::string terminated_title(title);
		visible = ImGui::Begin(terminated_title.c_str(), open, flags);
	}

	inline imgui_context::main_menu_bar_scope::main_menu_bar_scope(
		imgui_context& owner
	)
		: owner(&owner)
		, visible(false)
	{
		if (!owner.context || !owner.frame_active)
		{
			throw std::logic_error("An ImGui main menu bar must be created inside an active frame");
		}

		owner.make_current();
		visible = ImGui::BeginMainMenuBar();
	}

	inline imgui_context::main_menu_bar_scope::~main_menu_bar_scope() noexcept
	{
		if (owner && owner->context && visible)
		{
			owner->make_current();
			ImGui::EndMainMenuBar();
		}
	}

	inline imgui_context::menu_scope::menu_scope(
		imgui_context& owner,
		std::string_view label
	)
		: owner(&owner)
		, visible(false)
	{
		if (!owner.context || !owner.frame_active)
		{
			throw std::logic_error("An ImGui menu must be created inside an active frame");
		}
		if (label.find('\0') != std::string_view::npos)
		{
			throw std::invalid_argument("Dear ImGui menu labels cannot contain embedded null characters");
		}

		owner.make_current();
		const std::string terminated_label(label);
		visible = ImGui::BeginMenu(terminated_label.c_str());
	}

	inline imgui_context::menu_scope::~menu_scope() noexcept
	{
		if (owner && owner->context && visible)
		{
			owner->make_current();
			ImGui::EndMenu();
		}
	}

	inline bool imgui_context::menu_scope::toggle(
		std::string_view label,
		bool& value
	) const
	{
		if (!owner || !owner->context || !visible)
		{
			return false;
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui menu item must be submitted inside an active frame");
		}
		if (label.find('\0') != std::string_view::npos)
		{
			throw std::invalid_argument("Dear ImGui menu item labels cannot contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_label(label);
		return ImGui::MenuItem(terminated_label.c_str(), nullptr, &value);
	}

	inline imgui_context::dockspace_scope
	imgui_context::window_scope::dockspace(std::string_view id) const
	{
		if (!owner || !owner->context || !owner->frame_active)
		{
			throw std::logic_error("An ImGui dockspace must be created inside an active frame");
		}
		if (id.empty() || id.find('\0') != std::string_view::npos)
		{
			throw std::invalid_argument("Dear ImGui dockspace IDs cannot be empty or contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_id(id);
		const ImGuiID dockspace_id = ImGui::GetID(terminated_id.c_str());
		// Keep the dock node alive even while its host window is collapsed.
		ImGui::DockSpace(
			dockspace_id,
			ImVec2(0.0f, 0.0f),
			ImGuiDockNodeFlags_None);
		return dockspace_scope(*owner, dockspace_id);
	}

	inline imgui_context::window_scope
	imgui_context::dockspace_scope::window(std::string_view title) const
	{
		return window_scope(*owner, title, id);
	}

	inline imgui_context::window_scope
	imgui_context::dockspace_scope::window(
		std::string_view title,
		bool& open
	) const
	{
		return window_scope(*owner, title, id, &open);
	}

	inline imgui_context::window_scope::~window_scope() noexcept
	{
		if (owner && owner->context)
		{
			owner->make_current();
			ImGui::End();
		}
	}

	inline void imgui_context::window_scope::text(std::string_view value) const
	{
		if (!owner || !owner->context || !visible)
		{
			return;
		}

		owner->make_current();
		const char* begin = value.empty() ? "" : value.data();
		ImGui::TextUnformatted(begin, begin + value.size());
	}

	inline void imgui_context::window_scope::text(const char* value) const
	{
		if (!value)
		{
			throw std::invalid_argument("Dear ImGui text cannot be null");
		}
		text(std::string_view(value));
	}

	template<imgui_compat::detail::scalar_numeric_value T>
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

	template<imgui_compat::detail::scalar_numeric_value T>
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

	inline void imgui_context::window_scope::progress_bar(
		float fraction,
		std::string_view overlay
	) const
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
			throw std::invalid_argument("Dear ImGui progress overlays cannot contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_overlay(overlay);
		ImGui::ProgressBar(
			fraction,
			ImVec2(-std::numeric_limits<float>::min(), 0.0f),
			terminated_overlay.c_str());
	}

	inline bool imgui_context::window_scope::selectable(
		std::string_view label,
		bool selected
	) const
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
			throw std::invalid_argument("Dear ImGui selectable labels cannot contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_label(label);
		return ImGui::Selectable(terminated_label.c_str(), selected);
	}

	inline bool imgui_context::window_scope::selectable(
		const char* label,
		bool selected
	) const
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

	inline bool imgui_context::window_scope::radio_button(
		std::string_view label,
		bool active
	) const
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
			throw std::invalid_argument("Dear ImGui radio button labels cannot contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_label(label);
		return ImGui::RadioButton(terminated_label.c_str(), active);
	}

	inline bool imgui_context::window_scope::radio_button(
		const char* label,
		bool active
	) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui radio button labels cannot be null");
		}
		return radio_button(std::string_view(label), active);
	}

	template<typename T>
		requires std::equality_comparable<T> &&
			std::assignable_from<T&, std::type_identity_t<T>>
	inline bool imgui_context::window_scope::radio_button(
		std::string_view label,
		T& selected,
		std::type_identity_t<T> value
	) const
	{
		const bool activated = radio_button(label, selected == value);
		if (activated)
		{
			selected = value;
		}
		return activated;
	}

	template<imgui_compat::detail::combo_range Range>
	inline imgui_compat::detail::combo_result_t<Range>
	imgui_context::window_scope::combo(
		std::string_view label,
		Range&& values
	) const
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

		for (auto iterator = std::ranges::begin(values);
			iterator != std::ranges::end(values);
			++iterator)
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
		if (selected_index < -1 ||
			selected_index >= static_cast<int>(item_labels.size()))
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
					if (ImGui::Selectable(
						"##mngl_combo_option",
						is_selected,
						ImGuiSelectableFlags_None,
						ImVec2(0.0f, ImGui::GetTextLineHeight())))
					{
						selected_index = static_cast<int>(index);
						storage->SetInt(storage_id, selected_index);
					}
					ImGui::GetWindowDrawList()->AddText(
						text_position,
						ImGui::GetColorU32(ImGuiCol_Text),
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
			return result_type{ std::move(selected_iterator) };
		}
		else if constexpr (imgui_compat::detail::combo_copyable_result<Range>)
		{
			return result_type{ std::in_place, *selected_iterator };
		}
		else
		{
			return result_type{ std::in_place, static_cast<std::size_t>(selected_index) };
		}
	}

	template<typename T>
		requires imgui_compat::detail::combo_range<std::initializer_list<T>>
	inline imgui_compat::detail::combo_result_t<std::initializer_list<T>>
	imgui_context::window_scope::combo(
		std::string_view label,
		std::initializer_list<T>&& values
	) const
	{
		return combo<std::initializer_list<T>>(label, std::move(values));
	}

	template<
		imgui_compat::detail::scalar_numeric_value Label,
		typename T
	>
		requires imgui_compat::detail::combo_range<std::initializer_list<T>>
	inline imgui_compat::detail::combo_result_t<std::initializer_list<T>>
	imgui_context::window_scope::combo(
		const Label& label,
		std::initializer_list<T>&& values
	) const
	{
		const std::string formatted_label =
			imgui_compat::detail::format_scalar_value(label);
		return combo(std::string_view(formatted_label), std::move(values));
	}

	template<typename T>
		requires imgui_compat::detail::combo_range<std::initializer_list<T>>
	inline imgui_compat::detail::combo_result_t<std::initializer_list<T>>
	imgui_context::window_scope::combo(
		bool label,
		std::initializer_list<T>&& values
	) const
	{
		const std::string formatted_label =
			imgui_compat::detail::format_scalar_value(label);
		return combo(std::string_view(formatted_label), std::move(values));
	}

	template<imgui_compat::detail::combo_range Range>
	inline imgui_compat::detail::combo_result_t<Range>
	imgui_context::window_scope::combo(
		const char* label,
		Range&& values
	) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui combo labels cannot be null");
		}
		return combo(std::string_view(label), std::forward<Range>(values));
	}

	template<
		imgui_compat::detail::scalar_numeric_value Label,
		imgui_compat::detail::combo_range Range
	>
	inline imgui_compat::detail::combo_result_t<Range>
	imgui_context::window_scope::combo(
		const Label& label,
		Range&& values
	) const
	{
		return combo(
			imgui_compat::detail::format_scalar_value(label),
			std::forward<Range>(values));
	}

	template<imgui_compat::detail::combo_range Range>
	inline imgui_compat::detail::combo_result_t<Range>
	imgui_context::window_scope::combo(
		bool label,
		Range&& values
	) const
	{
		return combo(
			imgui_compat::detail::format_scalar_value(label),
			std::forward<Range>(values));
	}

	template<imgui_compat::detail::combo_range Range>
	inline imgui_compat::detail::context_menu_result_t<Range>
	imgui_context::window_scope::context_menu(
		Range&& values
	) const
	{
		using result_type =
			imgui_compat::detail::context_menu_result_t<Range>;

		if (!owner || !owner->context || !visible)
		{
			return {};
		}
		if (!owner->frame_active)
		{
			throw std::logic_error("An ImGui context menu must be submitted inside an active frame");
		}

		owner->make_current();
		if (!ImGui::BeginPopupContextItem(
			nullptr,
			ImGuiPopupFlags_MouseButtonRight))
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

		for (auto iterator = std::ranges::begin(values);
			iterator != std::ranges::end(values);
			++iterator)
		{
			if (item_labels.size() == static_cast<std::size_t>(std::numeric_limits<int>::max()))
			{
				throw std::length_error("Dear ImGui context menu supports at most INT_MAX items");
			}
			std::string item_label =
				imgui_compat::detail::combo_item_to_string(*iterator);
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
			return result_type{ std::move(selected_iterator) };
		}
		else if constexpr (imgui_compat::detail::combo_copyable_result<Range>)
		{
			return result_type{ std::in_place, *selected_iterator };
		}
		else
		{
			return result_type{
				std::in_place,
				static_cast<std::size_t>(selected_index)
			};
		}
	}

	template<typename T>
		requires imgui_compat::detail::combo_range<std::initializer_list<T>>
	inline imgui_compat::detail::context_menu_result_t<std::initializer_list<T>>
	imgui_context::window_scope::context_menu(
		std::initializer_list<T>&& values
	) const
	{
		return context_menu<std::initializer_list<T>>(std::move(values));
	}

	template<imgui_compat::detail::scalar_input_value T>
	inline bool imgui_context::window_scope::slider(
		std::string_view label,
		T& value,
		std::type_identity_t<T> min,
		std::type_identity_t<T> max
	) const
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
		return imgui_compat::detail::slider_scalar(
			terminated_label.c_str(),
			value,
			min,
			max);
	}

	template<imgui_compat::detail::scalar_input_value T>
	inline bool imgui_context::window_scope::slider(
		const char* label,
		T& value,
		std::type_identity_t<T> min,
		std::type_identity_t<T> max
	) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui slider label cannot be null");
		}
		return slider(std::string_view(label), value, min, max);
	}

	template<
		imgui_compat::detail::scalar_numeric_value Label,
		imgui_compat::detail::scalar_input_value T
	>
	inline bool imgui_context::window_scope::slider(
		const Label& label,
		T& value,
		std::type_identity_t<T> min,
		std::type_identity_t<T> max
	) const
	{
		return slider(
			imgui_compat::detail::format_scalar_value(label),
			value,
			min,
			max);
	}

	template<imgui_compat::detail::scalar_input_value T>
	inline bool imgui_context::window_scope::slider(
		bool label,
		T& value,
		std::type_identity_t<T> min,
		std::type_identity_t<T> max
	) const
	{
		return slider(
			imgui_compat::detail::format_scalar_value(label),
			value,
			min,
			max);
	}

	template<imgui_compat::detail::glm_slider_vector Vector>
	inline bool imgui_context::window_scope::slider(
		std::string_view label,
		Vector& value,
		typename imgui_compat::detail::glm_slider_vector_traits<
			std::remove_cv_t<Vector>>::scalar_type min,
		typename imgui_compat::detail::glm_slider_vector_traits<
			std::remove_cv_t<Vector>>::scalar_type max
	) const
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
			throw std::invalid_argument("Dear ImGui vector slider labels cannot contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_label(label);
		return imgui_compat::detail::slider_vector(
			terminated_label.c_str(),
			value,
			min,
			max);
	}

	template<imgui_compat::detail::glm_slider_vector Vector>
	inline bool imgui_context::window_scope::slider(
		const char* label,
		Vector& value,
		typename imgui_compat::detail::glm_slider_vector_traits<
			std::remove_cv_t<Vector>>::scalar_type min,
		typename imgui_compat::detail::glm_slider_vector_traits<
			std::remove_cv_t<Vector>>::scalar_type max
	) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui vector slider labels cannot be null");
		}
		return slider(std::string_view(label), value, min, max);
	}

	inline bool imgui_context::window_scope::input(
		std::string_view label,
		std::string& value,
		const std::regex& pattern
	) const
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
			throw std::invalid_argument("The current Dear ImGui input value does not match its regular expression");
		}
		if (value.capacity() >= static_cast<std::size_t>(std::numeric_limits<int>::max()))
		{
			throw std::length_error("Dear ImGui text input exceeds its buffer size limit");
		}

		const std::string previous_value = value;
		imgui_compat::detail::regex_input_state state{
			&value,
			&pattern,
			value
		};
		const std::string terminated_label(label);
		const bool edited = ImGui::InputText(
			terminated_label.c_str(),
			value.data(),
			value.capacity() + 1,
			ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackEdit,
			imgui_compat::detail::regex_input_callback,
			&state);
		return edited && value != previous_value;
	}

	template<imgui_compat::detail::scalar_input_value T>
	inline bool imgui_context::window_scope::input(
		std::string_view label,
		T& value
	) const
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

	inline bool imgui_context::window_scope::input(
		std::string_view label,
		bool& value
	) const
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

	inline bool imgui_context::window_scope::checkbox(
		std::string_view label,
		bool& value
	) const
	{
		return input(label, value);
	}

	inline bool imgui_context::window_scope::checkbox(
		const char* label,
		bool& value
	) const
	{
		if (!label)
		{
			throw std::invalid_argument("Dear ImGui checkbox labels cannot be null");
		}
		return checkbox(std::string_view(label), value);
	}

	inline void imgui_context::window_scope::image(
		const ::mngl::texture& source,
		glm::vec2 display_size
	) const
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
		if (!owner->attached_window ||
			gpu_texture.device_raw() != owner->attached_window->gpu_device().raw())
		{
			throw std::invalid_argument("The MNGL texture and Dear ImGui context must use the same GPU device");
		}
		if (!std::isfinite(display_size.x) || !std::isfinite(display_size.y) ||
			display_size.x <= 0.0f || display_size.y <= 0.0f)
		{
			throw std::invalid_argument("Dear ImGui image dimensions must be finite and positive");
		}

		owner->make_current();
		const ImTextureID texture_id = reinterpret_cast<ImTextureID>(gpu_texture.raw());
		ImGui::Image(ImTextureRef(texture_id), ImVec2(display_size.x, display_size.y));
	}

	inline imgui_context::font_scope imgui_context::window_scope::use_font(
		imgui_compat::font_role role,
		float size_pixels
	) const
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

	inline imgui_context::font_scope::font_scope(
		imgui_context& owner,
		ImFont* font,
		float size_pixels
	)
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

	inline imgui_context::main_menu_bar_scope
	imgui_context::frame_scope::main_menu_bar() const
	{
		if (!owner)
		{
			throw std::logic_error("Cannot create a menu bar from an inactive frame scope");
		}
		return main_menu_bar_scope(*owner);
	}

	inline imgui_context::window_scope imgui_context::frame_scope::window(
		std::string_view title
	) const
	{
		if (!owner)
		{
			throw std::logic_error("Cannot create an ImGui window from an inactive frame scope");
		}
		return window_scope(*owner, title);
	}

	inline imgui_context::window_scope imgui_context::frame_scope::window(
		std::string_view title,
		ImGuiWindowFlags flags
	) const
	{
		if (!owner)
		{
			throw std::logic_error("Cannot create an ImGui window from an inactive frame scope");
		}
		return window_scope(*owner, title, std::nullopt, nullptr, flags);
	}

	inline imgui_context::menu_scope
	imgui_context::main_menu_bar_scope::menu(std::string_view label) const
	{
		if (!owner || !visible)
		{
			return {};
		}
		return menu_scope(*owner, label);
	}
}
