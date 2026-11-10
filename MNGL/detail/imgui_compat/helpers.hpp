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

	// These types describe common UI behavior without exposing Dear ImGui's
	// flags, keys, vectors, or IO structures to application code.
	enum class key : std::uint8_t
	{
		tab,
		left,
		right,
		up,
		down,
		page_up,
		page_down,
		home,
		end,
		insert,
		delete_key,
		backspace,
		space,
		enter,
		escape,
		left_control,
		left_shift,
		left_alt,
		left_super,
		right_control,
		right_shift,
		right_alt,
		right_super,
		menu,
		digit_0,
		digit_1,
		digit_2,
		digit_3,
		digit_4,
		digit_5,
		digit_6,
		digit_7,
		digit_8,
		digit_9,
		a,
		b,
		c,
		d,
		e,
		f,
		g,
		h,
		i,
		j,
		k,
		l,
		m,
		n,
		o,
		p,
		q,
		r,
		s,
		t,
		u,
		v,
		w,
		x,
		y,
		z,
		f1,
		f2,
		f3,
		f4,
		f5,
		f6,
		f7,
		f8,
		f9,
		f10,
		f11,
		f12,
		f13,
		f14,
		f15,
		f16,
		f17,
		f18,
		f19,
		f20,
		f21,
		f22,
		f23,
		f24,
		apostrophe,
		comma,
		minus,
		period,
		slash,
		semicolon,
		equal,
		left_bracket,
		backslash,
		right_bracket,
		grave_accent,
		caps_lock,
		scroll_lock,
		num_lock,
		print_screen,
		pause,
		keypad_0,
		keypad_1,
		keypad_2,
		keypad_3,
		keypad_4,
		keypad_5,
		keypad_6,
		keypad_7,
		keypad_8,
		keypad_9,
		keypad_decimal,
		keypad_divide,
		keypad_multiply,
		keypad_subtract,
		keypad_add,
		keypad_enter,
		keypad_equal,
		app_back,
		app_forward
	};

	enum class color_role : std::uint8_t
	{
		text,
		disabled_text,
		window_background,
		border,
		button,
		button_hovered,
		button_active,
		header,
		header_active
	};

	enum class condition : std::uint8_t
	{
		always,
		first_use,
		once,
		appearing
	};

	struct window_options
	{
		bool always_auto_resize = false;
		bool no_title_bar = false;
		bool no_resize = false;
		bool no_move = false;
		bool no_scrollbar = false;
		bool no_scroll_with_mouse = false;
		bool no_collapse = false;
		bool no_docking = false;
		bool no_saved_settings = false;
		bool no_background = false;
		bool no_mouse_inputs = false;
		bool menu_bar = false;
		bool horizontal_scrollbar = false;
		bool no_focus_on_appearing = false;
		bool no_bring_to_front_on_focus = false;
		bool no_nav_inputs = false;
		bool no_nav_focus = false;
	};

	struct child_options
	{
		glm::vec2 size{};
		bool border = false;
		bool resize_x = false;
		bool resize_y = false;
		bool auto_resize_x = false;
		bool auto_resize_y = false;
		bool always_auto_resize = false;
		bool frame_style = false;
		bool no_scrollbar = false;
		bool no_scroll_with_mouse = false;
	};

	struct tab_bar_options
	{
		bool reorderable = false;
		bool auto_select_new_tabs = false;
	};

	struct tab_item_options
	{
		bool unsaved_document = false;
		bool set_selected = false;
		bool leading = false;
		bool trailing = false;
	};

	struct table_options
	{
		bool resizable = false;
		bool reorderable = false;
		bool hideable = false;
		bool sortable = false;
		bool row_background = false;
		bool inner_horizontal_borders = false;
		bool inner_vertical_borders = false;
		bool stretch_columns = false;
		bool no_saved_settings = false;
	};

	struct tree_node_options
	{
		bool selected = false;
		bool default_open = false;
		bool leaf = false;
		bool span_available_width = false;
		bool open_on_arrow = false;
		bool open_on_double_click = false;
		bool framed = false;
	};

	struct canvas_options
	{
		bool left_button = true;
		bool middle_button = false;
		bool right_button = false;
	};

	struct input_state
	{
		bool wants_keyboard = false;
		bool wants_mouse = false;
		bool wants_text = false;
		bool control = false;
		bool shift = false;
		bool alt = false;
		bool super = false;
		float delta_time = 0.0f;
		glm::vec2 framebuffer_scale{1.0f};
		std::optional<glm::vec2> mouse_position;
		glm::vec2 mouse_delta{};
		glm::vec2 mouse_wheel{};
	};

	struct viewport_state
	{
		glm::vec2 position{};
		glm::vec2 work_position{};
		glm::vec2 size{};
		glm::vec2 work_size{};
	};

	struct style_metrics
	{
		glm::vec2 item_spacing{};
		glm::vec2 frame_padding{};
		float scrollbar_size = 0.0f;
		float text_line_height = 0.0f;
	};

	struct item_state
	{
		bool visible = false;
		bool hovered = false;
		bool active = false;
		bool focused = false;
		bool edited = false;
		bool activated = false;
		bool deactivated = false;
		bool deactivated_after_edit = false;
		bool clicked_left = false;
		bool clicked_middle = false;
		bool clicked_right = false;
	};
} // namespace mngl::imgui_compat

namespace mngl::imgui_compat::detail
{
	[[nodiscard]] inline std::string widget_label(std::string_view value)
	{
		if (value.find('\0') != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui labels cannot contain embedded null characters");
		}
		return std::string(value);
	}

	[[nodiscard]] inline ImGuiWindowFlags to_imgui_window_flags(const window_options& options) noexcept
	{
		ImGuiWindowFlags result = ImGuiWindowFlags_None;
		if (options.always_auto_resize) result |= ImGuiWindowFlags_AlwaysAutoResize;
		if (options.no_title_bar) result |= ImGuiWindowFlags_NoTitleBar;
		if (options.no_resize) result |= ImGuiWindowFlags_NoResize;
		if (options.no_move) result |= ImGuiWindowFlags_NoMove;
		if (options.no_scrollbar) result |= ImGuiWindowFlags_NoScrollbar;
		if (options.no_scroll_with_mouse) result |= ImGuiWindowFlags_NoScrollWithMouse;
		if (options.no_collapse) result |= ImGuiWindowFlags_NoCollapse;
		if (options.no_docking) result |= ImGuiWindowFlags_NoDocking;
		if (options.no_saved_settings) result |= ImGuiWindowFlags_NoSavedSettings;
		if (options.no_background) result |= ImGuiWindowFlags_NoBackground;
		if (options.no_mouse_inputs) result |= ImGuiWindowFlags_NoMouseInputs;
		if (options.menu_bar) result |= ImGuiWindowFlags_MenuBar;
		if (options.horizontal_scrollbar) result |= ImGuiWindowFlags_HorizontalScrollbar;
		if (options.no_focus_on_appearing) result |= ImGuiWindowFlags_NoFocusOnAppearing;
		if (options.no_bring_to_front_on_focus) result |= ImGuiWindowFlags_NoBringToFrontOnFocus;
		if (options.no_nav_inputs) result |= ImGuiWindowFlags_NoNavInputs;
		if (options.no_nav_focus) result |= ImGuiWindowFlags_NoNavFocus;
		return result;
	}

	namespace combo_string_adl
	{
		using std::to_string;

		template <typename T> [[nodiscard]] auto convert(const T& value) -> decltype(to_string(value))
		{
			return to_string(value);
		}
	} // namespace combo_string_adl

	template <typename T>
	concept combo_stringifiable =
	    std::convertible_to<T, std::string_view> ||
	    requires(const std::remove_reference_t<T>& value) { std::string{combo_string_adl::convert(value)}; };

	template <typename T> [[nodiscard]] std::string combo_item_to_string(T&& value)
	{
		if constexpr (std::convertible_to<T, std::string_view>)
		{
			if constexpr (std::is_pointer_v<std::remove_reference_t<T>> &&
			              std::same_as<std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>,
			                           char>)
			{
				if (!value) [[unlikely]]
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

	template <typename Range>
	concept combo_range =
	    std::ranges::forward_range<Range> && combo_stringifiable<std::ranges::range_reference_t<Range>>;

	template <typename Range>
	concept combo_copyable_result =
	    std::copy_constructible<std::ranges::range_value_t<Range>> &&
	    std::constructible_from<std::ranges::range_value_t<Range>, std::ranges::range_reference_t<Range>>;

	template <typename Range> struct combo_result
	{
		using iterator_type = std::ranges::iterator_t<Range>;
		using value_type = std::ranges::range_value_t<Range>;
		using type =
		    std::conditional_t<std::ranges::borrowed_range<Range>, std::optional<iterator_type>,
			                   std::conditional_t<combo_copyable_result<Range>, std::optional<value_type>,
			                                      std::optional<std::size_t>>>;
	};

	template <typename Range> using combo_result_t = typename combo_result<Range>::type;

	template <typename Range> using context_menu_result_t = combo_result_t<Range>;

	template <typename T>
	concept scalar_numeric_value =
	    !std::is_volatile_v<T> &&
	    ((std::integral<std::remove_cv_t<T>> && !std::same_as<std::remove_cv_t<T>, bool> &&
		  (sizeof(std::remove_cv_t<T>) == 1 || sizeof(std::remove_cv_t<T>) == 2 ||
		   sizeof(std::remove_cv_t<T>) == 4 || sizeof(std::remove_cv_t<T>) == 8)) ||
		 std::same_as<std::remove_cv_t<T>, float> || std::same_as<std::remove_cv_t<T>, double>);

	template <typename T>
	concept scalar_input_value = scalar_numeric_value<T> && !std::is_const_v<T> && !std::is_volatile_v<T>;

	template <typename T> struct glm_slider_vector_traits
	{
		static constexpr bool valid = false;
	};

	template <glm::length_t Length, typename Scalar, glm::qualifier Qualifier>
	struct glm_slider_vector_traits<glm::vec<Length, Scalar, Qualifier>>
	{
		using scalar_type = Scalar;
		static constexpr glm::length_t length = Length;
		static constexpr bool valid = Length >= 2 && Length <= 4 && scalar_input_value<Scalar>;
	};

	template <typename T>
	concept glm_slider_vector =
	    !std::is_const_v<T> && !std::is_volatile_v<T> && glm_slider_vector_traits<std::remove_cv_t<T>>::valid;

	template <scalar_numeric_value T> [[nodiscard]] inline std::string format_scalar_value(const T& value)
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

	template <typename T>
	concept widget_label_value =
	    !std::is_volatile_v<T> && (scalar_numeric_value<T> || std::same_as<std::remove_cv_t<T>, bool>);

	template <widget_label_value T> [[nodiscard]] inline std::string format_widget_label(const T& value)
	{
		return format_scalar_value(value);
	}

	inline void validate_progress_fraction(float fraction)
	{
		if (!std::isfinite(fraction) || fraction < 0.0f || fraction > 1.0f) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui progress fraction must be finite and between 0 and 1");
		}
	}

	template <scalar_input_value T> [[nodiscard]] constexpr ImGuiDataType scalar_input_data_type() noexcept
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

	template <typename T> struct scalar_input_storage
	{
		using value_type = std::remove_cv_t<T>;
		using type = std::conditional_t<
		    std::same_as<value_type, float>, float,
		    std::conditional_t<
		        std::same_as<value_type, double>, double,
		        std::conditional_t<
		            std::is_signed_v<value_type>,
		            std::conditional_t<
		                sizeof(value_type) == 1, ImS8,
		                std::conditional_t<sizeof(value_type) == 2, ImS16,
						                   std::conditional_t<sizeof(value_type) == 4, ImS32, ImS64>>>,
		            std::conditional_t<
		                sizeof(value_type) == 1, ImU8,
		                std::conditional_t<sizeof(value_type) == 2, ImU16,
						                   std::conditional_t<sizeof(value_type) == 4, ImU32, ImU64>>>>>>;
	};

	template <glm_slider_vector Vector>
	[[nodiscard]] bool slider_vector(
	    const char* label, Vector& value,
	    typename glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type min,
	    typename glm_slider_vector_traits<std::remove_cv_t<Vector>>::scalar_type max)
	{
		using traits = glm_slider_vector_traits<std::remove_cv_t<Vector>>;
		using scalar_type = typename traits::scalar_type;
		using storage_type = typename scalar_input_storage<scalar_type>::type;

		if constexpr (std::floating_point<scalar_type>)
		{
			if (!std::isfinite(min) || !std::isfinite(max)) [[unlikely]]
			{
				throw std::invalid_argument("Dear ImGui vector slider bounds must be finite");
			}
		}
		if (!(min < max)) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui vector slider minimum must be less than its maximum");
		}

		std::array<storage_type, static_cast<std::size_t>(traits::length)> components{};
		for (glm::length_t index = 0; index < traits::length; ++index)
		{
			const scalar_type component = value[index];
			if constexpr (std::floating_point<scalar_type>)
			{
				if (!std::isfinite(component)) [[unlikely]]
				{
					throw std::invalid_argument("Dear ImGui vector slider values must be finite");
				}
			}
			if (component < min || component > max) [[unlikely]]
			{
				throw std::invalid_argument("Dear ImGui vector slider values must be within their range");
			}
			components[static_cast<std::size_t>(index)] = static_cast<storage_type>(component);
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
			changed = ImGui::SliderScalarN(label, scalar_input_data_type<scalar_type>(), components.data(),
			                               static_cast<int>(traits::length), &scalar_min, &scalar_max);
		}
		if (!changed)
		{
			return false;
		}

		for (glm::length_t index = 0; index < traits::length; ++index)
		{
			value[index] = static_cast<scalar_type>(components[static_cast<std::size_t>(index)]);
		}
		return true;
	}

	template <scalar_input_value T> [[nodiscard]] bool input_scalar(const char* label, T& value)
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

	template <scalar_input_value T>
	[[nodiscard]] bool slider_scalar(const char* label, T& value, std::type_identity_t<T> min,
	                                 std::type_identity_t<T> max)
	{
		if constexpr (std::floating_point<std::remove_cv_t<T>>)
		{
			if (!std::isfinite(value) || !std::isfinite(min) || !std::isfinite(max)) [[unlikely]]
			{
				throw std::invalid_argument("Dear ImGui slider values must be finite");
			}
		}
		if (!(min < max)) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui slider minimum must be less than its maximum");
		}
		if (value < min || value > max) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui slider value must be within its range");
		}

		using storage_type = typename scalar_input_storage<T>::type;
		storage_type scalar = static_cast<storage_type>(value);
		const storage_type scalar_min = static_cast<storage_type>(min);
		const storage_type scalar_max = static_cast<storage_type>(max);
		if (!ImGui::SliderScalar(label, scalar_input_data_type<T>(), &scalar, &scalar_min, &scalar_max))
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

		const std::string_view candidate(data->Buf, static_cast<std::size_t>(data->BufTextLen));
		if (std::regex_match(candidate.begin(), candidate.end(), *state.pattern))
		{
			state.accepted_value.assign(candidate);
			return 0;
		}

		if (state.accepted_value.size() >= static_cast<std::size_t>(data->BufSize)) [[unlikely]]
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
		return std::string(reinterpret_cast<const char*>(encoded_path.data()), encoded_path.size());
	}

	inline ImFont* load_font(ImFontAtlas& atlas, const font_source& source, ImFont* fallback = nullptr)
	{
		if (!std::isfinite(source.size_pixels) || source.size_pixels <= 0.0f) [[unlikely]]
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
			if (!font) [[unlikely]]
			{
				throw std::runtime_error("Failed to load Dear ImGui's embedded vector font");
			}
			return font;
		}

		const std::string filename = path_to_utf8(source.file);
		ImFontConfig file_config{};
		file_config.Flags |= ImFontFlags_NoLoadError;
		static const ImWchar latin_cyrillic_ranges[] = {0x0020, 0x00FF, 0x0400, 0x052F, 0};
		file_config.GlyphRanges = latin_cyrillic_ranges;
		ImFont* font = atlas.AddFontFromFileTTF(filename.c_str(), source.size_pixels, &file_config);
		if (!font) [[unlikely]]
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
			return ImVec4(static_cast<float>((value >> 16) & 0xFF) / 255.0f,
			              static_cast<float>((value >> 8) & 0xFF) / 255.0f,
			              static_cast<float>(value & 0xFF) / 255.0f, alpha);
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
} // namespace mngl::imgui_compat::detail
