#pragma once

#include "geometry.hpp"
#include "svg_effect_types.hpp"
#include "svg_image_decode.hpp"
#include "types.hpp"
#include "../../hit_test.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#if defined(__APPLE__)
#include <cerrno>
#include <locale.h>
#include <stdlib.h>
#endif
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <limits>
#include <memory>
#include <numbers>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mngl::two_d::detail::svg
{
	enum class gradient_kind : std::uint32_t
	{
		linear,
		radial
	};

	enum class gradient_spread : std::uint32_t
	{
		pad,
		repeat,
		reflect
	};

	struct gradient_coordinate
	{
		float value = 0.0f;
		bool percentage = false;
	};

	struct gradient_stop
	{
		float offset = 0.0f;
		::mngl::color value{};
	};

	struct gradient_definition
	{
		std::string id;
		gradient_kind kind = gradient_kind::linear;
		gradient_spread spread = gradient_spread::pad;
		// SVG gradients use sRGB interpolation unless explicitly set to linearRGB.
		bool linear_rgb = false;
		bool object_bounding_box = true;
		glm::mat4 transform{1.0f};
		gradient_coordinate x1{0.0f, true};
		gradient_coordinate y1{0.0f, true};
		gradient_coordinate x2{100.0f, true};
		gradient_coordinate y2{0.0f, true};
		gradient_coordinate cx{50.0f, true};
		gradient_coordinate cy{50.0f, true};
		gradient_coordinate radius{50.0f, true};
		gradient_coordinate fx{50.0f, true};
		gradient_coordinate fy{50.0f, true};
		gradient_coordinate focal_radius{0.0f, true};
		bool fx_explicit = false;
		bool fy_explicit = false;
		std::vector<gradient_stop> stops;
	};

	struct gradient_paint
	{
		std::shared_ptr<const gradient_definition> definition;
		// Maps element-local coordinates to gradient coordinates for fragment evaluation.
		glm::mat4 coordinate_transform{1.0f};
		glm::vec4 parameters0{};
		glm::vec4 parameters1{};
	};

	struct marker_vertex
	{
		glm::vec2 point{};
		glm::vec2 incoming{};
		glm::vec2 outgoing{};
	};

	struct image_paint
	{
		std::shared_ptr<const image_pixels> pixels;
		rectangle bounds;
		glm::vec4 uv{0, 1, 1, 0}; // The first SVG vertex is at the top of the image.
	};

	struct effect_layer;

	struct geometry_data
	{
		std::vector<glm::vec2> triangles;
		::mngl::color tint{};
		glm::mat4 transform{1.0f};
		std::optional<gradient_paint> gradient;
		::mngl::svg_hit_test_element svg_element;
		std::optional<image_paint> image;
		std::shared_ptr<const effect_layer> effect;
		std::optional<rectangle> object_bounds;
		bool pointer_events = true;
		std::shared_ptr<const std::vector<glm::vec2>> shared_triangles;

		[[nodiscard]] std::span<const glm::vec2> triangle_view() const noexcept
		{
			return shared_triangles ? std::span<const glm::vec2>(*shared_triangles)
			                        : std::span<const glm::vec2>(triangles);
		}

		void make_triangles_writable()
		{
			if (shared_triangles)
			{
				triangles = *shared_triangles;
				shared_triangles.reset();
			}
		}
	};

	struct document_data
	{
		glm::vec4 view_box{};
		glm::vec2 viewport_size{300.0f, 150.0f};
		std::vector<geometry_data> geometry;
	};

	using decoded_image_cache =
	    std::unordered_map<std::string, std::shared_ptr<const image_pixels>>;

	struct effect_layer
	{
		rectangle region;
		std::vector<geometry_data> source;
		std::vector<geometry_data> mask_source;
		std::optional<mask_data> mask;
		std::optional<filter_data> filter;
		std::optional<rectangle> hit_clip;
		std::vector<glm::vec2> hit_clip_triangles;
		bool hit_clip_empty = false;
	};

	struct contour
	{
		std::vector<glm::vec2> points;
		bool closed = false;
		std::vector<marker_vertex> marker_vertices;
		bool has_drawn_segment = false;
	};

	enum class stroke_linecap
	{
		butt,
		round,
		square
	};

	enum class stroke_linejoin
	{
		miter,
		round,
		bevel
	};

	struct paint_server_fallback
	{
		bool provided = false;
		bool none = false;
		::mngl::color color{0.0f, 0.0f, 0.0f, 1.0f};
	};

	struct stroke_path
	{
		std::vector<glm::vec2> points;
		bool closed = false;
		glm::vec2 zero_tangent{1.0f, 0.0f};
	};

	struct paint_state
	{
		glm::mat4 transform{1.0f};
		std::optional<glm::vec2> viewport_size;
		glm::vec2 viewport_origin{};
		::mngl::color fill{0.0f, 0.0f, 0.0f, 1.0f};
		::mngl::color stroke{0.0f, 0.0f, 0.0f, 1.0f};
		::mngl::color current_color{0.0f, 0.0f, 0.0f, 1.0f};
		bool fill_uses_current_color = false;
		bool stroke_uses_current_color = false;
		float opacity = 1.0f;
		float inherited_opacity = 1.0f;
		float fill_opacity = 1.0f;
		float stroke_opacity = 1.0f;
		bool clip_even_odd = false;
		::mngl::color stop_color{0.0f, 0.0f, 0.0f, 1.0f};
		float stop_opacity = 1.0f;
		float stroke_width = 1.0f;
		float stroke_miterlimit = 4.0f;
		float stroke_dashoffset = 0.0f;
		std::vector<float> stroke_dasharray;
		stroke_linecap linecap = stroke_linecap::butt;
		stroke_linejoin linejoin = stroke_linejoin::miter;
		bool fill_enabled = true;
		bool stroke_enabled = false;
		bool even_odd = false;
		bool display = true;
		bool visible = true;
		bool pointer_events = true;
		std::string fill_gradient_id;
		std::string stroke_gradient_id;
		paint_server_fallback fill_gradient_fallback;
		paint_server_fallback stroke_gradient_fallback;
		std::array<std::string, 3> marker_ids;
		std::string mask_id;
		std::string filter_id;
		std::string clip_path_id;
		std::optional<::mngl::color> context_fill;
		std::optional<::mngl::color> context_stroke;
	};

	[[nodiscard]] inline glm::vec4 paint_view_box(const paint_state& style, glm::vec4 fallback) noexcept;

	[[nodiscard]] inline std::string_view trim(std::string_view value) noexcept
	{
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
		{
			value.remove_prefix(1);
		}
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
		{
			value.remove_suffix(1);
		}
		return value;
	}

	class number_reader
	{
	public:
		explicit number_reader(std::string_view text) noexcept : m_text(text) {}

		void skip_separators() noexcept
		{
			while (m_offset < m_text.size() &&
			       (std::isspace(static_cast<unsigned char>(m_text[m_offset])) || m_text[m_offset] == ','))
			{
				++m_offset;
			}
		}

		[[nodiscard]] bool at_end() noexcept
		{
			skip_separators();
			return m_offset == m_text.size();
		}

		[[nodiscard]] bool has_number() noexcept
		{
			skip_separators();
			if (m_offset == m_text.size())
			{
				return false;
			}
			const char value = m_text[m_offset];
			return std::isdigit(static_cast<unsigned char>(value)) || value == '+' || value == '-' ||
			       value == '.';
		}

		[[nodiscard]] float number()
		{
			skip_separators();
			if (m_offset == m_text.size()) [[unlikely]]
			{
				throw std::invalid_argument("Unexpected end of SVG number list");
			}

#if defined(__APPLE__)
			const std::size_t number_start = m_offset;
			std::size_t token_end = number_start;
			if (m_text[token_end] == '+' || m_text[token_end] == '-')
			{
				++token_end;
			}
			std::size_t significand_digits = 0;
			while (token_end < m_text.size() &&
			       std::isdigit(static_cast<unsigned char>(m_text[token_end])))
			{
				++token_end;
				++significand_digits;
			}
			if (token_end < m_text.size() && m_text[token_end] == '.')
			{
				++token_end;
				while (token_end < m_text.size() &&
				       std::isdigit(static_cast<unsigned char>(m_text[token_end])))
				{
					++token_end;
					++significand_digits;
				}
			}
			if (significand_digits == 0) [[unlikely]]
			{
				throw std::invalid_argument("SVG contains an invalid or non-finite number");
			}
			if (token_end < m_text.size() && (m_text[token_end] == 'e' || m_text[token_end] == 'E'))
			{
				std::size_t exponent_end = token_end + 1;
				if (exponent_end < m_text.size() &&
				    (m_text[exponent_end] == '+' || m_text[exponent_end] == '-'))
				{
					++exponent_end;
				}
				const std::size_t exponent_digits_start = exponent_end;
				while (exponent_end < m_text.size() &&
				       std::isdigit(static_cast<unsigned char>(m_text[exponent_end])))
				{
					++exponent_end;
				}
				if (exponent_end != exponent_digits_start)
				{
					token_end = exponent_end;
				}
			}
			const std::string token(m_text.substr(number_start, token_end - number_start));
			static const locale_t c_numeric_locale = []
			{
				const locale_t locale = newlocale(LC_NUMERIC_MASK, "C", nullptr);
				if (locale == nullptr)
				{
					throw std::runtime_error("Could not create the C numeric locale for SVG parsing");
				}
				return locale;
			}();
			char* parsed_end = nullptr;
			errno = 0;
			const float result_value = ::strtof_l(token.c_str(), &parsed_end, c_numeric_locale);
			if (errno == ERANGE || parsed_end != token.c_str() + token.size() ||
			    !std::isfinite(result_value)) [[unlikely]]
			{
				throw std::invalid_argument("SVG contains an invalid or non-finite number");
			}
			m_offset = token_end;
			return result_value;
#else
			const char* first = m_text.data() + m_offset;
			const char* last = m_text.data() + m_text.size();
			if (*first == '+')
			{
				++first;
			}
			float result_value = 0.0f;
			const auto result = std::from_chars(first, last, result_value, std::chars_format::general);
			if (result.ec != std::errc{} || result.ptr == first || !std::isfinite(result_value)) [[unlikely]]
			{
				throw std::invalid_argument("SVG contains an invalid or non-finite number");
			}
			m_offset = static_cast<std::size_t>(result.ptr - m_text.data());
			return result_value;
#endif
		}

		[[nodiscard]] float length()
		{
			const float result = number();
			if (m_text.substr(m_offset).starts_with("px"))
			{
				m_offset += 2;
			}
			if (m_offset < m_text.size() &&
			    (std::isalpha(static_cast<unsigned char>(m_text[m_offset])) || m_text[m_offset] == '%'))
			{
				throw std::invalid_argument("SVG lengths support only user units and px");
			}
			return result;
		}

		[[nodiscard]] char peek() noexcept
		{
			skip_separators();
			return m_offset < m_text.size() ? m_text[m_offset] : '\0';
		}

		[[nodiscard]] char take() noexcept
		{
			const char value = peek();
			if (value != '\0')
			{
				++m_offset;
			}
			return value;
		}

	private:
		std::string_view m_text;
		std::size_t m_offset = 0;
	};

	[[nodiscard]] inline float parse_number(std::string_view value)
	{
		number_reader reader(trim(value));
		const float result = reader.number();
		if (!reader.at_end()) [[unlikely]]
		{
			throw std::invalid_argument("SVG length must be a finite number in user units");
		}
		return result;
	}

	[[nodiscard]] inline float parse_length(std::string_view value)
	{
		value = trim(value);
		if (value.ends_with("px"))
		{
			value.remove_suffix(2);
		}
		if (value.find('%') != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument(
			    "Percentage SVG lengths are not supported by the initial 2D renderer");
		}
		return parse_number(value);
	}

	[[nodiscard]] inline std::vector<float> parse_number_list(std::string_view value)
	{
		number_reader reader(value);
		std::vector<float> result;
		while (reader.has_number())
		{
			result.push_back(reader.number());
		}
		if (!reader.at_end()) [[unlikely]]
		{
			throw std::invalid_argument("SVG number list contains an unsupported token");
		}
		return result;
	}

	[[nodiscard]] inline std::vector<float> parse_dash_array(std::string_view value)
	{
		if (value == "none")
		{
			return {};
		}

		number_reader reader(value);
		std::vector<float> result;
		while (reader.has_number())
		{
			const float dash_length = reader.length();
			if (dash_length < 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("SVG stroke-dasharray values must be non-negative");
			}
			result.push_back(dash_length);
		}
		if (!reader.at_end() || result.empty()) [[unlikely]]
		{
			throw std::invalid_argument("SVG stroke-dasharray must be 'none' or a list of lengths");
		}

		if (result.size() % 2 != 0)
		{
			const auto original_size = result.size();
			if (original_size > result.max_size() / 2) [[unlikely]]
			{
				throw std::length_error("SVG stroke-dasharray is too large");
			}
			auto repeated = result;
			result.reserve(original_size * 2);
			result.insert(result.end(), repeated.begin(), repeated.end());
		}
		if (std::ranges::all_of(result, [](float length) { return length == 0.0f; }))
		{
			return {};
		}
		return result;
	}

	[[nodiscard]] inline glm::vec2 apply_transform(const glm::mat4& matrix, glm::vec2 point) noexcept
	{
		const glm::vec4 transformed = matrix * glm::vec4(point, 0.0f, 1.0f);
		return {transformed.x, transformed.y};
	}

	[[nodiscard]] inline glm::mat4 parse_transform(std::string_view value)
	{
		glm::mat4 result(1.0f);
		std::size_t offset = 0;
		while (offset < value.size())
		{
			while (offset < value.size() &&
			       (std::isspace(static_cast<unsigned char>(value[offset])) || value[offset] == ','))
			{
				++offset;
			}
			if (offset == value.size())
			{
				break;
			}

			const std::size_t name_start = offset;
			while (offset < value.size() &&
			       (std::isalpha(static_cast<unsigned char>(value[offset])) || value[offset] == '_'))
			{
				++offset;
			}
			if (name_start == offset) [[unlikely]]
			{
				throw std::invalid_argument("SVG transform contains an invalid function");
			}
			const auto name = value.substr(name_start, offset - name_start);
			while (offset < value.size() && std::isspace(static_cast<unsigned char>(value[offset])))
			{
				++offset;
			}
			if (offset == value.size() || value[offset] != '(') [[unlikely]]
			{
				throw std::invalid_argument("SVG transform function is missing '('");
			}
			const std::size_t arguments_start = ++offset;
			const std::size_t arguments_end = value.find(')', offset);
			if (arguments_end == std::string_view::npos) [[unlikely]]
			{
				throw std::invalid_argument("SVG transform function is missing ')'");
			}
			const auto arguments =
			    parse_number_list(value.substr(arguments_start, arguments_end - arguments_start));
			offset = arguments_end + 1;

			glm::mat4 operation(1.0f);
			if (name == "matrix")
			{
				if (arguments.size() != 6) [[unlikely]]
				{
					throw std::invalid_argument("SVG matrix() requires six values");
				}
				operation[0][0] = arguments[0];
				operation[0][1] = arguments[1];
				operation[1][0] = arguments[2];
				operation[1][1] = arguments[3];
				operation[3][0] = arguments[4];
				operation[3][1] = arguments[5];
			}
			else if (name == "translate")
			{
				if (arguments.empty() || arguments.size() > 2) [[unlikely]]
				{
					throw std::invalid_argument("SVG translate() requires one or two values");
				}
				operation = glm::translate(
				    operation, glm::vec3(arguments[0], arguments.size() == 2 ? arguments[1] : 0.0f, 0.0f));
			}
			else if (name == "scale")
			{
				if (arguments.empty() || arguments.size() > 2) [[unlikely]]
				{
					throw std::invalid_argument("SVG scale() requires one or two values");
				}
				const float y = arguments.size() == 2 ? arguments[1] : arguments[0];
				operation = glm::scale(operation, glm::vec3(arguments[0], y, 1.0f));
			}
			else if (name == "rotate")
			{
				if (arguments.size() != 1 && arguments.size() != 3) [[unlikely]]
				{
					throw std::invalid_argument("SVG rotate() requires one value or an angle and center");
				}
				const float radians = glm::radians(arguments[0]);
				if (arguments.size() == 3)
				{
					operation = glm::translate(operation, glm::vec3(arguments[1], arguments[2], 0.0f));
				}
				operation = glm::rotate(operation, radians, glm::vec3(0.0f, 0.0f, 1.0f));
				if (arguments.size() == 3)
				{
					operation = glm::translate(operation, glm::vec3(-arguments[1], -arguments[2], 0.0f));
				}
			}
			else if (name == "skewX" || name == "skewY")
			{
				if (arguments.size() != 1) [[unlikely]]
				{
					throw std::invalid_argument("SVG skew transform requires one angle");
				}
				const float tangent = std::tan(glm::radians(arguments[0]));
				if (!std::isfinite(tangent)) [[unlikely]]
				{
					throw std::invalid_argument("SVG skew transform angle is out of range");
				}
				if (name == "skewX")
				{
					operation[1][0] = tangent;
				}
				else
				{
					operation[0][1] = tangent;
				}
			}
			else
			{
				throw std::invalid_argument("SVG transform function is not supported");
			}
			result *= operation;
		}
		return result;
	}

	[[nodiscard]] inline std::uint8_t hex_digit(char value)
	{
		if (value >= '0' && value <= '9')
		{
			return static_cast<std::uint8_t>(value - '0');
		}
		if (value >= 'a' && value <= 'f')
		{
			return static_cast<std::uint8_t>(value - 'a' + 10);
		}
		if (value >= 'A' && value <= 'F')
		{
			return static_cast<std::uint8_t>(value - 'A' + 10);
		}
		throw std::invalid_argument("SVG hex color contains a non-hexadecimal digit");
	}

	[[nodiscard]] inline std::string lower_ascii(std::string_view value)
	{
		std::string result(value);
		std::ranges::transform(result, result.begin(), [](unsigned char character)
		                       { return static_cast<char>(std::tolower(character)); });
		return result;
	}

	[[nodiscard]] inline float parse_opacity(std::string_view value)
	{
		value = trim(value);
		const bool percentage = value.ends_with('%');
		if (percentage)
		{
			value.remove_suffix(1);
		}
		const float opacity = parse_number(value) / (percentage ? 100.0f : 1.0f);
		return std::clamp(opacity, 0.0f, 1.0f);
	}

	[[nodiscard]] inline float parse_color_channel(std::string_view value, bool& percentage)
	{
		value = trim(value);
		const bool channel_percentage = value.ends_with('%');
		if (percentage != channel_percentage && !value.empty()) [[unlikely]]
		{
			throw std::invalid_argument("CSS rgb() color channels must use one consistent unit");
		}
		percentage = channel_percentage;
		if (channel_percentage)
		{
			value.remove_suffix(1);
		}
		const float channel = parse_number(value);
		return std::clamp(channel / (channel_percentage ? 100.0f : 255.0f), 0.0f, 1.0f);
	}

	[[nodiscard]] inline float parse_color_alpha(std::string_view value)
	{
		value = trim(value);
		const bool percentage = value.ends_with('%');
		if (percentage)
		{
			value.remove_suffix(1);
		}
		const float alpha = parse_number(value) / (percentage ? 100.0f : 1.0f);
		return std::clamp(alpha, 0.0f, 1.0f);
	}

	struct named_srgb_color
	{
		std::string_view name;
		std::uint32_t rgb;
	};

	inline constexpr std::array<named_srgb_color, 148> named_srgb_colors{{
	    {"aliceblue", 0xf0f8ff}, {"antiquewhite", 0xfaebd7}, {"aqua", 0x00ffff},
	    {"aquamarine", 0x7fffd4}, {"azure", 0xf0ffff}, {"beige", 0xf5f5dc},
	    {"bisque", 0xffe4c4}, {"black", 0x000000}, {"blanchedalmond", 0xffebcd},
	    {"blue", 0x0000ff}, {"blueviolet", 0x8a2be2}, {"brown", 0xa52a2a},
	    {"burlywood", 0xdeb887}, {"cadetblue", 0x5f9ea0}, {"chartreuse", 0x7fff00},
	    {"chocolate", 0xd2691e}, {"coral", 0xff7f50}, {"cornflowerblue", 0x6495ed},
	    {"cornsilk", 0xfff8dc}, {"crimson", 0xdc143c}, {"cyan", 0x00ffff},
	    {"darkblue", 0x00008b}, {"darkcyan", 0x008b8b}, {"darkgoldenrod", 0xb8860b},
	    {"darkgray", 0xa9a9a9}, {"darkgreen", 0x006400}, {"darkgrey", 0xa9a9a9},
	    {"darkkhaki", 0xbdb76b}, {"darkmagenta", 0x8b008b}, {"darkolivegreen", 0x556b2f},
	    {"darkorange", 0xff8c00}, {"darkorchid", 0x9932cc}, {"darkred", 0x8b0000},
	    {"darksalmon", 0xe9967a}, {"darkseagreen", 0x8fbc8f}, {"darkslateblue", 0x483d8b},
	    {"darkslategray", 0x2f4f4f}, {"darkslategrey", 0x2f4f4f}, {"darkturquoise", 0x00ced1},
	    {"darkviolet", 0x9400d3}, {"deeppink", 0xff1493}, {"deepskyblue", 0x00bfff},
	    {"dimgray", 0x696969}, {"dimgrey", 0x696969}, {"dodgerblue", 0x1e90ff},
	    {"firebrick", 0xb22222}, {"floralwhite", 0xfffaf0}, {"forestgreen", 0x228b22},
	    {"fuchsia", 0xff00ff}, {"gainsboro", 0xdcdcdc}, {"ghostwhite", 0xf8f8ff},
	    {"gold", 0xffd700}, {"goldenrod", 0xdaa520}, {"gray", 0x808080},
	    {"green", 0x008000}, {"greenyellow", 0xadff2f}, {"grey", 0x808080},
	    {"honeydew", 0xf0fff0}, {"hotpink", 0xff69b4}, {"indianred", 0xcd5c5c},
	    {"indigo", 0x4b0082}, {"ivory", 0xfffff0}, {"khaki", 0xf0e68c},
	    {"lavender", 0xe6e6fa}, {"lavenderblush", 0xfff0f5}, {"lawngreen", 0x7cfc00},
	    {"lemonchiffon", 0xfffacd}, {"lightblue", 0xadd8e6}, {"lightcoral", 0xf08080},
	    {"lightcyan", 0xe0ffff}, {"lightgoldenrodyellow", 0xfafad2}, {"lightgray", 0xd3d3d3},
	    {"lightgreen", 0x90ee90}, {"lightgrey", 0xd3d3d3}, {"lightpink", 0xffb6c1},
	    {"lightsalmon", 0xffa07a}, {"lightseagreen", 0x20b2aa}, {"lightskyblue", 0x87cefa},
	    {"lightslategray", 0x778899}, {"lightslategrey", 0x778899}, {"lightsteelblue", 0xb0c4de},
	    {"lightyellow", 0xffffe0}, {"lime", 0x00ff00}, {"limegreen", 0x32cd32},
	    {"linen", 0xfaf0e6}, {"magenta", 0xff00ff}, {"maroon", 0x800000},
	    {"mediumaquamarine", 0x66cdaa}, {"mediumblue", 0x0000cd}, {"mediumorchid", 0xba55d3},
	    {"mediumpurple", 0x9370db}, {"mediumseagreen", 0x3cb371}, {"mediumslateblue", 0x7b68ee},
	    {"mediumspringgreen", 0x00fa9a}, {"mediumturquoise", 0x48d1cc}, {"mediumvioletred", 0xc71585},
	    {"midnightblue", 0x191970}, {"mintcream", 0xf5fffa}, {"mistyrose", 0xffe4e1},
	    {"moccasin", 0xffe4b5}, {"navajowhite", 0xffdead}, {"navy", 0x000080},
	    {"oldlace", 0xfdf5e6}, {"olive", 0x808000}, {"olivedrab", 0x6b8e23},
	    {"orange", 0xffa500}, {"orangered", 0xff4500}, {"orchid", 0xda70d6},
	    {"palegoldenrod", 0xeee8aa}, {"palegreen", 0x98fb98}, {"paleturquoise", 0xafeeee},
	    {"palevioletred", 0xdb7093}, {"papayawhip", 0xffefd5}, {"peachpuff", 0xffdab9},
	    {"peru", 0xcd853f}, {"pink", 0xffc0cb}, {"plum", 0xdda0dd},
	    {"powderblue", 0xb0e0e6}, {"purple", 0x800080}, {"rebeccapurple", 0x663399},
	    {"red", 0xff0000}, {"rosybrown", 0xbc8f8f}, {"royalblue", 0x4169e1},
	    {"saddlebrown", 0x8b4513}, {"salmon", 0xfa8072}, {"sandybrown", 0xf4a460},
	    {"seagreen", 0x2e8b57}, {"seashell", 0xfff5ee}, {"sienna", 0xa0522d},
	    {"silver", 0xc0c0c0}, {"skyblue", 0x87ceeb}, {"slateblue", 0x6a5acd},
	    {"slategray", 0x708090}, {"slategrey", 0x708090}, {"snow", 0xfffafa},
	    {"springgreen", 0x00ff7f}, {"steelblue", 0x4682b4}, {"tan", 0xd2b48c},
	    {"teal", 0x008080}, {"thistle", 0xd8bfd8}, {"tomato", 0xff6347},
	    {"turquoise", 0x40e0d0}, {"violet", 0xee82ee}, {"wheat", 0xf5deb3},
	    {"white", 0xffffff}, {"whitesmoke", 0xf5f5f5}, {"yellow", 0xffff00},
	    {"yellowgreen", 0x9acd32},
	}};
	static_assert(named_srgb_colors.size() == 148);

	[[nodiscard]] inline std::vector<std::string_view> parse_rgb_components(std::string_view value)
	{
		std::vector<std::string_view> components;
		if (value.find(',') != std::string_view::npos)
		{
			if (value.find('/') != std::string_view::npos) [[unlikely]]
			{
				throw std::invalid_argument("CSS rgb() cannot mix comma and slash separators");
			}
			std::size_t start = 0;
			while (start <= value.size())
			{
				const auto comma = value.find(',', start);
				const auto end = comma == std::string_view::npos ? value.size() : comma;
				const auto component = trim(value.substr(start, end - start));
				if (component.empty()) [[unlikely]]
				{
					throw std::invalid_argument("CSS rgb() contains an empty component");
				}
				components.push_back(component);
				if (comma == std::string_view::npos)
				{
					break;
				}
				start = comma + 1;
			}
			return components;
		}

		bool alpha = false;
		std::size_t start = 0;
		while (start < value.size())
		{
			while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
			{
				++start;
			}
			if (start == value.size())
			{
				break;
			}
			if (value[start] == '/')
			{
				if (alpha) [[unlikely]]
				{
					throw std::invalid_argument("CSS rgb() contains multiple alpha separators");
				}
				alpha = true;
				++start;
				continue;
			}
			const auto end = value.find_first_of("/ \t\r\n", start);
			components.push_back(value.substr(start, end == std::string_view::npos ? value.size() - start
			                                                                    : end - start));
			start = end == std::string_view::npos ? value.size() : end;
		}
		return components;
	}

	[[nodiscard]] inline ::mngl::color parse_color(std::string_view value)
	{
		value = trim(value);
		const std::string normalized = lower_ascii(value);
		value = normalized;
		if (value == "transparent")
		{
			return {0.0f, 0.0f, 0.0f, 0.0f};
		}
		const auto named_color =
		    std::ranges::lower_bound(named_srgb_colors, value, {}, &named_srgb_color::name);
		if (named_color != named_srgb_colors.end() && named_color->name == value)
		{
			const auto channel = [&](std::uint32_t shift)
			{
				return static_cast<float>((named_color->rgb >> shift) & 0xffu) / 255.0f;
			};
			return {channel(16), channel(8), channel(0), 1.0f};
		}
		if ((value.size() == 4 || value.size() == 5) && value.front() == '#')
		{
			const float r = hex_digit(value[1]) / 15.0f;
			const float g = hex_digit(value[2]) / 15.0f;
			const float b = hex_digit(value[3]) / 15.0f;
			const float a = value.size() == 5 ? hex_digit(value[4]) / 15.0f : 1.0f;
			return {r, g, b, a};
		}
		if ((value.size() == 7 || value.size() == 9) && value.front() == '#')
		{
			const float r = (hex_digit(value[1]) * 16 + hex_digit(value[2])) / 255.0f;
			const float g = (hex_digit(value[3]) * 16 + hex_digit(value[4])) / 255.0f;
			const float b = (hex_digit(value[5]) * 16 + hex_digit(value[6])) / 255.0f;
			const float a =
			    value.size() == 9 ? (hex_digit(value[7]) * 16 + hex_digit(value[8])) / 255.0f : 1.0f;
			return {r, g, b, a};
		}
		if ((value.starts_with("rgb(") || value.starts_with("rgba(")) && value.ends_with(')'))
		{
			const auto open = value.find('(');
			const auto components = parse_rgb_components(value.substr(open + 1, value.size() - open - 2));
			if (components.size() != 3 && components.size() != 4) [[unlikely]]
			{
				throw std::invalid_argument("CSS rgb() color requires three channels and optional alpha");
			}
			bool percentage = components[0].ends_with('%');
			const float red = parse_color_channel(components[0], percentage);
			const float green = parse_color_channel(components[1], percentage);
			const float blue = parse_color_channel(components[2], percentage);
			const float alpha = components.size() == 4 ? parse_color_alpha(components[3]) : 1.0f;
			return {red, green, blue, alpha};
		}
		throw std::invalid_argument(
		    "SVG paint color is unsupported; use a named sRGB color, rgb()/rgba(), or hexadecimal color");
	}

	[[nodiscard]] inline ::mngl::color multiply_alpha(::mngl::color value, float factor)
	{
		auto rgba = value.rgba_glsl();
		rgba.a *= factor;
		return ::mngl::color(rgba);
	}

	struct xml_attribute
	{
		std::string name;
		std::string value;
		std::string namespace_uri;
	};

	struct xml_tag
	{
		std::string name;
		std::string namespace_uri;
		std::vector<xml_attribute> attributes;
		const xml_tag* parent = nullptr;
		const xml_tag* first_child = nullptr;
		const xml_tag* previous_sibling = nullptr;
		const xml_tag* next_sibling = nullptr;
		std::size_t sibling_index = 0;
		std::size_t sibling_index_from_end = 0;
		std::size_t type_index = 0;
		std::size_t type_index_from_end = 0;
		bool has_text = false;
		bool closing = false;
		bool self_closing = false;
	};

	[[nodiscard]] inline std::string decode_xml_entities(std::string_view value)
	{
		const auto append_code_point = [](std::string& output, std::uint32_t code_point)
		{
			const bool valid_xml_character = code_point == 0x9 || code_point == 0xa || code_point == 0xd ||
			                                 (code_point >= 0x20 && code_point <= 0xd7ff) ||
			                                 (code_point >= 0xe000 && code_point <= 0xfffd) ||
			                                 (code_point >= 0x10000 && code_point <= 0x10ffff);
			if (!valid_xml_character) [[unlikely]]
			{
				throw std::invalid_argument("SVG XML character reference is not a valid XML character");
			}
			if (code_point <= 0x7f)
			{
				output.push_back(static_cast<char>(code_point));
			}
			else if (code_point <= 0x7ff)
			{
				output.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
				output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
			}
			else if (code_point <= 0xffff)
			{
				output.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
				output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
				output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
			}
			else
			{
				output.push_back(static_cast<char>(0xf0 | (code_point >> 18)));
				output.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
				output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
				output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
			}
		};

		std::string decoded;
		decoded.reserve(value.size());
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			if (value[i] != '&')
			{
				decoded.push_back(value[i]);
				continue;
			}
			const std::size_t semicolon = value.find(';', i + 1);
			if (semicolon == std::string_view::npos) [[unlikely]]
			{
				throw std::invalid_argument("SVG XML contains an unterminated entity");
			}
			const auto entity = value.substr(i, semicolon - i + 1);
			if (entity == "&amp;")
				decoded.push_back('&');
			else if (entity == "&quot;")
				decoded.push_back('"');
			else if (entity == "&apos;")
				decoded.push_back('\'');
			else if (entity == "&lt;")
				decoded.push_back('<');
			else if (entity == "&gt;")
				decoded.push_back('>');
			else if (entity.size() > 3 && entity[1] == '#')
			{
				const bool hexadecimal = entity.size() > 4 && entity[2] == 'x';
				const std::size_t digits_start = hexadecimal ? 3 : 2;
				const auto digits = entity.substr(digits_start, entity.size() - digits_start - 1);
				std::uint32_t code_point = 0;
				const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), code_point,
				                                    hexadecimal ? 16 : 10);
				if (digits.empty() || parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size())
					throw std::invalid_argument("SVG XML contains an invalid character reference");
				append_code_point(decoded, code_point);
			}
			else
				throw std::invalid_argument("SVG XML entity is not supported");
			i = semicolon;
		}
		return decoded;
	}

	class xml_reader
	{
	public:
		explicit xml_reader(std::string_view source) noexcept : m_source(source) {}

		[[nodiscard]] std::optional<xml_tag> next(std::string* preceding_text = nullptr)
		{
			if (preceding_text)
			{
				preceding_text->clear();
			}
			while (m_offset < m_source.size())
			{
				const std::size_t open = m_source.find('<', m_offset);
				if (open == std::string_view::npos)
				{
					m_offset = m_source.size();
					break;
				}
				if (open > m_offset && preceding_text)
				{
					preceding_text->append(decode_xml_entities(m_source.substr(m_offset, open - m_offset)));
				}
				m_offset = open;

				if (m_source.substr(m_offset).starts_with("<!--"))
				{
					skip_until("-->", 4);
					continue;
				}
				if (m_source.substr(m_offset).starts_with("<![CDATA["))
				{
					const std::size_t text_start = m_offset + 9;
					const std::size_t end = m_source.find("]]>", text_start);
					if (end == std::string_view::npos) [[unlikely]]
					{
						throw std::invalid_argument(
						    "SVG XML contains an unterminated comment or CDATA section");
					}
					if (preceding_text)
					{
						preceding_text->append(m_source.substr(text_start, end - text_start));
					}
					m_offset = end + 3;
					continue;
				}
				if (m_source.substr(m_offset).starts_with("<?") ||
				    m_source.substr(m_offset).starts_with("<!DOCTYPE"))
				{
					skip_declaration();
					continue;
				}

				return read_tag();
			}
			return std::nullopt;
		}

	private:
		void skip_until(std::string_view terminator, std::size_t prefix_size)
		{
			const std::size_t end = m_source.find(terminator, m_offset + prefix_size);
			if (end == std::string_view::npos) [[unlikely]]
			{
				throw std::invalid_argument("SVG XML contains an unterminated comment or CDATA section");
			}
			m_offset = end + terminator.size();
		}

		void skip_declaration()
		{
			char quote = '\0';
			while (m_offset < m_source.size())
			{
				const char value = m_source[m_offset++];
				if (quote != '\0')
				{
					if (value == quote)
					{
						quote = '\0';
					}
					continue;
				}
				if (value == '\'' || value == '"')
				{
					quote = value;
				}
				else if (value == '>')
				{
					return;
				}
			}
			throw std::invalid_argument("SVG XML contains an unterminated declaration");
		}

		[[nodiscard]] xml_tag read_tag()
		{
			xml_tag tag;
			++m_offset;
			if (m_offset < m_source.size() && m_source[m_offset] == '/')
			{
				tag.closing = true;
				++m_offset;
			}
			skip_space();
			tag.name = read_name();
			if (tag.name.empty()) [[unlikely]]
			{
				throw std::invalid_argument("SVG XML contains an empty element name");
			}
			if (tag.closing)
			{
				skip_to_close();
				return tag;
			}

			while (m_offset < m_source.size())
			{
				skip_space();
				if (m_offset == m_source.size()) [[unlikely]]
				{
					break;
				}
				if (m_source[m_offset] == '>')
				{
					++m_offset;
					return tag;
				}
				if (m_source[m_offset] == '/' && m_offset + 1 < m_source.size() &&
				    m_source[m_offset + 1] == '>')
				{
					tag.self_closing = true;
					m_offset += 2;
					return tag;
				}

				xml_attribute attribute;
				attribute.name = read_name();
				if (attribute.name.empty()) [[unlikely]]
				{
					throw std::invalid_argument("SVG XML contains an invalid attribute name");
				}
				skip_space();
				if (m_offset == m_source.size() || m_source[m_offset++] != '=') [[unlikely]]
				{
					throw std::invalid_argument("SVG XML attribute is missing '='");
				}
				skip_space();
				if (m_offset == m_source.size() || (m_source[m_offset] != '\'' && m_source[m_offset] != '"'))
				    [[unlikely]]
				{
					throw std::invalid_argument("SVG XML attribute value must be quoted");
				}
				const char quote = m_source[m_offset++];
				const std::size_t value_start = m_offset;
				const std::size_t value_end = m_source.find(quote, value_start);
				if (value_end == std::string_view::npos) [[unlikely]]
				{
					throw std::invalid_argument("SVG XML contains an unterminated attribute value");
				}
				attribute.value = decode_xml_entities(m_source.substr(value_start, value_end - value_start));
				m_offset = value_end + 1;
				tag.attributes.push_back(std::move(attribute));
			}
			throw std::invalid_argument("SVG XML contains an unterminated start tag");
		}

		void skip_space() noexcept
		{
			while (m_offset < m_source.size() && std::isspace(static_cast<unsigned char>(m_source[m_offset])))
			{
				++m_offset;
			}
		}

		[[nodiscard]] std::string read_name()
		{
			const std::size_t start = m_offset;
			while (m_offset < m_source.size())
			{
				const char value = m_source[m_offset];
				if (!std::isalnum(static_cast<unsigned char>(value)) && value != '_' && value != '-' &&
				    value != ':' && value != '.')
				{
					break;
				}
				++m_offset;
			}
			return std::string(m_source.substr(start, m_offset - start));
		}

		void skip_to_close()
		{
			char quote = '\0';
			while (m_offset < m_source.size())
			{
				const char value = m_source[m_offset++];
				if (quote != '\0')
				{
					if (value == quote)
					{
						quote = '\0';
					}
				}
				else if (value == '\'' || value == '"')
				{
					quote = value;
				}
				else if (value == '>')
				{
					return;
				}
			}
			throw std::invalid_argument("SVG XML contains an unterminated closing tag");
		}

		std::string_view m_source;
		std::size_t m_offset = 0;
	};

	[[nodiscard]] inline std::string_view local_name(std::string_view name) noexcept
	{
		const std::size_t colon = name.rfind(':');
		return colon == std::string_view::npos ? name : name.substr(colon + 1);
	}

	inline constexpr std::string_view svg_namespace_uri = "http://www.w3.org/2000/svg";
	inline constexpr std::string_view xml_namespace_uri = "http://www.w3.org/XML/1998/namespace";
	inline constexpr std::string_view xmlns_namespace_uri = "http://www.w3.org/2000/xmlns/";
	inline constexpr std::string_view xlink_namespace_uri = "http://www.w3.org/1999/xlink";
	using xml_namespace_scope = std::unordered_map<std::string, std::string>;

	[[nodiscard]] inline bool is_svg_element(const xml_tag& tag) noexcept
	{
		// Namespace-free documents remain supported for compatibility with existing callers.
		return tag.namespace_uri.empty() || tag.namespace_uri == svg_namespace_uri;
	}

	[[nodiscard]] inline std::string resolve_element_namespace(std::string_view name,
	                                                          const xml_namespace_scope& scope)
	{
		const auto colon = name.find(':');
		if (colon == std::string_view::npos)
		{
			const auto default_namespace = scope.find("");
			return default_namespace == scope.end() ? std::string{} : default_namespace->second;
		}
		if (colon == 0 || colon + 1 == name.size() || name.find(':', colon + 1) != std::string_view::npos)
		{
			throw std::invalid_argument("SVG XML contains an invalid qualified element name");
		}
		const auto prefix = scope.find(std::string(name.substr(0, colon)));
		if (prefix == scope.end()) [[unlikely]]
		{
			throw std::invalid_argument("SVG XML element uses an undeclared namespace prefix");
		}
		return prefix->second;
	}

	[[nodiscard]] inline std::string resolve_attribute_namespace(std::string_view name,
	                                                            const xml_namespace_scope& scope)
	{
		return name.find(':') == std::string_view::npos ? std::string{} : resolve_element_namespace(name, scope);
	}

	[[nodiscard]] inline std::optional<std::string_view> attribute_value(const xml_tag& tag,
	                                                                     std::string_view name) noexcept
	{
		for (const auto& attribute : tag.attributes)
		{
			if (attribute.namespace_uri.empty() && attribute.name == name)
			{
				return attribute.value;
			}
		}
		if (name == "id")
		{
			for (const auto& attribute : tag.attributes)
			{
				if (attribute.namespace_uri == xml_namespace_uri && attribute.name == "xml:id")
				{
					return attribute.value;
				}
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] inline std::optional<std::string_view> svg_href_value(const xml_tag& tag) noexcept
	{
		for (const auto& attribute : tag.attributes)
		{
			if (attribute.name == "href")
			{
				return attribute.value;
			}
		}
		for (const auto& attribute : tag.attributes)
		{
			if (attribute.namespace_uri == xlink_namespace_uri && local_name(attribute.name) == "href")
			{
				return attribute.value;
			}
		}
		return std::nullopt;
	}

	struct xml_node
	{
		xml_tag tag;
		std::vector<xml_node> children;
	};

	struct contour_cache_key
	{
		const xml_node* node = nullptr;
		float tolerance = 0.0f;
		std::size_t max_curve_segments = 0;
		std::array<float, 4> view_box{};
		bool record_markers = false;
		bool operator==(const contour_cache_key&) const = default;
	};

	[[nodiscard]] inline std::size_t combine_hash(std::size_t seed, std::size_t value) noexcept
	{
		return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
	}

	struct contour_cache_key_hash
	{
		[[nodiscard]] std::size_t operator()(const contour_cache_key& key) const noexcept
		{
			std::size_t hash = std::hash<const xml_node*>{}(key.node);
			hash = combine_hash(hash, std::hash<float>{}(key.tolerance));
			hash = combine_hash(hash, std::hash<std::size_t>{}(key.max_curve_segments));
			for (const auto value : key.view_box)
				hash = combine_hash(hash, std::hash<float>{}(value));
			return combine_hash(hash, std::hash<bool>{}(key.record_markers));
		}
	};

	struct tessellation_cache_key
	{
		contour_cache_key contour;
		bool fill_enabled = false;
		bool stroke_enabled = false;
		bool even_odd = false;
		float stroke_width = 0.0f;
		float stroke_miterlimit = 0.0f;
		float stroke_dashoffset = 0.0f;
		stroke_linecap linecap = stroke_linecap::butt;
		stroke_linejoin linejoin = stroke_linejoin::miter;
		std::vector<float> stroke_dasharray;
		bool operator==(const tessellation_cache_key&) const = default;
	};

	struct tessellation_cache_key_hash
	{
		[[nodiscard]] std::size_t operator()(const tessellation_cache_key& key) const noexcept
		{
			std::size_t hash = contour_cache_key_hash{}(key.contour);
			hash = combine_hash(hash, std::hash<bool>{}(key.fill_enabled));
			hash = combine_hash(hash, std::hash<bool>{}(key.stroke_enabled));
			hash = combine_hash(hash, std::hash<bool>{}(key.even_odd));
			hash = combine_hash(hash, std::hash<float>{}(key.stroke_width));
			hash = combine_hash(hash, std::hash<float>{}(key.stroke_miterlimit));
			hash = combine_hash(hash, std::hash<float>{}(key.stroke_dashoffset));
			hash = combine_hash(hash, std::hash<int>{}(static_cast<int>(key.linecap)));
			hash = combine_hash(hash, std::hash<int>{}(static_cast<int>(key.linejoin)));
			for (const auto dash : key.stroke_dasharray)
				hash = combine_hash(hash, std::hash<float>{}(dash));
			return hash;
		}
	};

	inline void link_xml_parents(xml_node& node)
	{
		node.tag.first_child = node.children.empty() ? nullptr : &node.children.front().tag;
		std::unordered_map<std::string, std::size_t> type_counts;
		for (std::size_t i = 0; i < node.children.size(); ++i)
		{
			auto& child = node.children[i];
			child.tag.parent = &node.tag;
			child.tag.previous_sibling = i == 0 ? nullptr : &node.children[i - 1].tag;
			child.tag.next_sibling = i + 1 == node.children.size() ? nullptr : &node.children[i + 1].tag;
			child.tag.sibling_index = i + 1;
			std::string expanded_name = child.tag.namespace_uri;
			expanded_name.push_back('\0');
			expanded_name.append(local_name(child.tag.name));
			child.tag.type_index = ++type_counts[std::move(expanded_name)];
			link_xml_parents(child);
		}
		type_counts.clear();
		for (std::size_t i = node.children.size(); i > 0; --i)
		{
			auto& child = node.children[i - 1];
			child.tag.sibling_index_from_end = node.children.size() - i + 1;
			std::string expanded_name = child.tag.namespace_uri;
			expanded_name.push_back('\0');
			expanded_name.append(local_name(child.tag.name));
			child.tag.type_index_from_end = ++type_counts[std::move(expanded_name)];
		}
	}

	struct css_stylesheet;
	inline void append_css_rules(std::string_view source, css_stylesheet& stylesheet,
	                             std::vector<std::string> media_conditions = {}, std::size_t nesting = 0);

	[[nodiscard]] inline xml_node parse_xml_tree(std::string_view source,
	                                             css_stylesheet* stylesheet = nullptr)
	{
		xml_reader reader(source);
		std::optional<xml_node> root;
		std::vector<xml_node*> parents;
		std::vector<xml_namespace_scope> namespace_scopes;
		std::string preceding_text;
		std::string style_text;
		std::vector<std::string> style_media_conditions;
		while (auto tag = reader.next(&preceding_text))
		{
			xml_namespace_scope namespace_scope = namespace_scopes.empty()
			                                               ? xml_namespace_scope{{"xml", std::string(xml_namespace_uri)}}
			                                               : namespace_scopes.back();
			if (!tag->closing)
			{
				for (const auto& attribute : tag->attributes)
				{
					if (attribute.name == "xmlns")
					{
						namespace_scope.insert_or_assign("", attribute.value);
					}
					else if (attribute.name.starts_with("xmlns:"))
					{
						namespace_scope.insert_or_assign(attribute.name.substr(6), attribute.value);
					}
				}
				for (auto& attribute : tag->attributes)
				{
					attribute.namespace_uri = attribute.name == "xmlns" || attribute.name.starts_with("xmlns:")
					                              ? std::string(xmlns_namespace_uri)
					                              : resolve_attribute_namespace(attribute.name, namespace_scope);
				}
			}
			tag->namespace_uri = resolve_element_namespace(tag->name, namespace_scope);
			if (!parents.empty() && !preceding_text.empty())
			{
				parents.back()->tag.has_text = true;
			}
			const auto name = local_name(tag->name);
			if (!parents.empty() && is_svg_element(parents.back()->tag) &&
			    local_name(parents.back()->tag.name) == "style")
			{
				style_text += preceding_text;
				if (!tag->closing) [[unlikely]]
				{
					throw std::invalid_argument("SVG <style> cannot contain child elements");
				}
				if (stylesheet && is_svg_element(parents.back()->tag) && name == "style")
				{
					append_css_rules(style_text, *stylesheet, std::move(style_media_conditions));
					style_text.clear();
					style_media_conditions.clear();
				}
			}
			if (tag->closing)
			{
				if (parents.empty() || parents.back()->tag.name != tag->name) [[unlikely]]
				{
					throw std::invalid_argument("SVG XML contains a mismatched closing tag");
				}
				parents.pop_back();
				namespace_scopes.pop_back();
				continue;
			}
			if (stylesheet && is_svg_element(*tag) && name == "style")
			{
				style_media_conditions.clear();
				if (const auto media = attribute_value(*tag, "media"))
				{
					style_media_conditions.emplace_back(*media);
				}
				if (const auto type = attribute_value(*tag, "type");
				    type && lower_ascii(trim(*type)) != "text/css") [[unlikely]]
				{
					throw std::invalid_argument("SVG <style> supports only text/css");
				}
				if (tag->self_closing)
				{
					append_css_rules({}, *stylesheet, std::move(style_media_conditions));
					style_media_conditions.clear();
				}
				else
				{
					style_text.clear();
				}
			}

			xml_node* inserted = nullptr;
			if (parents.empty())
			{
				if (root) [[unlikely]]
				{
					throw std::invalid_argument("SVG XML contains multiple root elements");
				}
				root.emplace(xml_node{std::move(*tag), {}});
				inserted = &*root;
			}
			else
			{
				parents.back()->children.push_back(xml_node{std::move(*tag), {}});
				inserted = &parents.back()->children.back();
			}

			if (!inserted->tag.self_closing)
			{
				if (parents.size() >= 256) [[unlikely]]
				{
					throw std::length_error("SVG XML nesting exceeds the depth limit");
				}
				parents.push_back(inserted);
				namespace_scopes.push_back(std::move(namespace_scope));
			}
		}
		if (!root || !parents.empty()) [[unlikely]]
		{
			throw std::invalid_argument("SVG XML contains a missing root or unclosed element");
		}
		return std::move(*root);
	}

	enum class css_attribute_operator
	{
		exists,
		equals,
		includes,
		dash_prefix,
		prefix,
		suffix,
		substring
	};

	struct css_attribute_selector
	{
		std::string name;
		std::string value;
		css_attribute_operator operation = css_attribute_operator::exists;
		bool case_insensitive = false;
	};

	enum class css_pseudo_class_kind
	{
		root,
		empty,
		first_child,
		last_child,
		only_child,
		first_of_type,
		last_of_type,
		only_of_type,
		nth_child,
		nth_last_child,
		nth_of_type,
		nth_last_of_type,
		not_,
		is,
		where,
		lang
	};

	struct css_selector;

	struct css_pseudo_class_selector
	{
		css_pseudo_class_kind kind = css_pseudo_class_kind::root;
		std::vector<std::shared_ptr<const css_selector>> selectors;
		std::string value;
		std::int64_t nth_a = 0;
		std::int64_t nth_b = 0;
	};

	struct css_compound_selector
	{
		std::string element;
		std::string id;
		std::vector<std::string> classes;
		std::vector<css_attribute_selector> attributes;
		std::vector<css_pseudo_class_selector> pseudo_classes;
	};

	struct css_selector
	{
		std::vector<css_compound_selector> descendants;
		std::array<std::size_t, 4> specificity{};
	};

	struct css_declaration
	{
		std::string name;
		std::string value;
		std::size_t order = 0;
		bool important = false;
	};

	struct css_rule
	{
		css_selector selector;
		std::vector<css_declaration> declarations;
		std::vector<std::string> media_conditions;
		bool media_matches = true;
	};

	struct css_cascaded_value
	{
		std::string value;
		std::array<std::size_t, 4> specificity{};
		std::size_t order = 0;
		bool important = false;
	};

	struct css_stylesheet
	{
		std::vector<css_rule> rules;
		std::unordered_map<std::string, std::vector<std::size_t>> rules_by_id;
		std::unordered_map<std::string, std::vector<std::size_t>> rules_by_class;
		std::unordered_map<std::string, std::vector<std::size_t>> rules_by_element;
		std::vector<std::size_t> unindexed_rules;
		std::size_t next_order = 1;
		float viewport_width = 300.0f;
		float viewport_height = 150.0f;
		mutable std::unordered_map<const xml_tag*, std::unordered_map<std::string, css_cascaded_value>>
		    resolved_properties;
	};

	[[nodiscard]] inline std::string strip_css_comments(std::string_view source)
	{
		std::string result;
		result.reserve(source.size());
		char quote = '\0';
		bool escaped = false;
		for (std::size_t offset = 0; offset < source.size(); ++offset)
		{
			const char value = source[offset];
			if (quote != '\0')
			{
				result.push_back(value);
				if (escaped)
				{
					escaped = false;
				}
				else if (value == '\\')
				{
					escaped = true;
				}
				else if (value == quote)
				{
					quote = '\0';
				}
				continue;
			}
			if (value == '\'' || value == '"')
			{
				quote = value;
				result.push_back(value);
			}
			else if (value == '/' && offset + 1 < source.size() && source[offset + 1] == '*')
			{
				const std::size_t end = source.find("*/", offset + 2);
				if (end == std::string_view::npos) [[unlikely]]
				{
					throw std::invalid_argument("SVG CSS contains an unterminated comment");
				}
				const auto is_name_character = [](char character)
				{
					const auto byte = static_cast<unsigned char>(character);
					return std::isalnum(byte) || character == '_' || character == '-' || character == '\\' ||
					       byte >= 0x80;
				};
				if (!result.empty() && end + 2 < source.size() && is_name_character(result.back()) &&
				    is_name_character(source[end + 2]))
				{
					result.push_back(' ');
				}
				offset = end + 1;
			}
			else
			{
				result.push_back(value);
			}
		}
		return result;
	}

	[[nodiscard]] inline bool css_name_start(char value) noexcept
	{
		const auto character = static_cast<unsigned char>(value);
		return std::isalpha(character) || character == '_' || character == '-' || character >= 0x80;
	}

	[[nodiscard]] inline bool css_name_character(char value) noexcept
	{
		const auto character = static_cast<unsigned char>(value);
		return css_name_start(value) || std::isdigit(character);
	}

	inline void append_utf8(std::string& output, std::uint32_t codepoint)
	{
		if (codepoint == 0 || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
			codepoint = 0xfffd;
		if (codepoint <= 0x7f)
			output.push_back(static_cast<char>(codepoint));
		else if (codepoint <= 0x7ff)
		{
			output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
			output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
		}
		else if (codepoint <= 0xffff)
		{
			output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
			output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
			output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
		}
		else
		{
			output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
			output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
			output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
			output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
		}
	}

	[[nodiscard]] inline std::string read_css_escape(std::string_view source, std::size_t& offset)
	{
		if (offset >= source.size() || source[offset] != '\\' || ++offset >= source.size()) [[unlikely]]
			throw std::invalid_argument("SVG CSS name ends with an escape");
		const auto hex = [](char value) noexcept
		{
			if (value >= '0' && value <= '9') return value - '0';
			if (value >= 'a' && value <= 'f') return value - 'a' + 10;
			if (value >= 'A' && value <= 'F') return value - 'A' + 10;
			return -1;
		};
		std::uint32_t codepoint = 0;
		int digit = hex(source[offset]);
		if (digit >= 0)
		{
			std::size_t digits = 0;
			while (offset < source.size() && digits < 6 && (digit = hex(source[offset])) >= 0)
			{
				codepoint = codepoint * 16 + static_cast<std::uint32_t>(digit);
				++offset;
				++digits;
			}
			if (offset < source.size() && std::isspace(static_cast<unsigned char>(source[offset])))
			{
				if (source[offset] == '\r' && offset + 1 < source.size() && source[offset + 1] == '\n')
					++offset;
				++offset;
			}
		}
		else
		{
			codepoint = static_cast<unsigned char>(source[offset++]);
		}
		std::string result;
		append_utf8(result, codepoint);
		return result;
	}

	[[nodiscard]] inline std::string read_css_name(std::string_view source, std::size_t& offset)
	{
		if (offset >= source.size() || (!css_name_start(source[offset]) && source[offset] != '\\')) [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS selector contains an invalid name");
		}
		std::string result;
		while (offset < source.size())
		{
			if (source[offset] == '\\')
				result += read_css_escape(source, offset);
			else if (css_name_character(source[offset]))
				result.push_back(source[offset++]);
			else
				break;
		}
		return result;
	}

	[[nodiscard]] inline css_attribute_selector parse_css_attribute_selector(std::string_view source,
	                                                                         std::size_t& offset)
	{
		if (offset >= source.size() || source[offset] != '[') [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS attribute selector must start with '['");
		}
		++offset;
		const auto skip_space = [&]()
		{
			while (offset < source.size() && std::isspace(static_cast<unsigned char>(source[offset])))
			{
				++offset;
			}
		};
		skip_space();

		if (offset >= source.size() || (!css_name_start(source[offset]) && source[offset] != '\\')) [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS attribute selector has an invalid attribute name");
		}
		std::string attribute_name;
		while (offset < source.size())
		{
			if (source[offset] == '\\')
				attribute_name += read_css_escape(source, offset);
			else if (css_name_character(source[offset]) || source[offset] == ':')
				attribute_name.push_back(source[offset++]);
			else
				break;
		}
		css_attribute_selector result;
		result.name = std::move(attribute_name);
		skip_space();
		if (offset >= source.size()) [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS attribute selector is missing ']'");
		}
		if (source[offset] == ']')
		{
			++offset;
			return result;
		}

		const auto remaining = source.substr(offset);
		if (remaining.starts_with("~="))
		{
			result.operation = css_attribute_operator::includes;
			offset += 2;
		}
		else if (remaining.starts_with("|="))
		{
			result.operation = css_attribute_operator::dash_prefix;
			offset += 2;
		}
		else if (remaining.starts_with("^="))
		{
			result.operation = css_attribute_operator::prefix;
			offset += 2;
		}
		else if (remaining.starts_with("$="))
		{
			result.operation = css_attribute_operator::suffix;
			offset += 2;
		}
		else if (remaining.starts_with("*="))
		{
			result.operation = css_attribute_operator::substring;
			offset += 2;
		}
		else if (source[offset] == '=')
		{
			result.operation = css_attribute_operator::equals;
			++offset;
		}
		else
		{
			throw std::invalid_argument("SVG CSS attribute selector contains an unsupported operator");
		}

		skip_space();
		if (offset >= source.size() || source[offset] == ']') [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS attribute selector requires a value");
		}
		if (source[offset] == '"' || source[offset] == '\'')
		{
			const char quote = source[offset++];
			bool closed = false;
			while (offset < source.size())
			{
				const char value = source[offset++];
				if (value == '\\')
				{
					--offset;
					result.value += read_css_escape(source, offset);
				}
				else if (value == quote)
				{
					closed = true;
					break;
				}
				else
				{
					result.value.push_back(value);
				}
			}
			if (!closed) [[unlikely]]
			{
				throw std::invalid_argument("SVG CSS attribute selector contains an unterminated string");
			}
		}
		else
		{
			while (offset < source.size() && source[offset] != ']' &&
			       !std::isspace(static_cast<unsigned char>(source[offset])))
			{
				if (source[offset] == '\\')
					result.value += read_css_escape(source, offset);
				else
					result.value.push_back(source[offset++]);
			}
			if (result.value.empty()) [[unlikely]]
				throw std::invalid_argument("SVG CSS attribute selector requires a value");
		}

		const std::size_t trailing_space = offset;
		skip_space();
		if (offset < source.size() && source[offset] != ']')
		{
			if (trailing_space == offset) [[unlikely]]
			{
				throw std::invalid_argument("SVG CSS attribute selector flags must follow whitespace");
			}
			const char flag = static_cast<char>(std::tolower(static_cast<unsigned char>(source[offset])));
			if (flag != 'i' && flag != 's') [[unlikely]]
			{
				throw std::invalid_argument("SVG CSS attribute selector flag must be 'i' or 's'");
			}
			result.case_insensitive = flag == 'i';
			++offset;
			skip_space();
		}
		if (offset >= source.size() || source[offset] != ']') [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS attribute selector is missing ']'");
		}
		++offset;
		return result;
	}

	[[nodiscard]] inline bool css_attribute_text_equal(std::string_view left, std::string_view right,
	                                                   bool case_insensitive) noexcept
	{
		if (left.size() != right.size())
		{
			return false;
		}
		for (std::size_t i = 0; i < left.size(); ++i)
		{
			auto left_character = static_cast<unsigned char>(left[i]);
			auto right_character = static_cast<unsigned char>(right[i]);
			if (case_insensitive)
			{
				if (left_character >= 'A' && left_character <= 'Z')
					left_character += 'a' - 'A';
				if (right_character >= 'A' && right_character <= 'Z')
					right_character += 'a' - 'A';
			}
			if (left_character != right_character)
			{
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] inline bool matches_css_attribute_value(const css_attribute_selector& selector,
	                                                      std::string_view actual) noexcept
	{
		const auto equal = [&](std::string_view left, std::string_view right)
		{ return css_attribute_text_equal(left, right, selector.case_insensitive); };
		switch (selector.operation)
		{
		case css_attribute_operator::exists:
			return true;
		case css_attribute_operator::equals:
			return equal(actual, selector.value);
		case css_attribute_operator::includes:
		{
			if (selector.value.empty())
				return false;
			std::size_t offset = 0;
			while (offset < actual.size())
			{
				while (offset < actual.size() && std::isspace(static_cast<unsigned char>(actual[offset])))
					++offset;
				const std::size_t start = offset;
				while (offset < actual.size() && !std::isspace(static_cast<unsigned char>(actual[offset])))
					++offset;
				if (start != offset && equal(actual.substr(start, offset - start), selector.value))
					return true;
			}
			return false;
		}
		case css_attribute_operator::dash_prefix:
			return equal(actual, selector.value) ||
			       (actual.size() > selector.value.size() && actual[selector.value.size()] == '-' &&
			        equal(actual.substr(0, selector.value.size()), selector.value));
		case css_attribute_operator::prefix:
			return !selector.value.empty() && actual.size() >= selector.value.size() &&
			       equal(actual.substr(0, selector.value.size()), selector.value);
		case css_attribute_operator::suffix:
			return !selector.value.empty() && actual.size() >= selector.value.size() &&
			       equal(actual.substr(actual.size() - selector.value.size()), selector.value);
		case css_attribute_operator::substring:
			if (selector.value.empty() || actual.size() < selector.value.size())
				return false;
			for (std::size_t offset = 0; offset + selector.value.size() <= actual.size(); ++offset)
			{
				if (equal(actual.substr(offset, selector.value.size()), selector.value))
					return true;
			}
			return false;
		}
		return false;
	}

	[[nodiscard]] inline std::vector<std::string_view> split_css_selector_list(std::string_view source);

	[[nodiscard]] inline std::string_view read_css_function_argument(std::string_view source,
	                                                                 std::size_t& offset)
	{
		if (offset >= source.size() || source[offset] != '(') [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS pseudo-class function is missing '('");
		}
		const std::size_t start = ++offset;
		std::size_t depth = 1;
		std::size_t bracket_depth = 0;
		char quote = '\0';
		bool escaped = false;
		for (; offset < source.size(); ++offset)
		{
			const char value = source[offset];
			if (quote != '\0')
			{
				if (escaped)
				{
					escaped = false;
				}
				else if (value == '\\')
				{
					escaped = true;
				}
				else if (value == quote)
				{
					quote = '\0';
				}
				continue;
			}
			if (value == '\'' || value == '"')
			{
				quote = value;
			}
			else if (value == '[')
			{
				++bracket_depth;
			}
			else if (value == ']' && bracket_depth > 0)
			{
				--bracket_depth;
			}
			else if (bracket_depth == 0 && value == '(')
			{
				++depth;
			}
			else if (bracket_depth == 0 && value == ')')
			{
				if (--depth == 0)
				{
					const auto result = source.substr(start, offset - start);
					++offset;
					return result;
				}
			}
		}
		throw std::invalid_argument("SVG CSS pseudo-class has an unterminated argument");
	}

	[[nodiscard]] inline std::int64_t parse_css_integer(std::string_view source)
	{
		if (source.empty()) [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS nth-child formula has an invalid integer");
		}
		if (source.front() == '+')
		{
			source.remove_prefix(1);
			if (source.empty()) [[unlikely]]
			{
				throw std::invalid_argument("SVG CSS nth-child formula has an invalid integer");
			}
		}
		std::int64_t result = 0;
		const auto parsed = std::from_chars(source.data(), source.data() + source.size(), result);
		if (parsed.ec != std::errc{} || parsed.ptr != source.data() + source.size()) [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS nth-child formula has an invalid integer");
		}
		return result;
	}

	[[nodiscard]] inline std::pair<std::int64_t, std::int64_t> parse_css_nth_formula(std::string_view source)
	{
		std::string expression = lower_ascii(trim(source));
		std::erase_if(expression, [](char value) { return std::isspace(static_cast<unsigned char>(value)); });
		if (expression == "odd")
			return {2, 1};
		if (expression == "even")
			return {2, 0};

		const std::size_t n_position = expression.find('n');
		if (n_position == std::string::npos)
			return {0, parse_css_integer(expression)};
		if (expression.find('n', n_position + 1) != std::string::npos) [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS nth-child formula contains multiple 'n' terms");
		}

		const auto coefficient = std::string_view(expression).substr(0, n_position);
		std::int64_t a = 1;
		if (coefficient == "-")
			a = -1;
		else if (!coefficient.empty() && coefficient != "+")
			a = parse_css_integer(coefficient);

		const auto constant = std::string_view(expression).substr(n_position + 1);
		std::int64_t b = 0;
		if (!constant.empty())
		{
			b = parse_css_integer(constant);
		}
		return {a, b};
	}

	[[nodiscard]] inline std::string parse_css_lang_argument(std::string_view source)
	{
		source = trim(source);
		std::string value;
		if (!source.empty() && (source.front() == '\'' || source.front() == '"'))
		{
			const char quote = source.front();
			if (source.size() < 2 || source.back() != quote) [[unlikely]]
			{
				throw std::invalid_argument("SVG CSS :lang() contains an invalid string");
			}
			for (std::size_t offset = 1; offset + 1 < source.size();)
			{
				if (source[offset] == '\\')
				{
					value += read_css_escape(source, offset);
					if (offset >= source.size()) [[unlikely]]
					{
						throw std::invalid_argument("SVG CSS :lang() string escape crosses its closing quote");
					}
				}
				else
				{
					value.push_back(source[offset++]);
				}
			}
		}
		else if (source == "*")
		{
			value = "*";
		}
		else
		{
			std::size_t offset = 0;
			value = read_css_name(source, offset);
			if (offset != source.size()) [[unlikely]]
			{
				throw std::invalid_argument("SVG CSS :lang() expects a single language identifier");
			}
		}
		if (value.empty()) [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS :lang() expects a non-empty language tag");
		}
		return lower_ascii(value);
	}

	[[nodiscard]] inline css_selector parse_css_selector(std::string_view source, std::size_t nesting = 0)
	{
		if (nesting >= 64) [[unlikely]]
		{
			throw std::length_error("SVG CSS pseudo-class nesting exceeds the depth limit");
		}
		source = trim(source);
		if (source.empty()) [[unlikely]]
		{
			throw std::invalid_argument("SVG CSS contains an empty selector");
		}

		css_selector result;
		std::size_t offset = 0;
		while (offset < source.size())
		{
			css_compound_selector compound;
			if (source[offset] == '*')
			{
				compound.element = "*";
				++offset;
			}
			else if (css_name_start(source[offset]) || source[offset] == '\\')
			{
				compound.element = std::string(read_css_name(source, offset));
				++result.specificity[3];
			}

			while (offset < source.size() && (source[offset] == '.' || source[offset] == '#' ||
			                                  source[offset] == '[' || source[offset] == ':'))
			{
				if (source[offset] == '[')
				{
					compound.attributes.push_back(parse_css_attribute_selector(source, offset));
					++result.specificity[2];
					continue;
				}
				if (source[offset] == ':')
				{
					if (offset + 1 < source.size() && source[offset + 1] == ':') [[unlikely]]
					{
						throw std::invalid_argument("SVG CSS pseudo-elements are not supported");
					}
					++offset;
					const auto pseudo_name = lower_ascii(read_css_name(source, offset));
					const bool functional = offset < source.size() && source[offset] == '(';
					std::string_view argument;
					if (functional)
					{
						argument = read_css_function_argument(source, offset);
					}

					css_pseudo_class_selector pseudo;
					if (pseudo_name == "root")
						pseudo.kind = css_pseudo_class_kind::root;
					else if (pseudo_name == "empty")
						pseudo.kind = css_pseudo_class_kind::empty;
					else if (pseudo_name == "first-child")
						pseudo.kind = css_pseudo_class_kind::first_child;
					else if (pseudo_name == "last-child")
						pseudo.kind = css_pseudo_class_kind::last_child;
					else if (pseudo_name == "only-child")
						pseudo.kind = css_pseudo_class_kind::only_child;
					else if (pseudo_name == "first-of-type")
						pseudo.kind = css_pseudo_class_kind::first_of_type;
					else if (pseudo_name == "last-of-type")
						pseudo.kind = css_pseudo_class_kind::last_of_type;
					else if (pseudo_name == "only-of-type")
						pseudo.kind = css_pseudo_class_kind::only_of_type;
					else if (pseudo_name == "nth-child")
						pseudo.kind = css_pseudo_class_kind::nth_child;
					else if (pseudo_name == "nth-last-child")
						pseudo.kind = css_pseudo_class_kind::nth_last_child;
					else if (pseudo_name == "nth-of-type")
						pseudo.kind = css_pseudo_class_kind::nth_of_type;
					else if (pseudo_name == "nth-last-of-type")
						pseudo.kind = css_pseudo_class_kind::nth_last_of_type;
					else if (pseudo_name == "not")
						pseudo.kind = css_pseudo_class_kind::not_;
					else if (pseudo_name == "is")
						pseudo.kind = css_pseudo_class_kind::is;
					else if (pseudo_name == "where")
						pseudo.kind = css_pseudo_class_kind::where;
					else if (pseudo_name == "lang")
						pseudo.kind = css_pseudo_class_kind::lang;
					else
					{
						throw std::invalid_argument("Unsupported SVG CSS pseudo-class :" + pseudo_name);
					}

					const bool needs_argument = pseudo.kind == css_pseudo_class_kind::nth_child ||
					                            pseudo.kind == css_pseudo_class_kind::nth_last_child ||
					                            pseudo.kind == css_pseudo_class_kind::nth_of_type ||
					                            pseudo.kind == css_pseudo_class_kind::nth_last_of_type ||
					                            pseudo.kind == css_pseudo_class_kind::not_ ||
					                            pseudo.kind == css_pseudo_class_kind::is ||
					                            pseudo.kind == css_pseudo_class_kind::where ||
					                            pseudo.kind == css_pseudo_class_kind::lang;
					if (functional != needs_argument) [[unlikely]]
					{
						throw std::invalid_argument("SVG CSS pseudo-class has invalid function syntax");
					}

					if (pseudo.kind == css_pseudo_class_kind::nth_child ||
					    pseudo.kind == css_pseudo_class_kind::nth_last_child ||
					    pseudo.kind == css_pseudo_class_kind::nth_of_type ||
					    pseudo.kind == css_pseudo_class_kind::nth_last_of_type)
					{
						const auto [a, b] = parse_css_nth_formula(argument);
						pseudo.nth_a = a;
						pseudo.nth_b = b;
						++result.specificity[2];
					}
					else if (pseudo.kind == css_pseudo_class_kind::not_ ||
					         pseudo.kind == css_pseudo_class_kind::is ||
					         pseudo.kind == css_pseudo_class_kind::where)
					{
						const auto alternatives = split_css_selector_list(argument);
						if (alternatives.empty()) [[unlikely]]
						{
							throw std::invalid_argument("SVG CSS pseudo-class requires a selector");
						}
						std::array<std::size_t, 4> maximum_specificity{};
						for (const auto alternative : alternatives)
						{
							if (trim(alternative).empty())
							{
								if (pseudo.kind == css_pseudo_class_kind::is ||
								    pseudo.kind == css_pseudo_class_kind::where)
								{
									continue;
								}
								throw std::invalid_argument(
								    "SVG CSS pseudo-class contains an empty selector");
							}
							std::shared_ptr<css_selector> nested;
							try
							{
								nested = std::make_shared<css_selector>(
								    parse_css_selector(alternative, nesting + 1));
							}
							catch (const std::invalid_argument&)
							{
								if (pseudo.kind == css_pseudo_class_kind::is ||
								    pseudo.kind == css_pseudo_class_kind::where)
								{
									continue;
								}
								throw;
							}
							if (nested->specificity > maximum_specificity)
								maximum_specificity = nested->specificity;
							pseudo.selectors.push_back(std::move(nested));
						}
						if (pseudo.kind != css_pseudo_class_kind::where)
						{
							for (std::size_t i = 0; i < result.specificity.size(); ++i)
								result.specificity[i] += maximum_specificity[i];
						}
					}
					else if (pseudo.kind == css_pseudo_class_kind::lang)
					{
						pseudo.value = parse_css_lang_argument(argument);
						++result.specificity[2];
					}
					else
					{
						++result.specificity[2];
					}
					compound.pseudo_classes.push_back(std::move(pseudo));
					continue;
				}

				const char marker = source[offset++];
				const auto name = read_css_name(source, offset);
				if (marker == '#')
				{
					if (!compound.id.empty()) [[unlikely]]
					{
						throw std::invalid_argument("SVG CSS selectors cannot contain multiple ID selectors");
					}
					compound.id = std::string(name);
					++result.specificity[1];
				}
				else
				{
					compound.classes.emplace_back(name);
					++result.specificity[2];
				}
			}

			if (compound.element.empty() && compound.id.empty() && compound.classes.empty() &&
			    compound.attributes.empty() && compound.pseudo_classes.empty()) [[unlikely]]
			{
				throw std::invalid_argument("SVG CSS selector does not identify an element");
			}
			result.descendants.push_back(std::move(compound));

			if (offset == source.size())
			{
				break;
			}
			if (!std::isspace(static_cast<unsigned char>(source[offset]))) [[unlikely]]
			{
				throw std::invalid_argument("SVG CSS supports only descendant combinators between selectors");
			}
			while (offset < source.size() && std::isspace(static_cast<unsigned char>(source[offset])))
			{
				++offset;
			}
			if (offset < source.size() &&
			    (source[offset] == '>' || source[offset] == '+' || source[offset] == '~'))
			{
				throw std::invalid_argument(
				    "SVG CSS supports descendant selectors but not child or sibling combinators");
			}
		}
		return result;
	}

	[[nodiscard]] inline std::vector<css_declaration> parse_css_declarations(std::string_view source,
	                                                                         std::size_t& next_order)
	{
		const std::string uncommented = strip_css_comments(source);
		source = uncommented;
		std::vector<css_declaration> result;
		std::size_t offset = 0;
		while (offset < source.size())
		{
			std::size_t end = offset;
			std::size_t parenthesis_depth = 0;
			char quote = '\0';
			bool escaped = false;
			for (; end < source.size(); ++end)
			{
				const char value = source[end];
				if (quote != '\0')
				{
					if (escaped)
					{
						escaped = false;
					}
					else if (value == '\\')
					{
						escaped = true;
					}
					else if (value == quote)
					{
						quote = '\0';
					}
					continue;
				}
				if (value == '\'' || value == '"')
				{
					quote = value;
				}
				else if (value == '(')
				{
					++parenthesis_depth;
				}
				else if (value == ')' && parenthesis_depth > 0)
				{
					--parenthesis_depth;
				}
				else if (value == ';' && parenthesis_depth == 0)
				{
					break;
				}
			}

			const auto declaration = trim(source.substr(offset, end - offset));
			if (!declaration.empty())
			{
				std::size_t colon = 0;
				parenthesis_depth = 0;
				quote = '\0';
				escaped = false;
				for (; colon < declaration.size(); ++colon)
				{
					const char value = declaration[colon];
					if (quote != '\0')
					{
						if (escaped)
						{
							escaped = false;
						}
						else if (value == '\\')
						{
							escaped = true;
						}
						else if (value == quote)
						{
							quote = '\0';
						}
						continue;
					}
					if (value == '\'' || value == '"')
					{
						quote = value;
					}
					else if (value == '(')
					{
						++parenthesis_depth;
					}
					else if (value == ')' && parenthesis_depth > 0)
					{
						--parenthesis_depth;
					}
					else if (value == ':' && parenthesis_depth == 0)
					{
						break;
					}
				}
				if (colon < declaration.size())
				{
					const auto name = trim(declaration.substr(0, colon));
					auto value = trim(declaration.substr(colon + 1));
					if (!name.empty() && !value.empty())
					{
						bool important = false;
						const auto lowered_value = lower_ascii(value);
						constexpr std::string_view important_keyword = "important";
						if (lowered_value.ends_with(important_keyword))
						{
							auto marker = lowered_value.size() - important_keyword.size();
							while (marker > 0 && std::isspace(static_cast<unsigned char>(lowered_value[marker - 1])))
								--marker;
							if (marker > 0 && lowered_value[marker - 1] == '!')
							{
								value = trim(value.substr(0, marker - 1));
								if (!value.empty())
									important = true;
							}
						}
						if (!important || !value.empty())
							result.push_back({lower_ascii(name), std::string(value), next_order++, important});
					}
				}
			}
			offset = end == source.size() ? end : end + 1;
		}
		return result;
	}

	[[nodiscard]] inline std::size_t find_css_rule_open(std::string_view source, std::size_t start)
	{
		char quote = '\0';
		bool escaped = false;
		std::size_t bracket_depth = 0;
		std::size_t parenthesis_depth = 0;
		for (std::size_t offset = start; offset < source.size(); ++offset)
		{
			const char value = source[offset];
			if (quote != '\0')
			{
				if (escaped)
					escaped = false;
				else if (value == '\\')
					escaped = true;
				else if (value == quote)
					quote = '\0';
				continue;
			}
			if (value == '\'' || value == '"')
				quote = value;
			else if (value == '[')
				++bracket_depth;
			else if (value == ']' && bracket_depth > 0)
				--bracket_depth;
			else if (bracket_depth == 0 && value == '(')
				++parenthesis_depth;
			else if (bracket_depth == 0 && value == ')' && parenthesis_depth > 0)
				--parenthesis_depth;
			else if (bracket_depth == 0 && parenthesis_depth == 0 && value == '{')
				return offset;
		}
		return std::string_view::npos;
	}

	[[nodiscard]] inline std::size_t find_css_at_rule_semicolon(std::string_view source, std::size_t start)
	{
		char quote = '\0';
		bool escaped = false;
		std::size_t parenthesis_depth = 0;
		for (std::size_t offset = start; offset < source.size(); ++offset)
		{
			const char value = source[offset];
			if (quote != '\0')
			{
				if (escaped)
					escaped = false;
				else if (value == '\\')
					escaped = true;
				else if (value == quote)
					quote = '\0';
				continue;
			}
			if (value == '\'' || value == '\"')
				quote = value;
			else if (value == '(')
				++parenthesis_depth;
			else if (value == ')' && parenthesis_depth > 0)
				--parenthesis_depth;
			else if (value == ';' && parenthesis_depth == 0)
				return offset;
		}
		return std::string_view::npos;
	}

	[[nodiscard]] inline std::size_t find_css_rule_close(std::string_view source, std::size_t start)
	{
		char quote = '\0';
		bool escaped = false;
		std::size_t parenthesis_depth = 0;
		std::size_t block_depth = 1;
		for (std::size_t offset = start; offset < source.size(); ++offset)
		{
			const char value = source[offset];
			if (quote != '\0')
			{
				if (escaped)
					escaped = false;
				else if (value == '\\')
					escaped = true;
				else if (value == quote)
					quote = '\0';
				continue;
			}
			if (value == '\'' || value == '"')
				quote = value;
			else if (value == '(')
				++parenthesis_depth;
			else if (value == ')' && parenthesis_depth > 0)
				--parenthesis_depth;
			else if (parenthesis_depth == 0 && value == '{')
				++block_depth;
			else if (parenthesis_depth == 0 && value == '}' && --block_depth == 0)
				return offset;
		}
		return std::string_view::npos;
	}

	[[nodiscard]] inline std::vector<std::string_view> split_css_selector_list(std::string_view source)
	{
		std::vector<std::string_view> result;
		std::size_t start = 0;
		std::size_t bracket_depth = 0;
		std::size_t parenthesis_depth = 0;
		char quote = '\0';
		bool escaped = false;
		for (std::size_t offset = 0; offset < source.size(); ++offset)
		{
			const char value = source[offset];
			if (quote != '\0')
			{
				if (escaped)
				{
					escaped = false;
				}
				else if (value == '\\')
				{
					escaped = true;
				}
				else if (value == quote)
				{
					quote = '\0';
				}
				continue;
			}
			if (value == '\'' || value == '"')
			{
				quote = value;
			}
			else if (value == '[')
			{
				++bracket_depth;
			}
			else if (value == ']' && bracket_depth > 0)
			{
				--bracket_depth;
			}
			else if (bracket_depth == 0 && value == '(')
			{
				++parenthesis_depth;
			}
			else if (bracket_depth == 0 && value == ')' && parenthesis_depth > 0)
			{
				--parenthesis_depth;
			}
			else if (value == ',' && bracket_depth == 0 && parenthesis_depth == 0)
			{
				result.push_back(source.substr(start, offset - start));
				start = offset + 1;
			}
		}
		result.push_back(source.substr(start));
		return result;
	}

	inline void append_css_rules(std::string_view source, css_stylesheet& stylesheet,
	                             std::vector<std::string> media_conditions, std::size_t nesting)
	{
		if (nesting > 32) [[unlikely]]
		{
			throw std::length_error("SVG CSS conditional nesting exceeds the depth limit");
		}
		const std::string uncommented = strip_css_comments(source);
		source = uncommented;
		std::size_t offset = 0;
		while (offset < source.size())
		{
			while (offset < source.size() && std::isspace(static_cast<unsigned char>(source[offset])))
			{
				++offset;
			}
			if (offset == source.size())
			{
				break;
			}
			if (source[offset] == '@')
			{
				std::size_t name_offset = offset + 1;
				const auto name = lower_ascii(read_css_name(source, name_offset));
				const std::size_t open_brace = find_css_rule_open(source, name_offset);
				const std::size_t semicolon = find_css_at_rule_semicolon(source, name_offset);
				if (name != "media")
				{
					if (semicolon != std::string_view::npos &&
					    (open_brace == std::string_view::npos || semicolon < open_brace))
					{
						offset = semicolon + 1;
						continue;
					}
					if (open_brace == std::string_view::npos) [[unlikely]]
					{
						throw std::invalid_argument("SVG CSS at-rule is missing its terminator");
					}
					const std::size_t close_brace = find_css_rule_close(source, open_brace + 1);
					if (close_brace == std::string_view::npos) [[unlikely]]
					{
						throw std::invalid_argument("SVG CSS at-rule contains an unclosed block");
					}
					offset = close_brace + 1;
					continue;
				}
				if (open_brace == std::string_view::npos ||
				    (semicolon != std::string_view::npos && semicolon < open_brace)) [[unlikely]]
				{
					throw std::invalid_argument("SVG CSS @media rule is missing its block");
				}
				const std::size_t close_brace = find_css_rule_close(source, open_brace + 1);
				if (close_brace == std::string_view::npos) [[unlikely]]
				{
					throw std::invalid_argument("SVG CSS @media rule contains an unclosed block");
				}
				const auto condition = trim(source.substr(name_offset, open_brace - name_offset));
				if (condition.empty()) [[unlikely]]
				{
					throw std::invalid_argument("SVG CSS @media rule requires a media query");
				}
				auto nested_conditions = media_conditions;
				nested_conditions.emplace_back(condition);
				append_css_rules(source.substr(open_brace + 1, close_brace - open_brace - 1), stylesheet,
				                 std::move(nested_conditions), nesting + 1);
				offset = close_brace + 1;
				continue;
			}

			const std::size_t open_brace = find_css_rule_open(source, offset);
			const std::size_t close_brace = open_brace == std::string_view::npos
			                                    ? std::string_view::npos
			                                    : find_css_rule_close(source, open_brace + 1);
			if (open_brace == std::string_view::npos || close_brace == std::string_view::npos) [[unlikely]]
			{
				throw std::invalid_argument("SVG CSS rule is missing a selector block or closing brace");
			}

			const auto selector_list = trim(source.substr(offset, open_brace - offset));
			if (selector_list.empty()) [[unlikely]]
			{
				throw std::invalid_argument("SVG CSS rule has an empty selector list");
			}
			auto declarations = parse_css_declarations(
			    source.substr(open_brace + 1, close_brace - open_brace - 1), stylesheet.next_order);
			std::vector<css_rule> parsed_rules;
			try
			{
				for (const auto selector : split_css_selector_list(selector_list))
				{
					if (trim(selector).empty()) [[unlikely]]
					{
						throw std::invalid_argument("SVG CSS selector list has an empty selector");
					}
					parsed_rules.push_back({parse_css_selector(selector), declarations, media_conditions});
				}
			}
			catch (const std::invalid_argument&)
			{
				// Unsupported selectors invalidate this rule, not unrelated CSS rules.
				offset = close_brace + 1;
				continue;
			}
			std::ranges::move(parsed_rules, std::back_inserter(stylesheet.rules));
			stylesheet.resolved_properties.clear();
			offset = close_brace + 1;
		}
	}

	[[nodiscard]] inline bool css_class_contains(std::string_view classes, std::string_view wanted) noexcept
	{
		std::size_t offset = 0;
		while (offset < classes.size())
		{
			while (offset < classes.size() && std::isspace(static_cast<unsigned char>(classes[offset])))
			{
				++offset;
			}
			const std::size_t start = offset;
			while (offset < classes.size() && !std::isspace(static_cast<unsigned char>(classes[offset])))
			{
				++offset;
			}
			if (classes.substr(start, offset - start) == wanted)
			{
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] inline bool matches_css_selector(const css_selector& selector, const xml_tag& tag);

	[[nodiscard]] inline std::size_t css_sibling_index(const xml_tag& tag, bool of_type,
	                                                   bool from_end) noexcept
	{
		if (!tag.parent)
		{
			return 0;
		}
		if (of_type)
			return from_end ? tag.type_index_from_end : tag.type_index;
		return from_end ? tag.sibling_index_from_end : tag.sibling_index;
	}

	[[nodiscard]] inline bool matches_css_nth(std::size_t index,
	                                          const css_pseudo_class_selector& selector) noexcept
	{
		if (index == 0)
		{
			return false;
		}
		const auto position = static_cast<std::int64_t>(index);
		if (selector.nth_a == 0)
		{
			return position == selector.nth_b;
		}
		const auto difference = position - selector.nth_b;
		if ((selector.nth_a > 0 && difference < 0) || (selector.nth_a < 0 && difference > 0))
		{
			return false;
		}
		return difference % selector.nth_a == 0;
	}

	[[nodiscard]] inline bool matches_css_language(const xml_tag& tag, std::string_view language)
	{
		for (auto* current = &tag; current; current = current->parent)
		{
			auto value = attribute_value(*current, "lang");
			if (!value)
			{
				for (const auto& attribute : current->attributes)
				{
					if (attribute.namespace_uri == xml_namespace_uri && attribute.name == "xml:lang")
					{
						value = attribute.value;
						break;
					}
				}
			}
			if (value)
			{
				if (language == "*")
				{
					return true;
				}
				const auto actual = lower_ascii(*value);
				return actual == language || (actual.size() > language.size() &&
				                              actual.starts_with(language) && actual[language.size()] == '-');
			}
		}
		return false;
	}

	[[nodiscard]] inline bool matches_css_pseudo_class(const css_pseudo_class_selector& selector,
	                                                   const xml_tag& tag)
	{
		switch (selector.kind)
		{
		case css_pseudo_class_kind::root:
			return tag.parent == nullptr;
		case css_pseudo_class_kind::empty:
			return tag.first_child == nullptr && !tag.has_text;
		case css_pseudo_class_kind::first_child:
			return tag.parent && tag.previous_sibling == nullptr;
		case css_pseudo_class_kind::last_child:
			return tag.parent && tag.next_sibling == nullptr;
		case css_pseudo_class_kind::only_child:
			return tag.parent && tag.previous_sibling == nullptr && tag.next_sibling == nullptr;
		case css_pseudo_class_kind::first_of_type:
			return tag.parent && tag.type_index == 1;
		case css_pseudo_class_kind::last_of_type:
			return tag.parent && tag.type_index_from_end == 1;
		case css_pseudo_class_kind::only_of_type:
			return tag.parent && tag.type_index == 1 && tag.type_index_from_end == 1;
		case css_pseudo_class_kind::nth_child:
			return matches_css_nth(css_sibling_index(tag, false, false), selector);
		case css_pseudo_class_kind::nth_last_child:
			return matches_css_nth(css_sibling_index(tag, false, true), selector);
		case css_pseudo_class_kind::nth_of_type:
			return matches_css_nth(css_sibling_index(tag, true, false), selector);
		case css_pseudo_class_kind::nth_last_of_type:
			return matches_css_nth(css_sibling_index(tag, true, true), selector);
		case css_pseudo_class_kind::not_:
		case css_pseudo_class_kind::is:
		case css_pseudo_class_kind::where:
		{
			const bool any_match = std::ranges::any_of(selector.selectors, [&](const auto& alternative)
			                                           { return matches_css_selector(*alternative, tag); });
			return selector.kind == css_pseudo_class_kind::not_ ? !any_match : any_match;
		}
		case css_pseudo_class_kind::lang:
			return matches_css_language(tag, selector.value);
		}
		return false;
	}

	[[nodiscard]] inline bool matches_css_compound(const css_compound_selector& selector, const xml_tag& tag)
	{
		if (!selector.element.empty() && selector.element != "*" && selector.element != local_name(tag.name))
		{
			return false;
		}
		const auto id = attribute_value(tag, "id");
		if (!selector.id.empty() && (!id || *id != selector.id))
		{
			return false;
		}
		if (!selector.classes.empty())
		{
			const auto classes = attribute_value(tag, "class");
			if (!classes)
			{
				return false;
			}
			for (const auto& class_name : selector.classes)
			{
				if (!css_class_contains(*classes, class_name))
				{
					return false;
				}
			}
		}
		for (const auto& condition : selector.attributes)
		{
			const auto attribute = std::ranges::find_if(
			    tag.attributes, [&](const auto& candidate)
			    { return candidate.namespace_uri.empty() && candidate.name == condition.name; });
			if (attribute == tag.attributes.end() ||
			    !matches_css_attribute_value(condition, attribute->value))
			{
				return false;
			}
		}
		for (const auto& pseudo : selector.pseudo_classes)
		{
			if (!matches_css_pseudo_class(pseudo, tag))
			{
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] inline bool matches_css_selector(const css_selector& selector, const xml_tag& tag)
	{
		if (selector.descendants.empty() || !matches_css_compound(selector.descendants.back(), tag))
		{
			return false;
		}

		std::size_t ancestor_selector = selector.descendants.size() - 1;
		for (auto* ancestor = tag.parent; ancestor && ancestor_selector > 0; ancestor = ancestor->parent)
		{
			if (matches_css_compound(selector.descendants[ancestor_selector - 1], *ancestor))
			{
				--ancestor_selector;
			}
		}
		return ancestor_selector == 0;
	}

	[[nodiscard]] inline std::vector<std::string_view> split_css_media_and(std::string_view source)
	{
		std::vector<std::string_view> result;
		std::size_t start = 0;
		std::size_t depth = 0;
		char quote = '\0';
		bool escaped = false;
		for (std::size_t i = 0; i + 3 < source.size(); ++i)
		{
			const char value = source[i];
			if (quote != '\0')
			{
				if (escaped)
					escaped = false;
				else if (value == '\\')
					escaped = true;
				else if (value == quote)
					quote = '\0';
				continue;
			}
			if (value == '\'' || value == '"')
			{
				quote = value;
				continue;
			}
			if (value == '(')
			{
				++depth;
				continue;
			}
			if (value == ')' && depth > 0)
			{
				--depth;
				continue;
			}
			if (depth != 0 || i == 0 || !std::isspace(static_cast<unsigned char>(source[i - 1])) ||
			    !std::isspace(static_cast<unsigned char>(source[i + 3])))
			{
				continue;
			}
			if (std::tolower(static_cast<unsigned char>(source[i])) == 'a' &&
			    std::tolower(static_cast<unsigned char>(source[i + 1])) == 'n' &&
			    std::tolower(static_cast<unsigned char>(source[i + 2])) == 'd')
			{
				result.push_back(source.substr(start, i - start));
				start = i + 3;
				i += 2;
			}
		}
		result.push_back(source.substr(start));
		return result;
	}

	enum class css_media_match
	{
		invalid,
		no_match,
		match
	};

	[[nodiscard]] inline css_media_match matches_css_media_feature(std::string_view feature, float width,
	                                                               float height)
	{
		feature = trim(feature);
		if (feature.size() < 2 || feature.front() != '(' || feature.back() != ')')
		{
			return css_media_match::invalid;
		}
		feature = trim(feature.substr(1, feature.size() - 2));
		const std::size_t colon = feature.find(':');
		if (colon == std::string_view::npos)
		{
			const auto name = lower_ascii(feature);
			if (name == "width")
				return width > 0.0f ? css_media_match::match : css_media_match::no_match;
			if (name == "height")
				return height > 0.0f ? css_media_match::match : css_media_match::no_match;
			if (name == "aspect-ratio")
				return width > 0.0f && height > 0.0f ? css_media_match::match : css_media_match::no_match;
			return css_media_match::invalid;
		}
		if (feature.find(':', colon + 1) != std::string_view::npos)
			return css_media_match::invalid;
		const auto name = lower_ascii(trim(feature.substr(0, colon)));
		const auto value = trim(feature.substr(colon + 1));
		if (name.empty() || value.empty())
			return css_media_match::invalid;

		if (name == "orientation")
		{
			const auto orientation = lower_ascii(value);
			if (orientation == "landscape")
				return width > height ? css_media_match::match : css_media_match::no_match;
			if (orientation == "portrait")
				return width <= height ? css_media_match::match : css_media_match::no_match;
			return css_media_match::invalid;
		}

		const auto compare_length = [&](float actual, std::string_view operation)
		{
			float requested = 0.0f;
			auto length = value;
			if (length.ends_with("px"))
				length.remove_suffix(2);
			else if (trim(length) != "0")
				return css_media_match::invalid;
			try
			{
				requested = parse_number(length);
			}
			catch (const std::invalid_argument&)
			{
				return css_media_match::invalid;
			}
			if (operation == "min")
				return actual >= requested ? css_media_match::match : css_media_match::no_match;
			if (operation == "max")
				return actual <= requested ? css_media_match::match : css_media_match::no_match;
			return std::abs(actual - requested) <= 1.0e-5f ? css_media_match::match
			                                                : css_media_match::no_match;
		};

		if (name == "width" || name == "min-width" || name == "max-width")
		{
			const auto operation = name == "min-width" ? "min" : (name == "max-width" ? "max" : "equal");
			return compare_length(width, operation);
		}
		if (name == "height" || name == "min-height" || name == "max-height")
		{
			const auto operation = name == "min-height" ? "min" : (name == "max-height" ? "max" : "equal");
			return compare_length(height, operation);
		}
		if (name == "aspect-ratio" || name == "min-aspect-ratio" || name == "max-aspect-ratio")
		{
			const std::size_t slash = value.find('/');
			if (slash == std::string_view::npos || value.find('/', slash + 1) != std::string_view::npos)
				return css_media_match::invalid;
			if (height <= 0.0f)
				return css_media_match::no_match;
			float numerator = 0.0f;
			float denominator = 0.0f;
			try
			{
				numerator = parse_number(trim(value.substr(0, slash)));
				denominator = parse_number(trim(value.substr(slash + 1)));
			}
			catch (const std::invalid_argument&)
			{
				return css_media_match::invalid;
			}
			if (numerator < 0.0f || denominator <= 0.0f)
				return css_media_match::invalid;
			const float actual = width / height;
			const float requested = numerator / denominator;
			if (!std::isfinite(requested))
				return css_media_match::invalid;
			if (name == "min-aspect-ratio")
				return actual >= requested ? css_media_match::match : css_media_match::no_match;
			if (name == "max-aspect-ratio")
				return actual <= requested ? css_media_match::match : css_media_match::no_match;
			return std::abs(actual - requested) <= 1.0e-5f ? css_media_match::match
			                                                : css_media_match::no_match;
		}
		return css_media_match::invalid;
	}

	[[nodiscard]] inline css_media_match matches_css_media_query(std::string_view query, float width,
	                                                             float height)
	{
		query = trim(query);
		if (query.empty())
		{
			return css_media_match::invalid;
		}
		bool negate = false;
		if (query.size() > 3 && lower_ascii(query.substr(0, 3)) == "not" &&
		    std::isspace(static_cast<unsigned char>(query[3])))
		{
			negate = true;
			query = trim(query.substr(3));
		}
		else if (query.size() > 4 && lower_ascii(query.substr(0, 4)) == "only" &&
		         std::isspace(static_cast<unsigned char>(query[4])))
		{
			query = trim(query.substr(4));
		}

		const auto conditions = split_css_media_and(query);
		if (conditions.empty())
			return css_media_match::invalid;
		bool result = true;
		bool has_media_type = false;
		for (std::size_t index = 0; index < conditions.size(); ++index)
		{
			const auto term = trim(conditions[index]);
			if (term.empty())
				return css_media_match::invalid;
			css_media_match term_result = css_media_match::invalid;
			if (term.front() == '(')
			{
				term_result = matches_css_media_feature(term, width, height);
			}
			else if (index == 0 && !has_media_type)
			{
				has_media_type = true;
				const auto media_type = lower_ascii(term);
				if (media_type == "all" || media_type == "screen")
					term_result = css_media_match::match;
				else if (media_type == "print" || media_type == "speech" || media_type == "projection" ||
				         media_type == "handheld" || media_type == "tv" || media_type == "tty" ||
				         media_type == "embossed" || media_type == "braille")
					term_result = css_media_match::no_match;
			}
			if (term_result == css_media_match::invalid)
				return css_media_match::invalid;
			result = result && term_result == css_media_match::match;
		}
		if (negate)
			result = !result;
		return result ? css_media_match::match : css_media_match::no_match;
	}

	[[nodiscard]] inline bool matches_css_media_condition(std::string_view condition,
	                                                      const css_stylesheet& stylesheet)
	{
		for (const auto query : split_css_selector_list(condition))
		{
			if (matches_css_media_query(query, stylesheet.viewport_width, stylesheet.viewport_height) ==
			    css_media_match::match)
			{
				return true;
			}
		}
		return false;
	}

	inline void evaluate_css_media_conditions(css_stylesheet& stylesheet)
	{
		stylesheet.resolved_properties.clear();
		stylesheet.rules_by_id.clear();
		stylesheet.rules_by_class.clear();
		stylesheet.rules_by_element.clear();
		stylesheet.unindexed_rules.clear();
		std::unordered_map<std::string, bool> condition_matches;
		for (std::size_t index = 0; index < stylesheet.rules.size(); ++index)
		{
			auto& rule = stylesheet.rules[index];
			rule.media_matches = std::ranges::all_of(
			    rule.media_conditions, [&](const auto& condition)
			    {
				    const auto [match, inserted] = condition_matches.try_emplace(condition, false);
				    if (inserted)
					    match->second = matches_css_media_condition(condition, stylesheet);
				    return match->second;
			    });

			if (rule.selector.descendants.empty())
			{
				stylesheet.unindexed_rules.push_back(index);
				continue;
			}
			const auto& rightmost = rule.selector.descendants.back();
			if (!rightmost.id.empty())
				stylesheet.rules_by_id[rightmost.id].push_back(index);
			else if (!rightmost.classes.empty())
				stylesheet.rules_by_class[rightmost.classes.front()].push_back(index);
			else if (!rightmost.element.empty() && rightmost.element != "*")
				stylesheet.rules_by_element[rightmost.element].push_back(index);
			else
				stylesheet.unindexed_rules.push_back(index);
		}
	}

	[[nodiscard]] inline bool valid_color_declaration(std::string_view name, std::string_view value)
	{
		if (name != "color" && name != "fill" && name != "stroke" && name != "stop-color" &&
		    name != "flood-color")
		{
			return true;
		}
		value = trim(value);
		const auto keyword = lower_ascii(value);
		if (keyword == "inherit")
		{
			return true;
		}
		if (name == "fill" || name == "stroke")
		{
			if (keyword == "none" || keyword == "currentcolor" || keyword == "context-fill" ||
			    keyword == "context-stroke" || keyword.starts_with("url("))
			{
				return true;
			}
		}
		else if ((name == "stop-color" || name == "flood-color") && keyword == "currentcolor")
		{
			return true;
		}
		try
		{
			(void)parse_color(value);
			return true;
		}
		catch (const std::invalid_argument&)
		{
			return false;
		}
	}

	[[nodiscard]] inline bool valid_css_property_value(std::string_view name, std::string_view value);
	[[nodiscard]] inline gradient_coordinate parse_gradient_coordinate(std::string_view value);

	inline void consider_css_value(std::unordered_map<std::string, css_cascaded_value>& properties,
	                               std::string name, std::string value, bool important,
	                               const std::array<std::size_t, 4>& specificity, std::size_t order)
	{
		if (!valid_color_declaration(name, value) || !valid_css_property_value(name, value))
			return;
		if (name == "marker")
		{
			for (const auto* property : {"marker-start", "marker-mid", "marker-end"})
			{
				consider_css_value(properties, property, value, important, specificity, order);
			}
			return;
		}
		auto found = properties.find(name);
		const css_cascaded_value candidate{std::move(value), specificity, order, important};
		if (found == properties.end() || candidate.important > found->second.important ||
		    (candidate.important == found->second.important &&
		     (candidate.specificity > found->second.specificity ||
		      (candidate.specificity == found->second.specificity &&
		       candidate.order >= found->second.order))))
		{
			properties.insert_or_assign(std::move(name), candidate);
		}
	}

	[[nodiscard]] inline const std::unordered_map<std::string, css_cascaded_value>& resolve_css_properties(
	    const xml_tag& tag, const css_stylesheet& stylesheet)
	{
		if (const auto found = stylesheet.resolved_properties.find(&tag);
		    found != stylesheet.resolved_properties.end())
		{
			return found->second;
		}

		std::unordered_map<std::string, css_cascaded_value> result;
		constexpr std::array<std::size_t, 4> presentation_specificity{};
		for (const auto& attribute : tag.attributes)
		{
			if (!attribute.namespace_uri.empty())
			{
				continue;
			}
			const auto name = local_name(attribute.name);
			if (name == "id" || name == "class" || name == "style" || name == "xmlns" || name == "version" ||
			    name == "viewBox" || name == "preserveAspectRatio" || name == "href" || name == "offset" ||
			    name == "gradientUnits" ||
			    name == "gradientTransform" || name == "spreadMethod" || name == "fx" || name == "fy" ||
			    name == "fr")
			{
				continue;
			}
			// Presentation attributes are hints and always precede author stylesheet rules.
			consider_css_value(result, lower_ascii(name), attribute.value, false, presentation_specificity, 0);
		}

		std::vector<std::size_t> candidates = stylesheet.unindexed_rules;
		const auto append_candidates = [&](const auto& index, std::string_view key)
		{
			if (const auto found = index.find(std::string(key)); found != index.end())
				candidates.insert(candidates.end(), found->second.begin(), found->second.end());
		};
		if (const auto id = attribute_value(tag, "id"))
			append_candidates(stylesheet.rules_by_id, *id);
		if (const auto class_attribute = attribute_value(tag, "class"))
		{
			std::size_t start = 0;
			while (start < class_attribute->size())
			{
				while (start < class_attribute->size() &&
				       std::isspace(static_cast<unsigned char>((*class_attribute)[start])))
					++start;
				const auto end = class_attribute->find_first_of(" \t\r\n", start);
				if (end == start)
					break;
				const auto token_end = end == std::string_view::npos ? class_attribute->size() : end;
				append_candidates(stylesheet.rules_by_class, class_attribute->substr(start, token_end - start));
				start = token_end;
			}
		}
		append_candidates(stylesheet.rules_by_element, local_name(tag.name));
		std::ranges::sort(candidates);
		candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

		for (const auto index : candidates)
		{
			const auto& rule = stylesheet.rules[index];
			if (!rule.media_matches)
				continue;
			if (!matches_css_selector(rule.selector, tag))
			{
				continue;
			}
			for (const auto& declaration : rule.declarations)
			{
				consider_css_value(result, declaration.name, declaration.value, declaration.important,
				                   rule.selector.specificity, declaration.order);
			}
		}

		if (const auto inline_style = attribute_value(tag, "style"))
		{
			std::size_t inline_order = stylesheet.next_order;
			auto declarations = parse_css_declarations(*inline_style, inline_order);
			constexpr std::array<std::size_t, 4> inline_specificity{1, 0, 0, 0};
			for (const auto& declaration : declarations)
			{
				consider_css_value(result, declaration.name, declaration.value, declaration.important,
				                   inline_specificity, declaration.order);
			}
		}
		return stylesheet.resolved_properties.emplace(&tag, std::move(result)).first->second;
	}

	[[nodiscard]] inline std::optional<std::string> parse_gradient_reference(std::string_view value)
	{
		value = trim(value);
		if (value.size() < 4 || lower_ascii(value.substr(0, 4)) != "url(")
		{
			return std::nullopt;
		}

		if (!value.ends_with(')')) [[unlikely]]
		{
			throw std::invalid_argument("SVG resource must use a local url(#id) reference");
		}
		auto reference = trim(value.substr(4, value.size() - 5));
		if (reference.size() >= 2 && ((reference.front() == '\'' && reference.back() == '\'') ||
		                              (reference.front() == '"' && reference.back() == '"')))
		{
			reference = trim(reference.substr(1, reference.size() - 2));
		}
		if (reference.size() < 2 || reference.front() != '#' ||
		    reference.find_first_of(" \t\r\n") != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument("Only local SVG references of the form url(#id) are supported");
		}
		return std::string(reference.substr(1));
	}

	struct paint_server_reference
	{
		std::string id;
		paint_server_fallback fallback;
	};

	[[nodiscard]] inline std::optional<paint_server_reference> parse_paint_server_reference(
	    std::string_view value, const ::mngl::color& current_color)
	{
		value = trim(value);
		if (value.size() < 4 || lower_ascii(value.substr(0, 4)) != "url(")
		{
			return std::nullopt;
		}
		const auto close = value.find(')');
		if (close == std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument("SVG resource must use a local url(#id) reference");
		}
		const auto url = value.substr(0, close + 1);
		auto id = parse_gradient_reference(url);
		if (!id) [[unlikely]]
		{
			throw std::invalid_argument("SVG paint server reference is invalid");
		}
		paint_server_reference result{std::move(*id), {}};
		auto fallback = trim(value.substr(close + 1));
		if (fallback.empty())
		{
			return result;
		}
		if (!std::isspace(static_cast<unsigned char>(value[close + 1]))) [[unlikely]]
		{
			throw std::invalid_argument("SVG paint server fallback must be separated by whitespace");
		}
		result.fallback.provided = true;
		if (lower_ascii(fallback) == "none")
		{
			result.fallback.none = true;
		}
		else
		{
			result.fallback.color = lower_ascii(fallback) == "currentcolor" ? current_color : parse_color(fallback);
		}
		return result;
	}

	[[nodiscard]] inline std::string paint_reference(std::string_view value, std::string_view property)
	{
		if (lower_ascii(trim(value)) == "none")
		{
			return {};
		}
		if (auto id = parse_gradient_reference(value))
		{
			return std::move(*id);
		}
		throw std::invalid_argument("SVG " + std::string(property) + " requires none or url(#id)");
	}

	[[nodiscard]] inline bool valid_css_property_value(std::string_view name, std::string_view value)
	{
		value = trim(value);
		const auto keyword = lower_ascii(value);
		if (keyword == "inherit")
			return true;
		if (name == "color-interpolation" || name == "color-interpolation-filters")
			return keyword == "srgb" || keyword == "linearrgb";
		try
		{
			if (name == "fill" || name == "stroke" || name == "stop-color" || name == "flood-color" ||
			    name == "lighting-color" || name == "color")
			{
				if (name == "fill" || name == "stroke")
				{
					if (keyword == "none" || keyword == "currentcolor" || keyword == "context-fill" ||
					    keyword == "context-stroke")
						return true;
					if (parse_paint_server_reference(value, ::mngl::color{}))
						return true;
				}
				else if (name != "color" && keyword == "currentcolor")
				{
					return true;
				}
				(void)parse_color(value);
			}
			else if (name == "stroke-width" || name == "stroke-dashoffset")
			{
				const float parsed = parse_length(value);
				return name != "stroke-width" || parsed >= 0.0f;
			}
			else if (name == "rx" || name == "ry")
			{
				if (keyword == "auto")
					return true;
				const auto radius = parse_gradient_coordinate(value);
				return radius.value >= 0.0f;
			}
			else if (name == "x" || name == "y" || name == "cx" || name == "cy" || name == "r" ||
			         name == "x1" || name == "y1" || name == "x2" || name == "y2" || name == "dx" ||
			         name == "dy" || name == "width" || name == "height")
			{
				if (keyword == "auto")
					return name == "width" || name == "height";
				const auto coordinate = parse_gradient_coordinate(value);
				return name == "width" || name == "height" || name == "r"
			                   ? coordinate.value >= 0.0f
			                   : true;
			}
			else if (name == "stroke-miterlimit")
			{
				return parse_number(value) >= 0.0f;
			}
			else if (name == "stroke-dasharray")
			{
				(void)parse_dash_array(value);
			}
			else if (name == "stroke-linecap")
			{
				return keyword == "butt" || keyword == "round" || keyword == "square";
			}
			else if (name == "stroke-linejoin")
			{
				return keyword == "miter" || keyword == "round" || keyword == "bevel";
			}
			else if (name == "opacity" || name == "fill-opacity" || name == "stroke-opacity" ||
			         name == "stop-opacity" || name == "flood-opacity")
			{
				(void)parse_opacity(value);
			}
			else if (name == "fill-rule" || name == "clip-rule")
			{
				return keyword == "nonzero" || keyword == "evenodd";
			}
			else if (name == "mask" || name == "filter" || name == "marker-start" || name == "marker-mid" ||
			         name == "marker-end")
			{
				(void)paint_reference(value, name);
			}
			else if (name == "transform")
			{
				if (keyword != "none")
					(void)parse_transform(value);
			}
			else if (name == "visibility")
			{
				return keyword == "visible" || keyword == "hidden" || keyword == "collapse";
			}
		}
		catch (const std::exception&)
		{
			return false;
		}
		return true;
	}

	inline void apply_property(paint_state& state, std::string_view name, std::string_view value)
	{
		value = trim(value);
		const auto keyword = lower_ascii(value);
		if (name == "fill")
		{
			state.fill_gradient_fallback = {};
			if (keyword == "context-fill" || keyword == "context-stroke")
			{
				const auto& paint = keyword == "context-fill" ? state.context_fill : state.context_stroke;
				state.fill_enabled = paint.has_value();
				state.fill_gradient_id.clear();
				state.fill_uses_current_color = false;
				if (paint)
					state.fill = *paint;
				return;
			}
			state.fill_enabled = keyword != "none";
			state.fill_gradient_id.clear();
			state.fill_uses_current_color = false;
			if (state.fill_enabled)
			{
				if (auto paint_server = parse_paint_server_reference(value, state.current_color))
				{
					state.fill_gradient_id = std::move(paint_server->id);
					state.fill_gradient_fallback = std::move(paint_server->fallback);
				}
				else if (keyword == "currentcolor")
				{
					state.fill_uses_current_color = true;
					state.fill = state.current_color;
				}
				else
				{
					state.fill = parse_color(value);
				}
			}
		}
		else if (name == "stroke")
		{
			state.stroke_gradient_fallback = {};
			if (keyword == "context-fill" || keyword == "context-stroke")
			{
				const auto& paint = keyword == "context-fill" ? state.context_fill : state.context_stroke;
				state.stroke_enabled = paint.has_value();
				state.stroke_gradient_id.clear();
				state.stroke_uses_current_color = false;
				if (paint)
					state.stroke = *paint;
				return;
			}
			state.stroke_enabled = keyword != "none";
			state.stroke_gradient_id.clear();
			state.stroke_uses_current_color = false;
			if (state.stroke_enabled)
			{
				if (auto paint_server = parse_paint_server_reference(value, state.current_color))
				{
					state.stroke_gradient_id = std::move(paint_server->id);
					state.stroke_gradient_fallback = std::move(paint_server->fallback);
				}
				else
				{
					if (keyword == "currentcolor")
					{
						state.stroke_uses_current_color = true;
						state.stroke = state.current_color;
					}
					else
					{
						state.stroke = parse_color(value);
					}
				}
			}
		}
		else if (name == "color")
		{
			state.current_color = parse_color(value);
		}
		else if (name == "stop-color")
		{
			state.stop_color = keyword == "currentcolor" ? state.current_color : parse_color(value);
		}
		else if (name == "stop-opacity")
		{
			state.stop_opacity = parse_opacity(value);
		}
		else if (name == "stroke-width")
		{
			state.stroke_width = parse_length(value);
			if (state.stroke_width < 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("SVG stroke-width must be non-negative");
			}
		}
		else if (name == "stroke-linecap")
		{
			if (keyword == "butt")
			{
				state.linecap = stroke_linecap::butt;
			}
			else if (keyword == "round")
			{
				state.linecap = stroke_linecap::round;
			}
			else if (keyword == "square")
			{
				state.linecap = stroke_linecap::square;
			}
			else
			{
				throw std::invalid_argument("SVG stroke-linecap must be 'butt', 'round', or 'square'");
			}
		}
		else if (name == "stroke-linejoin")
		{
			if (keyword == "miter")
			{
				state.linejoin = stroke_linejoin::miter;
			}
			else if (keyword == "round")
			{
				state.linejoin = stroke_linejoin::round;
			}
			else if (keyword == "bevel")
			{
				state.linejoin = stroke_linejoin::bevel;
			}
			else
			{
				throw std::invalid_argument("SVG stroke-linejoin must be 'miter', 'round', or 'bevel'");
			}
		}
		else if (name == "stroke-miterlimit")
		{
			state.stroke_miterlimit = parse_number(value);
			if (state.stroke_miterlimit < 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("SVG stroke-miterlimit must be non-negative");
			}
		}
		else if (name == "stroke-dasharray")
		{
			state.stroke_dasharray = parse_dash_array(value);
		}
		else if (name == "stroke-dashoffset")
		{
			state.stroke_dashoffset = parse_length(value);
		}
		else if (name == "opacity")
		{
			state.opacity = parse_opacity(value);
		}
		else if (name == "fill-opacity")
		{
			state.fill_opacity = parse_opacity(value);
		}
		else if (name == "stroke-opacity")
		{
			state.stroke_opacity = parse_opacity(value);
		}
		else if (name == "fill-rule")
		{
			if (keyword == "evenodd")
			{
				state.even_odd = true;
			}
			else if (keyword == "nonzero")
			{
				state.even_odd = false;
			}
			else
			{
				throw std::invalid_argument("SVG fill-rule must be 'nonzero' or 'evenodd'");
			}
		}
		else if (name == "clip-rule")
		{
			state.clip_even_odd = keyword == "evenodd";
		}
		else if (name == "mask")
		{
			state.mask_id = paint_reference(value, name);
		}
		else if (name == "filter")
		{
			state.filter_id = paint_reference(value, name);
		}
		else if (name == "clip-path")
		{
			state.clip_path_id = paint_reference(value, name);
		}
		else if (name == "marker-start" || name == "marker-mid" || name == "marker-end")
		{
			const auto index = name == "marker-start" ? 0 : (name == "marker-mid" ? 1 : 2);
			state.marker_ids[index] = paint_reference(value, name);
		}
		else if (name == "vector-effect" || name == "paint-order")
		{
			throw std::invalid_argument(
			    "This SVG paint or path effect is not supported by the initial renderer");
		}
		else if (name == "display")
		{
			state.display = keyword != "none";
		}
		else if (name == "pointer-events")
		{
			state.pointer_events = keyword != "none";
		}
		else if (name == "visibility")
		{
			if (keyword == "visible")
			{
				state.visible = true;
			}
			else if (keyword == "hidden" || keyword == "collapse")
			{
				state.visible = false;
			}
			else
			{
				throw std::invalid_argument("SVG visibility must be 'visible', 'hidden', or 'collapse'");
			}
		}
	}

	inline void apply_style(paint_state& state, const xml_tag& tag, const css_stylesheet& stylesheet)
	{
		const glm::mat4 inherited_transform = state.transform;
		state.opacity = 1.0f;
		const auto& properties = resolve_css_properties(tag, stylesheet);
		state.mask_id.clear();
		state.filter_id.clear();
		state.clip_path_id.clear();
		if (const auto color = properties.find("color");
		    color != properties.end() && lower_ascii(trim(color->second.value)) != "inherit")
		{
			apply_property(state, "color", color->second.value);
		}
		for (const auto& [name, property] : properties)
		{
			if (name == "opacity" && lower_ascii(trim(property.value)) == "inherit")
			{
				state.opacity = state.inherited_opacity;
				continue;
			}
			if (name == "color" || name == "transform" || lower_ascii(trim(property.value)) == "inherit")
			{
				continue;
			}
			apply_property(state, name, property.value);
		}
		if (state.fill_uses_current_color)
			state.fill = state.current_color;
		if (state.stroke_uses_current_color)
			state.stroke = state.current_color;
		if (const auto transform = properties.find("transform");
		    transform != properties.end() && lower_ascii(trim(transform->second.value)) != "inherit")
		{
			const auto transform_value = lower_ascii(trim(transform->second.value));
			state.transform = transform_value == "none"
			                      ? inherited_transform
			                      : inherited_transform * parse_transform(transform->second.value);
		}
	}

	[[nodiscard]] inline gradient_coordinate parse_gradient_coordinate(std::string_view value)
	{
		value = trim(value);
		if (value.ends_with('%'))
		{
			value.remove_suffix(1);
			return {parse_number(value), true};
		}
		return {parse_length(value), false};
	}

	[[nodiscard]] inline paint_state computed_style_at(const xml_tag& tag, const css_stylesheet& stylesheet)
	{
		std::vector<const xml_tag*> ancestry;
		for (const xml_tag* current = &tag; current; current = current->parent)
		{
			ancestry.push_back(current);
		}
		paint_state style;
		for (auto current = ancestry.rbegin(); current != ancestry.rend(); ++current)
		{
			apply_style(style, **current, stylesheet);
			style.inherited_opacity = style.opacity;
		}
		return style;
	}

	[[nodiscard]] inline gradient_stop parse_gradient_stop(const xml_tag& tag, float previous_offset,
	                                                       const css_stylesheet& stylesheet)
	{
		gradient_stop result{.offset = 0.0f, .value = ::mngl::color(0.0f, 0.0f, 0.0f, 1.0f)};
		if (const auto offset = attribute_value(tag, "offset"))
		{
			const auto parsed = parse_gradient_coordinate(*offset);
			result.offset = parsed.percentage ? parsed.value / 100.0f : parsed.value;
		}

		const auto& properties = resolve_css_properties(tag, stylesheet);
		const auto computed_style = computed_style_at(tag, stylesheet);
		float stop_opacity = 1.0f;
		if (const auto stop_color = properties.find("stop-color"); stop_color != properties.end())
		{
			const auto value = lower_ascii(trim(stop_color->second.value));
			if (value == "inherit")
				result.value = computed_style.stop_color;
			else if (value == "currentcolor")
				result.value = computed_style.current_color;
			else
				result.value = parse_color(value);
		}
		if (const auto opacity = properties.find("stop-opacity"); opacity != properties.end())
		{
			stop_opacity = lower_ascii(trim(opacity->second.value)) == "inherit"
			                   ? computed_style.stop_opacity
			                   : parse_opacity(opacity->second.value);
		}
		result.offset = std::clamp(std::max(previous_offset, result.offset), 0.0f, 1.0f);
		result.value = multiply_alpha(result.value, stop_opacity);
		return result;
	}

	[[nodiscard]] inline std::string parse_gradient_href(std::string_view value)
	{
		value = trim(value);
		if (value.size() < 2 || value.front() != '#' ||
		    value.find_first_of(" \t\r\n") != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument("SVG gradient href must be a local reference of the form '#id'");
		}
		return std::string(value.substr(1));
	}

	[[nodiscard]] inline std::optional<bool> resolve_gradient_color_interpolation(
	    const xml_tag& tag, const css_stylesheet& stylesheet)
	{
		std::vector<const xml_tag*> ancestry;
		for (const xml_tag* current = &tag; current; current = current->parent)
		{
			ancestry.push_back(current);
		}

		std::optional<bool> linear_rgb;
		for (auto current = ancestry.rbegin(); current != ancestry.rend(); ++current)
		{
			const auto& properties = resolve_css_properties(**current, stylesheet);
			const auto interpolation = properties.find("color-interpolation");
			if (interpolation == properties.end())
			{
				continue;
			}

			if (lower_ascii(trim(interpolation->second.value)) == "inherit")
			{
				if (!linear_rgb)
				{
					linear_rgb = false;
				}
			}
			else
			{
				linear_rgb = lower_ascii(trim(interpolation->second.value)) == "linearrgb";
			}
		}
		return linear_rgb;
	}

	[[nodiscard]] inline gradient_definition parse_gradient_definition(
	    const xml_tag& tag, const css_stylesheet& stylesheet,
	    const gradient_definition* inherited = nullptr)
	{
		gradient_definition result = inherited ? *inherited : gradient_definition{};
		const auto name = local_name(tag.name);
		result.kind = name == "radialGradient" ? gradient_kind::radial : gradient_kind::linear;
		if (const auto id = attribute_value(tag, "id"))
		{
			result.id = std::string(*id);
		}
		if (result.id.empty()) [[unlikely]]
		{
			throw std::invalid_argument("SVG linearGradient and radialGradient elements require an id");
		}
		if (const auto units = attribute_value(tag, "gradientUnits"))
		{
			if (*units == "userSpaceOnUse")
			{
				result.object_bounding_box = false;
			}
			else if (*units == "objectBoundingBox")
			{
				result.object_bounding_box = true;
			}
			else [[unlikely]]
			{
				throw std::invalid_argument(
				    "SVG gradientUnits must be 'objectBoundingBox' or 'userSpaceOnUse'");
			}
		}
		if (const auto spread = attribute_value(tag, "spreadMethod"))
		{
			if (*spread == "repeat")
			{
				result.spread = gradient_spread::repeat;
			}
			else if (*spread == "reflect")
			{
				result.spread = gradient_spread::reflect;
			}
			else if (*spread == "pad")
			{
				result.spread = gradient_spread::pad;
			}
			else [[unlikely]]
			{
				throw std::invalid_argument("SVG spreadMethod must be 'pad', 'repeat', or 'reflect'");
			}
		}
		if (const auto transform = attribute_value(tag, "gradientTransform"))
		{
			result.transform = parse_transform(*transform);
		}
		if (const auto interpolation = resolve_gradient_color_interpolation(tag, stylesheet))
		{
			result.linear_rgb = *interpolation;
		}

		auto assign_coordinate = [&](std::string_view name, gradient_coordinate& coordinate)
		{
			if (const auto value = attribute_value(tag, name))
			{
				coordinate = parse_gradient_coordinate(*value);
			}
		};
		assign_coordinate("x1", result.x1);
		assign_coordinate("y1", result.y1);
		assign_coordinate("x2", result.x2);
		assign_coordinate("y2", result.y2);
		assign_coordinate("cx", result.cx);
		assign_coordinate("cy", result.cy);
		assign_coordinate("r", result.radius);
		if (!inherited)
		{
			result.fx = result.cx;
			result.fy = result.cy;
		}
		if (attribute_value(tag, "fx"))
		{
			assign_coordinate("fx", result.fx);
			result.fx_explicit = true;
		}
		else if (!result.fx_explicit)
		{
			result.fx = result.cx;
		}
		if (attribute_value(tag, "fy"))
		{
			assign_coordinate("fy", result.fy);
			result.fy_explicit = true;
		}
		else if (!result.fy_explicit)
		{
			result.fy = result.cy;
		}
		assign_coordinate("fr", result.focal_radius);
		if (result.kind == gradient_kind::radial &&
		    (result.radius.value < 0.0f || result.focal_radius.value < 0.0f)) [[unlikely]]
		{
			throw std::invalid_argument("SVG radial gradient radii must be non-negative");
		}
		return result;
	}

	using gradient_nodes = std::unordered_map<std::string, const xml_node*>;
	using resolved_gradient_definitions =
	    std::unordered_map<std::string, std::shared_ptr<const gradient_definition>>;

	inline void collect_gradient_nodes(const xml_node& node, gradient_nodes& result)
	{
		if (!is_svg_element(node.tag))
		{
			return;
		}
		const auto name = local_name(node.tag.name);
		if (name == "linearGradient" || name == "radialGradient")
		{
			const auto id = attribute_value(node.tag, "id");
			if (!id || trim(*id).empty()) [[unlikely]]
			{
				throw std::invalid_argument("SVG linearGradient and radialGradient elements require an id");
			}
			if (!result.emplace(std::string(*id), &node).second) [[unlikely]]
			{
				throw std::invalid_argument("SVG gradient ids must be unique");
			}
		}
		for (const auto& child : node.children)
		{
			collect_gradient_nodes(child, result);
		}
	}

	[[nodiscard]] inline std::shared_ptr<const gradient_definition> resolve_gradient_definition(
	    const std::string& id, const gradient_nodes& nodes, const css_stylesheet& stylesheet,
	    resolved_gradient_definitions& resolved, std::unordered_set<std::string>& active)
	{
		if (const auto found = resolved.find(id); found != resolved.end())
			return found->second;
		const auto node = nodes.find(id);
		if (node == nodes.end()) [[unlikely]]
			throw std::invalid_argument("SVG gradient href references an unknown id '#" + id + "'");
		if (!active.emplace(id).second) [[unlikely]]
			throw std::invalid_argument("Cyclic SVG gradient href at '#" + id + "'");

		const auto& tag = node->second->tag;
		const auto name = local_name(tag.name);
		const auto kind = name == "radialGradient" ? gradient_kind::radial : gradient_kind::linear;
		std::shared_ptr<const gradient_definition> inherited;
		if (const auto href = svg_href_value(tag))
		{
			const auto referenced_id = parse_gradient_href(*href);
			inherited = resolve_gradient_definition(referenced_id, nodes, stylesheet, resolved, active);
			if (inherited->kind != kind) [[unlikely]]
			{
				throw std::invalid_argument("SVG gradient href cannot reference a different gradient type");
			}
		}

		auto definition = parse_gradient_definition(tag, stylesheet, inherited.get());
		std::vector<gradient_stop> local_stops;
		for (const auto& child : node->second->children)
		{
			if (is_svg_element(child.tag) && local_name(child.tag.name) == "stop")
			{
				const float previous = local_stops.empty() ? 0.0f : local_stops.back().offset;
				local_stops.push_back(parse_gradient_stop(child.tag, previous, stylesheet));
			}
		}
		if (!local_stops.empty())
		{
			definition.stops = std::move(local_stops);
		}

		active.erase(id);
		auto stored = std::make_shared<const gradient_definition>(std::move(definition));
		resolved.emplace(id, stored);
		return stored;
	}

	[[nodiscard]] inline resolved_gradient_definitions
	parse_gradient_definitions(const xml_node& root, const css_stylesheet& stylesheet)
	{
		gradient_nodes nodes;
		collect_gradient_nodes(root, nodes);
		resolved_gradient_definitions resolved;
		std::unordered_set<std::string> active;
		for (const auto& [id, node] : nodes)
		{
			(void)node;
			(void)resolve_gradient_definition(id, nodes, stylesheet, resolved, active);
		}
		return resolved;
	}

	[[nodiscard]] inline float resolve_gradient_coordinate(gradient_coordinate coordinate,
	                                                       bool object_bounding_box, float origin,
	                                                       float extent, float normalized_diagonal = 0.0f)
	{
		if (!coordinate.percentage)
		{
			return coordinate.value;
		}
		if (object_bounding_box)
		{
			return coordinate.value / 100.0f;
		}
		if (normalized_diagonal > 0.0f)
		{
			return normalized_diagonal * coordinate.value / 100.0f;
		}
		return origin + extent * coordinate.value / 100.0f;
	}

	[[nodiscard]] inline gradient_paint resolve_gradient_paint(
	    const std::shared_ptr<const gradient_definition>& definition, glm::vec2 bounds_min,
	    glm::vec2 bounds_max, glm::vec4 view_box)
	{
		const glm::vec2 bounds_size = bounds_max - bounds_min;
		glm::mat4 gradient_to_user(1.0f);
		if (definition->object_bounding_box)
		{
			gradient_to_user = glm::translate(gradient_to_user, glm::vec3(bounds_min, 0.0f));
			gradient_to_user = glm::scale(gradient_to_user, glm::vec3(bounds_size, 1.0f));
		}
		gradient_to_user *= definition->transform;
		const glm::mat4 user_to_gradient = glm::inverse(gradient_to_user);
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				if (!std::isfinite(user_to_gradient[column][row])) [[unlikely]]
				{
					throw std::invalid_argument("SVG gradientTransform must be invertible");
				}
			}
		}

		const double diagonal_squared =
		    (static_cast<double>(view_box.z) * view_box.z + static_cast<double>(view_box.w) * view_box.w) *
		    0.5;
		const double diagonal_value = std::sqrt(diagonal_squared);
		if (!std::isfinite(diagonal_value) ||
		    diagonal_value > static_cast<double>(std::numeric_limits<float>::max())) [[unlikely]]
		{
			throw std::invalid_argument("SVG viewBox is too large for radial gradient percentages");
		}
		const float diagonal = static_cast<float>(diagonal_value);
		auto coordinate_x = [&](gradient_coordinate coordinate, float origin, float extent)
		{ return resolve_gradient_coordinate(coordinate, definition->object_bounding_box, origin, extent); };
		auto radius_coordinate = [&](gradient_coordinate coordinate)
		{
			return resolve_gradient_coordinate(coordinate, definition->object_bounding_box, 0.0f, 0.0f,
			                                   definition->object_bounding_box ? 0.0f : diagonal);
		};

		gradient_paint result{.definition = definition, .coordinate_transform = user_to_gradient};
		if (definition->kind == gradient_kind::linear)
		{
			result.parameters0 = {coordinate_x(definition->x1, view_box.x, view_box.z),
			                      coordinate_x(definition->y1, view_box.y, view_box.w),
			                      coordinate_x(definition->x2, view_box.x, view_box.z),
			                      coordinate_x(definition->y2, view_box.y, view_box.w)};
		}
		else
		{
			const float center_x = coordinate_x(definition->cx, view_box.x, view_box.z);
			const float center_y = coordinate_x(definition->cy, view_box.y, view_box.w);
			result.parameters0 = {center_x, center_y, radius_coordinate(definition->radius), 0.0f};
			result.parameters1 = {coordinate_x(definition->fx, view_box.x, view_box.z),
			                      coordinate_x(definition->fy, view_box.y, view_box.w),
			                      radius_coordinate(definition->focal_radius), 0.0f};
		}
		for (int index = 0; index < 4; ++index)
		{
			if (!std::isfinite(result.parameters0[index])) [[unlikely]]
			{
				throw std::invalid_argument("SVG gradient coordinates must be finite");
			}
		}
		for (int index = 0; index < 4; ++index)
		{
			if (!std::isfinite(result.parameters1[index])) [[unlikely]]
			{
				throw std::invalid_argument("SVG gradient coordinates must be finite");
			}
		}
		return result;
	}

	[[nodiscard]] inline std::vector<glm::vec2> triangulate_compound(
	    std::span<const std::span<const glm::vec2>> rings, bool even_odd)
	{
		// Separate contours with a gap on any tested projection before building global
		// scan strips. A projection gap makes their filled areas independent, while the
		// principal-axis perpendicular also separates long parallel contours whose AABBs
		// overlap on both axes.
		if (rings.size() > 1)
		{
			struct ring_bounds
			{
				std::span<const glm::vec2> points;
				float min_x;
				float max_x;
				float min_y;
				float max_y;
			};
			std::vector<ring_bounds> bounds;
			bounds.reserve(rings.size());
			for (const auto ring : rings)
			{
				if (ring.size() < 3)
					continue;
				glm::vec2 minimum = ring.front();
				glm::vec2 maximum = ring.front();
				for (const auto point : ring.subspan(1))
				{
					minimum = glm::min(minimum, point);
					maximum = glm::max(maximum, point);
				}
				bounds.push_back({ring, minimum.x, maximum.x, minimum.y, maximum.y});
			}

			const auto components_from_intervals = [&](const std::vector<std::pair<double, double>>& intervals,
			                                           double separation_tolerance)
			{
				std::vector<std::size_t> order(intervals.size());
				std::iota(order.begin(), order.end(), 0);
				std::ranges::sort(order, [&](std::size_t left, std::size_t right)
				                  { return intervals[left].first == intervals[right].first
				                               ? intervals[left].second < intervals[right].second
				                               : intervals[left].first < intervals[right].first; });

				std::vector<std::vector<std::size_t>> components;
				double component_high = 0.0;
				for (const std::size_t index : order)
				{
					if (components.empty() || intervals[index].first > component_high + separation_tolerance)
					{
						components.emplace_back();
						component_high = intervals[index].second;
					}
					else
					{
						component_high = std::max(component_high, intervals[index].second);
					}
					components.back().push_back(index);
				}
				return components;
			};
			const auto axis_components = [&](bool horizontal)
			{
				std::vector<std::pair<double, double>> intervals;
				intervals.reserve(bounds.size());
				for (const auto& bound : bounds)
				{
					intervals.emplace_back(horizontal ? bound.min_x : bound.min_y,
					                       horizontal ? bound.max_x : bound.max_y);
				}
				return components_from_intervals(intervals, 0.0);
			};
			auto principal_projection_components = [&]
			{
				std::size_t point_count = 0;
				double covariance_xx = 0.0;
				double covariance_xy = 0.0;
				double covariance_yy = 0.0;
				for (const auto& bound : bounds)
				{
					double mean_x = 0.0;
					double mean_y = 0.0;
					for (const auto point : bound.points)
					{
						mean_x += point.x;
						mean_y += point.y;
						++point_count;
					}
					if (bound.points.empty())
						continue;
					mean_x /= static_cast<double>(bound.points.size());
					mean_y /= static_cast<double>(bound.points.size());
					for (const auto point : bound.points)
					{
						const double dx = static_cast<double>(point.x) - mean_x;
						const double dy = static_cast<double>(point.y) - mean_y;
						covariance_xx += dx * dx;
						covariance_xy += dx * dy;
						covariance_yy += dy * dy;
					}
				}
				if (point_count == 0)
					return std::vector<std::vector<std::size_t>>{};

				const double angle = 0.5 * std::atan2(2.0 * covariance_xy, covariance_xx - covariance_yy);
				const double major_x = std::cos(angle);
				const double major_y = std::sin(angle);
				const double perpendicular_x = -major_y;
				const double perpendicular_y = major_x;
				std::vector<std::pair<double, double>> intervals;
				intervals.reserve(bounds.size());
				double coordinate_scale = 1.0;
				for (const auto& bound : bounds)
				{
					double minimum = std::numeric_limits<double>::max();
					double maximum = std::numeric_limits<double>::lowest();
					for (const auto point : bound.points)
					{
						const double x = point.x;
						const double y = point.y;
						coordinate_scale = std::max({coordinate_scale, std::abs(x), std::abs(y)});
						const double projection = x * perpendicular_x + y * perpendicular_y;
						minimum = std::min(minimum, projection);
						maximum = std::max(maximum, projection);
					}
					intervals.emplace_back(minimum, maximum);
				}
				const double separation_tolerance =
				    64.0 * std::numeric_limits<double>::epsilon() * coordinate_scale;
				return components_from_intervals(intervals, separation_tolerance);
			};

			auto components = axis_components(true);
			auto vertical_components = axis_components(false);
			if (vertical_components.size() > components.size())
				components = std::move(vertical_components);
			auto principal_components = principal_projection_components();
			if (principal_components.size() > components.size())
				components = std::move(principal_components);
			if (components.size() > 1)
			{
				std::vector<glm::vec2> triangles;
				for (const auto& component : components)
				{
					std::vector<std::span<const glm::vec2>> component_rings;
					component_rings.reserve(component.size());
					for (const std::size_t index : component)
						component_rings.push_back(bounds[index].points);
					auto component_triangles = triangulate_compound(component_rings, even_odd);
					if (component_triangles.size() > 12'000'000 - triangles.size()) [[unlikely]]
					{
						throw std::length_error("SVG fill exceeds the tessellated triangle limit");
					}
					triangles.insert(triangles.end(), component_triangles.begin(), component_triangles.end());
				}
				return triangles;
			}
		}

		struct scan_edge
		{
			glm::vec2 first;
			glm::vec2 second;
			std::size_t start_event = 0;
			std::size_t end_event = 0;
			int winding_delta = 0;
		};
		struct crossing
		{
			double x = 0.0;
			std::size_t edge = 0;
		};

		std::vector<double> y_levels;
		std::vector<scan_edge> edges;
		for (const auto ring : rings)
		{
			if (ring.size() < 3)
			{
				continue;
			}
			y_levels.reserve(y_levels.size() + ring.size());
			edges.reserve(edges.size() + ring.size());
			for (const auto point : ring)
			{
				y_levels.push_back(point.y);
			}
			for (std::size_t i = 0; i < ring.size(); ++i)
			{
				const auto first = ring[i];
				const auto second = ring[(i + 1) % ring.size()];
				if (first.y == second.y)
				{
					continue;
				}
				edges.push_back(first.y < second.y ? scan_edge{first, second, 0, 0, -1}
				                                   : scan_edge{second, first, 0, 0, 1});
			}
		}

		std::ranges::sort(y_levels);
		y_levels.erase(std::unique(y_levels.begin(), y_levels.end()), y_levels.end());
		if (y_levels.size() < 2 || edges.empty())
		{
			return {};
		}

		const auto event_index = [&](float y)
		{
			return static_cast<std::size_t>(std::lower_bound(y_levels.begin(), y_levels.end(), y) -
			                                y_levels.begin());
		};
		std::vector<std::size_t> start_order;
		std::vector<std::size_t> end_order;
		start_order.reserve(edges.size());
		end_order.reserve(edges.size());
		for (std::size_t i = 0; i < edges.size(); ++i)
		{
			auto& edge = edges[i];
			edge.start_event = event_index(std::min(edge.first.y, edge.second.y));
			edge.end_event = event_index(std::max(edge.first.y, edge.second.y));
			start_order.push_back(i);
			end_order.push_back(i);
		}
		std::ranges::sort(start_order, {}, [&](std::size_t index) { return edges[index].start_event; });
		std::ranges::sort(end_order, {}, [&](std::size_t index) { return edges[index].end_event; });

		const auto x_at = [&](const scan_edge& edge, double y)
		{
			return std::lerp(static_cast<double>(edge.first.x), static_cast<double>(edge.second.x),
			                 (y - edge.first.y) / (static_cast<double>(edge.second.y) - edge.first.y));
		};
		std::vector<std::size_t> active;
		active.reserve(edges.size());
		const auto no_position = std::numeric_limits<std::size_t>::max();
		std::vector<std::size_t> active_positions(edges.size(), no_position);
		std::vector<crossing> crossings;
		std::vector<glm::vec2> triangles;
		crossings.reserve(edges.size());
		std::size_t next_start = 0;
		std::size_t next_end = 0;

		auto append_triangle = [&](glm::vec2 first, glm::vec2 second, glm::vec2 third)
		{
			const double area =
			    (static_cast<double>(second.x) - first.x) * (static_cast<double>(third.y) - first.y) -
			    (static_cast<double>(second.y) - first.y) * (static_cast<double>(third.x) - first.x);
			if (area == 0.0)
			{
				return;
			}
			if (triangles.size() >= 12'000'000) [[unlikely]]
			{
				throw std::length_error("SVG fill exceeds the tessellated triangle limit");
			}
			if (area > 0.0)
			{
				triangles.insert(triangles.end(), {first, second, third});
			}
			else
			{
				triangles.insert(triangles.end(), {first, third, second});
			}
		};
		auto append_trapezoid =
		    [&](const scan_edge& left, const scan_edge& right, double low_y, double high_y)
		{
			const auto endpoints = [&](double level)
			{
				double x0 = x_at(left, level);
				double x1 = x_at(right, level);
				if (x0 > x1)
				{
					// Rounding the intersection to a strip boundary can reverse its endpoints.
					x0 = x1 = std::midpoint(x0, x1);
				}
				return std::array{glm::vec2{static_cast<float>(x0), static_cast<float>(level)},
				                  glm::vec2{static_cast<float>(x1), static_cast<float>(level)}};
			};
			const auto [low_left, low_right] = endpoints(low_y);
			const auto [high_left, high_right] = endpoints(high_y);
			append_triangle(low_left, low_right, high_right);
			append_triangle(low_left, high_right, high_left);
		};

		const auto emit_strip = [&](double low_y, double high_y)
		{
			const double middle_y = std::midpoint(low_y, high_y);
			crossings.clear();
			for (const std::size_t edge : active)
			{
				crossings.push_back({x_at(edges[edge], middle_y), edge});
			}
			std::ranges::sort(crossings, [](const crossing& left, const crossing& right)
			                  { return left.x == right.x ? left.edge < right.edge : left.x < right.x; });

			std::int64_t winding = 0;
			bool parity = false;
			std::size_t left_edge = no_position;
			for (const auto& crossing : crossings)
			{
				const bool was_filled = even_odd ? parity : winding != 0;
				if (even_odd)
				{
					parity = !parity;
				}
				else
				{
					winding += edges[crossing.edge].winding_delta;
				}
				const bool is_filled = even_odd ? parity : winding != 0;
				if (!was_filled && is_filled)
				{
					left_edge = crossing.edge;
				}
				else if (was_filled && !is_filled)
				{
					if (left_edge == no_position) [[unlikely]]
					{
						throw std::invalid_argument("SVG compound fill has unbalanced contours");
					}
					append_trapezoid(edges[left_edge], edges[crossing.edge], low_y, high_y);
					left_edge = no_position;
				}
			}
			if ((even_odd ? parity : winding != 0) || left_edge != no_position) [[unlikely]]
			{
				throw std::invalid_argument("SVG compound fill has unbalanced contours");
			}
		};

		struct edge_interval
		{
			double low_x;
			double high_x;
		};
		std::vector<edge_interval> intervals;
		std::vector<double> intersections;
		std::size_t intersection_count = 0;
		for (std::size_t strip = 0; strip + 1 < y_levels.size(); ++strip)
		{
			while (next_end < end_order.size() && edges[end_order[next_end]].end_event == strip)
			{
				const std::size_t ending = end_order[next_end++];
				const std::size_t position = active_positions[ending];
				if (position != no_position)
				{
					const std::size_t last = active.back();
					active[position] = last;
					active_positions[last] = position;
					active.pop_back();
					active_positions[ending] = no_position;
				}
			}
			while (next_start < start_order.size() && edges[start_order[next_start]].start_event == strip)
			{
				const std::size_t starting = start_order[next_start++];
				active_positions[starting] = active.size();
				active.push_back(starting);
			}
			if (active.empty())
			{
				continue;
			}

			const double low_y = y_levels[strip];
			const double high_y = y_levels[strip + 1];
			intervals.clear();
			intersections.clear();
			for (const auto edge : active)
			{
				intervals.push_back({x_at(edges[edge], low_y), x_at(edges[edge], high_y)});
			}
			std::ranges::sort(intervals, [](const edge_interval& a, const edge_interval& b)
			                  { return a.low_x == b.low_x ? a.high_x < b.high_x : a.low_x < b.low_x; });

			// Each inversion in the upper order is a crossing inside this strip.
			for (std::size_t i = 1; i < intervals.size(); ++i)
			{
				const auto right = intervals[i];
				std::size_t j = i;
				while (j > 0 && intervals[j - 1].high_x > right.high_x)
				{
					const auto left = intervals[j - 1];
					const double low_distance = right.low_x - left.low_x;
					const double high_distance = left.high_x - right.high_x;
					const double y = std::lerp(low_y, high_y, low_distance / (low_distance + high_distance));
					if (low_y < y && y < high_y)
					{
						if (++intersection_count > 1'000'000) [[unlikely]]
						{
							throw std::length_error("SVG filled contours exceed the intersection limit");
						}
						intersections.push_back(y);
					}
					intervals[j--] = left;
				}
				intervals[j] = right;
			}
			std::ranges::sort(intersections);
			double lower = low_y;
			for (const double crossing_y : intersections)
			{
				if (crossing_y > lower)
				{
					emit_strip(lower, crossing_y);
					lower = crossing_y;
				}
			}
			emit_strip(lower, high_y);
		}
		return triangles;
	}

	class path_reader
	{
	public:
		explicit path_reader(std::string_view data) noexcept : m_numbers(data) {}

		[[nodiscard]] bool at_end() noexcept { return m_numbers.at_end(); }
		[[nodiscard]] bool has_number() noexcept { return m_numbers.has_number(); }
		[[nodiscard]] float number() { return m_numbers.number(); }
		[[nodiscard]] bool arc_flag()
		{
			const char value = m_numbers.take();
			if (value != '0' && value != '1') [[unlikely]]
			{
				throw std::invalid_argument("SVG arc flags must be 0 or 1");
			}
			return value == '1';
		}
		[[nodiscard]] char peek() noexcept { return m_numbers.peek(); }
		[[nodiscard]] char take() noexcept { return m_numbers.take(); }

	private:
		number_reader m_numbers;
	};

	[[nodiscard]] inline std::size_t curve_segment_count(double sweep_angle, double max_radius,
	                                                     float curve_tolerance,
	                                                     std::size_t max_curve_segments)
	{
		if (max_curve_segments == 0) [[unlikely]]
		{
			throw std::length_error("SVG curve exceeds the configured curve segment limit");
		}
		const double max_step =
		    4.0 * std::asin(std::sqrt(
		              std::clamp(static_cast<double>(curve_tolerance) / (2.0 * max_radius), 0.0, 1.0)));
		const double required_segments = std::ceil(std::abs(sweep_angle) / max_step);
		if (!std::isfinite(required_segments) ||
		    required_segments >= static_cast<double>(std::numeric_limits<std::size_t>::max())) [[unlikely]]
		{
			throw std::length_error("SVG curve exceeds the configured curve segment limit");
		}
		const auto count = static_cast<std::size_t>(required_segments);
		if (count > max_curve_segments) [[unlikely]]
		{
			throw std::length_error("SVG curve exceeds the configured curve segment limit");
		}
		return std::max<std::size_t>(1, count);
	}

	[[nodiscard]] inline std::vector<contour> parse_path(std::string_view data, float curve_tolerance = 0.25f,
	                                                     std::size_t max_curve_segments = 1'000'000,
	                                                     bool record_markers = false)
	{
		if (!std::isfinite(curve_tolerance) || curve_tolerance <= 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("SVG curve tolerance must be finite and greater than zero");
		}
		if (max_curve_segments == 0) [[unlikely]]
		{
			throw std::invalid_argument("SVG maximum curve segment count must be greater than zero");
		}

		path_reader reader(data);
		std::vector<contour> result;
		contour current_contour;
		glm::vec2 current{0.0f};
		glm::vec2 subpath_start{0.0f};
		glm::vec2 last_cubic_control{0.0f};
		glm::vec2 last_quadratic_control{0.0f};
		char command = '\0';
		char previous_command = '\0';
		bool can_continue_after_close = false;
		std::size_t generated_curve_segments = 0;
		constexpr unsigned max_curve_depth = 32;

		auto append_point = [&](glm::vec2 point)
		{
			if (current_contour.points.empty() || current_contour.points.back().x != point.x ||
			    current_contour.points.back().y != point.y)
			{
				current_contour.points.push_back(point);
			}
			current = point;
		};
		const auto record_segment = [&](glm::vec2 point, glm::vec2 start_direction, glm::vec2 end_direction)
		{
			if (record_markers)
			{
				current_contour.marker_vertices.back().outgoing = start_direction;
				current_contour.marker_vertices.push_back({point, end_direction, {}});
			}
		};
		const auto append_line = [&](glm::vec2 point)
		{
			current_contour.has_drawn_segment = true;
			record_segment(point, point - current, point - current);
			append_point(point);
		};
		const auto first_direction = [](glm::vec2 first, glm::vec2 second, glm::vec2 last)
		{ return first != glm::vec2{} ? first : (second != glm::vec2{} ? second : last); };
		auto append_curve_point = [&](glm::vec2 point)
		{
			if (generated_curve_segments == max_curve_segments) [[unlikely]]
			{
				throw std::length_error("SVG path exceeds the configured curve segment limit");
			}
			++generated_curve_segments;
			current_contour.has_drawn_segment = true;
			append_point(point);
		};
		auto finish_contour = [&]
		{
			if (!current_contour.points.empty())
			{
				result.push_back(std::move(current_contour));
				current_contour = {};
			}
		};
		auto point_pair = [&](bool relative)
		{
			const float x = reader.number();
			const float y = reader.number();
			return relative ? current + glm::vec2{x, y} : glm::vec2{x, y};
		};
		const auto midpoint = [](glm::vec2 left, glm::vec2 right) noexcept
		{
			return glm::vec2{static_cast<float>((static_cast<double>(left.x) + right.x) * 0.5),
			                 static_cast<float>((static_cast<double>(left.y) + right.y) * 0.5)};
		};
		const auto point_segment_distance = [](glm::vec2 point, glm::vec2 start, glm::vec2 end) noexcept
		{
			const double dx = static_cast<double>(end.x) - start.x;
			const double dy = static_cast<double>(end.y) - start.y;
			const double length_squared = dx * dx + dy * dy;
			if (length_squared == 0.0)
			{
				return std::hypot(static_cast<double>(point.x) - start.x,
				                  static_cast<double>(point.y) - start.y);
			}
			const double projection = std::clamp(((static_cast<double>(point.x) - start.x) * dx +
			                                      (static_cast<double>(point.y) - start.y) * dy) /
			                                         length_squared,
			                                     0.0, 1.0);
			return std::hypot(static_cast<double>(point.x) - (start.x + projection * dx),
			                  static_cast<double>(point.y) - (start.y + projection * dy));
		};
		const auto point_distance = [](glm::vec2 left, glm::vec2 right) noexcept
		{ return std::hypot(static_cast<double>(left.x) - right.x, static_cast<double>(left.y) - right.y); };
		auto flatten_quadratic = [&](auto&& self, glm::vec2 start, glm::vec2 control, glm::vec2 end,
		                             unsigned depth) -> void
		{
			const double polygon_excess =
			    std::max(0.0, point_distance(start, control) + point_distance(control, end) -
				                  point_distance(start, end));
			if (std::max(point_segment_distance(control, start, end), polygon_excess) <= curve_tolerance)
			{
				append_curve_point(end);
				return;
			}
			if (depth == max_curve_depth) [[unlikely]]
			{
				throw std::length_error("SVG quadratic curve could not meet the configured tolerance");
			}

			const auto start_control = midpoint(start, control);
			const auto control_end = midpoint(control, end);
			const auto split_point = midpoint(start_control, control_end);
			self(self, start, start_control, split_point, depth + 1);
			self(self, split_point, control_end, end, depth + 1);
		};
		auto flatten_cubic = [&](auto&& self, glm::vec2 start, glm::vec2 first_control,
		                         glm::vec2 second_control, glm::vec2 end, unsigned depth) -> void
		{
			const double polygon_excess = std::max(
			    0.0, point_distance(start, first_control) + point_distance(first_control, second_control) +
			             point_distance(second_control, end) - point_distance(start, end));
			if (std::max({point_segment_distance(first_control, start, end),
			              point_segment_distance(second_control, start, end), polygon_excess}) <=
			    curve_tolerance)
			{
				append_curve_point(end);
				return;
			}
			if (depth == max_curve_depth) [[unlikely]]
			{
				throw std::length_error("SVG cubic curve could not meet the configured tolerance");
			}

			const auto start_first = midpoint(start, first_control);
			const auto first_second = midpoint(first_control, second_control);
			const auto second_end = midpoint(second_control, end);
			const auto split_first = midpoint(start_first, first_second);
			const auto split_second = midpoint(first_second, second_end);
			const auto split_point = midpoint(split_first, split_second);
			self(self, start, start_first, split_first, split_point, depth + 1);
			self(self, split_point, split_second, second_end, end, depth + 1);
		};
		auto flatten_arc =
		    [&](float rx, float ry, float rotation_degrees, bool large_arc, bool sweep, glm::vec2 end)
		{
			const glm::vec2 start = current;
			rx = std::abs(rx);
			ry = std::abs(ry);
			if (rx == 0.0f || ry == 0.0f || start == end)
			{
				append_line(end);
				return;
			}
			const double phi = glm::radians(static_cast<double>(rotation_degrees));
			const double cosine = std::cos(phi);
			const double sine = std::sin(phi);
			const double dx = (static_cast<double>(start.x) - end.x) * 0.5;
			const double dy = (static_cast<double>(start.y) - end.y) * 0.5;
			const double x_prime = cosine * dx + sine * dy;
			const double y_prime = -sine * dx + cosine * dy;
			double rx_double = rx;
			double ry_double = ry;
			const double lambda =
			    x_prime * x_prime / (rx_double * rx_double) + y_prime * y_prime / (ry_double * ry_double);
			if (lambda > 1.0)
			{
				const double scale = std::sqrt(lambda);
				rx_double *= scale;
				ry_double *= scale;
			}
			const double rx2 = rx_double * rx_double;
			const double ry2 = ry_double * ry_double;
			const double numerator =
			    std::max(0.0, rx2 * ry2 - rx2 * y_prime * y_prime - ry2 * x_prime * x_prime);
			const double denominator = rx2 * y_prime * y_prime + ry2 * x_prime * x_prime;
			const double sign = large_arc == sweep ? -1.0 : 1.0;
			const double factor = denominator == 0.0 ? 0.0 : sign * std::sqrt(numerator / denominator);
			const double center_x_prime = factor * rx_double * y_prime / ry_double;
			const double center_y_prime = factor * -ry_double * x_prime / rx_double;
			const double center_x = cosine * center_x_prime - sine * center_y_prime +
			                        (static_cast<double>(start.x) + end.x) * 0.5;
			const double center_y = sine * center_x_prime + cosine * center_y_prime +
			                        (static_cast<double>(start.y) + end.y) * 0.5;
			const double ux = (x_prime - center_x_prime) / rx_double;
			const double uy = (y_prime - center_y_prime) / ry_double;
			const double vx = (-x_prime - center_x_prime) / rx_double;
			const double vy = (-y_prime - center_y_prime) / ry_double;
			const double start_angle = std::atan2(uy, ux);
			double delta_angle = std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
			if (!sweep && delta_angle > 0.0)
			{
				delta_angle -= 2.0 * std::numbers::pi;
			}
			else if (sweep && delta_angle < 0.0)
			{
				delta_angle += 2.0 * std::numbers::pi;
			}
			if (record_markers)
			{
				const auto direction = [&](double angle)
				{
					const double x = -rx_double * std::sin(angle);
					const double y = ry_double * std::cos(angle);
					const double length = std::hypot(x, y) * (sweep ? 1.0 : -1.0);
					return glm::vec2{static_cast<float>((cosine * x - sine * y) / length),
					                 static_cast<float>((sine * x + cosine * y) / length)};
				};
				record_segment(end, direction(start_angle), direction(start_angle + delta_angle));
			}
			const auto remaining_segments = max_curve_segments - generated_curve_segments;
			const auto segments = curve_segment_count(delta_angle, std::max(rx_double, ry_double),
			                                          curve_tolerance, remaining_segments);
			for (std::size_t i = 1; i <= segments; ++i)
			{
				const double angle = start_angle + delta_angle * static_cast<double>(i) / segments;
				const double x = rx_double * std::cos(angle);
				const double y = ry_double * std::sin(angle);
				const glm::vec2 point = i == segments
				                            ? end
				                            : glm::vec2{static_cast<float>(cosine * x - sine * y + center_x),
				                                        static_cast<float>(sine * x + cosine * y + center_y)};
				append_curve_point(point);
			}
		};

		while (!reader.at_end())
		{
			if (std::isalpha(static_cast<unsigned char>(reader.peek())))
			{
				command = reader.take();
			}
			else if (!reader.has_number() || command == '\0')
			{
				throw std::invalid_argument("SVG path data contains an unexpected token");
			}
			const bool relative = std::islower(static_cast<unsigned char>(command));
			const char operation = static_cast<char>(std::toupper(static_cast<unsigned char>(command)));

			if (operation == 'Z')
			{
				if (!current_contour.points.empty())
				{
					current_contour.closed = true;
					current_contour.has_drawn_segment = true;
					record_segment(subpath_start, subpath_start - current, subpath_start - current);
					current = subpath_start;
					finish_contour();
					can_continue_after_close = true;
					last_cubic_control = current;
					last_quadratic_control = current;
				}
				previous_command = command;
				command = '\0';
				continue;
			}

			if (operation == 'M')
			{
				if (!reader.has_number()) [[unlikely]]
				{
					throw std::invalid_argument("SVG path moveto command is missing coordinates");
				}
				finish_contour();
				can_continue_after_close = false;
				const glm::vec2 point = point_pair(relative);
				current_contour.points.push_back(point);
				if (record_markers)
				{
					current_contour.marker_vertices.push_back({point, {}, {}});
				}
				current = point;
				subpath_start = point;
				command = relative ? 'l' : 'L';
				while (reader.has_number())
				{
					append_line(point_pair(relative));
				}
				previous_command = 'M';
				continue;
			}

			if (current_contour.points.empty() && can_continue_after_close)
			{
				current_contour.points.push_back(subpath_start);
				if (record_markers)
				{
					current_contour.marker_vertices.push_back({subpath_start, {}, {}});
				}
				current = subpath_start;
				can_continue_after_close = false;
			}
			if (current_contour.points.empty()) [[unlikely]]
			{
				throw std::invalid_argument("SVG path must begin with a moveto command");
			}
			if (operation == 'L' || operation == 'H' || operation == 'V')
			{
				bool consumed = false;
				while (reader.has_number())
				{
					glm::vec2 point = current;
					if (operation == 'L')
					{
						point = point_pair(relative);
					}
					else if (operation == 'H')
					{
						const float x = reader.number();
						point.x = relative ? point.x + x : x;
					}
					else
					{
						const float y = reader.number();
						point.y = relative ? point.y + y : y;
					}
					append_line(point);
					consumed = true;
				}
				if (!consumed) [[unlikely]]
				{
					throw std::invalid_argument("SVG path line command is missing coordinates");
				}
			}
			else if (operation == 'C')
			{
				bool consumed = false;
				while (reader.has_number())
				{
					const glm::vec2 first = point_pair(relative);
					const glm::vec2 second = point_pair(relative);
					const glm::vec2 end = point_pair(relative);
					record_segment(end, first_direction(first - current, second - current, end - current),
					               first_direction(end - second, end - first, end - current));
					flatten_cubic(flatten_cubic, current, first, second, end, 0);
					last_cubic_control = second;
					previous_command = command;
					consumed = true;
				}
				if (!consumed) [[unlikely]]
				{
					throw std::invalid_argument("SVG cubic curve command is missing coordinates");
				}
			}
			else if (operation == 'S')
			{
				bool consumed = false;
				while (reader.has_number())
				{
					const glm::vec2 first = (previous_command == 'C' || previous_command == 'c' ||
					                         previous_command == 'S' || previous_command == 's')
					                            ? current * 2.0f - last_cubic_control
					                            : current;
					const glm::vec2 second = point_pair(relative);
					const glm::vec2 end = point_pair(relative);
					record_segment(end, first_direction(first - current, second - current, end - current),
					               first_direction(end - second, end - first, end - current));
					flatten_cubic(flatten_cubic, current, first, second, end, 0);
					last_cubic_control = second;
					previous_command = command;
					consumed = true;
				}
				if (!consumed) [[unlikely]]
				{
					throw std::invalid_argument("SVG smooth cubic curve command is missing coordinates");
				}
			}
			else if (operation == 'Q')
			{
				bool consumed = false;
				while (reader.has_number())
				{
					const glm::vec2 control = point_pair(relative);
					const glm::vec2 end = point_pair(relative);
					record_segment(end, first_direction(control - current, end - current, {}),
					               first_direction(end - control, end - current, {}));
					flatten_quadratic(flatten_quadratic, current, control, end, 0);
					last_quadratic_control = control;
					previous_command = command;
					consumed = true;
				}
				if (!consumed) [[unlikely]]
				{
					throw std::invalid_argument("SVG quadratic curve command is missing coordinates");
				}
			}
			else if (operation == 'T')
			{
				bool consumed = false;
				while (reader.has_number())
				{
					const glm::vec2 control = (previous_command == 'Q' || previous_command == 'q' ||
					                           previous_command == 'T' || previous_command == 't')
					                              ? current * 2.0f - last_quadratic_control
					                              : current;
					const glm::vec2 end = point_pair(relative);
					record_segment(end, first_direction(control - current, end - current, {}),
					               first_direction(end - control, end - current, {}));
					flatten_quadratic(flatten_quadratic, current, control, end, 0);
					last_quadratic_control = control;
					previous_command = command;
					consumed = true;
				}
				if (!consumed) [[unlikely]]
				{
					throw std::invalid_argument("SVG smooth quadratic curve command is missing coordinates");
				}
			}
			else if (operation == 'A')
			{
				bool consumed = false;
				while (reader.has_number())
				{
					const float rx = reader.number();
					const float ry = reader.number();
					const float rotation = reader.number();
					const bool large = reader.arc_flag();
					const bool sweep = reader.arc_flag();
					const glm::vec2 end = point_pair(relative);
					flatten_arc(rx, ry, rotation, large, sweep, end);
					consumed = true;
				}
				if (!consumed) [[unlikely]]
				{
					throw std::invalid_argument("SVG arc command is missing coordinates");
				}
			}
			else
			{
				throw std::invalid_argument("SVG path command is not supported");
			}
			previous_command = command;
		}
		finish_contour();
		return result;
	}

	[[nodiscard]] inline std::vector<glm::vec2> parse_points(std::string_view value)
	{
		const auto numbers = parse_number_list(value);
		if (numbers.size() % 2 != 0) [[unlikely]]
		{
			throw std::invalid_argument("SVG points attribute requires x/y pairs");
		}
		std::vector<glm::vec2> points;
		points.reserve(numbers.size() / 2);
		for (std::size_t i = 0; i < numbers.size(); i += 2)
		{
			points.emplace_back(numbers[i], numbers[i + 1]);
		}
		return points;
	}

	[[nodiscard]] inline std::vector<glm::vec2> rounded_rectangle_points(
	    float x, float y, float width, float height, float rx, float ry, float curve_tolerance = 0.25f,
	    std::size_t max_curve_segments = 1'000'000)
	{
		if (width < 0.0f || height < 0.0f || rx < 0.0f || ry < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("SVG rectangle dimensions and radii must be non-negative");
		}
		if (width == 0.0f || height == 0.0f)
		{
			return {};
		}
		if (rx == 0.0f || ry == 0.0f)
		{
			return {{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}};
		}
		rx = std::min(rx, width * 0.5f);
		ry = std::min(ry, height * 0.5f);
		std::vector<glm::vec2> points;
		const auto corner_segments = curve_segment_count(std::numbers::pi / 2.0, std::max(rx, ry),
		                                                 curve_tolerance, max_curve_segments);
		if (corner_segments > (std::numeric_limits<std::size_t>::max() - 4) / 4 ||
		    corner_segments * 4 > max_curve_segments) [[unlikely]]
		{
			throw std::length_error("SVG rounded rectangle exceeds the configured curve segment limit");
		}
		constexpr std::array<float, 4> start_angles{-std::numbers::pi_v<float> * 0.5f, 0.0f,
		                                            std::numbers::pi_v<float> * 0.5f,
		                                            std::numbers::pi_v<float>};
		const std::array<glm::vec2, 4> centers{glm::vec2{x + width - rx, y + ry},
		                                       glm::vec2{x + width - rx, y + height - ry},
		                                       glm::vec2{x + rx, y + height - ry}, glm::vec2{x + rx, y + ry}};
		points.reserve(corner_segments * 4 + 4);
		for (std::size_t corner = 0; corner < centers.size(); ++corner)
		{
			for (std::size_t i = 0; i <= corner_segments; ++i)
			{
				const float angle = start_angles[corner] + std::numbers::pi_v<float> * 0.5f *
				                                               static_cast<float>(i) /
				                                               static_cast<float>(corner_segments);
				points.emplace_back(centers[corner].x + rx * std::cos(angle),
				                    centers[corner].y + ry * std::sin(angle));
			}
		}
		return points;
	}

	[[nodiscard]] inline std::vector<glm::vec2> ellipse_points(float cx, float cy, float rx, float ry,
	                                                           float curve_tolerance = 0.25f,
	                                                           std::size_t max_curve_segments = 1'000'000)
	{
		if (rx < 0.0f || ry < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("SVG ellipse radii must be non-negative");
		}
		if (rx == 0.0f || ry == 0.0f)
		{
			return {};
		}
		const auto segments = curve_segment_count(2.0 * std::numbers::pi, std::max(rx, ry), curve_tolerance,
		                                          max_curve_segments);
		if (segments < 3) [[unlikely]]
		{
			if (max_curve_segments < 3)
				throw std::length_error("SVG ellipse requires a curve segment limit of at least three");
		}
		const auto polygon_segments = std::max<std::size_t>(segments, 3);
		std::vector<glm::vec2> points;
		points.reserve(polygon_segments);
		for (std::size_t i = 0; i < polygon_segments; ++i)
		{
			const float angle =
			    2.0f * std::numbers::pi_v<float> * static_cast<float>(i) /
			    static_cast<float>(polygon_segments);
			points.emplace_back(cx + rx * std::cos(angle), cy + ry * std::sin(angle));
		}
		return points;
	}

	[[nodiscard]] inline bool same_point(glm::vec2 first, glm::vec2 second) noexcept
	{
		return first.x == second.x && first.y == second.y;
	}

	inline void append_stroke_point(std::vector<glm::vec2>& points, glm::vec2 point)
	{
		if (points.empty() || !same_point(points.back(), point))
		{
			points.push_back(point);
		}
	}

	inline void count_stroke_segment(std::size_t& generated_segments, std::size_t max_curve_segments)
	{
		if (generated_segments >= max_curve_segments) [[unlikely]]
		{
			throw std::length_error("SVG stroke exceeds the configured curve segment limit");
		}
		++generated_segments;
	}

	inline void append_stroke_arc(std::vector<glm::vec2>& triangles, glm::vec2 center, float radius,
	                              float start_angle, float sweep_angle, float curve_tolerance,
	                              std::size_t max_curve_segments, std::size_t& generated_segments)
	{
		const auto segments = curve_segment_count(sweep_angle, radius, curve_tolerance, max_curve_segments);
		if (segments > max_curve_segments - generated_segments) [[unlikely]]
		{
			throw std::length_error("SVG stroke exceeds the configured curve segment limit");
		}
		generated_segments += segments;

		glm::vec2 previous{center.x + radius * std::cos(start_angle),
		                   center.y + radius * std::sin(start_angle)};
		for (std::size_t segment = 1; segment <= segments; ++segment)
		{
			const float angle =
			    start_angle + sweep_angle * static_cast<float>(segment) / static_cast<float>(segments);
			const glm::vec2 next{center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
			::mngl::two_d::detail::append_triangle(triangles, center, previous, next);
			previous = next;
		}
	}

	[[nodiscard]] inline std::vector<stroke_path> make_stroke_paths(const contour& source,
	                                                                const paint_state& style,
	                                                                std::size_t max_curve_segments,
	                                                                std::size_t& generated_segments)
	{
		std::vector<glm::vec2> points;
		points.reserve(source.points.size());
		for (const auto point : source.points)
		{
			if (!::mngl::two_d::detail::finite(point)) [[unlikely]]
			{
				throw std::invalid_argument("SVG stroke contains a non-finite point");
			}
			append_stroke_point(points, point);
		}
		if (source.closed && points.size() > 1 && same_point(points.front(), points.back()))
		{
			points.pop_back();
		}
		if (points.empty())
		{
			return {};
		}
		if (points.size() == 1 && !source.has_drawn_segment)
		{
			return {};
		}

		glm::vec2 initial_tangent{1.0f, 0.0f};
		for (std::size_t point = 1; point < points.size(); ++point)
		{
			const auto delta = points[point] - points[point - 1];
			const double length = std::hypot(static_cast<double>(delta.x), static_cast<double>(delta.y));
			if (length > 0.0)
			{
				initial_tangent = {static_cast<float>(delta.x / length),
				                   static_cast<float>(delta.y / length)};
				break;
			}
		}

		if (style.stroke_dasharray.empty())
		{
			return {{std::move(points), source.closed, initial_tangent}};
		}
		if (style.stroke_dasharray.size() > max_curve_segments) [[unlikely]]
		{
			throw std::length_error("SVG stroke-dasharray exceeds the configured curve segment limit");
		}

		double dash_period = 0.0;
		for (const float dash_length : style.stroke_dasharray)
		{
			dash_period += dash_length;
		}
		if (!std::isfinite(dash_period)) [[unlikely]]
		{
			throw std::invalid_argument("SVG stroke-dasharray has an unrepresentable total length");
		}
		if (dash_period == 0.0)
		{
			return {{std::move(points), source.closed, initial_tangent}};
		}
		if (points.size() == 1)
		{
			double point_phase = std::fmod(static_cast<double>(style.stroke_dashoffset), dash_period);
			if (point_phase < 0.0)
			{
				point_phase += dash_period;
			}
			bool dash_at_start = false;
			for (std::size_t dash = 0; dash < style.stroke_dasharray.size(); ++dash)
			{
				const double dash_length = style.stroke_dasharray[dash];
				if (dash_length == 0.0)
				{
					dash_at_start |= point_phase == 0.0 && dash % 2 == 0;
					continue;
				}
				if (point_phase < dash_length)
				{
					dash_at_start |= dash % 2 == 0;
					break;
				}
				if (point_phase == dash_length)
				{
					dash_at_start |= dash % 2 == 0;
				}
				point_phase -= dash_length;
			}
			return dash_at_start && style.linecap != stroke_linecap::butt
			           ? std::vector<stroke_path>{{std::move(points), false, initial_tangent}}
					   : std::vector<stroke_path>{};
		}

		const std::size_t segment_count =
		    source.closed && points.size() > 1 ? points.size() : points.size() - 1;
		std::vector<double> lengths(segment_count);
		for (std::size_t segment = 0; segment < segment_count; ++segment)
		{
			const auto delta = points[(segment + 1) % points.size()] - points[segment];
			if (!::mngl::two_d::detail::finite(delta)) [[unlikely]]
			{
				throw std::invalid_argument("SVG stroke contains an unrepresentable segment");
			}
			lengths[segment] = std::hypot(static_cast<double>(delta.x), static_cast<double>(delta.y));
			if (!std::isfinite(lengths[segment])) [[unlikely]]
			{
				throw std::invalid_argument("SVG stroke contains a segment with an unrepresentable length");
			}
		}

		std::vector<stroke_path> result;
		std::vector<glm::vec2> current_run;
		auto flush_run = [&]
		{
			if (!current_run.empty())
			{
				result.push_back({std::move(current_run), false, initial_tangent});
				current_run.clear();
			}
		};
		auto append_dot = [&](glm::vec2 position, glm::vec2 tangent)
		{
			if (style.linecap != stroke_linecap::butt)
			{
				result.push_back({{position}, false, tangent});
			}
		};

		double phase = std::fmod(static_cast<double>(style.stroke_dashoffset), dash_period);
		if (phase < 0.0)
		{
			phase += dash_period;
		}
		std::size_t dash_index = 0;
		double remaining = 0.0;
		while (true)
		{
			const double dash_length = style.stroke_dasharray[dash_index];
			if (dash_length == 0.0)
			{
				if (phase == 0.0 && dash_index % 2 == 0)
				{
					append_dot(points.front(), initial_tangent);
				}
				dash_index = (dash_index + 1) % style.stroke_dasharray.size();
				continue;
			}
			if (phase >= dash_length)
			{
				if (phase == dash_length && dash_index % 2 == 0)
				{
					append_dot(points.front(), initial_tangent);
				}
				phase -= dash_length;
				dash_index = (dash_index + 1) % style.stroke_dasharray.size();
				continue;
			}
			remaining = dash_length - phase;
			break;
		}

		auto advance_dash = [&](glm::vec2 position, glm::vec2 tangent)
		{
			const bool was_on = dash_index % 2 == 0;
			dash_index = (dash_index + 1) % style.stroke_dasharray.size();
			while (style.stroke_dasharray[dash_index] == 0.0f)
			{
				if (dash_index % 2 == 0)
				{
					append_dot(position, tangent);
				}
				dash_index = (dash_index + 1) % style.stroke_dasharray.size();
			}
			const bool is_on = dash_index % 2 == 0;
			if (was_on && !is_on)
			{
				flush_run();
			}
			remaining = style.stroke_dasharray[dash_index];
		};

		for (std::size_t segment = 0; segment < segment_count; ++segment)
		{
			const glm::vec2 first = points[segment];
			const glm::vec2 second = points[(segment + 1) % points.size()];
			const auto delta = second - first;
			const double segment_length = lengths[segment];
			if (segment_length == 0.0)
			{
				continue;
			}
			const glm::vec2 tangent{static_cast<float>(delta.x / segment_length),
			                        static_cast<float>(delta.y / segment_length)};
			double segment_position = 0.0;
			while (segment_position < segment_length)
			{
				count_stroke_segment(generated_segments, max_curve_segments);
				const double step = std::min(remaining, segment_length - segment_position);
				if (step <= 0.0) [[unlikely]]
				{
					throw std::invalid_argument("SVG stroke-dasharray did not advance along the path");
				}
				if (dash_index % 2 == 0)
				{
					const float start_t = static_cast<float>(segment_position / segment_length);
					const float end_t = static_cast<float>((segment_position + step) / segment_length);
					const glm::vec2 start = first + delta * start_t;
					const glm::vec2 end = first + delta * end_t;
					append_stroke_point(current_run, start);
					append_stroke_point(current_run, end);
				}
				segment_position += step;
				remaining -= step;
				if (remaining == 0.0)
				{
					const glm::vec2 position =
					    first + delta * static_cast<float>(segment_position / segment_length);
					advance_dash(position, tangent);
				}
			}
		}
		flush_run();
		if (source.closed && result.size() == 1 && result.front().points.size() > 1 &&
		    same_point(result.front().points.front(), points.front()) &&
		    same_point(result.front().points.back(), points.front()))
		{
			result.front().points.pop_back();
			result.front().closed = true;
		}
		else if (source.closed && result.size() > 1 &&
		         same_point(result.front().points.front(), points.front()) &&
		         same_point(result.back().points.back(), points.front()))
		{
			auto joined = std::move(result.back().points);
			joined.insert(joined.end(), std::next(result.front().points.begin()), result.front().points.end());
			result.front().points = std::move(joined);
			result.pop_back();
		}
		return result;
	}

	inline void append_stroke_geometry(std::vector<glm::vec2>& triangles, const stroke_path& path,
	                                   const paint_state& style, float curve_tolerance,
	                                   std::size_t max_curve_segments, std::size_t& generated_segments)
	{
		if (path.points.empty() || style.stroke_width <= 0.0f)
		{
			return;
		}

		const float half_width = style.stroke_width * 0.5f;
		if (path.points.size() == 1)
		{
			if (style.linecap == stroke_linecap::round)
			{
				append_stroke_arc(triangles, path.points.front(), half_width, 0.0f,
				                  2.0f * std::numbers::pi_v<float>, curve_tolerance, max_curve_segments,
				                  generated_segments);
			}
			else if (style.linecap == stroke_linecap::square)
			{
				const glm::vec2 tangent = glm::normalize(path.zero_tangent);
				const glm::vec2 normal{-tangent.y, tangent.x};
				const glm::vec2 center = path.points.front();
				const auto along = tangent * half_width;
				const auto across = normal * half_width;
				::mngl::two_d::detail::append_quad(triangles, center + along + across,
				                                   center + along - across, center - along + across,
				                                   center - along - across);
			}
			return;
		}

		const std::size_t segment_count = path.closed ? path.points.size() : path.points.size() - 1;
		std::vector<glm::vec2> directions(segment_count);
		std::vector<glm::vec2> normals(segment_count);
		for (std::size_t segment = 0; segment < segment_count; ++segment)
		{
			const auto delta = path.points[(segment + 1) % path.points.size()] - path.points[segment];
			if (!::mngl::two_d::detail::finite(delta)) [[unlikely]]
			{
				throw std::invalid_argument("SVG stroke contains an unrepresentable segment");
			}
			const double length = std::hypot(static_cast<double>(delta.x), static_cast<double>(delta.y));
			if (!std::isfinite(length) || length == 0.0) [[unlikely]]
			{
				throw std::invalid_argument("SVG stroke contains a degenerate segment");
			}
			directions[segment] = {static_cast<float>(delta.x / length),
			                       static_cast<float>(delta.y / length)};
			normals[segment] = {-directions[segment].y, directions[segment].x};
		}

		for (std::size_t segment = 0; segment < segment_count; ++segment)
		{
			glm::vec2 first = path.points[segment];
			glm::vec2 second = path.points[(segment + 1) % path.points.size()];
			if (!path.closed && style.linecap == stroke_linecap::square)
			{
				if (segment == 0)
				{
					first -= directions[segment] * half_width;
				}
				if (segment + 1 == segment_count)
				{
					second += directions[segment] * half_width;
				}
			}
			::mngl::two_d::detail::append_quad(
			    triangles, first + normals[segment] * half_width, first - normals[segment] * half_width,
			    second + normals[segment] * half_width, second - normals[segment] * half_width);
		}

		const std::size_t first_join = path.closed ? 0 : 1;
		const std::size_t join_end = path.closed ? path.points.size() : path.points.size() - 1;
		for (std::size_t point = first_join; point < join_end; ++point)
		{
			const std::size_t previous_segment = (point + segment_count - 1) % segment_count;
			const std::size_t next_segment = point % segment_count;
			const float turn = directions[previous_segment].x * directions[next_segment].y -
			                   directions[previous_segment].y * directions[next_segment].x;
			if (std::abs(turn) <= 1e-6f)
			{
				continue;
			}

			const float outer_side = turn > 0.0f ? -1.0f : 1.0f;
			const glm::vec2 center = path.points[point];
			const glm::vec2 outer_previous = center + normals[previous_segment] * (outer_side * half_width);
			const glm::vec2 outer_next = center + normals[next_segment] * (outer_side * half_width);
			if (style.linejoin == stroke_linejoin::bevel)
			{
				::mngl::two_d::detail::append_triangle(triangles, center, outer_previous, outer_next);
				continue;
			}
			if (style.linejoin == stroke_linejoin::round)
			{
				const auto first_radius = outer_previous - center;
				const auto second_radius = outer_next - center;
				const float start_angle = std::atan2(first_radius.y, first_radius.x);
				const float sweep_angle =
				    std::atan2(first_radius.x * second_radius.y - first_radius.y * second_radius.x,
					           glm::dot(first_radius, second_radius));
				append_stroke_arc(triangles, center, half_width, start_angle, sweep_angle, curve_tolerance,
				                  max_curve_segments, generated_segments);
				continue;
			}

			const glm::vec2 miter = (normals[previous_segment] + normals[next_segment]) * outer_side;
			const float miter_length = glm::length(miter);
			if (miter_length == 0.0f)
			{
				::mngl::two_d::detail::append_triangle(triangles, center, outer_previous, outer_next);
				continue;
			}
			const glm::vec2 miter_direction = miter / miter_length;
			const float denominator =
			    std::abs(glm::dot(miter_direction, normals[previous_segment] * outer_side));
			const float miter_ratio =
			    denominator > 0.0f ? 1.0f / denominator : std::numeric_limits<float>::infinity();
			if (miter_ratio > style.stroke_miterlimit)
			{
				::mngl::two_d::detail::append_triangle(triangles, center, outer_previous, outer_next);
				continue;
			}
			const glm::vec2 tip = center + miter_direction * (half_width * miter_ratio);
			::mngl::two_d::detail::append_triangle(triangles, center, outer_previous, tip);
			::mngl::two_d::detail::append_triangle(triangles, center, tip, outer_next);
		}

		if (!path.closed && style.linecap == stroke_linecap::round)
		{
			const float start_angle = std::atan2(normals.front().y, normals.front().x);
			const float end_angle = std::atan2(-normals.back().y, -normals.back().x);
			append_stroke_arc(triangles, path.points.front(), half_width, start_angle,
			                  std::numbers::pi_v<float>, curve_tolerance, max_curve_segments,
			                  generated_segments);
			append_stroke_arc(triangles, path.points.back(), half_width, end_angle, std::numbers::pi_v<float>,
			                  curve_tolerance, max_curve_segments, generated_segments);
		}
	}

	inline void union_stroke_triangles(std::vector<glm::vec2>& triangles)
	{
		if (triangles.empty())
		{
			return;
		}
		if (triangles.size() % 3 != 0) [[unlikely]]
		{
			throw std::logic_error("SVG stroke mesh contains an incomplete triangle");
		}

		std::vector<std::span<const glm::vec2>> rings;
		rings.reserve(triangles.size() / 3);
		for (std::size_t i = 0; i < triangles.size(); i += 3)
		{
			auto& first = triangles[i];
			auto& second = triangles[i + 1];
			auto& third = triangles[i + 2];
			const double area =
			    (static_cast<double>(second.x) - first.x) * (static_cast<double>(third.y) - first.y) -
			    (static_cast<double>(second.y) - first.y) * (static_cast<double>(third.x) - first.x);
			if (!std::isfinite(area)) [[unlikely]]
			{
				throw std::invalid_argument("SVG stroke contains an unrepresentable triangle");
			}
			if (area == 0.0)
			{
				continue;
			}
			if (area < 0.0)
			{
				std::swap(second, third);
			}
			rings.emplace_back(triangles.data() + i, 3);
		}
		triangles = triangulate_compound(rings, false);
	}

	using gradient_definitions = std::unordered_map<std::string, std::shared_ptr<const gradient_definition>>;

	struct bounds_accumulator
	{
		glm::vec2 minimum{std::numeric_limits<float>::max()};
		glm::vec2 maximum{std::numeric_limits<float>::lowest()};
		bool valid = false;

		void include(glm::vec2 point)
		{
			if (!::mngl::detail::finite(point)) [[unlikely]]
			{
				throw std::invalid_argument("SVG geometry contains a non-finite point");
			}
			minimum = glm::min(minimum, point);
			maximum = glm::max(maximum, point);
			valid = true;
		}

		[[nodiscard]] rectangle bounds() const
		{
			if (!valid)
			{
				return {};
			}
			const auto size = maximum - minimum;
			if (!::mngl::detail::finite(size)) [[unlikely]]
			{
				throw std::length_error("SVG geometry exceeds the supported coordinate range");
			}
			return {minimum, size};
		}
	};

	struct transformed_bounds
	{
		rectangle bounds;
		glm::mat4 transform{1.0f};
	};

	struct geometry_bounds_scope
	{
		bounds_accumulator bounds;
		glm::mat4 to_local{1.0f};
	};

	[[nodiscard]] inline std::optional<gradient_paint> shape_gradient(const gradient_definitions& gradients,
	                                                                  const std::string& id, rectangle bounds,
	                                                                  glm::vec4 view_box)
	{
		if (id.empty())
		{
			return std::nullopt;
		}
		const auto definition = gradients.find(id);
		if (definition == gradients.end()) [[unlikely]]
		{
			throw std::invalid_argument("SVG paint references an unknown gradient id '#" + id + "'");
		}
		if (definition->second->object_bounding_box && (bounds.size.x == 0.0f || bounds.size.y == 0.0f))
		{
			return std::nullopt;
		}
		if (definition->second->stops.empty())
		{
			return std::nullopt;
		}
		return resolve_gradient_paint(definition->second, bounds.pos, bounds.pos + bounds.size, view_box);
	}

	inline void add_geometry(document_data& document, std::span<const contour> contours, rectangle bounds,
	                         const paint_state& style, float curve_tolerance, std::size_t max_curve_segments,
	                         const gradient_definitions& gradients,
	                         const ::mngl::svg_hit_test_element& svg_element,
	                         std::array<std::shared_ptr<const std::vector<glm::vec2>>, 2>* cached_triangles)
	{
		const auto add_paint = [&](auto&& create_triangles, std::size_t cache_slot, const ::mngl::color& paint,
		                           float opacity, const std::string& gradient_id,
		                           const paint_server_fallback& fallback)
		{
			std::vector<glm::vec2> triangles;
			std::shared_ptr<const std::vector<glm::vec2>> shared_triangles;
			if (cached_triangles && (*cached_triangles)[cache_slot])
				shared_triangles = (*cached_triangles)[cache_slot];
			else
			{
				triangles = create_triangles();
				detail::validate_points(triangles, "SVG paint produced non-finite geometry");
				if (cached_triangles)
				{
					shared_triangles = std::make_shared<const std::vector<glm::vec2>>(std::move(triangles));
					(*cached_triangles)[cache_slot] = shared_triangles;
				}
			}
			const auto vertices = shared_triangles ? std::span<const glm::vec2>(*shared_triangles)
			                                       : std::span<const glm::vec2>(triangles);
			if (vertices.empty())
				return;
			std::optional<gradient_paint> gradient;
			const ::mngl::color* effective_paint = &paint;
			if (!gradient_id.empty())
			{
				const auto definition = gradients.find(gradient_id);
				if (definition == gradients.end())
				{
					if (!fallback.provided)
					{
						throw std::invalid_argument("SVG paint references an unknown gradient id '#" + gradient_id + "'");
					}
				}
				else
				{
					gradient = shape_gradient(gradients, gradient_id, bounds, paint_view_box(style, document.view_box));
				}
				if (!gradient)
				{
					if (!fallback.provided)
					{
						return; // An unusable objectBoundingBox paint has no area on this geometry.
					}
					if (fallback.none)
					{
						return;
					}
					effective_paint = &fallback.color;
				}
			}
			const auto tint = multiply_alpha(gradient ? ::mngl::color(1.0f) : *effective_paint,
			                                 style.opacity * opacity);
			document.geometry.push_back({std::move(triangles),
			                             tint,
			                             style.transform,
			                             std::move(gradient),
			                             svg_element,
			                             {},
			                             {},
			                             bounds});
			document.geometry.back().shared_triangles = std::move(shared_triangles);
			document.geometry.back().pointer_events = style.pointer_events;
		};

		if (style.fill_enabled)
		{
			std::vector<std::span<const glm::vec2>> rings;
			rings.reserve(contours.size());
			for (const auto& path : contours)
			{
				if (path.points.size() >= 3)
				{
					rings.emplace_back(path.points);
				}
			}
			add_paint([&] { return triangulate_compound(rings, style.even_odd); }, 0, style.fill, style.fill_opacity,
			          style.fill_gradient_id, style.fill_gradient_fallback);
		}

		if (style.stroke_enabled && style.stroke_width > 0.0f)
		{
			const auto create_stroke = [&]
			{
				std::size_t generated_stroke_segments = 0;
				std::vector<glm::vec2> triangles;
				for (const auto& path : contours)
				{
					const auto strokes =
					    make_stroke_paths(path, style, max_curve_segments, generated_stroke_segments);
					for (const auto& stroke : strokes)
					{
						append_stroke_geometry(triangles, stroke, style, curve_tolerance, max_curve_segments,
						                       generated_stroke_segments);
					}
				}
				union_stroke_triangles(triangles);
				return triangles;
			};
			add_paint(create_stroke, 1, style.stroke, style.stroke_opacity, style.stroke_gradient_id,
			          style.stroke_gradient_fallback);
		}
	}

	[[nodiscard]] inline double contour_length(const contour& path)
	{
		double length = 0.0;
		if (path.points.size() < 2)
		{
			return length;
		}
		for (std::size_t index = 1; index < path.points.size(); ++index)
		{
			const auto delta = path.points[index] - path.points[index - 1];
			length += std::hypot(static_cast<double>(delta.x), static_cast<double>(delta.y));
		}
		if (path.closed)
		{
			const auto delta = path.points.front() - path.points.back();
			length += std::hypot(static_cast<double>(delta.x), static_cast<double>(delta.y));
		}
		return length;
	}


	[[nodiscard]] inline std::vector<float> parse_view_box(std::string_view value)
	{
		auto numbers = parse_number_list(value);
		if (numbers.size() != 4 || numbers[2] < 0.0f || numbers[3] < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument(
			    "SVG viewBox must contain min-x, min-y, and non-negative width and height");
		}
		return numbers;
	}

	struct view_tessellation_context
	{
		float curve_tolerance_pixels = 0.25f;
		double pixels_per_world_unit = 1.0;
		glm::vec2 viewport_size{300.0f, 150.0f};
		glm::mat4 object_transform{1.0f};
	};

	[[nodiscard]] inline double maximum_2d_scale(const glm::mat4& transform) noexcept
	{
		const double a = transform[0][0];
		const double b = transform[1][0];
		const double c = transform[0][1];
		const double d = transform[1][1];
		const double squared_sum = a * a + b * b + c * c + d * d;
		const double determinant = a * d - b * c;
		const double discriminant =
		    std::max(0.0, squared_sum * squared_sum - 4.0 * determinant * determinant);
		return std::sqrt(0.5 * (squared_sum + std::sqrt(discriminant)));
	}

	[[nodiscard]] inline float curve_tolerance_for_element(float default_tolerance,
	                                                       const view_tessellation_context* view_context,
	                                                       const glm::mat4& element_transform)
	{
		if (!view_context)
		{
			return default_tolerance;
		}

		const double transform_scale = maximum_2d_scale(view_context->object_transform * element_transform);
		if (!std::isfinite(transform_scale)) [[unlikely]]
		{
			throw std::overflow_error("SVG view transform exceeds the supported numeric range");
		}
		if (transform_scale == 0.0)
		{
			return default_tolerance;
		}

		const double scale = view_context->pixels_per_world_unit * transform_scale;
		const double tolerance = static_cast<double>(view_context->curve_tolerance_pixels) / scale;
		if (!std::isfinite(tolerance) || tolerance <= 0.0) [[unlikely]]
		{
			throw std::overflow_error("SVG view-aware curve tolerance exceeds the supported numeric range");
		}
		return static_cast<float>(std::clamp(tolerance,
		                                     static_cast<double>(std::numeric_limits<float>::min()),
		                                     static_cast<double>(std::numeric_limits<float>::max())));
	}

	using reusable_elements = std::unordered_map<std::string, const xml_node*>;

	inline void collect_reusable_elements(const xml_node& node, reusable_elements& definitions)
	{
		if (!is_svg_element(node.tag))
			return;
		if (const auto id = attribute_value(node.tag, "id"))
		{
			if (!definitions.emplace(std::string(*id), &node).second) [[unlikely]]
			{
				throw std::invalid_argument("SVG element ids must be unique");
			}
		}
		for (const auto& child : node.children)
		{
			collect_reusable_elements(child, definitions);
		}
	}

	[[nodiscard]] inline std::string parse_use_reference(std::string_view value)
	{
		value = trim(value);
		if (value.size() < 2 || value.front() != '#' ||
		    value.find_first_of(" \t\r\n") != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument("SVG <use> supports local references of the form href=\"#id\"");
		}
		return std::string(value.substr(1));
	}

	[[nodiscard]] inline float viewport_length(std::string_view value, double extent)
	{
		const auto parsed = parse_gradient_coordinate(value);
		const double length = parsed.percentage ? extent * (parsed.value / 100.0) : parsed.value;
		if (std::abs(length) > std::numeric_limits<float>::max())
		{
			throw std::invalid_argument("SVG percentage length exceeds the supported coordinate range");
		}
		return static_cast<float>(length);
	}

	[[nodiscard]] inline std::vector<contour> parse_shape_contours(const xml_tag& tag, std::string_view name,
	                                                               float curve_tolerance,
	                                                               std::size_t max_curve_segments,
	                                                               glm::vec4 view_box, bool record_markers,
	                                                               const std::unordered_map<std::string,
	                                                                                        css_cascaded_value>&
	                                                                   properties)
	{
		const auto value_for = [&](std::string_view property) -> std::optional<std::string_view>
		{
			const auto found = properties.find(std::string(property));
			return found == properties.end() ? std::nullopt
			                                 : std::optional<std::string_view>(found->second.value);
		};
		const auto length = [&](std::string_view property, double reference)
		{
			const auto value = value_for(property);
			if (value && lower_ascii(trim(*value)) == "auto")
				return 0.0f;
			return value ? viewport_length(*value, reference) : 0.0f;
		};
		std::vector<contour> contours;
		if (name == "path")
		{
			auto data = value_for("d");
			if (!data || lower_ascii(trim(*data)) == "none")
			{
				return contours;
			}
			auto path_data = trim(*data);
			if (path_data.size() >= 5 && lower_ascii(path_data.substr(0, 5)) == "path(")
			{
				if (!path_data.ends_with(')')) [[unlikely]]
				{
					throw std::invalid_argument("SVG CSS d property has an unterminated path() value");
				}
				path_data = trim(path_data.substr(5, path_data.size() - 6));
				if (path_data.size() < 2 ||
				    !((path_data.front() == '"' && path_data.back() == '"') ||
				      (path_data.front() == '\'' && path_data.back() == '\''))) [[unlikely]]
				{
					throw std::invalid_argument("SVG CSS d property path() requires a quoted path string");
				}
				path_data = path_data.substr(1, path_data.size() - 2);
			}
			contours = parse_path(path_data, curve_tolerance, max_curve_segments, record_markers);
		}
		else if (name == "rect")
		{
			const float x = length("x", view_box.z);
			const float y = length("y", view_box.w);
			const float width = length("width", view_box.z);
			const float height = length("height", view_box.w);
			const auto rx_attribute = value_for("rx");
			const auto ry_attribute = value_for("ry");
			const auto radius = [&](std::optional<std::string_view> value, float reference)
			{
				if (!value || lower_ascii(trim(*value)) == "auto")
					return 0.0f;
				const auto parsed = parse_gradient_coordinate(*value);
				const double resolved = parsed.percentage ? reference * (parsed.value / 100.0) : parsed.value;
				if (resolved > std::numeric_limits<float>::max())
					throw std::invalid_argument("SVG rectangle radius exceeds the supported coordinate range");
				return static_cast<float>(resolved);
			};
			float rx = radius(rx_attribute, width);
			float ry = radius(ry_attribute, height);
			const bool rx_auto = !rx_attribute || lower_ascii(trim(*rx_attribute)) == "auto";
			const bool ry_auto = !ry_attribute || lower_ascii(trim(*ry_attribute)) == "auto";
			if (rx_auto && !ry_auto)
			{
				rx = ry;
			}
			if (ry_auto && !rx_auto)
			{
				ry = rx;
			}
			auto points =
			    rounded_rectangle_points(x, y, width, height, rx, ry, curve_tolerance, max_curve_segments);
			if (!points.empty())
			{
				contours.push_back({std::move(points), true});
				if (record_markers && rx > 0 && ry > 0)
				{
					rx = std::min(rx, width * 0.5f);
					ry = std::min(ry, height * 0.5f);
					const auto x2 = x + width;
					const auto y2 = y + height;
					contours.back().marker_vertices = {
					    {{x + rx, y}, {}, {1, 0}},         {{x2 - rx, y}, {1, 0}, {1, 0}},
					    {{x2, y + ry}, {0, 1}, {0, 1}},    {{x2, y2 - ry}, {0, 1}, {0, 1}},
					    {{x2 - rx, y2}, {-1, 0}, {-1, 0}}, {{x + rx, y2}, {-1, 0}, {-1, 0}},
					    {{x, y2 - ry}, {0, -1}, {0, -1}},  {{x, y + ry}, {0, -1}, {0, -1}},
					    {{x + rx, y}, {1, 0}, {}}};
				}
			}
		}
		else if (name == "circle" || name == "ellipse")
		{
			const float cx = length("cx", view_box.z);
			const float cy = length("cy", view_box.w);
			float rx = 0.0f;
			float ry = 0.0f;
			if (name == "circle")
			{
				rx = ry = length("r", std::hypot(static_cast<double>(view_box.z), view_box.w) /
				                          std::numbers::sqrt2);
			}
			else
			{
				const auto rx_value = value_for("rx");
				const auto ry_value = value_for("ry");
				const bool rx_auto = !rx_value || lower_ascii(trim(*rx_value)) == "auto";
				const bool ry_auto = !ry_value || lower_ascii(trim(*ry_value)) == "auto";
				rx = rx_auto ? 0.0f : viewport_length(*rx_value, view_box.z);
				ry = ry_auto ? 0.0f : viewport_length(*ry_value, view_box.w);
				if (rx_auto && !ry_auto)
					rx = ry;
				if (ry_auto && !rx_auto)
					ry = rx;
			}
			auto points = ellipse_points(cx, cy, rx, ry, curve_tolerance, max_curve_segments);
			if (!points.empty())
			{
				contours.push_back({std::move(points), true});
				if (record_markers)
				{
					contours.back().marker_vertices = {{{cx + rx, cy}, {}, {0, 1}},
					                                   {{cx, cy + ry}, {-1, 0}, {-1, 0}},
					                                   {{cx - rx, cy}, {0, -1}, {0, -1}},
					                                   {{cx, cy - ry}, {1, 0}, {1, 0}},
					                                   {{cx + rx, cy}, {0, 1}, {}}};
				}
			}
		}
		else if (name == "polygon" || name == "polyline")
		{
			const auto points = value_for("points");
			if (!points) [[unlikely]]
			{
				throw std::invalid_argument("SVG polygon or polyline is missing its points attribute");
			}
			auto parsed_points = parse_points(*points);
			contours.push_back({std::move(parsed_points), name == "polygon"});
		}
		else if (name == "line")
		{
			const float x1 = length("x1", view_box.z);
			const float y1 = length("y1", view_box.w);
			const float x2 = length("x2", view_box.z);
			const float y2 = length("y2", view_box.w);
			contours.push_back({{{x1, y1}, {x2, y2}}, false});
		}
		else if (name == "text" || name == "foreignObject")
		{
			throw std::invalid_argument("SVG text and foreignObject elements are not supported");
		}
		else
		{
			throw std::invalid_argument("Unsupported SVG element <" + std::string(name) + ">");
		}
		if (record_markers && name != "path")
		{
			for (auto& path : contours)
			{
				if (!path.marker_vertices.empty() || path.points.empty())
					continue;
				auto& vertices = path.marker_vertices;
				for (const auto point : path.points)
				{
					if (!vertices.empty())
						vertices.back().outgoing = point - vertices.back().point;
					vertices.push_back(
					    {point, vertices.empty() ? glm::vec2{} : vertices.back().outgoing, {}});
				}
				if (path.closed && vertices.size() > 1)
				{
					const auto first = vertices.front().point;
					vertices.back().outgoing = first - vertices.back().point;
					vertices.push_back({first, vertices.back().outgoing, {}});
				}
			}
		}
		return contours;
	}

	[[nodiscard]] inline float length_attribute(const xml_tag& tag, std::string_view name, float fallback = 0)
	{
		const auto value = attribute_value(tag, name);
		return value ? parse_length(*value) : fallback;
	}

	struct viewport_fit
	{
		glm::vec2 scale{1.0f};
		glm::vec2 offset{};
	};

	[[nodiscard]] inline viewport_fit fit_viewport(glm::vec2 content, glm::vec2 viewport,
	                                               std::string_view aspect_ratio)
	{
		if (content.x == 0.0f || content.y == 0.0f)
			return {glm::vec2(0.0f), glm::vec2(0.0f)};
		glm::vec2 scale = viewport / content;
		aspect_ratio = trim(aspect_ratio);
		if (aspect_ratio.starts_with("defer "))
		{
			aspect_ratio = trim(aspect_ratio.substr(6));
		}
		const auto split = aspect_ratio.find_first_of(" \t\r\n");
		const auto align = aspect_ratio.substr(0, split);
		const auto mode =
		    split == std::string_view::npos ? std::string_view{} : trim(aspect_ratio.substr(split));
		if (!mode.empty() && mode != "meet" && mode != "slice")
		{
			throw std::invalid_argument("SVG preserveAspectRatio must use meet or slice");
		}
		if (align == "none")
		{
			return {scale, {}};
		}
		glm::vec2 alignment(0.5f);
		if (!align.empty())
		{
			const auto axis = [](std::string_view value)
			{
				if (value == "Min")
					return 0.0f;
				if (value == "Mid")
					return 0.5f;
				if (value == "Max")
					return 1.0f;
				throw std::invalid_argument("Invalid SVG preserveAspectRatio alignment");
			};
			if (align.size() != 8 || align[0] != 'x' || align[4] != 'Y')
			{
				throw std::invalid_argument("Invalid SVG preserveAspectRatio alignment");
			}
			alignment = {axis(align.substr(1, 3)), axis(align.substr(5, 3))};
		}
		scale = glm::vec2(mode == "slice" ? std::max(scale.x, scale.y) : std::min(scale.x, scale.y));
		return {scale, (viewport - scale * content) * alignment};
	}

	[[nodiscard]] inline glm::vec4 paint_view_box(const paint_state& style, glm::vec4 fallback) noexcept
	{
		const auto size = style.viewport_size.value_or(glm::vec2(fallback.z, fallback.w));
		return {style.viewport_origin.x, style.viewport_origin.y, size.x, size.y};
	}

	[[nodiscard]] inline bool has_area(rectangle bounds) noexcept
	{
		return bounds.size.x > 0 && bounds.size.y > 0;
	}

	[[nodiscard]] inline std::vector<glm::vec2> quad_triangles(rectangle bounds)
	{
		const auto minimum = bounds.pos;
		const auto maximum = bounds.pos + bounds.size;
		const glm::vec2 top_right{maximum.x, minimum.y};
		const glm::vec2 bottom_left{minimum.x, maximum.y};
		return {minimum, top_right, maximum, minimum, maximum, bottom_left};
	}

	[[nodiscard]] inline bool object_units(const xml_tag& tag, std::string_view name, bool fallback)
	{
		if (const auto units = attribute_value(tag, name))
		{
			if (*units == "objectBoundingBox")
				return true;
			if (*units == "userSpaceOnUse")
				return false;
			throw std::invalid_argument("Invalid coordinate units in SVG " + std::string(name));
		}
		return fallback;
	}

	[[nodiscard]] inline rectangle effect_region(const xml_tag& tag, rectangle box, glm::vec4 view_box,
	                                             bool in_object_units,
	                                             std::optional<rectangle> fallback = std::nullopt)
	{
		const auto coordinate = [&](std::string_view name, int axis, bool size, float default_percentage)
		{
			const auto value = attribute_value(tag, name);
			if (!value && fallback)
			{
				return size ? fallback->size[axis] : fallback->pos[axis];
			}
			const auto parsed =
			    value ? parse_gradient_coordinate(*value) : gradient_coordinate{default_percentage, true};
			if (in_object_units)
			{
				return (size ? 0.0f : box.pos[axis]) +
				       box.size[axis] * (parsed.percentage ? parsed.value / 100.0f : parsed.value);
			}
			return parsed.percentage
			           ? (size ? 0.0f : view_box[axis]) + view_box[axis + 2] * parsed.value / 100.0f
					   : parsed.value;
		};
		const rectangle result{coordinate("x", 0, false, -10), coordinate("y", 1, false, -10),
		                       coordinate("width", 0, true, 120), coordinate("height", 1, true, 120)};
		if (!::mngl::detail::finite(result.pos) || !::mngl::detail::finite(result.pos + result.size) ||
		    result.size.x < 0 || result.size.y < 0)
		{
			throw std::invalid_argument(
			    "SVG effect region must have finite coordinates and nonnegative size");
		}
		return result;
	}

	[[nodiscard]] inline std::string_view css_value(
	    const std::unordered_map<std::string, css_cascaded_value>& properties, const char* name,
	    std::string_view default_value = {})
	{
		const auto property = properties.find(name);
		return property == properties.end() ? default_value : trim(property->second.value);
	}

	[[nodiscard]] inline std::string inherited_css_value(const xml_tag& tag, const css_stylesheet& stylesheet,
	                                                      std::string_view property, std::string_view fallback)
	{
		std::vector<const xml_tag*> ancestry;
		for (const xml_tag* current = &tag; current; current = current->parent)
			ancestry.push_back(current);
		std::string result(fallback);
		for (auto current = ancestry.rbegin(); current != ancestry.rend(); ++current)
		{
			const auto& properties = resolve_css_properties(**current, stylesheet);
			if (const auto found = properties.find(std::string(property)); found != properties.end() &&
			    lower_ascii(trim(found->second.value)) != "inherit")
			{
				result = std::string(trim(found->second.value));
			}
		}
		return result;
	}

	[[nodiscard]] inline glm::vec4 flood_color(const xml_tag& tag, const css_stylesheet& sheet,
	                                           bool linear_rgb)
	{
		const auto& props = resolve_css_properties(tag, sheet);
		const auto current_color = computed_style_at(tag, sheet).current_color;
		const auto flood = css_value(props, "flood-color", "black");
		auto rgba = lower_ascii(flood) == "currentcolor" ? current_color.rgba_glsl() : parse_color(flood).rgba_glsl();
		rgba.a *= parse_opacity(css_value(props, "flood-opacity", "1"));
		if (linear_rgb)
		{
			for (int i = 0; i < 3; ++i)
			{
				rgba[i] =
				    rgba[i] <= 0.04045f ? rgba[i] / 12.92f : std::pow((rgba[i] + 0.055f) / 1.055f, 2.4f);
			}
		}
		return rgba;
	}

	inline void color_matrix_parameters(filter_step& step, const xml_tag& tag)
	{
		const auto type = attribute_value(tag, "type").value_or("matrix");
		const auto values = attribute_value(tag, "values");
		const auto entries = values ? parse_number_list(*values) : std::vector<float>{};
		if (type == "matrix")
		{
			if (values && entries.size() != 20)
			{
				throw std::invalid_argument("SVG feColorMatrix requires 20 matrix values");
			}
			for (int row = 0; row < 4; ++row)
			{
				for (int column = 0; column < 4; ++column)
				{
					step.matrix[row][column] = values ? entries[row * 5 + column] : float(row == column);
				}
				step.matrix[4][row] = values ? entries[row * 5 + 4] : 0.0f;
			}
			return;
		}
		if (type == "luminanceToAlpha")
		{
			step.matrix[3] = {0.2125f, 0.7154f, 0.0721f, 0};
			return;
		}
		if ((type != "saturate" && type != "hueRotate") || (values && entries.size() != 1))
		{
			throw std::invalid_argument("Invalid SVG feColorMatrix type or value");
		}
		step.matrix[3].a = 1.0f;
		if (type == "saturate")
		{
			const float saturation = entries.empty() ? 1.0f : entries[0];
			const glm::vec4 luma{0.213f, 0.715f, 0.072f, 0};
			for (int row = 0; row < 3; ++row)
			{
				step.matrix[row] = luma * (1 - saturation);
				step.matrix[row][row] += saturation;
			}
		}
		else
		{
			const float angle = glm::radians(entries.empty() ? 0.0f : entries[0]);
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			const glm::vec4 luma{0.213f, 0.715f, 0.072f, 0};
			step.matrix[0] = luma + glm::vec4(0.787f, -0.715f, -0.072f, 0) * c +
			                 glm::vec4(-0.213f, -0.715f, 0.928f, 0) * s;
			step.matrix[1] =
			    luma + glm::vec4(-0.213f, 0.285f, -0.072f, 0) * c + glm::vec4(0.143f, 0.140f, -0.283f, 0) * s;
			step.matrix[2] =
			    luma + glm::vec4(-0.213f, -0.715f, 0.928f, 0) * c + glm::vec4(-0.787f, 0.715f, 0.072f, 0) * s;
		}
	}

	[[nodiscard]] inline filter_data parse_filter(const xml_node& node, rectangle bounds, glm::vec4 view_box,
	                                              const css_stylesheet& sheet)
	{
		const bool primitive_object_units = object_units(node.tag, "primitiveUnits", false);
		const auto scale = primitive_object_units ? bounds.size : glm::vec2(1);
		const auto filter_interpolation = lower_ascii(
		    inherited_css_value(node.tag, sheet, "color-interpolation-filters", "linearRGB"));
		if (filter_interpolation != "linearrgb" && filter_interpolation != "srgb")
			throw std::invalid_argument("Invalid SVG color-interpolation-filters value");
		filter_data filter{
		    effect_region(node.tag, bounds, view_box, object_units(node.tag, "filterUnits", true)),
		    filter_interpolation == "linearrgb",
		    {}};
		std::unordered_map<std::string, std::size_t> named{{"SourceGraphic", 0}, {"SourceAlpha", 1}};
		std::vector<rectangle> output_regions{bounds, bounds};
		std::size_t previous = 0;
		const auto region_for_input = [&](std::size_t index) -> rectangle
		{
			return index < output_regions.size() ? output_regions[index] : filter.region;
		};
		const auto default_region_for_input = [&](std::size_t index) -> rectangle
		{
			// SourceGraphic and SourceAlpha are standard inputs. Their default
			// primitive subregion is the full filter region, not the source bounds.
			return index < 2 ? filter.region : region_for_input(index);
		};
		const auto unite_regions = [](rectangle first, rectangle second)
		{
			const bool first_has_area = has_area(first);
			const bool second_has_area = has_area(second);
			if (!first_has_area)
				return second_has_area ? second : first;
			if (!second_has_area)
				return first;
			const auto minimum = glm::min(first.pos, second.pos);
			const auto maximum = glm::max(first.pos + first.size, second.pos + second.size);
			return rectangle{minimum, maximum - minimum};
		};
		const auto input_index = [&](const xml_tag& tag, std::string_view attribute, std::size_t fallback)
		{
			if (const auto value = attribute_value(tag, attribute))
			{
				const auto found = named.find(std::string(trim(*value)));
				if (found == named.end())
				{
					throw std::invalid_argument("Unknown SVG filter input '" + std::string(*value) + "'");
				}
				return found->second;
			}
			return fallback;
		};
		const auto add_step = [&](const filter_step& step)
		{
			if (filter.steps.size() >= 128)
			{
				throw std::length_error("SVG filter exceeds 128 steps");
			}
			filter.steps.push_back(step);
			output_regions.push_back(step.region.value_or(filter.region));
			return filter.steps.size() + 1;
		};
		const auto enum_index = [](std::string_view value, std::initializer_list<std::string_view> choices)
		{
			const auto match = std::ranges::find(choices, value);
			if (match == choices.end())
			{
				throw std::invalid_argument("Unsupported SVG filter operator '" + std::string(value) + "'");
			}
			return static_cast<std::uint32_t>(match - choices.begin());
		};
		for (const auto& child : node.children)
		{
			const auto& tag = child.tag;
			if (!is_svg_element(tag))
				continue;
			const auto name = local_name(tag.name);
			if (name == "desc" || name == "title" || name == "metadata" || name == "style")
				continue;
			filter_step step;
			step.input = name == "feFlood" ? 0 : input_index(tag, "in", previous);
			const auto interpolation = lower_ascii(inherited_css_value(
			    tag, sheet, "color-interpolation-filters", filter.linear_rgb ? "linearrgb" : "srgb"));
			if (interpolation != "linearrgb" && interpolation != "srgb")
			{
				throw std::invalid_argument("Invalid SVG color-interpolation-filters value");
			}
			step.linear_rgb = interpolation == "linearrgb";
			auto default_region = default_region_for_input(step.input);
			if (name == "feFlood")
				default_region = filter.region;
			if (name == "feGaussianBlur" || name == "feDropShadow")
			{
				const bool shadow = name == "feDropShadow";
				auto deviations =
				    parse_number_list(attribute_value(tag, "stdDeviation").value_or(shadow ? "2" : "0"));
				if (deviations.empty() || deviations.size() > 2 ||
				    std::ranges::any_of(deviations, [](float x) { return x < 0; }))
				{
					throw std::invalid_argument("SVG stdDeviation requires one or two nonnegative numbers");
				}
				step.operation = shadow ? filter_op::shadow : filter_op::blur;
				if (!shadow)
				{
					step.mode = enum_index(attribute_value(tag, "edgeMode").value_or("none"),
					                       {"none", "duplicate", "wrap"});
				}
				step.parameters = {deviations.front() * scale.x, deviations.back() * scale.y,
				                   shadow ? length_attribute(tag, "dx", 2) * scale.x : 0,
				                   shadow ? length_attribute(tag, "dy", 2) * scale.y : 0};
				if (shadow)
					step.matrix[0] = flood_color(tag, sheet, step.linear_rgb);
			}
			else if (name == "feOffset")
			{
				step.operation = filter_op::offset;
				step.parameters = {length_attribute(tag, "dx") * scale.x,
				                   length_attribute(tag, "dy") * scale.y, 0, 0};
			}
			else if (name == "feColorMatrix")
			{
				step.operation = filter_op::color_matrix;
				color_matrix_parameters(step, tag);
			}
			else if (name == "feFlood")
			{
				step.operation = filter_op::flood;
				step.parameters = flood_color(tag, sheet, step.linear_rgb);
			}
			else if (name == "feComposite")
			{
				step.operation = filter_op::composite;
				step.input2 = input_index(tag, "in2", 0);
				default_region = unite_regions(default_region, default_region_for_input(step.input2));
				step.mode = enum_index(attribute_value(tag, "operator").value_or("over"),
				                       {"over", "in", "out", "atop", "xor", "arithmetic"});
				if (step.mode == 5)
				{
					step.parameters = {length_attribute(tag, "k1"), length_attribute(tag, "k2"),
					                   length_attribute(tag, "k3"), length_attribute(tag, "k4")};
				}
			}
			else if (name == "feBlend")
			{
				step.operation = filter_op::blend;
				step.input2 = input_index(tag, "in2", 0);
				default_region = unite_regions(default_region, default_region_for_input(step.input2));
				step.mode = enum_index(attribute_value(tag, "mode").value_or("normal"),
				                       {"normal", "multiply", "screen", "darken", "lighten"});
			}
			else if (name == "feMerge")
			{
				std::optional<std::size_t> merged;
				std::optional<rectangle> merged_region;
				for (const auto& layer : child.children)
				{
					if (!is_svg_element(layer.tag))
						continue;
					if (local_name(layer.tag.name) != "feMergeNode")
					{
						throw std::invalid_argument("SVG feMerge expects feMergeNode children");
					}
					const auto input = input_index(layer.tag, "in", previous);
					const auto input_region = default_region_for_input(input);
					merged_region = merged_region ? unite_regions(*merged_region, input_region) : input_region;
					if (merged)
					{
						filter_step composite{
						    .operation = filter_op::composite, .input = input, .input2 = *merged,
						    .region = merged_region, .linear_rgb = step.linear_rgb};
						merged = add_step(composite);
					}
					else
					{
						merged = input;
					}
				}
				step = {.operation = merged ? filter_op::copy : filter_op::flood,
				        .input = merged.value_or(0),
				        .region = merged_region,
				        .linear_rgb = step.linear_rgb};
				if (merged_region)
					default_region = *merged_region;
			}
			else
			{
				throw std::invalid_argument("Unsupported SVG filter primitive <" + std::string(name) + ">");
			}
			step.region = effect_region(tag, bounds, view_box, primitive_object_units, default_region);
			if (!::mngl::detail::finite(glm::vec2(step.parameters)) ||
			    !::mngl::detail::finite(glm::vec2(step.parameters.z, step.parameters.w)))
			{
				throw std::invalid_argument("SVG filter parameters overflow");
			}
			previous = add_step(step);
			if (const auto result = attribute_value(tag, "result"); result && !trim(*result).empty())
			{
				if (trim(*result) == "SourceGraphic" || trim(*result) == "SourceAlpha")
				{
					throw std::invalid_argument("SVG filter result cannot replace a built-in input");
				}
				named.insert_or_assign(std::string(trim(*result)), previous);
			}
		}
		return filter;
	}

	struct parse_resources
	{
		const gradient_definitions& gradients;
		const css_stylesheet& stylesheet;
		const reusable_elements& definitions;
		const std::filesystem::path& image_directory;
		const view_tessellation_context* view_context;
		float curve_tolerance;
		std::size_t max_curve_segments;
		std::unordered_set<std::string> active_references;
		std::unordered_map<const xml_node*, std::shared_ptr<const image_pixels>> images;
		std::unordered_map<contour_cache_key, std::shared_ptr<const std::vector<contour>>,
		                   contour_cache_key_hash> contour_cache;
		std::unordered_map<tessellation_cache_key,
		                   std::array<std::shared_ptr<const std::vector<glm::vec2>>, 2>,
		                   tessellation_cache_key_hash> tessellation_cache;
		decoded_image_cache* decoded_images = nullptr;
		std::size_t next_element_index = 0;
	};

	[[nodiscard]] inline const xml_node& find_definition(const parse_resources& resources,
	                                                     const std::string& id, std::string_view type)
	{
		const auto definition = resources.definitions.find(id);
		if (definition == resources.definitions.end() ||
		    (!type.empty() && local_name(definition->second->tag.name) != type))
		{
			throw std::invalid_argument("Unknown SVG " + std::string(type.empty() ? "<use>" : type) +
			                            " reference '#" + id + "'");
		}
		return *definition->second;
	}

	inline void enter_reference(parse_resources& resources, const std::string& id)
	{
		if (!resources.active_references.emplace(id).second)
		{
			throw std::invalid_argument("Cyclic SVG reference at '#" + id + "'");
		}
	}

	inline std::optional<transformed_bounds> process_svg_element(document_data& document, const xml_node& node,
	                                const paint_state& inherited, parse_resources& resources,
	                                std::vector<geometry_bounds_scope*>& bounds_scopes,
	                                std::size_t depth = 0,
	                                const ::mngl::svg_hit_test_element* marker_owner = nullptr,
	                                bool clip_geometry_mode = false);

	[[nodiscard]] inline rectangle add_image(document_data& document, const xml_node& node,
	                                         const paint_state& style, parse_resources& resources,
	                                         const ::mngl::svg_hit_test_element& element,
	                                         bool emit_paint = true)
	{
		const auto& tag = node.tag;
			const auto& properties = resolve_css_properties(tag, resources.stylesheet);
		const auto href = svg_href_value(tag);
		if (!href || trim(*href).empty())
		{
			throw std::invalid_argument("SVG <image> requires href or xlink:href");
		}
		const auto get_dimension = [&](const char* name, float view_extent) -> std::optional<float>
		{
			if (const auto found = properties.find(name); found != properties.end() &&
			    lower_ascii(trim(found->second.value)) != "auto")
			{
				return viewport_length(found->second.value, view_extent);
			}
			return std::nullopt;
		};
		const auto viewport_size =
		    style.viewport_size.value_or(glm::vec2(document.view_box.z, document.view_box.w));
		const auto width = get_dimension("width", viewport_size.x);
		const auto height = get_dimension("height", viewport_size.y);
		if (width.value_or(0) < 0 || height.value_or(0) < 0)
		{
			throw std::invalid_argument("SVG image dimensions must be nonnegative");
		}
		if ((width && *width == 0) || (height && *height == 0))
		{
			return {};
		}
		auto& image = resources.images[&node];
		if (!image)
		{
			const std::string key(trim(*href));
			if (resources.decoded_images)
			{
				if (const auto found = resources.decoded_images->find(key);
				    found != resources.decoded_images->end())
				{
					image = found->second;
				}
			}
			if (!image)
			{
				image = std::make_shared<const image_pixels>(decode_svg_image(key, resources.image_directory));
				if (resources.decoded_images)
					resources.decoded_images->insert_or_assign(key, image);
			}
		}
		const glm::vec2 intrinsic(image->size);
		const rectangle viewport{
		    {get_dimension("x", viewport_size.x).value_or(0.0f),
	     get_dimension("y", viewport_size.y).value_or(0.0f)},
		    {width.value_or(height ? *height * (intrinsic.x / intrinsic.y) : intrinsic.x),
			 height.value_or(width ? *width * (intrinsic.y / intrinsic.x) : intrinsic.y)}};
		const auto fit =
		    fit_viewport(intrinsic, viewport.size, attribute_value(tag, "preserveAspectRatio").value_or(""));
		const rectangle fitted{viewport.pos + fit.offset, intrinsic * fit.scale};
		const auto cropped = intersection(viewport, fitted);
		if (!::mngl::detail::finite(cropped.pos) || !::mngl::detail::finite(cropped.pos + cropped.size))
		{
			throw std::invalid_argument("SVG image placement must have finite coordinates");
		}
		if (emit_paint && has_area(cropped))
		{
			const auto uv_low = (cropped.pos - fitted.pos) / fitted.size;
			const auto uv_high = (cropped.pos + cropped.size - fitted.pos) / fitted.size;
			document.geometry.push_back(
			    {quad_triangles(cropped),
				 color(1.0f, 1.0f, 1.0f, style.opacity),
				 style.transform,
				 {},
				 element,
				 image_paint{image, cropped, {uv_low.x, uv_high.y, uv_high.x, uv_low.y}},
				 {},
				 viewport});
			document.geometry.back().pointer_events = style.pointer_events;
		}
		return viewport;
	}

	inline void include_bounds(bounds_accumulator& bounds, rectangle rectangle, const glm::mat4& transform)
	{
		const auto end = rectangle.pos + rectangle.size;
		bounds.include(apply_transform(transform, rectangle.pos));
		bounds.include(apply_transform(transform, {end.x, rectangle.pos.y}));
		bounds.include(apply_transform(transform, end));
		bounds.include(apply_transform(transform, {rectangle.pos.x, end.y}));
	}

	inline void wrap_clip_path(document_data& document, std::size_t first, const paint_state& style,
	                           std::optional<rectangle> object_bounds, parse_resources& resources,
	                           std::size_t depth, const ::mngl::svg_hit_test_element* marker_owner);

	inline void wrap_effect(document_data& document, std::size_t first, const paint_state& style,
	                        std::optional<rectangle> object_bounds, parse_resources& resources,
	                        std::size_t depth, const ::mngl::svg_hit_test_element* marker_owner)
	{
		if (!style.clip_path_id.empty())
		{
			auto inner_style = style;
			inner_style.clip_path_id.clear();
			wrap_effect(document, first, inner_style, object_bounds, resources, depth, marker_owner);
			wrap_clip_path(document, first, style, object_bounds, resources, depth, marker_owner);
			return;
		}
		bool opacity_only = style.mask_id.empty() && style.filter_id.empty() && style.opacity < 1.0f;
		if (style.mask_id.empty() && style.filter_id.empty() && !opacity_only)
			return;
		const auto inverse = glm::inverse(style.transform);
		if (!::mngl::detail::finite(inverse))
		{
			document.geometry.resize(first);
			return;
		}
		bounds_accumulator object;
		if (opacity_only)
		{
			for (std::size_t i = first; i < document.geometry.size(); ++i)
			{
				const auto& part = document.geometry[i];
				const auto to_layer = inverse * part.transform;
				if (part.effect)
					include_bounds(object, part.effect->region, to_layer);
				else if (part.image)
					include_bounds(object, part.image->bounds, to_layer);
				else
					for (const auto point : part.triangle_view())
						object.include(apply_transform(to_layer, point));
			}
			if (!object.valid)
				return;
			object_bounds = object.bounds();
		}
		else if (!object_bounds)
		{
			for (std::size_t i = first; i < document.geometry.size(); ++i)
			{
				const auto& part = document.geometry[i];
				if (part.object_bounds)
					include_bounds(object, *part.object_bounds, inverse * part.transform);
			}
			if (!object.valid)
			{
				for (std::size_t i = first; i < document.geometry.size(); ++i)
				{
					const auto& part = document.geometry[i];
					const auto to_layer = inverse * part.transform;
					if (part.effect)
						include_bounds(object, part.effect->region, to_layer);
					else
						for (const auto point : part.triangle_view())
							object.include(apply_transform(to_layer, point));
				}
			}
		}
		if (!object_bounds && !object.valid)
			return;
		auto bounds = object_bounds.value_or(object.bounds());
		const auto* filter =
		    style.filter_id.empty() ? nullptr : &find_definition(resources, style.filter_id, "filter");
		const auto* mask_definition =
		    style.mask_id.empty() ? nullptr : &find_definition(resources, style.mask_id, "mask");
		const auto current_view_box = paint_view_box(style, document.view_box);
		if (!has_area(bounds))
		{
			if (filter && object_units(filter->tag, "filterUnits", true))
				filter = nullptr;
			if (mask_definition && (object_units(mask_definition->tag, "maskUnits", true) ||
			                        object_units(mask_definition->tag, "maskContentUnits", false)))
				mask_definition = nullptr;
		}
		if (!filter && !mask_definition && !opacity_only)
		{
			if (style.opacity >= 1.0f)
				return;
			bounds_accumulator painted;
			for (std::size_t i = first; i < document.geometry.size(); ++i)
			{
				const auto& part = document.geometry[i];
				const auto to_layer = inverse * part.transform;
				if (part.effect)
					include_bounds(painted, part.effect->region, to_layer);
				else if (part.image)
					include_bounds(painted, part.image->bounds, to_layer);
				else
					for (const auto point : part.triangle_view())
						painted.include(apply_transform(to_layer, point));
			}
			if (!painted.valid)
				return;
			bounds = painted.bounds();
			opacity_only = true;
		}
		auto layer = std::make_shared<effect_layer>();
		if (opacity_only)
			layer->region = bounds;
		if (filter)
		{
			layer->filter = parse_filter(*filter, bounds, current_view_box, resources.stylesheet);
			layer->region = layer->filter->region;
		}
		if (mask_definition)
		{
			const auto& definition = *mask_definition;
			const auto& props = resolve_css_properties(definition.tag, resources.stylesheet);
			const auto mask_type = lower_ascii(css_value(props, "mask-type", "luminance"));
			if (mask_type != "alpha" && mask_type != "luminance")
			{
				throw std::invalid_argument("SVG mask-type must be alpha or luminance");
			}
			layer->mask =
			    mask_data{effect_region(definition.tag, bounds, current_view_box,
				                        object_units(definition.tag, "maskUnits", true)),
				          mask_type == "alpha" ? mask_channel::alpha
				                               : (css_value(props, "color-interpolation") == "linearRGB"
			                                          ? mask_channel::linear_luminance
			                                          : mask_channel::luminance)};
			if (!layer->filter)
				layer->region = layer->mask->region;
			if (has_area(intersection(layer->region, layer->mask->region)))
			{
				paint_state mask_style = computed_style_at(definition.tag, resources.stylesheet);
				mask_style.transform = style.transform;
				document_data mask;
				mask.view_box = current_view_box;
				if (object_units(definition.tag, "maskContentUnits", false))
				{
					mask_style.transform = glm::scale(
					    glm::translate(style.transform, glm::vec3(bounds.pos, 0)), glm::vec3(bounds.size, 1));
					mask.view_box = {0, 0, 1, 1};
				}
				if (mask_style.display)
				{
					enter_reference(resources, style.mask_id);
					std::vector<geometry_bounds_scope*> mask_bounds_scopes;
					for (const auto& child : definition.children)
					{
						process_svg_element(mask, child, mask_style, resources, mask_bounds_scopes,
						                    depth + 1, marker_owner);
					}
					resources.active_references.erase(style.mask_id);
				}
				for (auto& part : mask.geometry)
					part.transform = inverse * part.transform;
				layer->mask_source = std::move(mask.geometry);
			}
		}
		layer->source.reserve(document.geometry.size() - first);
		for (std::size_t i = first; i < document.geometry.size(); ++i)
		{
			auto part = std::move(document.geometry[i]);
			part.transform = inverse * part.transform;
			layer->source.push_back(std::move(part));
		}
		document.geometry.resize(first);
		if (has_area(layer->region))
		{
			document.geometry.push_back({{},
			                             color(1.0f, 1.0f, 1.0f, style.opacity),
			                             style.transform,
			                             {},
			                             {},
			                             {},
			                             std::move(layer),
			                             bounds});
		}
	}

	inline void wrap_clip_path(document_data& document, std::size_t first, const paint_state& style,
	                           std::optional<rectangle> object_bounds, parse_resources& resources,
	                           std::size_t depth, const ::mngl::svg_hit_test_element* marker_owner)
	{
		const auto& definition = find_definition(resources, style.clip_path_id, "clipPath");
		const bool object_bounding_box = object_units(definition.tag, "clipPathUnits", false);
		const auto inverse = glm::inverse(style.transform);
		bounds_accumulator content_bounds;
		if (::mngl::detail::finite(inverse))
		{
			for (std::size_t i = first; i < document.geometry.size(); ++i)
			{
				const auto& part = document.geometry[i];
				const auto to_layer = inverse * part.transform;
				if (part.effect)
					include_bounds(content_bounds, part.effect->region, to_layer);
				else if (part.image)
					include_bounds(content_bounds, part.image->bounds, to_layer);
				else
					for (const auto point : part.triangle_view())
						content_bounds.include(apply_transform(to_layer, point));
			}
		}
		if (!object_bounds && content_bounds.valid)
			object_bounds = content_bounds.bounds();
		if (object_bounding_box && (!object_bounds || !has_area(*object_bounds)))
			throw std::invalid_argument("SVG clipPath with objectBoundingBox requires nonempty geometry bounds");

		if (!::mngl::detail::finite(inverse))
		{
			document.geometry.resize(first);
			return;
		}
		auto layer = std::make_shared<effect_layer>();
		// Keep the object's geometric bounds for objectBoundingBox units, but size the
		// compositing target to the actual paint (strokes and expanded effects included).
		layer->region = content_bounds.valid ? content_bounds.bounds() : object_bounds.value_or(rectangle{});
		layer->mask = mask_data{layer->region, mask_channel::alpha};
		paint_state clip_style = computed_style_at(definition.tag, resources.stylesheet);
		clip_style.transform = style.transform;
		const auto& clip_properties = resolve_css_properties(definition.tag, resources.stylesheet);
		if (const auto transform = clip_properties.find("transform"); transform != clip_properties.end() &&
		    lower_ascii(trim(transform->second.value)) != "none" &&
		    lower_ascii(trim(transform->second.value)) != "inherit")
		{
			clip_style.transform *= parse_transform(transform->second.value);
		}
		if (object_bounding_box)
		{
			clip_style.transform = style.transform;
			clip_style.transform = glm::translate(clip_style.transform,
			                                      glm::vec3(object_bounds->pos, 0.0f));
			clip_style.transform = glm::scale(clip_style.transform,
			                                 glm::vec3(object_bounds->size, 1.0f));
			if (const auto transform = clip_properties.find("transform");
			    transform != clip_properties.end() && lower_ascii(trim(transform->second.value)) != "none" &&
			    lower_ascii(trim(transform->second.value)) != "inherit")
			{
				clip_style.transform *= parse_transform(transform->second.value);
			}
			clip_style.viewport_size = glm::vec2(1.0f);
			clip_style.viewport_origin = {};
		}
		document_data clip_document;
		clip_document.view_box = object_bounding_box ? glm::vec4(0, 0, 1, 1)
		                                                   : paint_view_box(style, document.view_box);
		if (!object_bounding_box)
		{
			clip_style.viewport_size = glm::vec2(clip_document.view_box.z, clip_document.view_box.w);
			clip_style.viewport_origin = glm::vec2(clip_document.view_box.x, clip_document.view_box.y);
		}
		enter_reference(resources, style.clip_path_id);
		if (clip_style.display)
		{
			std::vector<geometry_bounds_scope*> clip_bounds_scopes;
			for (const auto& child : definition.children)
				process_svg_element(clip_document, child, clip_style, resources, clip_bounds_scopes,
				                    depth + 1, marker_owner, true);
		}
		resources.active_references.erase(style.clip_path_id);
		for (auto& part : clip_document.geometry)
		{
			part.transform = inverse * part.transform;
			for (const auto point : part.triangle_view())
				layer->hit_clip_triangles.push_back(apply_transform(part.transform, point));
			layer->mask_source.push_back(std::move(part));
		}
		layer->hit_clip_empty = layer->hit_clip_triangles.empty();
		layer->source.reserve(document.geometry.size() - first);
		for (std::size_t i = first; i < document.geometry.size(); ++i)
		{
			auto part = std::move(document.geometry[i]);
			part.transform = inverse * part.transform;
			layer->source.push_back(std::move(part));
		}
		document.geometry.resize(first);
		if (has_area(layer->region))
			document.geometry.push_back({{}, color(1.0f), style.transform, {}, {}, {}, std::move(layer), object_bounds});
	}

	inline void clip_root_viewport(document_data& document, rectangle clip_region,
	                               const glm::mat4& view_box_to_viewport)
	{
		const auto inverse_viewport = glm::inverse(view_box_to_viewport);
		if (!::mngl::detail::finite(inverse_viewport)) [[unlikely]]
			throw std::invalid_argument("SVG root viewport transform is not invertible");

		bool overflow = false;
		const auto outside = [&](glm::vec2 point)
		{
			const auto local = apply_transform(inverse_viewport, point);
			return local.x < clip_region.pos.x || local.y < clip_region.pos.y ||
			       local.x > clip_region.pos.x + clip_region.size.x ||
			       local.y > clip_region.pos.y + clip_region.size.y;
		};
		for (const auto& part : document.geometry)
		{
			const auto inspect = [&](glm::vec2 point)
			{
				overflow = overflow || outside(apply_transform(part.transform, point));
			};
			if (part.effect)
			{
				const auto end = part.effect->region.pos + part.effect->region.size;
				inspect(part.effect->region.pos);
				inspect({end.x, part.effect->region.pos.y});
				inspect(end);
				inspect({part.effect->region.pos.x, end.y});
			}
			else if (part.image)
			{
				const auto end = part.image->bounds.pos + part.image->bounds.size;
				inspect(part.image->bounds.pos);
				inspect({end.x, part.image->bounds.pos.y});
				inspect(end);
				inspect({part.image->bounds.pos.x, end.y});
			}
			else
			{
				for (const auto point : part.triangle_view())
					inspect(point);
			}
		}
		if (!overflow || !has_area(clip_region))
			return;

		auto layer = std::make_shared<effect_layer>();
		layer->region = clip_region;
		layer->mask = mask_data{clip_region, mask_channel::alpha};
		layer->hit_clip = clip_region;
		layer->mask_source.push_back({.triangles = quad_triangles(clip_region), .tint = color(1.0f)});
		layer->source.reserve(document.geometry.size());
		for (auto& part : document.geometry)
		{
			part.transform = inverse_viewport * part.transform;
			layer->source.push_back(std::move(part));
		}
		document.geometry.clear();
		document.geometry.push_back({.triangles = quad_triangles(clip_region),
		                             .tint = color(1.0f),
		                             .transform = view_box_to_viewport,
		                             .effect = std::move(layer),
		                             .object_bounds = clip_region});
	}

	[[nodiscard]] inline glm::vec2 unit_direction(glm::vec2 value) noexcept
	{
		const auto length = std::hypot(static_cast<double>(value.x), static_cast<double>(value.y));
		return length != 0 && std::isfinite(length) ? glm::vec2(glm::dvec2(value) / length) : glm::vec2(0);
	}

	[[nodiscard]] inline float marker_angle(const std::vector<marker_vertex>& vertices, std::size_t index)
	{
		auto incoming = unit_direction(vertices[index].incoming);
		for (std::size_t i = index; incoming == glm::vec2{} && i > 0; --i)
			incoming = unit_direction(vertices[i - 1].incoming);
		auto outgoing = unit_direction(vertices[index].outgoing);
		for (std::size_t i = index + 1; outgoing == glm::vec2{} && i < vertices.size(); ++i)
			outgoing = unit_direction(vertices[i].outgoing);
		if (incoming == glm::vec2{})
			return std::atan2(outgoing.y, outgoing.x);
		const float angle = std::atan2(incoming.y, incoming.x);
		if (outgoing == glm::vec2{})
			return angle;
		const float difference =
		    std::remainder(std::atan2(outgoing.y, outgoing.x) - angle, 2 * std::numbers::pi_v<float>);
		return angle + difference * 0.5f;
	}

	[[nodiscard]] inline float explicit_marker_angle(std::string_view angle)
	{
		angle = trim(angle);
		if (angle.ends_with("grad"))
			return parse_number(angle.substr(0, angle.size() - 4)) * (std::numbers::pi_v<float> / 200);
		if (angle.ends_with("rad"))
			return parse_number(angle.substr(0, angle.size() - 3));
		if (angle.ends_with("turn"))
			return parse_number(angle.substr(0, angle.size() - 4)) * (2 * std::numbers::pi_v<float>);
		if (angle.ends_with("deg"))
			angle.remove_suffix(3);
		return glm::radians(parse_number(angle));
	}

	// Clip tessellated marker triangles in their original coordinate space so gradients retain their
	// coordinates.
	inline void clip_marker_geometry(geometry_data& part, const glm::mat4& to_viewport, glm::vec2 size)
	{
		part.make_triangles_writable();
		std::vector<glm::vec2> clipped;
		for (std::size_t i = 0; i < part.triangles.size(); i += 3)
		{
			std::array<glm::vec2, 8> polygon{};
			std::copy_n(part.triangles.begin() + i, 3, polygon.begin());
			std::size_t count = 3;
			for (int side = 0; side < 4 && count != 0; ++side)
			{
				const int axis = side / 2;
				const auto distance = [&](glm::vec2 point)
				{
					const float value = apply_transform(to_viewport, point)[axis];
					return side % 2 == 0 ? value : size[axis] - value;
				};
				std::array<glm::vec2, 8> output{};
				std::size_t next_count = 0;
				auto previous = polygon[count - 1];
				auto previous_distance = distance(previous);
				for (std::size_t vertex = 0; vertex < count; ++vertex)
				{
					const auto point = polygon[vertex];
					const float current_distance = distance(point);
					if ((previous_distance < 0 && current_distance > 0) ||
					    (previous_distance > 0 && current_distance < 0))
					{
						const auto weight = static_cast<float>(
						    previous_distance / (static_cast<double>(previous_distance) - current_distance));
						output[next_count++] = glm::mix(previous, point, weight);
					}
					if (current_distance >= 0)
						output[next_count++] = point;
					previous = point;
					previous_distance = current_distance;
				}
				polygon = output;
				count = next_count;
			}
			for (std::size_t j = 1; j + 1 < count; ++j)
				clipped.insert(clipped.end(), {polygon[0], polygon[j], polygon[j + 1]});
		}
		part.triangles = std::move(clipped);
	}

	inline void add_markers(document_data& document, std::span<const contour> contours,
	                        const paint_state& style, const ::mngl::svg_hit_test_element& owner,
	                        parse_resources& resources, std::size_t depth)
	{
		const auto has_vertices = [](const contour& value) { return value.marker_vertices.size() >= 2; };
		const auto first = std::ranges::find_if(contours, has_vertices);
		if (first == contours.end())
			return;
		const auto last = std::find_if(contours.rbegin(), contours.rend(), has_vertices).base() - 1;
		for (auto path = first; path <= last; ++path)
		{
			const auto& vertices = path->marker_vertices;
			if (vertices.size() < 2)
				continue;
			for (std::size_t i = 0; i < vertices.size(); ++i)
			{
				const std::size_t kind =
				    path == first && i == 0 ? 0 : (path == last && i + 1 == vertices.size() ? 2 : 1);
				const auto& id = style.marker_ids[kind];
				if (id.empty())
					continue;
				const auto& definition = find_definition(resources, id, "marker");
				const auto& tag = definition.tag;
				const glm::vec2 size{length_attribute(tag, "markerWidth", 3),
				                     length_attribute(tag, "markerHeight", 3)};
				const auto units = attribute_value(tag, "markerUnits").value_or("strokeWidth");
				if (size.x < 0 || size.y < 0 || (units != "strokeWidth" && units != "userSpaceOnUse"))
					throw std::invalid_argument("Invalid SVG marker dimensions or units");
				const float scale = units == "strokeWidth" ? style.stroke_width : 1;
				if (size.x == 0 || size.y == 0 || scale == 0)
					continue;

				glm::vec4 view_box{0, 0, size.x, size.y};
				if (const auto value = attribute_value(tag, "viewBox"))
				{
					const auto box = parse_view_box(*value);
					view_box = {box[0], box[1], box[2], box[3]};
				}
				if (view_box.z == 0.0f || view_box.w == 0.0f)
					continue;
				const auto fit = fit_viewport(glm::vec2(view_box.z, view_box.w), size,
				                              attribute_value(tag, "preserveAspectRatio").value_or(""));
				const auto reference =
				    [&](const char* name, int axis, std::string_view minimum, std::string_view maximum)
				{
					const auto value = trim(attribute_value(tag, name).value_or("0"));
					if (value == minimum || value == "center" || value == maximum)
						return view_box[axis] +
						       view_box[axis + 2] * (value == minimum ? 0 : (value == maximum ? 1 : 0.5f));
					const auto position = parse_gradient_coordinate(value);
					return position.percentage ? view_box[axis] + view_box[axis + 2] * position.value / 100
					                           : position.value;
				};
				const glm::vec2 origin{view_box.x, view_box.y};
				const glm::vec2 ref{reference("refX", 0, "left", "right"),
				                    reference("refY", 1, "top", "bottom")};
				const auto orient = trim(attribute_value(tag, "orient").value_or("0"));
				float angle = orient == "auto" || orient == "auto-start-reverse"
				                  ? marker_angle(vertices, i)
				                  : explicit_marker_angle(orient);
				if (kind == 0 && orient == "auto-start-reverse")
					angle += std::numbers::pi_v<float>;

				auto viewport = glm::translate(style.transform, glm::vec3(vertices[i].point, 0));
				viewport = glm::rotate(viewport, angle, glm::vec3(0, 0, 1));
				viewport = glm::scale(viewport, glm::vec3(scale, scale, 1));
				const glm::vec2 ref_in_viewport = fit.offset + (ref - origin) * fit.scale;
				viewport = glm::translate(viewport, glm::vec3(-ref_in_viewport, 0));
				const auto marker_transform =
				    glm::scale(glm::translate(viewport, glm::vec3(fit.offset - origin * fit.scale, 0)),
					           glm::vec3(fit.scale, 1));
				paint_state marker = tag.parent ? computed_style_at(*tag.parent, resources.stylesheet) : paint_state{};
				marker.transform = marker_transform;
				marker.viewport_size = {view_box.z, view_box.w};
				marker.viewport_origin = {view_box.x, view_box.y};
				marker.opacity = style.opacity;
				marker.pointer_events = style.pointer_events;
				if (style.fill_enabled && style.fill_gradient_id.empty())
					marker.context_fill = style.fill;
				if (style.stroke_enabled && style.stroke_gradient_id.empty())
					marker.context_stroke = style.stroke;
				apply_style(marker, tag, resources.stylesheet);
				if (!marker.display)
					continue;
				enter_reference(resources, id);
				document_data contents;
				contents.view_box = view_box;
				std::vector<geometry_bounds_scope*> marker_bounds_scopes;
				for (const auto& child : definition.children)
					process_svg_element(contents, child, marker, resources, marker_bounds_scopes,
					                    depth + 1, &owner);
				resources.active_references.erase(id);
				wrap_effect(contents, 0, marker, std::nullopt, resources, depth + 1, &owner);

				const auto& properties = resolve_css_properties(tag, resources.stylesheet);
				if (css_value(properties, "overflow", "hidden") != "visible")
				{
					const auto inverse = glm::inverse(viewport);
					if (!::mngl::detail::finite(inverse))
						continue;
					if (std::ranges::any_of(contents.geometry,
					                        [](const auto& part) { return part.image || part.effect; }))
					{
						auto layer = std::make_shared<effect_layer>();
						layer->region = {{}, size};
						layer->hit_clip = layer->region;
						for (auto& part : contents.geometry)
							part.transform = inverse * part.transform;
						layer->source = std::move(contents.geometry);
						document.geometry.push_back(
						    {{}, color(1.0f), viewport, {}, owner, {}, std::move(layer), {}});
						continue;
					}
					for (auto& part : contents.geometry)
						clip_marker_geometry(part, inverse * part.transform, size);
				}
				for (auto& part : contents.geometry)
				{
					if (!part.triangle_view().empty() || part.effect)
					{
						part.object_bounds.reset();
						document.geometry.push_back(std::move(part));
					}
				}
			}
		}
	}

	inline std::optional<transformed_bounds> process_svg_element(document_data& document, const xml_node& node,
	                                const paint_state& inherited, parse_resources& resources,
	                                std::vector<geometry_bounds_scope*>& bounds_scopes,
	                                std::size_t depth, const ::mngl::svg_hit_test_element* marker_owner,
	                                bool clip_geometry_mode)
	{
		if (depth > 256)
			throw std::length_error("SVG nested groups and references exceed the depth limit");
		if (!is_svg_element(node.tag))
		{
			return std::nullopt;
		}
		const auto name = local_name(node.tag.name);
		if (name == "defs" || name == "symbol" || name == "mask" || name == "clipPath" || name == "pattern" ||
		    name == "marker" ||
		    name == "filter" || name == "linearGradient" || name == "radialGradient" || name == "metadata" ||
		    name == "title" || name == "desc" || name == "script" || name == "style")
			return std::nullopt;
		if (name == "svg")
			throw std::invalid_argument("Nested SVG viewports are not supported");

		auto style = inherited;
		apply_style(style, node.tag, resources.stylesheet);
		if (clip_geometry_mode)
		{
			style.fill_enabled = true;
			style.fill = color(1.0f);
			style.fill_uses_current_color = false;
			style.fill_gradient_id.clear();
			style.fill_gradient_fallback = {};
			style.fill_opacity = 1.0f;
			style.stroke_enabled = false;
			style.stroke_gradient_id.clear();
			style.stroke_gradient_fallback = {};
			style.stroke_opacity = 1.0f;
			style.opacity = 1.0f;
			style.mask_id.clear();
			style.filter_id.clear();
			style.even_odd = style.clip_even_odd;
		}
		if (!style.display)
			return std::nullopt;
		const auto first = document.geometry.size();
		auto source_style = style;
		source_style.inherited_opacity = style.opacity;
		source_style.opacity = 1;
		std::optional<rectangle> bounds;
		if (name == "g")
		{
			geometry_bounds_scope scope;
			const auto inverse = glm::inverse(style.transform);
			scope.to_local = ::mngl::detail::finite(inverse) ? inverse : glm::mat4(1.0f);
			bounds_scopes.push_back(&scope);
			for (const auto& child : node.children)
				process_svg_element(document, child, source_style, resources, bounds_scopes,
				                    depth + 1, marker_owner, clip_geometry_mode);
			bounds_scopes.pop_back();
			if (scope.bounds.valid)
				bounds = scope.bounds.bounds();
		}
		else if (name == "use")
		{
			const auto href = svg_href_value(node.tag);
			if (!node.children.empty() || !href)
				throw std::invalid_argument("SVG <use> requires href and cannot have children");
			const std::string reference = parse_use_reference(*href);
			const auto& definition = find_definition(resources, reference, {});
			const auto target = local_name(definition.tag.name);
			if (target != "g" && target != "use" && target != "path" && target != "rect" &&
			    target != "circle" && target != "ellipse" && target != "line" && target != "polygon" &&
			    target != "polyline" && target != "image" && target != "symbol")
				throw std::invalid_argument("SVG <use> must reference a drawable element or symbol");

			enter_reference(resources, reference);
			const auto& use_properties = resolve_css_properties(node.tag, resources.stylesheet);
			const glm::vec2 current_viewport =
			    style.viewport_size.value_or(glm::vec2(document.view_box.z, document.view_box.w));
			const auto use_length = [&](std::string_view property, double reference_extent, float fallback)
			{
				const auto value = use_properties.find(std::string(property));
				return value == use_properties.end() ? fallback : viewport_length(value->second.value, reference_extent);
			};
			const float x = use_length("x", current_viewport.x, 0.0f);
			const float y = use_length("y", current_viewport.y, 0.0f);
			const glm::mat4 instance_transform =
			    glm::translate(style.transform, glm::vec3(x, y, 0.0f));
			if (target == "symbol")
			{
				std::optional<std::vector<float>> view_box;
				if (const auto value = attribute_value(definition.tag, "viewBox"))
					view_box = parse_view_box(*value);
				const float width_fallback =
				    view_box ? (*view_box)[2] : length_attribute(definition.tag, "width", document.view_box.z);
				const float height_fallback =
				    view_box ? (*view_box)[3] : length_attribute(definition.tag, "height", document.view_box.w);
				const float width = use_length("width", current_viewport.x, width_fallback);
				const float height = use_length("height", current_viewport.y, height_fallback);
				if (width < 0.0f || height < 0.0f) [[unlikely]]
					throw std::invalid_argument("SVG <use> symbol viewport dimensions must be non-negative");
				if (width > 0.0f && height > 0.0f &&
				    (!view_box || ((*view_box)[2] > 0.0f && (*view_box)[3] > 0.0f)))
				{
					auto symbol_style = source_style;
					symbol_style.transform = instance_transform;
					symbol_style.viewport_size = view_box ? glm::vec2((*view_box)[2], (*view_box)[3])
					                                    : glm::vec2(width, height);
					symbol_style.viewport_origin = view_box ? glm::vec2((*view_box)[0], (*view_box)[1])
					                                      : glm::vec2(0.0f);
						const glm::vec2 symbol_view_origin = symbol_style.viewport_origin;
						const glm::vec2 symbol_view_size = *symbol_style.viewport_size;
						rectangle symbol_clip_region{symbol_view_origin, symbol_view_size};
					if (view_box)
					{
						const glm::vec4 box{(*view_box)[0], (*view_box)[1], (*view_box)[2], (*view_box)[3]};
						const auto fit = fit_viewport(
						    {box.z, box.w}, {width, height},
						    attribute_value(definition.tag, "preserveAspectRatio").value_or(""));
							const auto visible_start = glm::vec2(box.x, box.y) - fit.offset / fit.scale;
							const auto visible_end = glm::vec2(box.x, box.y) +
							                        (glm::vec2(width, height) - fit.offset) / fit.scale;
							const auto clip_start = glm::max(glm::vec2(box.x, box.y), visible_start);
							const auto clip_end = glm::min(glm::vec2(box.x + box.z, box.y + box.w), visible_end);
							symbol_clip_region = {clip_start, glm::max(clip_end - clip_start, glm::vec2(0.0f))};
						symbol_style.transform = glm::translate(symbol_style.transform, glm::vec3(fit.offset, 0.0f));
						symbol_style.transform =
						    glm::scale(symbol_style.transform, glm::vec3(fit.scale, 1.0f));
						symbol_style.transform = glm::translate(
						    symbol_style.transform, glm::vec3(-box.x, -box.y, 0.0f));
					}
					apply_style(symbol_style, definition.tag, resources.stylesheet);
					if (symbol_style.display)
					{
						const auto symbol_first = document.geometry.size();
						auto symbol_contents = symbol_style;
						symbol_contents.inherited_opacity = symbol_style.opacity;
						symbol_contents.opacity = 1.0f;
						geometry_bounds_scope use_scope;
						const auto use_inverse = glm::inverse(style.transform);
						use_scope.to_local =
						    ::mngl::detail::finite(use_inverse) ? use_inverse : glm::mat4(1.0f);
						geometry_bounds_scope symbol_scope;
						const auto symbol_inverse = glm::inverse(symbol_style.transform);
						symbol_scope.to_local =
						    ::mngl::detail::finite(symbol_inverse) ? symbol_inverse : glm::mat4(1.0f);
						bounds_scopes.push_back(&use_scope);
						bounds_scopes.push_back(&symbol_scope);
						for (const auto& child : definition.children)
							process_svg_element(document, child, symbol_contents, resources, bounds_scopes,
							                   depth + 1, marker_owner, clip_geometry_mode);
						bounds_scopes.pop_back();
						bounds_scopes.pop_back();
						const auto& symbol_properties = resolve_css_properties(definition.tag, resources.stylesheet);
						if (css_value(symbol_properties, "overflow", "hidden") != "visible")
						{
							document_data symbol_document;
							symbol_document.view_box = view_box
							                               ? glm::vec4((*view_box)[0], (*view_box)[1], (*view_box)[2],
							                                           (*view_box)[3])
							                               : glm::vec4(0, 0, width, height);
							const auto end = document.geometry.size();
							symbol_document.geometry.reserve(end - symbol_first);
							for (std::size_t i = symbol_first; i < end; ++i)
								symbol_document.geometry.push_back(std::move(document.geometry[i]));
							document.geometry.resize(symbol_first);
							clip_root_viewport(symbol_document, symbol_clip_region, symbol_style.transform);
							for (auto& part : symbol_document.geometry)
								document.geometry.push_back(std::move(part));
						}
						if (use_scope.bounds.valid)
							bounds = use_scope.bounds.bounds();
						wrap_effect(document, symbol_first, symbol_style,
						            symbol_scope.bounds.valid
						                ? std::optional(symbol_scope.bounds.bounds())
						                : std::nullopt,
						            resources, depth + 1, marker_owner);
					}
				}
			}
			else
			{
				source_style.transform = instance_transform;
				geometry_bounds_scope use_scope;
				const auto use_inverse = glm::inverse(style.transform);
				use_scope.to_local = ::mngl::detail::finite(use_inverse) ? use_inverse : glm::mat4(1.0f);
				bounds_scopes.push_back(&use_scope);
				process_svg_element(document, definition, source_style, resources, bounds_scopes,
				                    depth + 1, marker_owner, clip_geometry_mode);
				bounds_scopes.pop_back();
				if (use_scope.bounds.valid)
					bounds = use_scope.bounds.bounds();
			}
			resources.active_references.erase(reference);
		}
		else
		{
			if (std::ranges::any_of(node.children,
				[](const auto& child)
				{
					if (!is_svg_element(child.tag))
						return false;
					const auto child_name = local_name(child.tag.name);
					return child_name != "title" && child_name != "desc" &&
					       child_name != "metadata";
				}))
				throw std::invalid_argument("SVG shapes cannot contain drawable children");

			const auto id = attribute_value(node.tag, "id").value_or("");
			const auto element = marker_owner
			                         ? *marker_owner
			                         : ::mngl::svg_hit_test_element{std::string(id), std::string(name),
			                                                        resources.next_element_index++};
			if (name == "image")
			{
				bounds = add_image(document, node, source_style, resources, element, style.visible);
				if (has_area(*bounds))
				{
					for (auto* scope : bounds_scopes)
						include_bounds(scope->bounds, *bounds, scope->to_local * style.transform);
				}
			}
			else
			{
				const auto tolerance = curve_tolerance_for_element(resources.curve_tolerance,
				                                                   resources.view_context, style.transform);
				const bool has_markers =
				    std::ranges::any_of(style.marker_ids, [](const auto& value) { return !value.empty(); });
				const auto& properties = resolve_css_properties(node.tag, resources.stylesheet);
				const glm::vec4 geometry_view_box = paint_view_box(style, document.view_box);
				const contour_cache_key contour_key{
				    &node, tolerance, resources.max_curve_segments,
				    {geometry_view_box.x, geometry_view_box.y, geometry_view_box.z, geometry_view_box.w}, has_markers};
				std::shared_ptr<const std::vector<contour>> shared_contours;
				std::vector<contour> parsed_contours;
				if (!resources.active_references.empty())
				{
					auto [entry, inserted] = resources.contour_cache.try_emplace(contour_key);
					if (inserted)
						entry->second = std::make_shared<const std::vector<contour>>(parse_shape_contours(
						    node.tag, name, tolerance, resources.max_curve_segments, geometry_view_box, has_markers,
						    properties));
					shared_contours = entry->second;
				}
				else
					parsed_contours = parse_shape_contours(
					    node.tag, name, tolerance, resources.max_curve_segments, geometry_view_box, has_markers,
					    properties);
				const auto& contours = shared_contours ? *shared_contours : parsed_contours;
				bounds_accumulator points;
				for (const auto& path : contours)
				{
					for (const auto point : path.points)
						points.include(point);
				}
				if (points.valid)
				{
					bounds = points.bounds();
					for (auto* scope : bounds_scopes)
						for (const auto& path : contours)
							for (const auto point : path.points)
								scope->bounds.include(apply_transform(scope->to_local * style.transform, point));
					if (style.visible)
					{
						auto geometry_style = source_style;
						if (name == "path")
						{
							if (const auto path_length = attribute_value(node.tag, "pathLength"))
							{
								const float authored_length = parse_number(*path_length);
								if (authored_length <= 0.0f)
								{
									throw std::invalid_argument("SVG pathLength must be greater than zero");
								}
								double actual_length = 0.0;
								for (const auto& path : contours)
								{
									actual_length += contour_length(path);
								}
								if (!std::isfinite(actual_length))
								{
									throw std::invalid_argument("SVG path has an unrepresentable total length");
								}
								const double calibration = actual_length / authored_length;
								for (auto& dash : geometry_style.stroke_dasharray)
								{
									const double scaled = dash * calibration;
									if (!std::isfinite(scaled) || scaled > std::numeric_limits<float>::max())
									{
										throw std::invalid_argument(
										    "SVG calibrated stroke-dasharray is unrepresentable");
									}
									dash = static_cast<float>(scaled);
								}
								const double scaled_offset = geometry_style.stroke_dashoffset * calibration;
								if (!std::isfinite(scaled_offset) ||
								    std::abs(scaled_offset) > std::numeric_limits<float>::max())
								{
									throw std::invalid_argument(
									    "SVG calibrated stroke-dashoffset is unrepresentable");
								}
								geometry_style.stroke_dashoffset = static_cast<float>(scaled_offset);
							}
						}
						tessellation_cache_key tessellation_key{
						    contour_key,
						    geometry_style.fill_enabled,
						    geometry_style.stroke_enabled,
						    geometry_style.even_odd,
						    geometry_style.stroke_width,
						    geometry_style.stroke_miterlimit,
						    geometry_style.stroke_dashoffset,
						    geometry_style.linecap,
						    geometry_style.linejoin,
						    geometry_style.stroke_dasharray};
						auto* cached_triangles = resources.active_references.empty()
						                             ? nullptr
						                             : &resources.tessellation_cache[tessellation_key];
						add_geometry(document, contours, *bounds, geometry_style, tolerance,
						             resources.max_curve_segments, resources.gradients, element, cached_triangles);
						if (has_markers)
							add_markers(document, contours, source_style, element, resources, depth);
					}
				}
			}
		}
		if (style.visible || name == "g" || name == "use")
			wrap_effect(document, first, style, bounds, resources, depth, marker_owner);
		if (!bounds)
			return std::nullopt;
		return transformed_bounds{*bounds, style.transform};
	}

	class parser
	{
	public:
		[[nodiscard]] document_data parse(std::string_view source, float curve_tolerance = 0.25f,
		                                  std::size_t max_curve_segments = 1'000'000,
		                                  const view_tessellation_context* view_context = nullptr,
		                                  const std::filesystem::path& image_directory = {},
		                                  decoded_image_cache* decoded_images = nullptr)
		{
			if (!std::isfinite(curve_tolerance) || curve_tolerance <= 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("SVG curve tolerance must be finite and greater than zero");
			}
			if (max_curve_segments == 0) [[unlikely]]
			{
				throw std::invalid_argument("SVG maximum curve segment count must be greater than zero");
			}
			if (view_context)
			{
				if (!std::isfinite(view_context->curve_tolerance_pixels) ||
				    view_context->curve_tolerance_pixels <= 0.0f ||
				    !std::isfinite(view_context->pixels_per_world_unit) ||
				    view_context->pixels_per_world_unit <= 0.0) [[unlikely]]
				{
					throw std::invalid_argument(
					    "SVG view-aware tolerance and pixel scale must be finite and greater than zero");
				}
				for (int column = 0; column < 4; ++column)
				{
					for (int row = 0; row < 4; ++row)
					{
						if (!std::isfinite(view_context->object_transform[column][row])) [[unlikely]]
						{
							throw std::invalid_argument("SVG object transform must be finite");
						}
					}
				}
			}

			css_stylesheet stylesheet;
			auto root = parse_xml_tree(source, &stylesheet);
			link_xml_parents(root);
			if (!is_svg_element(root.tag) || local_name(root.tag.name) != "svg") [[unlikely]]
			{
				throw std::invalid_argument("SVG source must have an <svg> root element");
			}
			const auto view_box_attribute = attribute_value(root.tag, "viewBox");
			auto root_aspect_ratio = trim(attribute_value(root.tag, "preserveAspectRatio").value_or(""));
			if (root_aspect_ratio.starts_with("defer "))
				root_aspect_ratio = trim(root_aspect_ratio.substr(6));
			const auto root_aspect_separator = root_aspect_ratio.find_first_of(" \t\r\n");
			const bool root_uses_slice =
			    root_aspect_separator != std::string_view::npos &&
			    trim(root_aspect_ratio.substr(root_aspect_separator)) == "slice";
			const glm::vec2 available_viewport = view_context ? view_context->viewport_size : glm::vec2(300, 150);
			const auto root_dimension = [&](std::string_view name, float fallback, float percentage_reference)
			{
				const auto value = attribute_value(root.tag, name);
				if (!value)
					return fallback;
				auto dimension = trim(*value);
				if (dimension.ends_with('%'))
				{
					dimension.remove_suffix(1);
					return parse_number(dimension) * percentage_reference / 100.0f;
				}
				return parse_length(dimension);
			};
			const float width = root_dimension("width", 300.0f, available_viewport.x);
			const float height = root_dimension("height", 150.0f, available_viewport.y);
			if (width <= 0.0f || height <= 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("SVG width and height must be positive lengths");
			}
			stylesheet.viewport_width = width;
			stylesheet.viewport_height = height;
			evaluate_css_media_conditions(stylesheet);
			std::vector<float> view_box{0.0f, 0.0f, width, height};
			if (view_box_attribute)
			{
				view_box = parse_view_box(*view_box_attribute);
			}

			document_data document;
			document.view_box = {view_box[0], view_box[1], view_box[2], view_box[3]};
			document.viewport_size = {width, height};
			if (view_box[2] == 0.0f || view_box[3] == 0.0f)
				return document;

			const auto gradients = parse_gradient_definitions(root, stylesheet);

			const auto root_fit = fit_viewport(glm::vec2(view_box[2], view_box[3]), {width, height},
			                                  attribute_value(root.tag, "preserveAspectRatio").value_or(""));
			glm::mat4 svg_to_mngl(1.0f);
			svg_to_mngl = glm::translate(svg_to_mngl, glm::vec3(root_fit.offset.x, height - root_fit.offset.y, 0.0f));
			svg_to_mngl = glm::scale(svg_to_mngl, glm::vec3(root_fit.scale.x, -root_fit.scale.y, 1.0f));
			svg_to_mngl = glm::translate(svg_to_mngl, glm::vec3(-view_box[0], -view_box[1], 0.0f));
			paint_state root_style;
			root_style.transform = svg_to_mngl;
			root_style.viewport_size = glm::vec2(view_box[2], view_box[3]);
			root_style.viewport_origin = glm::vec2(view_box[0], view_box[1]);
			apply_style(root_style, root.tag, stylesheet);

			reusable_elements definitions;
			collect_reusable_elements(root, definitions);
			parse_resources resources{gradients,    stylesheet,      definitions,       image_directory,
			                          view_context, curve_tolerance, max_curve_segments, {}, {}, {}, {}, decoded_images};
			if (root_style.display)
			{
				auto source_style = root_style;
				source_style.inherited_opacity = root_style.opacity;
				source_style.opacity = 1;
				geometry_bounds_scope root_bounds;
				const auto root_inverse = glm::inverse(root_style.transform);
				root_bounds.to_local = ::mngl::detail::finite(root_inverse) ? root_inverse : glm::mat4(1.0f);
				std::vector<geometry_bounds_scope*> root_bounds_scopes{&root_bounds};
				for (const auto& child : root.children)
					process_svg_element(document, child, source_style, resources, root_bounds_scopes);
				wrap_effect(document, 0, root_style,
				            root_bounds.bounds.valid ? std::optional(root_bounds.bounds.bounds()) : std::nullopt,
				            resources, 0, nullptr);
				const glm::vec2 view_box_origin{view_box[0], view_box[1]};
				const glm::vec2 view_box_size{view_box[2], view_box[3]};
				const glm::vec2 visible_start = view_box_origin - root_fit.offset / root_fit.scale;
				const glm::vec2 visible_end =
				    view_box_origin + (glm::vec2(width, height) - root_fit.offset) / root_fit.scale;
				const glm::vec2 clip_start = glm::max(view_box_origin, visible_start);
				const glm::vec2 clip_end = glm::min(view_box_origin + view_box_size, visible_end);
				if (root_uses_slice)
				{
					clip_root_viewport(document, {clip_start, glm::max(clip_end - clip_start, glm::vec2(0))},
					                   svg_to_mngl);
				}
			}
			return document;
		}
	};

	[[nodiscard]] inline document_data parse(std::string_view source, float curve_tolerance = 0.25f,
	                                         std::size_t max_curve_segments = 1'000'000,
	                                         const view_tessellation_context* view_context = nullptr,
	                                         const std::filesystem::path& image_directory = {},
	                                         decoded_image_cache* decoded_images = nullptr)
	{
		return parser{}.parse(source, curve_tolerance, max_curve_segments, view_context, image_directory,
		                      decoded_images);
	}
} // namespace mngl::two_d::detail::svg
