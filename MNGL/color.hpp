#pragma once

#include <algorithm>
#include <cstddef>
#include <charconv>

#include <cstdint>
// GLM hashing support for color values.

#include <glm/glm.hpp>
#include <functional>
#include <initializer_list>
#include <ios>
#include <ostream>
#include <concepts>
#include <stdexcept>
#include <string>
#include <SDL3/SDL.h>
// glsl style, means that rgba components are in range [0, 1]
namespace mngl
{
	class color
	{
	private:
		static int stream_format_index()
		{
			static const int index = std::ios_base::xalloc();
			return index;
		}

	public:
		template <std::integral R, std::integral G, std::integral B>
		constexpr color(R r, G g, B b) : color(r, g, b, 255)
		{
		}
		template <std::integral R, std::integral G, std::integral B, std::integral A>
		constexpr color(R r, G g, B b, A a) : rgba(r / 255., g / 255., b / 255., a / 255.)
		{
		}
		template <std::floating_point R, std::floating_point G, std::floating_point B>
		constexpr color(R r, G g, B b) : color(r, g, b, 1.0f)
		{
		}
		template <std::floating_point R, std::floating_point G, std::floating_point B, std::floating_point A>
		constexpr color(R r, G g, B b, A a) : rgba(r, g, b, a)
		{
		}
		constexpr explicit color(std::uint32_t rgba)
		    : rgba((rgba >> 24) / 255., ((rgba >> 16) & 0xFF) / 255., ((rgba >> 8) & 0xFF) / 255.,
			       (rgba & 0xFF) / 255.)
		{
		}
		constexpr color(SDL_Color col) : rgba(col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f) {}
		template <std::floating_point RGB, glm::qualifier Q>
		constexpr color(glm::vec<3, RGB, Q> col) : color(col, 1.0f)
		{
		}
		template <std::floating_point RGB, std::floating_point A, glm::qualifier Q>
		constexpr color(glm::vec<3, RGB, Q> col, A a) : rgba(col, a)
		{
		}
		template <std::floating_point RGBA, glm::qualifier Q>
		constexpr color(glm::vec<4, RGBA, Q> col) : rgba(col)
		{
		}
		template <std::integral RGB, glm::qualifier Q>
		constexpr color(glm::vec<3, RGB, Q> col) : color(col, 255)
		{
		}
		template <std::integral RGB, std::integral A, glm::qualifier Q>
		constexpr color(glm::vec<3, RGB, Q> col, A a) : rgba(glm::vec3(col) / 255.f, a / 255.f)
		{
		}
		template <std::integral RGBA, glm::qualifier Q>
		constexpr color(glm::vec<4, RGBA, Q> col) : rgba(glm::vec4(col) / 255.f)
		{
		}
		template <std::integral G> constexpr color(G gray) : color(gray, 255) {}
		template <std::integral G, std::integral A>
		constexpr color(G gray, A a) : rgba(gray / 255.f, gray / 255.f, gray / 255.f, a / 255.f)
		{
		}
		template <std::floating_point G> constexpr color(G gray) : color(gray, 1.0f) {}
		template <std::floating_point G, std::floating_point A>
		constexpr color(G gray, A a) : rgba(gray, gray, gray, a)
		{
		}
		color(std::string hex) // #RRGGBBAA or #RRGGBB
		{
			if (hex.size() != 7 && hex.size() != 9) [[unlikely]]
				throw std::invalid_argument("hex color must be in format #RRGGBBAA or #RRGGBB");
			if (hex[0] != '#') [[unlikely]]
				throw std::invalid_argument("hex color must start with #");
			std::uint32_t rgba = 0;
			const auto* end = hex.data() + hex.size();
			const auto parsed = std::from_chars(hex.data() + 1, end, rgba, 16);
			if (parsed.ec != std::errc{} || parsed.ptr != end) [[unlikely]]
			{
				throw std::invalid_argument("hex color must contain only hexadecimal digits after #");
			}
			if (hex.size() == 7)
				rgba = (rgba << 8) | 0xFF;
			this->rgba = color(rgba).rgba;
		}
		constexpr color() : rgba(0, 0, 0, 1) {}
		constexpr void mix(color other, float factor) { rgba = (1 - factor) * rgba + factor * other.rgba; }
		template <std::floating_point RGBA, glm::qualifier Q> constexpr operator glm::vec<4, RGBA, Q>() const
		{
			return rgba;
		}
		template <std::integral RGBA, glm::qualifier Q> constexpr operator glm::vec<4, RGBA, Q>() const
		{
			return glm::vec<4, RGBA, Q>(rgba * 255.f);
		}
		constexpr explicit operator SDL_Color() const
		{
			return SDL_Color{static_cast<std::uint8_t>(rgba.x * 255), static_cast<std::uint8_t>(rgba.y * 255),
			                 static_cast<std::uint8_t>(rgba.z * 255),
			                 static_cast<std::uint8_t>(rgba.w * 255)};
		}
		constexpr explicit operator std::uint32_t() const
		{
			return (static_cast<std::uint32_t>(rgba.x * 255) << 24) |
			       (static_cast<std::uint32_t>(rgba.y * 255) << 16) |
			       (static_cast<std::uint32_t>(rgba.z * 255) << 8) | static_cast<std::uint32_t>(rgba.w * 255);
		}
		template <std::floating_point RGB, glm::qualifier Q> constexpr operator glm::vec<3, RGB, Q>() const
		{
			return glm::vec<3, RGB, Q>(rgb_glsl());
		}
		template <std::integral RGB, glm::qualifier Q> constexpr operator glm::vec<3, RGB, Q>() const
		{
			return glm::vec<3, RGB, Q>(rgb_glsl() * 255.f);
		}
		constexpr explicit operator std::uint8_t() const { return static_cast<std::uint8_t>(r * 255); }
		template <std::floating_point R> constexpr explicit operator R() const { return r; }
		explicit operator std::string() const
		{
			std::uint32_t rgba = static_cast<std::uint32_t>(*this);
			char buf[10];
			snprintf(buf, sizeof(buf), "#%08X", rgba);
			return std::string(buf);
		}

		constexpr glm::vec4 rgba_glsl() const { return rgba; }
		constexpr glm::uvec4 rgba_255() const { return glm::uvec4(rgba * 255.f); }
		constexpr std::uint32_t rgba_uint() const { return static_cast<std::uint32_t>(*this); }
		constexpr glm::vec3 rgb_glsl() const { return glm::vec3(rgba); }
		constexpr glm::uvec3 rgb_255() const { return glm::uvec3(rgba * 255.f); }
		constexpr std::uint32_t rgb_uint() const { return static_cast<std::uint32_t>(*this) >> 8; }
		constexpr std::uint8_t gray_255() const { return static_cast<std::uint8_t>(rgba.x * 255); }
		constexpr float gray_glsl() const { return rgba.x; }
		std::string hex() const { return static_cast<std::string>(*this); }

		// hsv or hsl

		constexpr static color from_hsv(glm::vec3 hsv) { return {hsv_to_rgb(hsv), 1.f}; }
		constexpr glm::vec3 to_hsv() const { return rgb_to_hsv(rgb_glsl()); }

		constexpr static color from_hsv(glm::uvec3 hsv)
		{
			return from_hsv(glm::vec3{hsv.x / 360.f, hsv.y / 255.f, hsv.z / 255.f});
		}
		constexpr glm::uvec3 to_hsv_255() const
		{
			auto hsv = to_hsv();
			return {static_cast<std::uint32_t>(hsv.x * 360.f + 0.5f),
			        static_cast<std::uint32_t>(hsv.y * 255.f + 0.5f),
			        static_cast<std::uint32_t>(hsv.z * 255.f + 0.5f)};
		}

		constexpr static color from_hsl(glm::vec3 hsl) { return {hsl_to_rgb(hsl), 1.f}; }
		constexpr glm::vec3 to_hsl() const { return rgb_to_hsl(rgb_glsl()); }

		constexpr static color from_hsl(glm::uvec3 hsl)
		{
			return from_hsl(glm::vec3{hsl.x / 360.f, hsl.y / 255.f, hsl.z / 255.f});
		}
		constexpr glm::uvec3 to_hsl_255() const
		{
			auto hsl = to_hsl();
			return {static_cast<std::uint32_t>(hsl.x * 360.f + 0.5f),
			        static_cast<std::uint32_t>(hsl.y * 255.f + 0.5f),
			        static_cast<std::uint32_t>(hsl.z * 255.f + 0.5f)};
		}

		color& operator=(const color& other)
		{
			rgba = other.rgba;
			return *this;
		}

		constexpr bool operator==(const color& other) const { return rgba == other.rgba; }

		constexpr bool operator!=(const color& other) const { return rgba != other.rgba; }

		constexpr color operator*(float factor) const { return glm::clamp(rgba * factor, 0.f, 1.f); }

		constexpr color operator*(const color& other) const
		{
			return glm::clamp(rgba * other.rgba, 0.f, 1.f);
		}

		color& operator*=(float factor)
		{
			*this = *this * factor;
			return *this;
		}

		color& operator*=(const color& other)
		{
			*this = *this * other;
			return *this;
		}

		constexpr color operator+(const color& other) const
		{
			return glm::clamp(rgba + other.rgba, 0.f, 1.f);
		}

		color& operator+=(const color& other)
		{
			*this = *this + other;
			return *this;
		}

		constexpr color operator~() const { return color(1.f - rgba); }

		constexpr color operator&(const color& other) const
		{
			return color(glm::vec4(std::min(rgba.x, other.rgba.x), std::min(rgba.y, other.rgba.y),
			                       std::min(rgba.z, other.rgba.z), std::min(rgba.w, other.rgba.w)));
		}

		constexpr color operator|(const color& other) const
		{
			return color(glm::vec4(std::max(rgba.x, other.rgba.x), std::max(rgba.y, other.rgba.y),
			                       std::max(rgba.z, other.rgba.z), std::max(rgba.w, other.rgba.w)));
		}

		constexpr static color lerp(const color& a, const color& b, float factor)
		{
			return glm::clamp((1 - factor) * a.rgba + factor * b.rgba, 0.f, 1.f);
		}

		constexpr float luminance() const
		{
			return linearize(rgba.x) * 0.2126f + linearize(rgba.y) * 0.7152f + linearize(rgba.z) * 0.0722f;
		}

		constexpr static float luminance(const color& c) { return c.luminance(); }

		constexpr float grayscale() const { return luminance(); }

		constexpr static float grayscale(color c) { return c.grayscale(); }

		constexpr static float contrast(color a, color b)
		{
			float l1 = a.luminance();
			float l2 = b.luminance();
			if (l1 > l2)
				return (l1 + 0.05f) / (l2 + 0.05f);
			else
				return (l2 + 0.05f) / (l1 + 0.05f);
		}

		constexpr static float linearize(float c)
		{
			if (c <= 0.04045f)
				return c / 12.92f;
			else
				return pow_12_over_5((c + 0.055f) / 1.055f);
		}

		constexpr static float gamma(float c)
		{
			if (c <= 0.0031308f)
				return c * 12.92f;
			else
				return 1.055f * pow_5_over_12(c) - 0.055f;
		}

		constexpr color premultiplied() { return {glm::clamp(rgb_glsl() * rgba.w, 0.f, 1.f), rgba.w}; }

		color& premultiply() { return (*this) = {glm::clamp(rgb_glsl() * rgba.w, 0.f, 1.f), rgba.w}; }

		constexpr color unmultiplied()
		{
			return rgba.w == 0 ? glm::vec4{0, 0, 0, rgba.w}
			                   : glm::vec4{glm::clamp(rgb_glsl() / rgba.w, 0.f, 1.f), rgba.w};
		}

		color& unmultiply()
		{
			return (*this) = rgba.w == 0 ? glm::vec4{0, 0, 0, rgba.w}
			                            : glm::vec4{glm::clamp(rgb_glsl() / rgba.w, 0.f, 1.f), rgba.w};
		}

		struct blends
		{
			// Arguments and result use straight alpha; a is composited over b.
			static constexpr color normal(color a, color b)
			{
				if (a.rgba.w == 0.f)
					return b;
				if (b.rgba.w == 0.f)
					return a;

				const float out_alpha = a.rgba.w + b.rgba.w * (1.f - a.rgba.w);
				const glm::vec3 out_premultiplied =
				    a.rgb_glsl() * a.rgba.w + b.rgb_glsl() * b.rgba.w * (1.f - a.rgba.w);
				const glm::vec3 out_rgb = out_premultiplied / out_alpha;
				return {std::clamp(out_rgb.x, 0.f, 1.f), std::clamp(out_rgb.y, 0.f, 1.f),
				        std::clamp(out_rgb.z, 0.f, 1.f), out_alpha};
			}

			static color additive(color a, color b)
			{
				return glm::clamp(glm::vec4{a.rgb_glsl() + b.rgb_glsl(), std::max(a.a, b.a)}, 0.f, 1.f);
			}

			static color multiply(color a, color b) { return a * b; }

			static color screen(color a, color b)
			{
				return glm::clamp(
				    glm::vec4{glm::vec3(1, 1, 1) - ((~a).rgb_glsl() * (~b).rgb_glsl()), std::max(a.a, b.a)},
				    0.f, 1.f);
			}
		};

		union
		{
			glm::vec4 rgba;
			struct
			{
				float r, g, b, a;
			};
		};

		struct formats
		{
			enum class fmts
			{
				standard,
				standart = standard,
				rgba255,
				rgb255,
				frgba,
				frgb,
				hex
			};
			struct color_fmt
			{
				fmts fmt;
				explicit color_fmt(fmts fmt) : fmt(fmt) {}

			private:
				std::ostream& apply(std::ostream& out) const
				{
					out.iword(color::stream_format_index()) = static_cast<long>(fmt);
					return out;
				}

				friend std::ostream& operator<<(std::ostream& out, color_fmt fmt)
				{
					return fmt.apply(out);
				}
			};

			inline static const auto rgba255 = color_fmt(fmts::rgba255);
			inline static const auto rgb255 = color_fmt(fmts::rgb255);
			inline static const auto frgba = color_fmt(fmts::frgba);
			inline static const auto frgb = color_fmt(fmts::frgb);
			inline static const auto hex = color_fmt(fmts::hex);
			inline static const auto standard = color_fmt(fmts::standard);
		};

		static constexpr color fromHSV(glm::vec3 value) { return from_hsv(value); }
		static constexpr color fromHSV(glm::uvec3 value) { return from_hsv(value); }
		[[nodiscard]] constexpr glm::vec3 toHSV() const { return to_hsv(); }
		[[nodiscard]] constexpr glm::uvec3 toHSV_255() const { return to_hsv_255(); }
		static constexpr color fromHSL(glm::vec3 value) { return from_hsl(value); }
		static constexpr color fromHSL(glm::uvec3 value) { return from_hsl(value); }
		[[nodiscard]] constexpr glm::vec3 toHSL() const { return to_hsl(); }
		[[nodiscard]] constexpr glm::uvec3 toHSL_255() const { return to_hsl_255(); }

		using fomats = formats;

		friend std::ostream& operator<<(std::ostream& out, color col)
		{
			const auto raw_format = out.iword(stream_format_index());
			const auto fmt = raw_format >= static_cast<long>(formats::fmts::standard) &&
			                         raw_format <= static_cast<long>(formats::fmts::hex)
			                     ? static_cast<formats::fmts>(raw_format)
			                     : formats::fmts::standard;
			switch (fmt)
			{
			case formats::fmts::standard:
				out << "color(" << col.r << ", " << col.g << ", " << col.b << ", " << col.a << ")";
				break;
			case formats::fmts::rgba255:
				out << "color(" << static_cast<std::uint32_t>(col.r * 255) << ", "
				    << static_cast<std::uint32_t>(col.g * 255) << ", "
				    << static_cast<std::uint32_t>(col.b * 255) << ", "
				    << static_cast<std::uint32_t>(col.a * 255) << ")";
				break;
			case formats::fmts::rgb255:
				out << "color(" << static_cast<std::uint32_t>(col.r * 255) << ", "
				    << static_cast<std::uint32_t>(col.g * 255) << ", "
				    << static_cast<std::uint32_t>(col.b * 255) << ")";
				break;
			case formats::fmts::frgba:
				out << "color(" << col.r << "f, " << col.g << "f, " << col.b << "f, " << col.a << "f)";
				break;
			case formats::fmts::frgb:
				out << "color(" << col.r << "f, " << col.g << "f, " << col.b << "f)";
				break;
			case formats::fmts::hex:
				out << static_cast<std::string>(col);
				break;
			default:
				out << "color(" << col.r << ", " << col.g << ", " << col.b << ", " << col.a << ")";
			}
			return out;
		}

	private:
		// C++23 does not make std::pow constant-evaluable. These roots cover the
		// non-negative inputs used by the sRGB transfer functions above.
		static constexpr float nth_root(float value, int degree)
		{
			if (value <= 0.f)
				return 0.f;

			int scale_exponent = 0;
			while (value > 1.f && scale_exponent < 256)
			{
				value *= 0.5f;
				++scale_exponent;
			}
			if (scale_exponent == 256)
				return value;

			const int root_scale_exponent = scale_exponent / degree;
			float remainder_scale = 1.f;
			for (int exponent = 0; exponent < scale_exponent % degree; ++exponent)
				remainder_scale *= 2.f;
			value *= remainder_scale;

			float guess = value > 1.f ? value : 1.f;
			for (int iteration = 0; iteration < 64; ++iteration)
			{
				float denominator = 1.f;
				for (int exponent = 1; exponent < degree; ++exponent)
					denominator *= guess;

				const float next =
				    ((degree - 1) * guess + value / denominator) / static_cast<float>(degree);
				if (next == guess)
					return guess;
				guess = next;
			}

			float result_scale = 1.f;
			for (int exponent = 0; exponent < root_scale_exponent; ++exponent)
				result_scale *= 2.f;
			return guess * result_scale;
		}

		static constexpr float pow_12_over_5(float value)
		{
			const float square = value * value;
			return square * nth_root(square, 5);
		}

		static constexpr float pow_5_over_12(float value)
		{
			const float cube_root = nth_root(value, 3);
			return cube_root * nth_root(nth_root(cube_root, 2), 2);
		}

		static constexpr glm::vec3 rgb_to_hsv(glm::vec3 c)
		{
			float mn = glm::min(c.x, glm::min(c.y, c.z));
			float mx = glm::max(c.x, glm::max(c.y, c.z));
			float d = mx - mn, h = 0.f, s = (mx < 1e-6f) ? 0.f : d / mx;

			if (d > 1e-6f)
			{
				if (c.x == mx)
					h = (c.y - c.z) / d + (c.y < c.z ? 6.f : 0.f);
				else if (c.y == mx)
					h = (c.z - c.x) / d + 2.f;
				else
					h = (c.x - c.y) / d + 4.f;
				h /= 6.f;
			}
			return {h, s, mx};
		}

		static constexpr glm::vec3 hsv_to_rgb(glm::vec3 c)
		{
			float h = c.x * 6.f, s = c.y, v = c.z;
			if (s < 1e-6f)
				return {v, v, v};
			int i = static_cast<int>(h);
			float f = h - i, p = v * (1.f - s), q = v * (1.f - s * f), t = v * (1.f - s * (1.f - f));
			switch (i % 6)
			{
			case 0:
				return {v, t, p};
			case 1:
				return {q, v, p};
			case 2:
				return {p, v, t};
			case 3:
				return {p, q, v};
			case 4:
				return {t, p, v};
			case 5:
				return {v, p, q};
			}
			return {v, v, v};
		}

		static constexpr glm::vec3 rgb_to_hsl(glm::vec3 c)
		{
			float mn = glm::min(c.x, glm::min(c.y, c.z));
			float mx = glm::max(c.x, glm::max(c.y, c.z));
			float d = mx - mn, l = (mx + mn) * 0.5f;
			float h = 0.f, s = (d < 1e-6f) ? 0.f : d / (l > 0.5f ? 2.f - mx - mn : mx + mn);

			if (d > 1e-6f)
			{
				if (c.x == mx)
					h = (c.y - c.z) / d + (c.y < c.z ? 6.f : 0.f);
				else if (c.y == mx)
					h = (c.z - c.x) / d + 2.f;
				else
					h = (c.x - c.y) / d + 4.f;
				h /= 6.f;
			}
			return {h, s, l};
		}

		static constexpr glm::vec3 hsl_to_rgb(glm::vec3 c)
		{
			float h = c.x, s = c.y, l = c.z;
			if (s < 1e-6f)
				return {l, l, l};
			auto hue = [](float p, float q, float t)
			{
				if (t < 0.f)
					t += 1.f;
				if (t > 1.f)
					t -= 1.f;
				if (t < 1.f / 6.f)
					return p + (q - p) * 6.f * t;
				if (t < 0.5f)
					return q;
				if (t < 2.f / 3.f)
					return p + (q - p) * (2.f / 3.f - t) * 6.f;
				return p;
			};
			float q = l < 0.5f ? l * (1.f + s) : l + s - l * s;
			float p = 2.f * l - q;
			return {hue(p, q, h + 1.f / 3.f), hue(p, q, h), hue(p, q, h - 1.f / 3.f)};
		}
	};
} // namespace mngl

// Use standard user-defined literal suffixes. Keep the short MSVC spelling as a compatibility extension.
constexpr mngl::color operator""_rgb(unsigned long long x)
{
	return mngl::color((x >> 16) & 255, (x >> 8) & 255, (x & 255));
}

constexpr mngl::color operator""_rgba(unsigned long long x)
{
	return mngl::color((x >> 24) & 255, (x >> 16) & 255, (x >> 8) & 255, (x & 255));
}

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4455)
constexpr mngl::color operator"" rgb(std::uint64_t x)
{
	return mngl::color((x >> 16) & 255, (x >> 8) & 255, (x & 255));
}

constexpr mngl::color operator"" rgba(std::uint64_t x)
{
	return mngl::color((x >> 24) & 255, (x >> 16) & 255, (x >> 8) & 255, (x & 255));
}
#pragma warning(pop)
#endif

namespace std
{
	template <> struct hash<mngl::color>
	{
		// Standard float hashing preserves equality, including +0 and -0, without quantization.
		std::size_t operator()(const mngl::color& value) const noexcept
		{
			std::size_t seed = 0;
			for (const float component : {value.r, value.g, value.b, value.a})
			{
				const auto part = std::hash<float>{}(component);
				seed ^= part + static_cast<std::size_t>(0x9e3779b9u) + (seed << 6) + (seed >> 2);
			}
			return seed;
		}
	};
} // namespace std

