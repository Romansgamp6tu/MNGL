#pragma once
// хешарики =))
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <ostream>
#include <string>
#include <SDL3/SDL.h>
// glsl style, means that rgba components are in range [0, 1]
namespace mngl
{
	class color
	{
	public:
		template<std::integral R, std::integral G, std::integral B>
		constexpr color(R r, G g, B b) : color(r, g, b, 255) {}
		template<std::integral R, std::integral G, std::integral B, std::integral A>
		constexpr color(R r, G g, B b, A a) : rgba(r / 255., g / 255., b / 255., a / 255.) {}
		template<std::floating_point R, std::floating_point G, std::floating_point B>
		constexpr color(R r, G g, B b) : color(r, g, b, 1.0f) {}
		template<std::floating_point R, std::floating_point G, std::floating_point B, std::floating_point A>
		constexpr color(R r, G g, B b, A a) : rgba(r, g, b, a) {}
		constexpr explicit color(uint32_t rgba) : rgba((rgba >> 24) / 255., ((rgba >> 16) & 0xFF) / 255., ((rgba >> 8) & 0xFF) / 255., (rgba & 0xFF) / 255.) {}
		constexpr color(SDL_Color col) : rgba(col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f) {}
		template<std::floating_point RGB, glm::qualifier Q>
		constexpr color(glm::vec<3, RGB, Q> col) : color(col, 1.0f) {}
		template<std::floating_point RGB, std::floating_point A, glm::qualifier Q>
		constexpr color(glm::vec<3, RGB, Q> col, A a) : rgba(col, a) {}
		template <std::floating_point RGBA, glm::qualifier Q>
		constexpr color(glm::vec<4, RGBA, Q> col) : rgba(col) {}
		template<std::integral RGB, glm::qualifier Q>
		constexpr color(glm::vec<3, RGB, Q> col) : color(col, 255) {}
		template<std::integral RGB, std::integral A, glm::qualifier Q>
		constexpr color(glm::vec<3, RGB, Q> col, A a) : rgba(glm::vec3(col) / 255.f, a / 255.f) {}
		template <std::integral RGBA, glm::qualifier Q>
		constexpr color(glm::vec<4, RGBA, Q> col) : rgba(glm::vec4(col) / 255.f) {}
		template<std::integral G>
		constexpr color(G gray) : color(gray, 255) {}
		template<std::integral G, std::integral A>
		constexpr color(G gray, A a) : rgba(gray / 255.f, gray / 255.f, gray / 255.f, a / 255.f) {}
		template<std::floating_point G>
		constexpr color(G gray) : color(gray, 1.0f) {}
		template<std::floating_point G, std::floating_point A>
		constexpr color(G gray, A a) : rgba(gray, gray, gray, a) {}
		color(std::string hex) // #RRGGBBAA or #RRGGBB
		{
			if (hex.size() != 7 && hex.size() != 9) throw std::invalid_argument("hex color must be in format #RRGGBBAA or #RRGGBB");
			if (hex[0] != '#') throw std::invalid_argument("hex color must start with #");
			uint32_t rgba = std::stoul(hex.substr(1), nullptr, 16);
			if (hex.size() == 7) rgba = (rgba << 8) | 0xFF;
			this->rgba = color(rgba).rgba;
		}
		constexpr color() : rgba(0, 0, 0, 1) {}
		constexpr void mix(color other, float factor)
		{
			rgba = (1 - factor) * rgba + factor * other.rgba;
		}
		template <std::floating_point RGBA, glm::qualifier Q>
		constexpr operator glm::vec<4, RGBA, Q>() const { return rgba; }
		template <std::integral RGBA, glm::qualifier Q>
		constexpr operator glm::vec<4, RGBA, Q>() const { return glm::vec<4, RGBA, Q>(rgba * 255.f); }
		constexpr explicit operator SDL_Color() const { return SDL_Color{ static_cast<uint8_t>(rgba.r * 255), static_cast<uint8_t>(rgba.g * 255), static_cast<uint8_t>(rgba.b * 255), static_cast<uint8_t>(rgba.a * 255) }; }
		constexpr explicit operator uint32_t() const { return (static_cast<uint32_t>(rgba.r * 255) << 24) | (static_cast<uint32_t>(rgba.g * 255) << 16) | (static_cast<uint32_t>(rgba.b * 255) << 8) | static_cast<uint32_t>(rgba.a * 255); }
		template <std::floating_point RGB, glm::qualifier Q>
		constexpr operator glm::vec<3, RGB, Q>() const { return glm::vec<3, RGB, Q>(rgb); }
		template <std::integral RGB, glm::qualifier Q>
		constexpr operator glm::vec<3, RGB, Q>() const { return glm::vec<3, RGB, Q>(rgb * 255.f); }
		constexpr explicit operator uint8_t() const { return static_cast<uint8_t>(r * 255); }
		template <std::floating_point R>
		constexpr explicit operator R() const { return r; }
		explicit operator			std::string() const
		{
			uint32_t rgba = static_cast<uint32_t>(*this);
			char buf[10];
			snprintf(buf, sizeof(buf), "#%08X", rgba);
			return std::string(buf);
		}

		constexpr glm::vec4   rgba_glsl() const { return rgba; }
		constexpr glm::uvec4  rgba_255() const { return glm::uvec4(rgba * 255.f); }
		constexpr uint32_t    rgba_uint() const { return static_cast<uint32_t>(*this); }
		constexpr glm::vec3   rgb_glsl() const { return glm::vec3(rgba); }
		constexpr glm::uvec3  rgb_255() const { return glm::uvec3(rgba * 255.f); }
		constexpr uint32_t    rgb_uint() const { return static_cast<uint32_t>(*this) & 0xFFFFFF; }
		constexpr uint8_t     gray_255() const { return static_cast<uint8_t>(rgba.r * 255); }
		constexpr float       gray_glsl() const { return rgba.r; }
		std::string			  hex() const { return static_cast<std::string>(*this); }

		//hsv or hsl

		constexpr static color fromHSV(glm::vec3 hsv) { return { hsv_to_rgb(hsv), 1.f }; }
		constexpr glm::vec3 toHSV() const { return rgb_to_hsv(rgb); }

		constexpr static color fromHSV(glm::uvec3 hsv)
		{
			return fromHSV(glm::vec3{ hsv.r / 360.f, hsv.g / 255.f, hsv.b / 255.f });
		}
		constexpr glm::uvec3 toHSV_255() const
		{
			auto hsv = toHSV();
			return { static_cast<uint32_t>(hsv.r * 360.f + 0.5f),
					static_cast<uint32_t>(hsv.g * 255.f + 0.5f),
					static_cast<uint32_t>(hsv.b * 255.f + 0.5f) };
		}

		constexpr static color fromHSL(glm::vec3 hsl) { return { hsl_to_rgb(hsl), 1.f }; }
		constexpr glm::vec3 toHSL() const { return rgb_to_hsl(rgb); }

		constexpr static color fromHSL(glm::uvec3 hsl)
		{
			return fromHSL(glm::vec3{ hsl.r / 360.f, hsl.g / 255.f, hsl.b / 255.f });
		}
		constexpr glm::uvec3 toHSL_255() const
		{
			auto hsl = toHSL();
			return { static_cast<uint32_t>(hsl.r * 360.f + 0.5f),
					static_cast<uint32_t>(hsl.g * 255.f + 0.5f),
					static_cast<uint32_t>(hsl.b * 255.f + 0.5f) };
		}

		color& operator=(const color& other)
		{
			rgba = other.rgba;
			return *this;
		}

		constexpr bool operator==(const color& other) const
		{
			return rgba == other.rgba;
		}

		constexpr bool operator!=(const color& other) const
		{
			return rgba != other.rgba;
		}

		constexpr color operator*(float factor) const
		{
			return glm::clamp(rgba * factor, 0.f, 1.f);
		}

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

		constexpr color operator~() const
		{
			return color(1.f - rgba);
		}

		constexpr color operator&(const color& other) const
		{
			return color(glm::vec4(std::min(rgba.r, other.rgba.r),
				std::min(rgba.g, other.rgba.g),
				std::min(rgba.b, other.rgba.b),
				std::min(rgba.a, other.rgba.a)));
		}

		constexpr color operator|(const color& other) const
		{
			return color(glm::vec4(std::max(rgba.r, other.rgba.r),
				std::max(rgba.g, other.rgba.g),
				std::max(rgba.b, other.rgba.b),
				std::max(rgba.a, other.rgba.a)));
		}

		constexpr static color lerp(const color& a, const color& b, float factor)
		{
			return glm::clamp((1 - factor) * a.rgba + factor * b.rgba, 0.f, 1.f);
		}

		constexpr float luminance() const
		{
			return linearize(rgba.r) * 0.2126f + linearize(rgba.g) * 0.7152f + linearize(rgba.b) * 0.0722f;
		}

		constexpr static float luminance(const color& c)
		{
			return c.luminance();
		}

		constexpr float grayscale() const
		{
			return luminance();
		}

		constexpr static float grayscale(color c)
		{
			return c.grayscale();
		}

		constexpr static float contrast(color a, color b)
		{
			float l1 = a.luminance();
			float l2 = b.luminance();
			if (l1 > l2) return (l1 + 0.05f) / (l2 + 0.05f);
			else return (l2 + 0.05f) / (l1 + 0.05f);
		}

		constexpr static float linearize(float c)
		{
			if (c <= 0.04045f) return c / 12.92f;
			else return std::pow((c + 0.055f) / 1.055f, 2.4f);
		}

		constexpr static float gamma(float c)
		{
			if (c <= 0.0031308f) return c * 12.92f;
			else return 1.055f * std::pow(c, 1 / 2.4f) - 0.055f;
		}

		constexpr color premultiplied()
		{
			return { glm::clamp(rgb * a, 0.f, 1.f), a };
		}

		color& premultiply()
		{
			return (*this) = { glm::clamp(rgb * a, 0.f, 1.f), a };
		}

		constexpr color unmultiplied()
		{
			return a == 0 ? glm::vec4{ 0, 0, 0, a } : glm::vec4{ glm::clamp(rgb / a, 0.f, 1.f), a };
		}

		color& unmultiply()
		{
			return (*this) = a == 0 ? glm::vec4{ 0, 0, 0, a } : glm::vec4{ glm::clamp(rgb / a, 0.f, 1.f), a };
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
				return glm::clamp(glm::vec4{ a.rgb + b.rgb, std::max(a.a, b.a) }, 0.f, 1.f);
			}

			static color multiply(color a, color b)
			{
				return a * b;
			}

			static color screen(color a, color b)
			{
				return glm::clamp(glm::vec4{ glm::vec3(1, 1, 1) - ((~a).rgb * (~b).rgb), std::max(a.a, b.a) }, 0.f, 1.f);
			}
		};

		union
		{
			glm::vec4 rgba;
			struct
			{
				float r, g, b, a;
			};
			struct
			{
				glm::vec3 rgb;
			};
		};

		struct fomats
		{
			enum class fmts
			{
				standart,
				rgba255,
				rgb255,
				frgba,
				frgb,
				hex
			};
			inline static fmts last_format = fmts::standart;
			struct color_fmt
			{
				fmts fmt;
				explicit color_fmt(fmts fmt) : fmt(fmt) {}
			};

			inline static const auto rgba255 = color_fmt(fmts::rgba255);
			inline static const auto rgb255 = color_fmt(fmts::rgb255);
			inline static const auto frgba = color_fmt(fmts::frgba);
			inline static const auto frgb = color_fmt(fmts::frgb);
			inline static const auto hex = color_fmt(fmts::hex);
			inline static const auto standart = color_fmt(fmts::standart);

			friend std::ostream& operator<<(std::ostream& out, color_fmt fmt)
			{
				last_format = fmt.fmt;
				return out;
			}
		};

		friend std::ostream& operator<<(std::ostream& out, color col)
		{
			auto fmt = fomats::last_format;
			switch (fmt)
			{
			case fomats::fmts::standart:
				out << "color(" << col.r << ", " << col.g << ", " << col.b << ", " << col.a << ")";
				break;
			case fomats::fmts::rgba255:
				out << "color(" << static_cast<uint32_t>(col.r * 255) << ", " << static_cast<uint32_t>(col.g * 255) << ", " << static_cast<uint32_t>(col.b * 255) << ", " << static_cast<uint32_t>(col.a * 255) << ")";
				break;
			case fomats::fmts::rgb255:
				out << "color(" << static_cast<uint32_t>(col.r * 255) << ", " << static_cast<uint32_t>(col.g * 255) << ", " << static_cast<uint32_t>(col.b * 255) << ")";
				break;
			case fomats::fmts::frgba:
				out << "color(" << col.r << "f, " << col.g << "f, " << col.b << "f, " << col.a << "f)";
				break;
			case fomats::fmts::frgb:
				out << "color(" << col.r << "f, " << col.g << "f, " << col.b << "f)";
				break;
			case fomats::fmts::hex:
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
				if (c.r == mx) h = (c.g - c.b) / d + (c.g < c.b ? 6.f : 0.f);
				else if (c.g == mx) h = (c.b - c.r) / d + 2.f;
				else h = (c.r - c.g) / d + 4.f;
				h /= 6.f;
			}
			return { h, s, mx };
		}

		static glm::vec3 hsv_to_rgb(glm::vec3 c)
		{
			float h = c.x * 6.f, s = c.y, v = c.z;
			if (s < 1e-6f) return { v, v, v };
			int i = static_cast<int>(h);
			float f = h - i, p = v * (1.f - s), q = v * (1.f - s * f), t = v * (1.f - s * (1.f - f));
			switch (i % 6)
			{
			case 0: return { v, t, p }; case 1: return { q, v, p }; case 2: return { p, v, t };
			case 3: return { p, q, v }; case 4: return { t, p, v }; case 5: return { v, p, q };
			}
			return { v, v, v };
		}

		static glm::vec3 rgb_to_hsl(glm::vec3 c)
		{
			float mn = glm::min(c.r, glm::min(c.g, c.b));
			float mx = glm::max(c.r, glm::max(c.g, c.b));
			float d = mx - mn, l = (mx + mn) * 0.5f;
			float h = 0.f, s = (d < 1e-6f) ? 0.f : d / (l > 0.5f ? 2.f - mx - mn : mx + mn);

			if (d > 1e-6f)
			{
				if (c.r == mx) h = (c.g - c.b) / d + (c.g < c.b ? 6.f : 0.f);
				else if (c.g == mx) h = (c.b - c.r) / d + 2.f;
				else h = (c.r - c.g) / d + 4.f;
				h /= 6.f;
			}
			return { h, s, l };
		}

		static glm::vec3 hsl_to_rgb(glm::vec3 c)
		{
			float h = c.x, s = c.y, l = c.z;
			if (s < 1e-6f) return { l, l, l };
			auto hue = [](float p, float q, float t)
				{
					if (t < 0.f) t += 1.f; if (t > 1.f) t -= 1.f;
					if (t < 1.f / 6.f) return p + (q - p) * 6.f * t;
					if (t < 0.5f) return q;
					if (t < 2.f / 3.f) return p + (q - p) * (2.f / 3.f - t) * 6.f;
					return p;
				};
			float q = l < 0.5f ? l * (1.f + s) : l + s - l * s;
			float p = 2.f * l - q;
			return { hue(p, q, h + 1.f / 3.f), hue(p, q, h), hue(p, q, h - 1.f / 3.f) };
		}
	};
}

constexpr mngl::color operator""rgb(uint64_t x)
{
	return mngl::color((x >> 16) & 255, (x >> 8) & 255, (x & 255));
}

constexpr mngl::color operator""rgba(uint64_t x)
{
	return mngl::color((x >> 24) & 255, (x >> 16) & 255, (x >> 8) & 255, (x & 255));
}

//есть несколько способов хеширования
//#define COLOR_HASH_UINT32_T
//#define COLOR_HASH_STRING
//#define COLOR_HASH_UINT40_T
#define COLOR_HASH_UINT64_T
//#define COLOR_HASH_GLM
namespace std
{
	template<>
	struct hash<mngl::color>
	{
#ifdef COLOR_HASH_UINT32_T
		//32 бита - плюсы, лёгкиц, минусы - жесточайшие колизии, проблемы при 10 битной передаче
		constexpr size_t operator()(const mngl::color& c) const noexcept
		{
			return static_cast<size_t>(static_cast<uint32_t>(c));
		}
#elifdef COLOR_HASH_STRING
		//строка даёт неравномерные коллизии, а так же сильную дистрибутивность хеша, но требует пиздецки много ресурсов для простого хеша
		constexpr size_t operator()(const mngl::color& c) const noexcept
		{
			return std::hash<std::string>()(static_cast<std::string>(c));
		}
#elifdef COLOR_HASH_UINT40_T
		//намного меньше коллизий, но они всё равно есть
		constexpr size_t operator()(const mngl::color& c) const noexcept
		{
			uint64_t r = c.r * (1 << 10);
			uint64_t g = c.g * (1 << 10);
			uint64_t b = c.b * (1 << 10);
			uint64_t a = c.a * (1 << 10);
			return r << 30 | g << 20 | b << 10 | a;
		}
#elifdef COLOR_HASH_UINT64_T
		//ещё меньше колизий, но всё равно для идеала нужно 128 бит, которые мало где поддерживаются
		constexpr size_t operator()(const mngl::color& c) const noexcept
		{
			uint64_t r = c.r * (1 << 16);
			uint64_t g = c.g * (1 << 16);
			uint64_t b = c.b * (1 << 16);
			uint64_t a = c.a * (1 << 16);
			return r << 48 | g << 32 | b << 16 | a;
		}
#elifdef COLOR_HASH_GLM
		//glm хеш вектора. тупо, но работает
		constexpr size_t operator()(const mngl::color& c) const noexcept
		{
			return std::hash<glm::vec4>()(c.rgba);
		}
#endif
	};
}