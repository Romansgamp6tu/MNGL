#pragma once

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
			return SDL_Color{static_cast<std::uint8_t>(rgba.r * 255), static_cast<std::uint8_t>(rgba.g * 255),
			                 static_cast<std::uint8_t>(rgba.b * 255),
			                 static_cast<std::uint8_t>(rgba.a * 255)};
		}
		constexpr explicit operator std::uint32_t() const
		{
			return (static_cast<std::uint32_t>(rgba.r * 255) << 24) |
			       (static_cast<std::uint32_t>(rgba.g * 255) << 16) |
			       (static_cast<std::uint32_t>(rgba.b * 255) << 8) | static_cast<std::uint32_t>(rgba.a * 255);
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
		constexpr std::uint8_t gray_255() const { return static_cast<std::uint8_t>(rgba.r * 255); }
		constexpr float gray_glsl() const { return rgba.r; }
		std::string hex() const { return static_cast<std::string>(*this); }

		// hsv or hsl

		constexpr static color from_hsv(glm::vec3 hsv) { return {hsv_to_rgb(hsv), 1.f}; }
		constexpr glm::vec3 to_hsv() const { return rgb_to_hsv(rgb_glsl()); }

		constexpr static color from_hsv(glm::uvec3 hsv)
		{
			return from_hsv(glm::vec3{hsv.r / 360.f, hsv.g / 255.f, hsv.b / 255.f});
		}
		constexpr glm::uvec3 to_hsv_255() const
		{
			auto hsv = to_hsv();
			return {static_cast<std::uint32_t>(hsv.r * 360.f + 0.5f),
			        static_cast<std::uint32_t>(hsv.g * 255.f + 0.5f),
			        static_cast<std::uint32_t>(hsv.b * 255.f + 0.5f)};
		}

		constexpr static color from_hsl(glm::vec3 hsl) { return {hsl_to_rgb(hsl), 1.f}; }
		constexpr glm::vec3 to_hsl() const { return rgb_to_hsl(rgb_glsl()); }

		constexpr static color from_hsl(glm::uvec3 hsl)
		{
			return from_hsl(glm::vec3{hsl.r / 360.f, hsl.g / 255.f, hsl.b / 255.f});
		}
		constexpr glm::uvec3 to_hsl_255() const
		{
			auto hsl = to_hsl();
			return {static_cast<std::uint32_t>(hsl.r * 360.f + 0.5f),
			        static_cast<std::uint32_t>(hsl.g * 255.f + 0.5f),
			        static_cast<std::uint32_t>(hsl.b * 255.f + 0.5f)};
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
			return color(glm::vec4(std::min(rgba.r, other.rgba.r), std::min(rgba.g, other.rgba.g),
			                       std::min(rgba.b, other.rgba.b), std::min(rgba.a, other.rgba.a)));
		}

		constexpr color operator|(const color& other) const
		{
			return color(glm::vec4(std::max(rgba.r, other.rgba.r), std::max(rgba.g, other.rgba.g),
			                       std::max(rgba.b, other.rgba.b), std::max(rgba.a, other.rgba.a)));
		}

		constexpr static color lerp(const color& a, const color& b, float factor)
		{
			return glm::clamp((1 - factor) * a.rgba + factor * b.rgba, 0.f, 1.f);
		}

		constexpr float luminance() const
		{
			return linearize(rgba.r) * 0.2126f + linearize(rgba.g) * 0.7152f + linearize(rgba.b) * 0.0722f;
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
				return std::pow((c + 0.055f) / 1.055f, 2.4f);
		}

		constexpr static float gamma(float c)
		{
			if (c <= 0.0031308f)
				return c * 12.92f;
			else
				return 1.055f * std::pow(c, 1 / 2.4f) - 0.055f;
		}

		constexpr color premultiplied() { return {glm::clamp(rgb_glsl() * a, 0.f, 1.f), a}; }

		color& premultiply() { return (*this) = {glm::clamp(rgb_glsl() * a, 0.f, 1.f), a}; }

		constexpr color unmultiplied()
		{
			return a == 0 ? glm::vec4{0, 0, 0, a} : glm::vec4{glm::clamp(rgb_glsl() / a, 0.f, 1.f), a};
		}

		color& unmultiply()
		{
			return (*this) =
			           a == 0 ? glm::vec4{0, 0, 0, a} : glm::vec4{glm::clamp(rgb_glsl() / a, 0.f, 1.f), a};
		}

		struct blends
		{
			// premultiplied делается внутри!!!
			static color normal(color a, color b)
			{
				a.premultiply();
				b.premultiply();
				return glm::clamp(a.rgba + b.rgba * (1 - a.a), 0.f, 1.f);
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
		static glm::vec3 rgb_to_hsv(glm::vec3 c)
		{
			float mn = glm::min(c.r, glm::min(c.g, c.b));
			float mx = glm::max(c.r, glm::max(c.g, c.b));
			float d = mx - mn, h = 0.f, s = (mx < 1e-6f) ? 0.f : d / mx;

			if (d > 1e-6f)
			{
				if (c.r == mx)
					h = (c.g - c.b) / d + (c.g < c.b ? 6.f : 0.f);
				else if (c.g == mx)
					h = (c.b - c.r) / d + 2.f;
				else
					h = (c.r - c.g) / d + 4.f;
				h /= 6.f;
			}
			return {h, s, mx};
		}

		static glm::vec3 hsv_to_rgb(glm::vec3 c)
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

		static glm::vec3 rgb_to_hsl(glm::vec3 c)
		{
			float mn = glm::min(c.r, glm::min(c.g, c.b));
			float mx = glm::max(c.r, glm::max(c.g, c.b));
			float d = mx - mn, l = (mx + mn) * 0.5f;
			float h = 0.f, s = (d < 1e-6f) ? 0.f : d / (l > 0.5f ? 2.f - mx - mn : mx + mn);

			if (d > 1e-6f)
			{
				if (c.r == mx)
					h = (c.g - c.b) / d + (c.g < c.b ? 6.f : 0.f);
				else if (c.g == mx)
					h = (c.b - c.r) / d + 2.f;
				else
					h = (c.r - c.g) / d + 4.f;
				h /= 6.f;
			}
			return {h, s, l};
		}

		static glm::vec3 hsl_to_rgb(glm::vec3 c)
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

