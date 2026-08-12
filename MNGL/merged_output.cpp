// ===== File: color.hpp =====
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
		template<std::integral R, std::integral G, std::integral B, std::integral A>
		constexpr color(R r, G g, B b, A a = 255) : rgba(r / 255., g / 255., b / 255., a / 255.) {}
		template<std::floating_point R, std::floating_point G, std::floating_point B, std::floating_point A>
		constexpr color(R r, G g, B b, A a = 1.0f) : rgba(r, g, b, a) {}
		constexpr explicit color(uint32_t rgba) : rgba((rgba >> 24) / 255., ((rgba >> 16) & 0xFF) / 255., ((rgba >> 8) & 0xFF) / 255., (rgba & 0xFF) / 255.) {}
		constexpr color(SDL_Color col) : rgba(col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f) {}
		template<std::floating_point RGB, std::floating_point A, glm::qualifier Q>
		constexpr color(glm::vec<3, RGB, Q> col, A a = 1.0f) : rgba(col, a) {}
		template <std::floating_point RGBA, glm::qualifier Q>
		constexpr color(glm::vec<4, RGBA, Q> col) : rgba(col) {}
		template<std::integral RGB, std::integral A, glm::qualifier Q>
		constexpr color(glm::vec<3, RGB, Q> col, A a = 255) : rgba(glm::vec3(col) / 255.f, a / 255.f) {}
		template <std::integral RGBA, glm::qualifier Q>
		constexpr color(glm::vec<4, RGBA, Q> col) : rgba(glm::vec4(col) / 255.f) {}
		template<std::integral G, std::integral A>
		constexpr color(G gray, A a = 255) : rgba(gray / 255.f, gray / 255.f, gray / 255.f, a / 255.f) {}
		template<std::floating_point G, std::floating_point A>
		constexpr color(G gray, A a = 1.0f) : rgba(gray, gray, gray, a) {}
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

// ===== File: logger.h =====
#pragma once
#include "color.hpp"
#include <fstream>
#include <iostream>
#include <array>
#include <chrono>
#include <string>
#include <format>
#include <mutex>

template<typename T>
concept Streamable = requires(std::ostream & os, T t)
{
	{ os << t } -> std::same_as<std::ostream&>;
};

class Logger
{
public:
	void setColor(mngl::color col)
	{
		auto rgba = col.rgba_255();
		cout << "\x1b[38;2;" << (int)rgba.r << ";" << (int)rgba.g << ";" << (int)rgba.b << "m" << std::flush;
	}
	void clearColor()
	{
		cout << "\x1b[0m";
	}

	Logger(std::ostream& cout, std::string out_file = "last.log") : cout(cout), fout(out_file) {}
	~Logger() = default;
	Logger() = delete;
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;
	Logger(Logger&&) = delete;

	template<Streamable... T>
	Logger& operator()(T&&... out)
	{
		((cout << std::forward<T>(out)), ...);
		((fout << std::forward<T>(out)), ...);
		cout << std::endl;
		fout << std::endl;
		return *this;
	}

	template<Streamable... T>
	void log(T&&... out)
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		cout << time() << "[LOG] ";
		this->operator()(out...);
	}

	template<Streamable... T>
	void debug(T&&... out)
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		if (debug_output)
		{
			cout << time() << "[DEBUG] ";
			this->operator()(out...);
		}
	}

	template<Streamable... T>
	void debug_warning(T&&... out)
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		if (debug_output)
		{
			this->setColor(mngl::color(0xF5, 0x9E, 0x0B));
			cout << time() << "[DEBUG/WARNING] ";
			this->operator()(out...);
			this->clearColor();
		}
	}

	template<Streamable... T>
	void info(T&&... out)
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		this->setColor(mngl::color(0x3B, 0x82, 0xF6));
		cout << time() << "[INFO] ";
		this->operator()(std::forward<T>(out)...);
		this->clearColor();
	}

	template<Streamable... T>
	void success(T&&... out)
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		this->setColor(mngl::color(0x22, 0xC5, 0x5E));
		cout << time() << "[SUCCESS] ";
		this->operator()(std::forward<T>(out)...);
		this->clearColor();
	}

	template<Streamable... T>
	void warning(T&&... out)
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		this->setColor(mngl::color(0xF5, 0x9E, 0x0B));
		cout << time() << "[WARNING] ";
		this->operator()(std::forward<T>(out)...);
		this->clearColor();
	}

	template<Streamable... T>
	void error(T&&... out)
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		this->setColor(mngl::color(0xEF, 0x44, 0x44));
		cout << time() << "[ERROR] ";
		this->operator()(std::forward<T>(out)...);
		this->clearColor();
	}


	template<Streamable... T>
	void critical(T&&... out)
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		this->setColor(mngl::color(0xDC, 0x26, 0x26));
		cout << time() << "[CRITICAL] ";
		this->operator()(std::forward<T>(out)...);
		this->clearColor();
	}

	void flush()
	{
		cout.flush();
		fout.flush();
	}

	void enableDebug()
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		debug_output = true;
	}

	void disableDebug()
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		debug_output = false;
	}

	void setDebug(bool to)
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		debug_output = to;
	}

	bool debugged() const
	{
		std::lock_guard<std::mutex> _lock_(mutex);
		return debug_output;
	}

private:

	std::string time()
	{
		auto now = std::chrono::system_clock::now();
		auto local_time = std::chrono::zoned_time{ std::chrono::current_zone(), now }.get_local_time();
		auto dp = std::chrono::floor<std::chrono::days>(local_time);
		auto time_of_day = local_time - dp;
		auto hms = std::chrono::hh_mm_ss{ time_of_day };
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(hms.subseconds()).count();

		// Используем {:03} для миллисекунд
		return std::format("[{:02}:{:02}:{:02}:{:03}]",
			hms.hours().count(),
			hms.minutes().count(),
			hms.seconds().count(),
			ms);
	}

	std::ofstream fout;
	std::ostream& cout;

	mutable std::mutex mutex;
	bool debug_output =
#ifdef _DEBUG
		true
#else
		false
#endif
		;
};

inline Logger logger(std::cout);

// ===== File: proxy.hpp =====
#pragma once
#include <type_traits>

namespace mngl
{
    //proxy для объектов из рода SDL_Rect и др, для сокращения кода
    template<typename F, typename T,
        T(*Convert)(const F&),
        void (*Sync)(F&, const T&)>
    class Proxy
    {
    public:
        explicit Proxy(F& obj) : obj_(obj), temp_(Convert(obj_)) {}

        operator T* () { return &temp_; }
        operator const T* () const { return &temp_; }

        ~Proxy() { Sync(obj_, temp_); }

        Proxy(const Proxy&) = delete;
        Proxy& operator=(const Proxy&) = delete;
    private:
        F& obj_;
        T temp_;
    };
}

// ===== File: iupdate.h =====
#pragma once

#include <vector>

namespace mngl
{
	// не использовать для сложных иерархий, это класс для простых обновлений по типу глобальных анимаций и другой шелухи
	class Iupdatable
	{
	public:
		Iupdatable();
		virtual ~Iupdatable();
		virtual void update(double dt) = 0;
	};

	class update_manager
	{
	public:
		update_manager() = delete;

		static void tick(double dt)
		{
			for (size_t i = 0; i < updatables.size(); ++i)
			{
				if (updatables[i])
				{
					updatables[i]->update(dt);
				}
			}

			// В конце кадра в один проход вычищаем все nullptr, которые образовались
			updatables.erase(
				std::remove(updatables.begin(), updatables.end(), nullptr),
				updatables.end()
			);
		}
	private:
		static inline std::vector<Iupdatable*> updatables;
		friend class Iupdatable;

		static void add(Iupdatable* i)
		{
			updatables.push_back(i);
		}

		static void remove(Iupdatable* i)
		{
			auto it = std::find(updatables.begin(), updatables.end(), i);
			if (it != updatables.end())
				*it = nullptr;
		}
	};

	Iupdatable::~Iupdatable()
	{
		update_manager::remove(this);
	}

	Iupdatable::Iupdatable()
	{
		update_manager::add(this);
	}
}

// ===== File: utilities.hpp =====
#pragma once
#pragma comment(lib, "SDL3.lib")
#pragma comment(lib, "SDL3_image.lib")
#pragma comment(lib, "SDL3_test.lib")
#pragma comment(lib, "freetype.lib")

// определим информацию о приложении, если не была задана пользователем

#define STR(x) #x

#ifndef APP_NAME
#define APP_NAME "SDL3 app"
#endif

#ifndef APP_VERSION
#define APP_VERSION 09072026
#endif

#ifndef APP_ID
#define APP_ID "com.sdl3.mngl." STR(APP_VERSION)
#endif

#ifndef APP_FLAGS
#define APP_FLAGS \
		SDL_INIT_AUDIO   |\
		SDL_INIT_VIDEO   |\
		SDL_INIT_GAMEPAD |\
		SDL_INIT_HAPTIC  |\
		SDL_INIT_SENSOR  |\
		SDL_INIT_CAMERA
#endif


// для хешей
#define GLM_ENABLE_EXPERIMENTAL

// для ui на базе IMGUI
#ifdef IMGUI_VERSION
#define MNGL_IMGUI
#endif

// включаем utf-8 и виртуализацию терминала на уровне винды
#ifdef _WIN32
#define NOMINMAX
#ifndef _WINDOWS_
#include <Windows.h>
#endif

class __onBegin
{
public:
	__onBegin()
	{
		SetConsoleCP(CP_UTF8);
		SetConsoleOutputCP(CP_UTF8);

		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

		DWORD mode = 0;
		GetConsoleMode(hOut, &mode);
		SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}
};

inline __onBegin __onBeginInstance;

#endif

// включаем SDL3 и инициализируем его
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdexcept>
#include <string>

class __SDL_While_Runtime__
{
	__SDL_While_Runtime__()
	{
		if (!SDL_SetAppMetadata(APP_NAME, STR(APP_VERSION), APP_ID))
		{
			has_err = true;
			err = "SDL metadata setup error: " + std::string(SDL_GetError());
			return;
		}
		if (!SDL_Init(init_flags))
		{
			has_err = true;
			err = ("SDL Init setup error: " + std::string(SDL_GetError()));
			return;
		}
		is_running = true;
	}

public:

	inline static constexpr uint32_t init_flags = APP_FLAGS; // флаги инициализации

	__SDL_While_Runtime__(const __SDL_While_Runtime__& other) = delete;
	__SDL_While_Runtime__(__SDL_While_Runtime__&& other) = delete;

	~__SDL_While_Runtime__()
	{
		is_running = false;
		SDL_Quit();
	}

	friend std::ostream& operator << (std::ostream& out, const __SDL_While_Runtime__& self)
	{
		out << std::string(self);
		return out;
	}

	operator std::string() const
	{
		return err;
	}

	operator bool() const
	{
		return has_err;
	}

	static bool error()
	{
		return has_err;
	}

	static std::string content()
	{
		return err;
	}

	static bool running()
	{
		return is_running;
	}

	static bool exit()
	{
		return is_exit;
	}

	static __SDL_While_Runtime__& instance()
	{
		static __SDL_While_Runtime__ inst;
		return inst;
	}

private:
	inline static bool is_running = false;
	inline static bool has_err = false;
	inline static bool is_exit = false;
	inline static std::string err;
};

inline auto& SDL_Runtime = __SDL_While_Runtime__::instance();

// ошибки с информацией из SDL3
class [[nodiscard]] sdl_error : public std::runtime_error
{
public:
	using _Mybase = runtime_error;
	explicit sdl_error(const std::string& _Message) : _Mybase((_Message + SDL_GetError()).c_str()) {}
};

#include "logger.h"
#include "proxy.hpp"
#include "iupdate.h"

// макросы для чеканья ошибок SDL3

#define TRY(expr, msg) do{ if(!(expr)) throw sdl_error(msg); } while(0)
#define TRY_SDL(expr) TRY(expr, "SDL error: ")

#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/string_cast.hpp>

#ifdef _DEBUG
constexpr inline bool debug_mode = true;
#else
constexpr inline bool debug_mode = false;
#endif

// ===== File: pixel_format.hpp =====
#pragma once

#include "utilities.hpp"

namespace mngl
{
	enum class pixel_format
	{
		unknown = SDL_PIXELFORMAT_UNKNOWN,
		index1lsb = SDL_PIXELFORMAT_INDEX1LSB,
		index1msb = SDL_PIXELFORMAT_INDEX1MSB,
		index2lsb = SDL_PIXELFORMAT_INDEX2LSB,
		index2msb = SDL_PIXELFORMAT_INDEX2MSB,
		index4lsb = SDL_PIXELFORMAT_INDEX4LSB,
		index4msb = SDL_PIXELFORMAT_INDEX4MSB,
		index8 = SDL_PIXELFORMAT_INDEX8,
		rgb332 = SDL_PIXELFORMAT_RGB332,
		xrgb4444 = SDL_PIXELFORMAT_XRGB4444,
		xbgr4444 = SDL_PIXELFORMAT_XBGR4444,
		xrgb1555 = SDL_PIXELFORMAT_XRGB1555,
		xbgr1555 = SDL_PIXELFORMAT_XBGR1555,
		argb4444 = SDL_PIXELFORMAT_ARGB4444,
		rgba4444 = SDL_PIXELFORMAT_RGBA4444,
		abgr4444 = SDL_PIXELFORMAT_ABGR4444,
		bgra4444 = SDL_PIXELFORMAT_BGRA4444,
		argb1555 = SDL_PIXELFORMAT_ARGB1555,
		rgba5551 = SDL_PIXELFORMAT_RGBA5551,
		abgr1555 = SDL_PIXELFORMAT_ABGR1555,
		bgra5551 = SDL_PIXELFORMAT_BGRA5551,
		rgb565 = SDL_PIXELFORMAT_RGB565,
		bgr565 = SDL_PIXELFORMAT_BGR565,
		rgb24 = SDL_PIXELFORMAT_RGB24,
		bgr24 = SDL_PIXELFORMAT_BGR24,
		xrgb8888 = SDL_PIXELFORMAT_XRGB8888,
		rgbx8888 = SDL_PIXELFORMAT_RGBX8888,
		xbgr8888 = SDL_PIXELFORMAT_XBGR8888,
		bgrx8888 = SDL_PIXELFORMAT_BGRX8888,
		argb8888 = SDL_PIXELFORMAT_ARGB8888,
		rgba8888 = SDL_PIXELFORMAT_RGBA8888,
		abgr8888 = SDL_PIXELFORMAT_ABGR8888,
		bgra8888 = SDL_PIXELFORMAT_BGRA8888,
		xrgb2101010 = SDL_PIXELFORMAT_XRGB2101010,
		xbgr2101010 = SDL_PIXELFORMAT_XBGR2101010,
		argb2101010 = SDL_PIXELFORMAT_ARGB2101010,
		abgr2101010 = SDL_PIXELFORMAT_ABGR2101010,
		rgb48 = SDL_PIXELFORMAT_RGB48,
		bgr48 = SDL_PIXELFORMAT_BGR48,
		rgba64 = SDL_PIXELFORMAT_RGBA64,
		argb64 = SDL_PIXELFORMAT_ARGB64,
		bgra64 = SDL_PIXELFORMAT_BGRA64,
		abgr64 = SDL_PIXELFORMAT_ABGR64,
		rgb48_float = SDL_PIXELFORMAT_RGB48_FLOAT,
		bgr48_float = SDL_PIXELFORMAT_BGR48_FLOAT,
		rgba64_float = SDL_PIXELFORMAT_RGBA64_FLOAT,
		argb64_float = SDL_PIXELFORMAT_ARGB64_FLOAT,
		bgra64_float = SDL_PIXELFORMAT_BGRA64_FLOAT,
		abgr64_float = SDL_PIXELFORMAT_ABGR64_FLOAT,
		rgb96_float = SDL_PIXELFORMAT_RGB96_FLOAT,
		bgr96_float = SDL_PIXELFORMAT_BGR96_FLOAT,
		rgba128_float = SDL_PIXELFORMAT_RGBA128_FLOAT,
		argb128_float = SDL_PIXELFORMAT_ARGB128_FLOAT,
		bgra128_float = SDL_PIXELFORMAT_BGRA128_FLOAT,
		abgr128_float = SDL_PIXELFORMAT_ABGR128_FLOAT,
		yv12 = SDL_PIXELFORMAT_YV12,
		iyuv = SDL_PIXELFORMAT_IYUV,
		yuy2 = SDL_PIXELFORMAT_YUY2,
		uyvy = SDL_PIXELFORMAT_UYVY,
		yvyu = SDL_PIXELFORMAT_YVYU,
		nv12 = SDL_PIXELFORMAT_NV12,
		nv21 = SDL_PIXELFORMAT_NV21,
		p010 = SDL_PIXELFORMAT_P010,
		external_oes = SDL_PIXELFORMAT_EXTERNAL_OES,
		mjpg = SDL_PIXELFORMAT_MJPG,

		// Platform-dependent aliases (resolved by SDL headers)
		rgba32 = SDL_PIXELFORMAT_RGBA32,
		argb32 = SDL_PIXELFORMAT_ARGB32,
		bgra32 = SDL_PIXELFORMAT_BGRA32,
		abgr32 = SDL_PIXELFORMAT_ABGR32,
		rgbx32 = SDL_PIXELFORMAT_RGBX32,
		xrgb32 = SDL_PIXELFORMAT_XRGB32,
		bgrx32 = SDL_PIXELFORMAT_BGRX32,
		xbgr32 = SDL_PIXELFORMAT_XBGR32
	};
}

// ===== File: line.hpp =====
#pragma once
#include "utilities.hpp"

namespace mngl
{
	bool linesIntersect(glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec2 p4, float epsilon = SDL_FLT_EPSILON)
	{
		float denom = (p4.y - p3.y) * (p2.x - p1.x) - (p4.x - p3.x) * (p2.y - p1.y);
		if (abs(denom) < epsilon)
		{
			return false;
		}
		float ua = ((p4.x - p3.x) * (p1.y - p3.y) - (p4.y - p3.y) * (p1.x - p3.x)) / denom;
		float ub = ((p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x)) / denom;
		return ua >= 0.0f && ua <= 1.0f && ub >= 0.0f && ub <= 1.0f;
	}

	struct line
	{
		glm::vec2 start;
		glm::vec2 end;
		line() = default;
		line(glm::vec2 s, glm::vec2 e) : start(s), end(e) {}
		bool intersects(const line& other) const
		{
			return linesIntersect(start, end, other.start, other.end);
		}
		friend bool intersects(const line& a, const line& b)
		{
			return a.intersects(b);
		}
		float length() const
		{
			return glm::length(end - start);
		}

		glm::vec2 direction() const
		{
			return glm::normalize(end - start);
		}

		std::optional<glm::vec2> intersectionPoint(const line& other, float epsilon = SDL_FLT_EPSILON) const
		{
			float denom = (other.end.y - other.start.y) * (end.x - start.x) - (other.end.x - other.start.x) * (end.y - start.y);
			if (abs(denom) < epsilon)
			{
				return std::nullopt;
			}
			float ua = ((other.end.x - other.start.x) * (start.y - other.start.y) - (other.end.y - other.start.y) * (start.x - other.start.x)) / denom;
			float ub = ((end.x - start.x) * (other.start.y - start.y) - (end.y - start.y) * (other.start.x - start.x)) / denom;
			if (ua >= 0.0f && ua <= 1.0f && ub >= 0.0f && ub <= 1.0f)
			{
				return start + ua * (end - start);
			}
			else
			{
				return std::nullopt;
			}
		}
	};
}

// ===== File: rectangle.hpp =====
#pragma once
#include "utilities.hpp"
#include "line.hpp"
#include <span>
namespace mngl
{
	struct rectangle
	{
		union
		{
			glm::vec2 pos;
			struct
			{
				float x, y;
			};
		};

		union
		{
			glm::vec2 size;
			struct
			{
				float w, h;
			};
		};

		rectangle() = default;
		rectangle(glm::vec2 p, glm::vec2 s) : pos(p), size(s) {}
		rectangle(float x, float y, float w, float h) : pos(x, y), size(w, h) {}
		rectangle(const SDL_Rect& r) : pos(r.x, r.y), size(r.w, r.h) {}
		rectangle(const SDL_FRect& r) : pos(r.x, r.y), size(r.w, r.h) {}

		bool empty() const
		{
			return size.x < 0 || size.y < 0;
		}
		bool equals(const rectangle& other, double epsilon = SDL_FLT_EPSILON) const
		{
			return (
				fabs(pos.x - other.pos.x) <= epsilon &&
				fabs(pos.y - other.pos.y) <= epsilon &&
				fabs(size.x - other.size.x) <= epsilon &&
				fabs(size.y - other.size.y) <= epsilon
				);
		}

		bool operator == (const rectangle& other) const
		{
			return equals(other);
		}

		bool operator != (const rectangle& other) const
		{
			return !equals(other);
		}

		bool intersects(const rectangle& other) const
		{
			return !(
				pos.x + size.x <= other.pos.x ||
				pos.y + size.y <= other.pos.y ||
				other.pos.x + other.size.x <= pos.x ||
				other.pos.y + other.size.y <= pos.y
				);
		}
		friend bool intersects(const rectangle& a, const rectangle& b)
		{
			return a.intersects(b);
		}

		rectangle intersection(const rectangle& other) const
		{
			if (!intersects(other))
			{
				return rectangle{ {0, 0}, {-1, -1} };
			}
			else
			{
				glm::vec2 newPos{
					std::max(pos.x, other.pos.x),
					std::max(pos.y, other.pos.y)
				};
				glm::vec2 newSize{
					std::min(pos.x + size.x, other.pos.x + other.size.x) - newPos.x,
					std::min(pos.y + size.y, other.pos.y + other.size.y) - newPos.y
				};
				return rectangle{ newPos, newSize };
			}
		}
		friend rectangle intersection(const rectangle& a, const rectangle& b)
		{
			return a.intersection(b);
		}

		bool intersectsLine(glm::vec2 lineStart, glm::vec2 lineEnd) const
		{
			return intersectsLine(line{ lineStart, lineEnd });
		}

		bool intersectsLine(const line& l) const
		{
			line top{ pos, {pos.x + size.x, pos.y} };
			line bottom{ {pos.x, pos.y + size.y}, {pos.x + size.x, pos.y + size.y} };
			line left{ pos, {pos.x, pos.y + size.y} };
			line right{ {pos.x + size.x, pos.y}, {pos.x + size.x, pos.y + size.y} };
			return l.intersects(top) ||
				l.intersects(bottom) ||
				l.intersects(left) ||
				l.intersects(right) ||
				contains(l.start) ||
				contains(l.end);
		}

		friend bool intersectsLine(const rectangle& other, glm::vec2 lineStart, glm::vec2 lineEnd)
		{
			return other.intersectsLine(line{ lineStart, lineEnd });
		}

		friend bool intersectsLine(const rectangle& other, const line& l)
		{
			return other.intersectsLine(l);
		}

		friend bool intersectsLine(glm::vec2 lineStart, glm::vec2 lineEnd, const rectangle& other)
		{
			return other.intersectsLine(line{ lineStart, lineEnd });
		}

		friend bool intersectsLine(const line& l, const rectangle& other)
		{
			return other.intersectsLine(l);
		}

		rectangle unionWith(const rectangle& other) const
		{

			glm::vec2 newPos{
				std::min(pos.x, other.pos.x),
				std::min(pos.y, other.pos.y)
			};
			glm::vec2 newSize{
				std::max(pos.x + size.x, other.pos.x + other.size.x) - newPos.x,
				std::max(pos.y + size.y, other.pos.y + other.size.y) - newPos.y
			};
			return rectangle{ newPos, newSize };
		}

		rectangle operator*(const rectangle& other) const
		{
			return intersection(other);
		}

		rectangle& operator *= (const rectangle& other)
		{
			*this = intersection(other);
			return *this;
		}

		rectangle operator+(const rectangle& other) const
		{
			return unionWith(other);
		}

		rectangle& operator += (const rectangle& other)
		{
			*this = unionWith(other);
			return *this;
		}

		friend rectangle enclosing_points(std::span<const glm::vec2> points, const rectangle& clip)
		{
			if (points.empty()) return { {0, 0}, {-1, -1} };

			float minX = points[0].x, maxX = points[0].x;
			float minY = points[0].y, maxY = points[0].y;

			for (size_t i = 1; i < points.size(); ++i)
			{
				minX = std::min(minX, points[i].x);
				maxX = std::max(maxX, points[i].x);
				minY = std::min(minY, points[i].y);
				maxY = std::max(maxY, points[i].y);
			}

			return rectangle{ {minX, minY}, {maxX - minX, maxY - minY} } * clip;
		}

		bool contains(glm::vec2 point) const
		{
			return (
				point.x >= pos.x &&
				point.x <= pos.x + size.x &&
				point.y >= pos.y &&
				point.y <= pos.y + size.y
				);
		}

		operator SDL_FRect() const
		{
			return { pos.x, pos.y, size.x, size.y };
		}

		operator SDL_FRect* () const
		{
			return (SDL_FRect*)(&pos);
		}

		operator SDL_FRect& ()
		{
			return *(SDL_FRect*)(&pos);
		}

		operator SDL_Rect() const
		{
			return { (int)pos.x, (int)pos.y, (int)size.x, (int)size.y };
		}

		glm::vec2 center() const
		{
			return pos + size * 0.5f;
		}

		float area() const
		{
			return size.x * size.y;
		}

		float perimeter() const
		{
			return 2 * (size.x + size.y);
		}

		rectangle inflate(float amount)
		{
			return rectangle{ pos - glm::vec2(amount), size + glm::vec2(2 * amount) };
		}

		glm::vec2 clamp(glm::vec2 point) const
		{
			return glm::clamp(point, pos, pos + size);
		}

		friend glm::vec2 clamp(glm::vec2 point, const rectangle& rect)
		{
			return rect.clamp(point);
		}

		glm::vec2 local(glm::vec2 point) const
		{
			return point - pos;
		}

		glm::vec2 global(glm::vec2 point) const
		{
			return point + pos;
		}

		float intersectionArea(const rectangle& other) const
		{
			rectangle inter = intersection(other);
			if (inter.empty()) return 0.0f;
			return inter.area();
		}

		friend float intersectionArea(const rectangle& a, const rectangle& b)
		{
			return a.intersectionArea(b);
		}
	};
	using rectangleProxy = Proxy < rectangle, SDL_Rect, [](const rectangle& r) { return SDL_Rect{ (int)r.pos.x, (int)r.pos.y, (int)r.size.x, (int)r.size.y }; }, [](rectangle& r, const SDL_Rect& s) { r.pos.x = s.x; r.pos.y = s.y; r.size.x = s.w; r.size.y = s.h; } > ;
}

// ===== File: surface.hpp =====
#pragma once
#include "utilities.hpp"
#include "iupdate.h"
#include <string>
#include <fstream>
#include <functional>

namespace mngl
{
	class surface : Iupdatable
	{
	public:
		surface(const std::string& path)
		{
			if (IMG_Animation* a = IMG_LoadAnimation(path.c_str()))
			{
				if (anim->count > 1) anim = a;
				else
				{
					single = a->frames[0];
					IMG_FreeAnimation(a);
				}
			}
			else if(SDL_Surface* s = IMG_Load(path.c_str()))
			{
				single = s;
			}
			else
			{
				throw sdl_error("failed to load surface from " + path + ": ");
			}
		}

		~surface()
		{
			if (anim) IMG_FreeAnimation(anim);
			if (single) SDL_DestroySurface(single);
		}

		surface(surface&& other) noexcept = default;
		surface& operator=(surface&& other) noexcept = default;

		void update(double dt)
		{
			if (!anim || anim->count <= 1) return;
			accum += dt * 1'000;
			int cur_delay = anim->delays[current_frame];
			if (cur_delay <= 0) cur_delay = 100;
			while (accum >= cur_delay)
			{
				accum -= cur_delay;
				int cur_delay = anim->delays[current_frame];
				if (cur_delay <= 0) cur_delay = 100;

				current_frame = (current_frame + 1) % anim->count;
				if (on_frame_changed) on_frame_changed(anim->frames[current_frame]);
			}
		}

		SDL_Surface* raw() const noexcept
		{
			if (animated()) return anim->frames[current_frame];
			return single;
		}

		bool animated() const
		{
			return anim;
		}

		std::function<void(SDL_Surface*)> on_frame_changed;

	private:
		
		SDL_Surface* single;
		IMG_Animation* anim;

		size_t current_frame = 0;
		float accum = 0;
	};
}

// ===== File: event.hpp =====
#pragma once

#include "utilities.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace mngl
{
    enum class Events
    {
        nothing = -1,

        // --- Application ---
        quit = SDL_EVENT_QUIT,
        terminating = SDL_EVENT_TERMINATING,
        low_memory = SDL_EVENT_LOW_MEMORY,
        will_enter_background = SDL_EVENT_WILL_ENTER_BACKGROUND,
        did_enter_background = SDL_EVENT_DID_ENTER_BACKGROUND,
        will_enter_foreground = SDL_EVENT_WILL_ENTER_FOREGROUND,
        did_enter_foreground = SDL_EVENT_DID_ENTER_FOREGROUND,
        locale_changed = SDL_EVENT_LOCALE_CHANGED,
        system_theme_changed = SDL_EVENT_SYSTEM_THEME_CHANGED,

        // --- Display ---
        display_orientation = SDL_EVENT_DISPLAY_ORIENTATION,
        display_added = SDL_EVENT_DISPLAY_ADDED,
        display_removed = SDL_EVENT_DISPLAY_REMOVED,
        display_moved = SDL_EVENT_DISPLAY_MOVED,
        display_desktop_mode_changed = SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED,
        display_current_mode_changed = SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED,
        display_content_scale_changed = SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED,
        display_usable_bounds_changed = SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED,

        // --- Window ---
        window_shown = SDL_EVENT_WINDOW_SHOWN,
        window_hidden = SDL_EVENT_WINDOW_HIDDEN,
        window_exposed = SDL_EVENT_WINDOW_EXPOSED,
        window_moved = SDL_EVENT_WINDOW_MOVED,
        window_resized = SDL_EVENT_WINDOW_RESIZED,
        window_pixel_size_changed = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,
        window_metal_view_resized = SDL_EVENT_WINDOW_METAL_VIEW_RESIZED,
        window_minimized = SDL_EVENT_WINDOW_MINIMIZED,
        window_maximized = SDL_EVENT_WINDOW_MAXIMIZED,
        window_restored = SDL_EVENT_WINDOW_RESTORED,
        window_mouse_enter = SDL_EVENT_WINDOW_MOUSE_ENTER,
        window_mouse_leave = SDL_EVENT_WINDOW_MOUSE_LEAVE,
        window_focus_gained = SDL_EVENT_WINDOW_FOCUS_GAINED,
        window_focus_lost = SDL_EVENT_WINDOW_FOCUS_LOST,
        window_close_request = SDL_EVENT_WINDOW_CLOSE_REQUESTED,
        window_hit_test = SDL_EVENT_WINDOW_HIT_TEST,
        window_iccprof_changed = SDL_EVENT_WINDOW_ICCPROF_CHANGED,
        window_display_changed = SDL_EVENT_WINDOW_DISPLAY_CHANGED,
        window_display_scale_changed = SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED,
        window_safe_area_changed = SDL_EVENT_WINDOW_SAFE_AREA_CHANGED,
        window_occluded = SDL_EVENT_WINDOW_OCCLUDED,
        window_enter_fullscreen = SDL_EVENT_WINDOW_ENTER_FULLSCREEN,
        window_leave_fullscreen = SDL_EVENT_WINDOW_LEAVE_FULLSCREEN,
        window_destroyed = SDL_EVENT_WINDOW_DESTROYED,
        window_hdr_state_changed = SDL_EVENT_WINDOW_HDR_STATE_CHANGED,

        // --- Keyboard ---
        key_down = SDL_EVENT_KEY_DOWN,
        key_up = SDL_EVENT_KEY_UP,
        text_editing = SDL_EVENT_TEXT_EDITING,
        text_input = SDL_EVENT_TEXT_INPUT,
        keymap_changed = SDL_EVENT_KEYMAP_CHANGED,
        keyboard_added = SDL_EVENT_KEYBOARD_ADDED,
        keyboard_removed = SDL_EVENT_KEYBOARD_REMOVED,
        text_editing_candidates = SDL_EVENT_TEXT_EDITING_CANDIDATES,
        screen_keyboard_shown = SDL_EVENT_SCREEN_KEYBOARD_SHOWN,
        screen_keyboard_hidden = SDL_EVENT_SCREEN_KEYBOARD_HIDDEN,

        // --- Mouse ---
        mouse_motion = SDL_EVENT_MOUSE_MOTION,
        mouse_button_down = SDL_EVENT_MOUSE_BUTTON_DOWN,
        mouse_button_up = SDL_EVENT_MOUSE_BUTTON_UP,
        mouse_wheel = SDL_EVENT_MOUSE_WHEEL,
        mouse_added = SDL_EVENT_MOUSE_ADDED,
        mouse_removed = SDL_EVENT_MOUSE_REMOVED,

        // --- Joystick ---
        joystick_axis_motion = SDL_EVENT_JOYSTICK_AXIS_MOTION,
        joystick_ball_motion = SDL_EVENT_JOYSTICK_BALL_MOTION,
        joystick_hat_motion = SDL_EVENT_JOYSTICK_HAT_MOTION,
        joystick_button_down = SDL_EVENT_JOYSTICK_BUTTON_DOWN,
        joystick_button_up = SDL_EVENT_JOYSTICK_BUTTON_UP,
        joystick_added = SDL_EVENT_JOYSTICK_ADDED,
        joystick_removed = SDL_EVENT_JOYSTICK_REMOVED,
        joystick_battery_updated = SDL_EVENT_JOYSTICK_BATTERY_UPDATED,
        joystick_update_complete = SDL_EVENT_JOYSTICK_UPDATE_COMPLETE,

        // --- Gamepad ---
        gamepad_axis_motion = SDL_EVENT_GAMEPAD_AXIS_MOTION,
        gamepad_button_down = SDL_EVENT_GAMEPAD_BUTTON_DOWN,
        gamepad_button_up = SDL_EVENT_GAMEPAD_BUTTON_UP,
        gamepad_added = SDL_EVENT_GAMEPAD_ADDED,
        gamepad_removed = SDL_EVENT_GAMEPAD_REMOVED,
        gamepad_remapped = SDL_EVENT_GAMEPAD_REMAPPED,
        gamepad_touchpad_down = SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN,
        gamepad_touchpad_motion = SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION,
        gamepad_touchpad_up = SDL_EVENT_GAMEPAD_TOUCHPAD_UP,
        gamepad_sensor_update = SDL_EVENT_GAMEPAD_SENSOR_UPDATE,
        gamepad_update_complete = SDL_EVENT_GAMEPAD_UPDATE_COMPLETE,
        gamepad_steam_handle_updated = SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED,

        // --- Touch ---
        finger_down = SDL_EVENT_FINGER_DOWN,
        finger_up = SDL_EVENT_FINGER_UP,
        finger_motion = SDL_EVENT_FINGER_MOTION,
        finger_canceled = SDL_EVENT_FINGER_CANCELED,

        // --- Gesture (pinch) ---
        pinch_begin = SDL_EVENT_PINCH_BEGIN,
        pinch_update = SDL_EVENT_PINCH_UPDATE,
        pinch_end = SDL_EVENT_PINCH_END,

        // --- Clipboard ---
        clipboard_update = SDL_EVENT_CLIPBOARD_UPDATE,

        // --- Drag & Drop ---
        drop_file = SDL_EVENT_DROP_FILE,
        drop_text = SDL_EVENT_DROP_TEXT,
        drop_begin = SDL_EVENT_DROP_BEGIN,
        drop_complete = SDL_EVENT_DROP_COMPLETE,
        drop_position = SDL_EVENT_DROP_POSITION,

        // --- Audio ---
        audio_device_added = SDL_EVENT_AUDIO_DEVICE_ADDED,
        audio_device_removed = SDL_EVENT_AUDIO_DEVICE_REMOVED,
        audio_device_format_changed = SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED,

        // --- Sensor ---
        sensor_update = SDL_EVENT_SENSOR_UPDATE,

        // --- Pen ---
        pen_proximity_in = SDL_EVENT_PEN_PROXIMITY_IN,
        pen_proximity_out = SDL_EVENT_PEN_PROXIMITY_OUT,
        pen_down = SDL_EVENT_PEN_DOWN,
        pen_up = SDL_EVENT_PEN_UP,
        pen_button_down = SDL_EVENT_PEN_BUTTON_DOWN,
        pen_button_up = SDL_EVENT_PEN_BUTTON_UP,
        pen_motion = SDL_EVENT_PEN_MOTION,
        pen_axis = SDL_EVENT_PEN_AXIS,

        // --- Camera ---
        camera_device_added = SDL_EVENT_CAMERA_DEVICE_ADDED,
        camera_device_removed = SDL_EVENT_CAMERA_DEVICE_REMOVED,
        camera_device_approved = SDL_EVENT_CAMERA_DEVICE_APPROVED,
        camera_device_denied = SDL_EVENT_CAMERA_DEVICE_DENIED,

        // --- Render ---
        render_targets_reset = SDL_EVENT_RENDER_TARGETS_RESET,
        render_device_reset = SDL_EVENT_RENDER_DEVICE_RESET,
        render_device_lost = SDL_EVENT_RENDER_DEVICE_LOST,

        // --- Internal / misc ---
        poll_sentinel = SDL_EVENT_POLL_SENTINEL,

        // --- User ---
        user = SDL_EVENT_USER,
    };

    enum class hat_position : uint8_t { centered, up, leftup, left, leftdown, down, rightdown, right, rightup, none = 0xFF };
    enum class power_state : uint8_t { unknown, on_battery, no_battery, charging, charged, error = 0xFF };
    enum class wheel_direction : uint8_t { normal, flipped };
    enum class drop_type : uint8_t { none, file, text, begin, complete, position };
    enum class camera_status : uint8_t { added, removed, approved, denied };
    enum class render_reason : uint8_t { targets_reset, device_reset, device_lost };
    enum class pen_axis_type : uint8_t { pressure, xtilt, ytilt, rotation, slider, distance };
    enum class gamepad_axis : uint8_t { invalid, leftx, lefty, rightx, righty, lefttrigger, righttrigger };
    enum class gamepad_button : uint8_t
    {
        invalid, a, b, x, y, back, guide, start,
        leftstick, rightstick, leftshoulder, rightshoulder,
        dpad_up, dpad_down, dpad_left, dpad_right, misc1, misc2
    };
    enum class sensor_type : uint8_t { invalid, accelerometer, gyroscope, accelerometer_uncalibrated, gyroscope_uncalibrated };
    enum class display_orientation
    {
        unknown = SDL_ORIENTATION_UNKNOWN,
        landscape = SDL_ORIENTATION_LANDSCAPE,
        landscape_flipped = SDL_ORIENTATION_LANDSCAPE_FLIPPED,
        portrait = SDL_ORIENTATION_PORTRAIT,
        portrait_flipped = SDL_ORIENTATION_PORTRAIT_FLIPPED
    };
    enum class keymod : uint16_t
    {
        none = SDL_KMOD_NONE,
        lshift = SDL_KMOD_LSHIFT,
        rshift = SDL_KMOD_RSHIFT,
        lctrl = SDL_KMOD_LCTRL,
        rctrl = SDL_KMOD_RCTRL,
        lalt = SDL_KMOD_LALT,
        ralt = SDL_KMOD_RALT,
        lgui = SDL_KMOD_LGUI,
        rgui = SDL_KMOD_RGUI,
        num = SDL_KMOD_NUM,
        caps = SDL_KMOD_CAPS,
        mode = SDL_KMOD_MODE,
        scroll = SDL_KMOD_SCROLL,

        shift = SDL_KMOD_SHIFT,
        ctrl = SDL_KMOD_CTRL,
        alt = SDL_KMOD_ALT,
        gui = SDL_KMOD_GUI,
    };

#define KEYS_SCANCODE_X_MACROS_DATA \
X(a, SDL_SCANCODE_A)\
    X(b, SDL_SCANCODE_B)\
        X(c, SDL_SCANCODE_C)\
        X(d, SDL_SCANCODE_D)\
        X(e, SDL_SCANCODE_E)\
        X(f, SDL_SCANCODE_F)\
        X(g, SDL_SCANCODE_G)\
        X(h, SDL_SCANCODE_H)\
        X(i, SDL_SCANCODE_I)\
        X(j, SDL_SCANCODE_J)\
        X(k, SDL_SCANCODE_K)\
        X(l, SDL_SCANCODE_L)\
        X(m, SDL_SCANCODE_M)\
        X(n, SDL_SCANCODE_N)\
        X(o, SDL_SCANCODE_O)\
        X(p, SDL_SCANCODE_P)\
        X(q, SDL_SCANCODE_Q)\
        X(r, SDL_SCANCODE_R)\
        X(s, SDL_SCANCODE_S)\
        X(t, SDL_SCANCODE_T)\
        X(u, SDL_SCANCODE_U)\
        X(v, SDL_SCANCODE_V)\
        X(w, SDL_SCANCODE_W)\
        X(x, SDL_SCANCODE_X)\
        X(y, SDL_SCANCODE_Y)\
        X(z, SDL_SCANCODE_Z)\
        X(num1, SDL_SCANCODE_1)\
        X(num2, SDL_SCANCODE_2)\
        X(num3, SDL_SCANCODE_3)\
        X(num4, SDL_SCANCODE_4)\
        X(num5, SDL_SCANCODE_5)\
        X(num6, SDL_SCANCODE_6)\
        X(num7, SDL_SCANCODE_7)\
        X(num8, SDL_SCANCODE_8)\
        X(num9, SDL_SCANCODE_9)\
        X(num0, SDL_SCANCODE_0)\
        X(return_key, SDL_SCANCODE_RETURN)\
        X(escape, SDL_SCANCODE_ESCAPE)\
        X(backspace, SDL_SCANCODE_BACKSPACE)\
        X(tab, SDL_SCANCODE_TAB)\
        X(space, SDL_SCANCODE_SPACE)\
        X(minus, SDL_SCANCODE_MINUS)\
        X(equals, SDL_SCANCODE_EQUALS)\
        X(leftbracket, SDL_SCANCODE_LEFTBRACKET)\
        X(rightbracket, SDL_SCANCODE_RIGHTBRACKET)\
        X(backslash, SDL_SCANCODE_BACKSLASH)\
        X(nonushash, SDL_SCANCODE_NONUSHASH)\
        X(semicolon, SDL_SCANCODE_SEMICOLON)\
        X(apostrophe, SDL_SCANCODE_APOSTROPHE)\
        X(grave, SDL_SCANCODE_GRAVE)\
        X(comma, SDL_SCANCODE_COMMA)\
        X(period, SDL_SCANCODE_PERIOD)\
        X(slash, SDL_SCANCODE_SLASH)\
        X(capslock, SDL_SCANCODE_CAPSLOCK)\
        X(f1, SDL_SCANCODE_F1)\
        X(f2, SDL_SCANCODE_F2)\
        X(f3, SDL_SCANCODE_F3)\
        X(f4, SDL_SCANCODE_F4)\
        X(f5, SDL_SCANCODE_F5)\
        X(f6, SDL_SCANCODE_F6)\
        X(f7, SDL_SCANCODE_F7)\
        X(f8, SDL_SCANCODE_F8)\
        X(f9, SDL_SCANCODE_F9)\
        X(f10, SDL_SCANCODE_F10)\
        X(f11, SDL_SCANCODE_F11)\
        X(f12, SDL_SCANCODE_F12)\
        X(printscreen, SDL_SCANCODE_PRINTSCREEN)\
        X(scrolllock, SDL_SCANCODE_SCROLLLOCK)\
        X(pause, SDL_SCANCODE_PAUSE)\
        X(insert, SDL_SCANCODE_INSERT)\
        X(home, SDL_SCANCODE_HOME)\
        X(pageup, SDL_SCANCODE_PAGEUP)\
        X(delete_key, SDL_SCANCODE_DELETE)\
        X(end, SDL_SCANCODE_END)\
        X(pagedown, SDL_SCANCODE_PAGEDOWN)\
        X(right, SDL_SCANCODE_RIGHT)\
        X(left, SDL_SCANCODE_LEFT)\
        X(down, SDL_SCANCODE_DOWN)\
        X(up, SDL_SCANCODE_UP)\
        X(numlockclear, SDL_SCANCODE_NUMLOCKCLEAR)\
        X(kp_divide, SDL_SCANCODE_KP_DIVIDE)\
        X(kp_multiply, SDL_SCANCODE_KP_MULTIPLY)\
        X(kp_minus, SDL_SCANCODE_KP_MINUS)\
        X(kp_plus, SDL_SCANCODE_KP_PLUS)\
        X(kp_enter, SDL_SCANCODE_KP_ENTER)\
        X(kp_1, SDL_SCANCODE_KP_1)\
        X(kp_2, SDL_SCANCODE_KP_2)\
        X(kp_3, SDL_SCANCODE_KP_3)\
        X(kp_4, SDL_SCANCODE_KP_4)\
        X(kp_5, SDL_SCANCODE_KP_5)\
        X(kp_6, SDL_SCANCODE_KP_6)\
        X(kp_7, SDL_SCANCODE_KP_7)\
        X(kp_8, SDL_SCANCODE_KP_8)\
        X(kp_9, SDL_SCANCODE_KP_9)\
        X(kp_0, SDL_SCANCODE_KP_0)\
        X(kp_period, SDL_SCANCODE_KP_PERIOD)\
        X(nonusbackslash, SDL_SCANCODE_NONUSBACKSLASH)\
        X(application, SDL_SCANCODE_APPLICATION)\
        X(power, SDL_SCANCODE_POWER)\
        X(kp_equals, SDL_SCANCODE_KP_EQUALS)\
        X(f13, SDL_SCANCODE_F13)\
        X(f14, SDL_SCANCODE_F14)\
        X(f15, SDL_SCANCODE_F15)\
        X(f16, SDL_SCANCODE_F16)\
        X(f17, SDL_SCANCODE_F17)\
        X(f18, SDL_SCANCODE_F18)\
        X(f19, SDL_SCANCODE_F19)\
        X(f20, SDL_SCANCODE_F20)\
        X(f21, SDL_SCANCODE_F21)\
        X(f22, SDL_SCANCODE_F22)\
        X(f23, SDL_SCANCODE_F23)\
        X(f24, SDL_SCANCODE_F24)\
        X(execute, SDL_SCANCODE_EXECUTE)\
        X(help, SDL_SCANCODE_HELP)\
        X(menu, SDL_SCANCODE_MENU)\
        X(select, SDL_SCANCODE_SELECT)\
        X(stop, SDL_SCANCODE_STOP)\
        X(again, SDL_SCANCODE_AGAIN)\
        X(undo, SDL_SCANCODE_UNDO)\
        X(cut, SDL_SCANCODE_CUT)\
        X(copy, SDL_SCANCODE_COPY)\
        X(paste, SDL_SCANCODE_PASTE)\
        X(find, SDL_SCANCODE_FIND)\
        X(mute, SDL_SCANCODE_MUTE)\
        X(volumeup, SDL_SCANCODE_VOLUMEUP)\
        X(volumedown, SDL_SCANCODE_VOLUMEDOWN)\
        X(kp_comma, SDL_SCANCODE_KP_COMMA)\
        X(kp_equalsas400, SDL_SCANCODE_KP_EQUALSAS400)\
        X(international1, SDL_SCANCODE_INTERNATIONAL1)\
        X(international2, SDL_SCANCODE_INTERNATIONAL2)\
        X(international3, SDL_SCANCODE_INTERNATIONAL3)\
        X(international4, SDL_SCANCODE_INTERNATIONAL4)\
        X(international5, SDL_SCANCODE_INTERNATIONAL5)\
        X(international6, SDL_SCANCODE_INTERNATIONAL6)\
        X(international7, SDL_SCANCODE_INTERNATIONAL7)\
        X(international8, SDL_SCANCODE_INTERNATIONAL8)\
        X(international9, SDL_SCANCODE_INTERNATIONAL9)\
        X(lang1, SDL_SCANCODE_LANG1)\
        X(lang2, SDL_SCANCODE_LANG2)\
        X(lang3, SDL_SCANCODE_LANG3)\
        X(lang4, SDL_SCANCODE_LANG4)\
        X(lang5, SDL_SCANCODE_LANG5)\
        X(lang6, SDL_SCANCODE_LANG6)\
        X(lang7, SDL_SCANCODE_LANG7)\
        X(lang8, SDL_SCANCODE_LANG8)\
        X(lang9, SDL_SCANCODE_LANG9)\
        X(alterase, SDL_SCANCODE_ALTERASE)\
        X(sysreq, SDL_SCANCODE_SYSREQ)\
        X(cancel, SDL_SCANCODE_CANCEL)\
        X(clear, SDL_SCANCODE_CLEAR)\
        X(prior, SDL_SCANCODE_PRIOR)\
        X(return2, SDL_SCANCODE_RETURN2)\
        X(separator, SDL_SCANCODE_SEPARATOR)\
        X(out, SDL_SCANCODE_OUT)\
        X(oper, SDL_SCANCODE_OPER)\
        X(clearagain, SDL_SCANCODE_CLEARAGAIN)\
        X(crsel, SDL_SCANCODE_CRSEL)\
        X(exsel, SDL_SCANCODE_EXSEL)\
        X(kp_00, SDL_SCANCODE_KP_00)\
        X(kp_000, SDL_SCANCODE_KP_000)\
        X(thousandsseparator, SDL_SCANCODE_THOUSANDSSEPARATOR)\
        X(decimalseparator, SDL_SCANCODE_DECIMALSEPARATOR)\
        X(currencyunit, SDL_SCANCODE_CURRENCYUNIT)\
        X(currencysubunit, SDL_SCANCODE_CURRENCYSUBUNIT)\
        X(kp_leftparen, SDL_SCANCODE_KP_LEFTPAREN)\
        X(kp_rightparen, SDL_SCANCODE_KP_RIGHTPAREN)\
        X(kp_leftbrace, SDL_SCANCODE_KP_LEFTBRACE)\
        X(kp_rightbrace, SDL_SCANCODE_KP_RIGHTBRACE)\
        X(kp_tab, SDL_SCANCODE_KP_TAB)\
        X(kp_backspace, SDL_SCANCODE_KP_BACKSPACE)\
        X(kp_a, SDL_SCANCODE_KP_A)\
        X(kp_b, SDL_SCANCODE_KP_B)\
        X(kp_c, SDL_SCANCODE_KP_C)\
        X(kp_d, SDL_SCANCODE_KP_D)\
        X(kp_e, SDL_SCANCODE_KP_E)\
        X(kp_f, SDL_SCANCODE_KP_F)\
        X(kp_xor, SDL_SCANCODE_KP_XOR)\
        X(kp_power, SDL_SCANCODE_KP_POWER)\
        X(kp_percent, SDL_SCANCODE_KP_PERCENT)\
        X(kp_less, SDL_SCANCODE_KP_LESS)\
        X(kp_greater, SDL_SCANCODE_KP_GREATER)\
        X(kp_ampersand, SDL_SCANCODE_KP_AMPERSAND)\
        X(kp_dblampersand, SDL_SCANCODE_KP_DBLAMPERSAND)\
        X(kp_verticalbar, SDL_SCANCODE_KP_VERTICALBAR)\
        X(kp_dblverticalbar, SDL_SCANCODE_KP_DBLVERTICALBAR)\
        X(kp_colon, SDL_SCANCODE_KP_COLON)\
        X(kp_hash, SDL_SCANCODE_KP_HASH)\
        X(kp_space, SDL_SCANCODE_KP_SPACE)\
        X(kp_at, SDL_SCANCODE_KP_AT)\
        X(kp_exclam, SDL_SCANCODE_KP_EXCLAM)\
        X(kp_memstore, SDL_SCANCODE_KP_MEMSTORE)\
        X(kp_memrecall, SDL_SCANCODE_KP_MEMRECALL)\
        X(kp_memclear, SDL_SCANCODE_KP_MEMCLEAR)\
        X(kp_memadd, SDL_SCANCODE_KP_MEMADD)\
        X(kp_memsubtract, SDL_SCANCODE_KP_MEMSUBTRACT)\
        X(kp_memmultiply, SDL_SCANCODE_KP_MEMMULTIPLY)\
        X(kp_memdivide, SDL_SCANCODE_KP_MEMDIVIDE)\
        X(kp_plusminus, SDL_SCANCODE_KP_PLUSMINUS)\
        X(kp_clear, SDL_SCANCODE_KP_CLEAR)\
        X(kp_clearentry, SDL_SCANCODE_KP_CLEARENTRY)\
        X(kp_binary, SDL_SCANCODE_KP_BINARY)\
        X(kp_octal, SDL_SCANCODE_KP_OCTAL)\
        X(kp_decimal, SDL_SCANCODE_KP_DECIMAL)\
        X(kp_hexadecimal, SDL_SCANCODE_KP_HEXADECIMAL)\
        X(lctrl, SDL_SCANCODE_LCTRL)\
        X(lshift, SDL_SCANCODE_LSHIFT)\
        X(lalt, SDL_SCANCODE_LALT)\
        X(lgui, SDL_SCANCODE_LGUI)\
        X(rctrl, SDL_SCANCODE_RCTRL)\
        X(rshift, SDL_SCANCODE_RSHIFT)\
        X(ralt, SDL_SCANCODE_RALT)\
        X(rgui, SDL_SCANCODE_RGUI)\
        X(mode, SDL_SCANCODE_MODE)\
        X(sleep, SDL_SCANCODE_SLEEP)\
        X(wake, SDL_SCANCODE_WAKE)\
        X(channel_increment, SDL_SCANCODE_CHANNEL_INCREMENT)\
        X(channel_decrement, SDL_SCANCODE_CHANNEL_DECREMENT)\
        X(media_play, SDL_SCANCODE_MEDIA_PLAY)\
        X(media_pause, SDL_SCANCODE_MEDIA_PAUSE)\
        X(media_record, SDL_SCANCODE_MEDIA_RECORD)\
        X(media_fast_forward, SDL_SCANCODE_MEDIA_FAST_FORWARD)\
        X(media_rewind, SDL_SCANCODE_MEDIA_REWIND)\
        X(media_next_track, SDL_SCANCODE_MEDIA_NEXT_TRACK)\
        X(media_previous_track, SDL_SCANCODE_MEDIA_PREVIOUS_TRACK)\
        X(media_stop, SDL_SCANCODE_MEDIA_STOP)\
        X(media_eject, SDL_SCANCODE_MEDIA_EJECT)\
        X(media_play_pause, SDL_SCANCODE_MEDIA_PLAY_PAUSE)\
        X(media_select, SDL_SCANCODE_MEDIA_SELECT)\
        X(ac_new, SDL_SCANCODE_AC_NEW)\
        X(ac_open, SDL_SCANCODE_AC_OPEN)\
        X(ac_close, SDL_SCANCODE_AC_CLOSE)\
        X(ac_exit, SDL_SCANCODE_AC_EXIT)\
        X(ac_save, SDL_SCANCODE_AC_SAVE)\
        X(ac_print, SDL_SCANCODE_AC_PRINT)\
        X(ac_properties, SDL_SCANCODE_AC_PROPERTIES)\
        X(ac_search, SDL_SCANCODE_AC_SEARCH)\
        X(ac_home, SDL_SCANCODE_AC_HOME)\
        X(ac_back, SDL_SCANCODE_AC_BACK)\
        X(ac_forward, SDL_SCANCODE_AC_FORWARD)\
        X(ac_stop, SDL_SCANCODE_AC_STOP)\
        X(ac_refresh, SDL_SCANCODE_AC_REFRESH)\
        X(ac_bookmarks, SDL_SCANCODE_AC_BOOKMARKS)\
        X(softleft, SDL_SCANCODE_SOFTLEFT)\
        X(softright, SDL_SCANCODE_SOFTRIGHT)\
        X(call, SDL_SCANCODE_CALL)\
        X(endcall, SDL_SCANCODE_ENDCALL)\
        X(reserved, SDL_SCANCODE_RESERVED)\
        X(count, SDL_SCANCODE_COUNT)
    enum class scancode : uint16_t
    {
#define X(a, b) a = b,
        KEYS_SCANCODE_X_MACROS_DATA
#undef X
        unknown = SDL_SCANCODE_UNKNOWN
    };

    inline std::string to_string(scancode what)
    {
        switch (what)
        {
#define X(a, b) case scancode::a: return #a;
            KEYS_SCANCODE_X_MACROS_DATA
#undef X
        case scancode::unknown: return "unknown";
        [[unlikely]] default: return "invalid";
        }
    }

#undef KEYS_SCANCODE_X_MACROS_DATA
    // =========================================================================
    // БАЗОВЫЕ СТРУКТУРЫ (Для иерархии данных)
    // =========================================================================
    struct eventBase
    {
        Events type;
        uint64_t timestamp;
    };

    struct windowEvent : eventBase
    {
        uint32_t windowID;
    };

    struct displayEvent : eventBase
    {
        uint32_t displayID;
    };

    // =========================================================================
    // ХЕЛПЕРЫ ВВОДА
    // =========================================================================
    struct Keycode
    {
        uint32_t value;

        constexpr bool isScancode() const noexcept
        {
            return value & SDLK_SCANCODE_MASK;
        }

        constexpr scancode get_scancode() const noexcept
        {
            return static_cast<scancode>(value & ~SDLK_SCANCODE_MASK);
        }

        constexpr char ascii() const noexcept
        {
            return static_cast<char>(value);
        }
    };

    // =========================================================================
    // СТРУКТУРЫ СОБЫТИЙ (Транзитные типы для шины событий)
    // =========================================================================

    // --- Application ---
    struct appQuit : eventBase { };
    struct appTerminating : eventBase { };
    struct appLowMemory : eventBase { };
    struct appWillEnterBackground : eventBase { };
    struct appDidEnterBackground : eventBase { };
    struct appWillEnterForeground : eventBase { };
    struct appDidEnterForeground : eventBase { };
    struct appLocaleChanged : eventBase { };
    struct appSystemThemeChanged : eventBase { };

    // --- Display ---
    struct displayAdded : displayEvent { };
    struct displayRemoved : displayEvent { };
    struct displayDesktopModeChanged : displayEvent { int width; int height; };
    struct displayCurrentModeChanged : displayEvent { int width; int height; };

    // --- Window ---
    struct windowShown : windowEvent { };
    struct windowHidden : windowEvent { };
    struct windowMinimized : windowEvent { };
    struct windowMaximized : windowEvent { };
    struct windowRestored : windowEvent { };
    struct windowOccluded : windowEvent { };
    struct windowEnterFullscreen : windowEvent { };
    struct windowLeaveFullscreen : windowEvent { };
    struct windowDestroyed : windowEvent { };
    struct windowMouseEnter : windowEvent { };
    struct windowMouseLeave : windowEvent { };
    struct windowFocusGained : windowEvent { };
    struct windowFocusLost : windowEvent { };
    struct windowPixelSizeChanged : windowEvent { glm::ivec2 size; };
    struct windowMetalViewResized : windowEvent { glm::ivec2 size; };
    struct windowCloseRequest : windowEvent { };
    struct windowIccProfChanged : windowEvent { };

    // --- Keyboard ---
    struct keyboardDevice : eventBase { uint32_t keyboardID; };
    struct keymapChanged : eventBase { };
    struct keyboardAdded : eventBase { uint32_t keyboardID; };
    struct keyboardRemoved : eventBase { uint32_t keyboardID; };
    struct screenKeyboardShown : eventBase { };
    struct screenKeyboardHidden : eventBase { };

    struct keyDown : windowEvent
    {
        uint32_t keyboardID; scancode code; Keycode key;
        keymod mod; bool repeat; bool is_down;
    };
    struct keyUp : windowEvent
    {
        uint32_t keyboardID; scancode code; Keycode key;
        keymod mod; bool repeat; bool is_down;
    };

    struct textEditing : windowEvent
    {
        std::string text; int32_t start; int32_t length;
    };
    struct textInput : windowEvent { std::string text; };
    struct textEditingCandidates : windowEvent
    {
        std::vector<std::string> candidates; int32_t selected; bool horizontal;
    };

    // --- Mouse ---
    struct mouseDevice : eventBase { uint32_t mouseID; };
    struct mouseAdded : eventBase { uint32_t mouseID; };
    struct mouseRemoved : eventBase { uint32_t mouseID; };

    struct mouseMotion : windowEvent
    {
        uint32_t mouseID; glm::vec2 position; glm::vec2 relative; uint32_t state;
    };
    struct mouseButtonDown : windowEvent
    {
        uint32_t mouseID; uint8_t button; uint8_t clicks; bool is_down; glm::vec2 position;
    };
    struct mouseButtonUp : windowEvent
    {
        uint32_t mouseID; uint8_t button; uint8_t clicks; bool is_down; glm::vec2 position;
    };
    struct mouseWheel : windowEvent
    {
        uint32_t mouseID; glm::vec2 scroll; glm::vec2 mousePos;
        int32_t intScroll[2]; wheel_direction direction;
    };

    // --- Joystick ---
    struct joystickEvent : eventBase { uint32_t joystickID; };
    struct joystickDevice : joystickEvent { };
    struct joystickAxisMoved : joystickEvent { uint8_t axis; int16_t value; };
    struct joystickBallMoved : joystickEvent { uint8_t ball; glm::ivec2 rel; };
    struct joystickHatPressed : joystickEvent { uint8_t hat; hat_position position; };
    struct joystickButtonDown : joystickEvent { uint8_t button; bool pressed; };
    struct joystickButtonUp : joystickEvent { uint8_t button; bool pressed; };
    struct joystickAdded : joystickEvent { };
    struct joystickRemoved : joystickEvent { };
    struct joystickUpdateComplete : joystickEvent { };
    struct joystickBattery : joystickEvent { power_state state; float percent; };

    // --- Gamepad ---
    struct gamepadEvent : eventBase { uint32_t joystickID; };
    struct gamepadDevice : gamepadEvent { };
    struct gamepadAdded : gamepadEvent { };
    struct gamepadRemoved : gamepadEvent { };
    struct gamepadRemapped : gamepadEvent { };
    struct gamepadUpdateComplete : gamepadEvent { };
    struct gamepadSteamHandleUpdated : gamepadEvent { };
    struct gamepadAxis : gamepadEvent { gamepad_axis axis; int16_t value; };
    struct gamepadButtonDown : gamepadEvent { gamepad_button button; bool is_down; };
    struct gamepadButtonUp : gamepadEvent { gamepad_button button; bool is_down; };

    struct gamepadTouchpadDown : gamepadEvent
    {
        int32_t touchpad; int32_t finger; glm::vec2 position; float pressure;
    };
    struct gamepadTouchpadMotion : gamepadEvent
    {
        int32_t touchpad; int32_t finger; glm::vec2 position; float pressure;
    };
    struct gamepadTouchpadUp : gamepadEvent
    {
        int32_t touchpad; int32_t finger; glm::vec2 position; float pressure;
    };
    struct gamepadSensor : gamepadEvent
    {
        sensor_type sensor; float data[3]; uint64_t timestamp;
    };

    // --- Touch ---
    struct touchEvent : eventBase
    {
        uint64_t touchID; uint64_t fingerID; uint32_t windowID;
        glm::vec2 position; glm::vec2 delta; float pressure; bool canceled;
    };
    struct fingerDown : eventBase
    {
        uint64_t touchID; uint64_t fingerID; uint32_t windowID;
        glm::vec2 position; glm::vec2 delta; float pressure;
    };
    struct fingerUp : eventBase
    {
        uint64_t touchID; uint64_t fingerID; uint32_t windowID;
        glm::vec2 position; glm::vec2 delta; float pressure;
    };
    struct fingerMotion : eventBase
    {
        uint64_t touchID; uint64_t fingerID; uint32_t windowID;
        glm::vec2 position; glm::vec2 delta; float pressure;
    };
    struct fingerCanceled : eventBase
    {
        uint64_t touchID; uint64_t fingerID; uint32_t windowID;
        glm::vec2 position; glm::vec2 delta; float pressure;
    };

    // --- Pinch ---
    struct pinchBegin : eventBase { uint32_t windowID; float scale; };
    struct pinchUpdate : eventBase { uint32_t windowID; float scale; };
    struct pinchEnd : eventBase { uint32_t windowID; float scale; };

    // --- Drag & Drop ---
    struct dropFile : windowEvent { glm::vec2 position; std::string source; std::string data; };
    struct dropText : windowEvent { glm::vec2 position; std::string source; std::string data; };
    struct dropBegin : windowEvent { glm::vec2 position; std::string source; };
    struct dropComplete : windowEvent { glm::vec2 position; std::string source; };
    struct dropPosition : windowEvent { glm::vec2 position; std::string source; std::string data; };

    // --- Audio ---
    struct audioDeviceAdded : eventBase { uint32_t deviceID; bool recording; };
    struct audioDeviceRemoved : eventBase { uint32_t deviceID; bool recording; };
    struct audioDeviceFormatChanged : eventBase { uint32_t deviceID; bool recording; };

    // --- Sensor ---
    struct sensorEvent : eventBase
    {
        uint32_t sensorID; float data[6]; uint64_t timestamp;
    };

    // --- Pen ---
    struct penProximityIn : eventBase { uint32_t penID; uint32_t windowID; glm::vec2 position; };
    struct penProximityOut : eventBase { uint32_t penID; uint32_t windowID; glm::vec2 position; };
    struct penMotion : eventBase { uint32_t penID; uint32_t windowID; glm::vec2 position; };

    // --- Camera ---
    struct cameraDeviceAdded : eventBase { uint32_t cameraID; };
    struct cameraDeviceRemoved : eventBase { uint32_t cameraID; };
    struct cameraDeviceApproved : eventBase { uint32_t cameraID; };
    struct cameraDeviceDenied : eventBase { uint32_t cameraID; };

    // --- Render ---
    struct renderTargetsReset : windowEvent { };
    struct renderDeviceReset : windowEvent { };
    struct renderDeviceLost : windowEvent { };

    // --- Internal ---
    struct pollSentinel : eventBase { };
    struct userEvent : eventBase
    {
        uint32_t windowID; int32_t code; void* data1; void* data2;
    };

    // =========================================================================
    // ДОБАВЛЕННЫЕ И ИСПРАВЛЕННЫЕ СТРУКТУРЫ ДЛЯ EVENT_STRUCTS.HPP
    // =========================================================================

    // --- Дополнительные оконные и дисплейные события, которых не хватало ---
    struct displayOrientation : displayEvent { display_orientation orientation; };
    struct displayAdded : displayEvent { };
    struct displayRemoved : displayEvent { };
    struct displayMoved : displayEvent { };
    struct displayDesktopModeChanged : displayEvent { };
    struct displayCurrentModeChanged : displayEvent { };
    struct displayContentScaleChanged : displayEvent { float contentScale; };

    struct windowShown : windowEvent { };
    struct windowHidden : windowEvent { };
    struct windowExposed : windowEvent { };
    struct windowMoved : windowEvent { glm::ivec2 position; };
    struct windowResized : windowEvent { glm::ivec2 size; };
    struct windowHitTest : windowEvent { };
    struct windowDisplayChanged : windowEvent { uint32_t displayID; };
    struct windowDisplayScaleChanged : windowEvent { };
    struct windowSafeAreaChanged : windowEvent { };

    // --- Исправленные события ввода (убираем narrowing conversion и исправляем типы) ---

    // Мышь (wheelDirectionEnum вместо wheelDirection)
    struct mouseWheel : windowEvent
    {
        uint32_t which;
        glm::vec2 scroll;
        glm::vec2 mousePos;
        glm::ivec2 integerScroll;
        wheel_direction direction; // в твоем enum_class это wheel_direction или wheelDirectionEnum
    };

    // Джойстики
    struct joystickAxisMoved : eventBase { uint32_t which; uint8_t axis; int16_t value; };
    struct joystickBallMoved : eventBase { uint32_t which; uint8_t ball; glm::ivec2 relative; };
    struct joystickHatPressed : eventBase { uint32_t which; uint8_t hat; hat_position value; };
    struct joystickButtonDown : eventBase { uint32_t which; uint8_t button; bool down; };
    struct joystickButtonUp : eventBase { uint32_t which; uint8_t button; bool down; };
    struct joystickAdded : eventBase { uint32_t which; };
    struct joystickRemoved : eventBase { uint32_t which; };
    struct joystickBattery : eventBase { uint32_t which; power_state state; float percent; };

    // Геймпады
    struct gamepadAxis : eventBase { uint32_t which; gamepad_axis axis; int16_t value; };
    struct gamepadButtonDown : eventBase { uint32_t which; gamepad_button button; bool down; };
    struct gamepadButtonUp : eventBase { uint32_t which; gamepad_button button; bool down; };
    struct gamepadAdded : eventBase { uint32_t which; };
    struct gamepadRemoved : eventBase { uint32_t which; };
    struct gamepadRemapped : eventBase { uint32_t which; };

    // В SDL3 координаты тачпадов геймпада — это float, поэтому строго glm::vec2!
    struct gamepadTouchpadDown : eventBase { uint32_t which; int32_t touchpad; int32_t finger; glm::vec2 position; float pressure; };
    struct gamepadTouchpadMotion : eventBase { uint32_t which; int32_t touchpad; int32_t finger; glm::vec2 position; float pressure; };
    struct gamepadTouchpadUp : eventBase { uint32_t which; int32_t touchpad; int32_t finger; glm::vec2 position; float pressure; };
    struct gamepadSensor : eventBase { uint32_t which; sensor_type sensor; glm::vec3 data; uint64_t sensorTimestamp; };

    // Тач (В SDL3 координаты нормализованные float, d — это тоже float дельта!)
    struct fingerDown : eventBase { uint64_t touchID; uint64_t fingerID; uint32_t windowID; glm::vec2 position; glm::vec2 delta; float pressure; };
    struct fingerMotion : eventBase { uint64_t touchID; uint64_t fingerID; uint32_t windowID; glm::vec2 position; glm::vec2 delta; float pressure; };
    struct fingerUp : eventBase { uint64_t touchID; uint64_t fingerID; uint32_t windowID; glm::vec2 position; glm::vec2 delta; float pressure; };

    // Дропы (В SDL3 оконные координаты x и y — это строго float!)
    struct dropFile : windowEvent { glm::vec2 position; std::string source; std::string data; };
    struct dropText : windowEvent { glm::vec2 position; std::string source; std::string data; };
    struct dropBegin : windowEvent { glm::vec2 position; std::string source; };
    struct dropComplete : windowEvent { glm::vec2 position; std::string source; };
    struct dropPosition : windowEvent { glm::vec2 position; std::string source; std::string data; };

    // =========================================================================
    // 1. КОНЦЕПТ ДЛЯ ПРОВЕРКИ ТИПА СОБЫТИЯ
    // =========================================================================
    // Проверяет, что тип T унаследован от eventBase. 
    // Это отсечет int, кастомные классы и пропустит только твои структуры событий.
    template<typename T>
    concept EventType = std::derived_from<T, eventBase>;

    // =========================================================================
    // 2. ЦЕНТРАЛЬНЫЙ МЕНЕДЖЕР СОБЫТИЙ (EVENT BUS + SDL WATCHER)
    // =========================================================================
    class event_manager
    {
    public:
        // Используем move_only_function для максимальной скорости и поддержки лямбд с захватом move-only объектов
        template<EventType T>
        using callback = std::move_only_function<void(const T&)>;

        event_manager(const event_manager&) = delete;
        event_manager& operator=(const event_manager&) = delete;

        static event_manager& instance()
        {
            static event_manager inst;
            return inst;
        }

        // Подписка на события с ограничением через концепт EventType
        template<EventType T>
        static void on(callback<T> cb)
        {
            getListeners<T>().push_back(std::move(cb));
        }

        // Ручной вызов события (если понадобится сгенерировать фейковое событие внутри движка)
        template<EventType T>
        static void emit(const T& event)
        {
            for (auto& cb : getListeners<T>())
            {
                if (cb) cb(event);
            }
        }

    private:
        event_manager()
        {
            // Регистрируем низкоуровневый фильтр SDL3, который перехватывает абсолютно все события на лету
            SDL_AddEventWatch(sdlEventWatcher, this);
        }

        ~event_manager()
        {
            SDL_RemoveEventWatch(sdlEventWatcher, this);
        }

        static bool SDLCALL sdlEventWatcher(void* userData, SDL_Event* event)
        {
            auto* manager = static_cast<event_manager*>(userData);
            manager->dispatchEvent(*event);
            return true; // Возвращаем true, чтобы событие оставалось в системной очереди SDL для PollEvent
        }

        // Хранилище подписчиков на основе статических векторов для каждого типа структуры
        template<EventType T>
        static auto& getListeners()
        {
            static std::vector<callback<T>> listeners;
            return listeners;
        }

        // Основной диспетчер: трансляция из структуры SDL_Event в структуры mngl и мгновенный emit
        void dispatchEvent(const SDL_Event& e)
        {
            const auto type = static_cast<Events>(e.type);
            const uint64_t ts = e.common.timestamp;

            // Вызываем только если на данный тип события есть активные подписчики
            switch (type)
            {
                // --- Application Events ---
            case Events::quit:
                if (!getListeners<appQuit>().empty()) emit(appQuit{ {type, ts} });
                break;
            case Events::terminating:
                if (!getListeners<appTerminating>().empty()) emit(appTerminating{ {type, ts} });
                break;
            case Events::low_memory:
                if (!getListeners<appLowMemory>().empty()) emit(appLowMemory{ {type, ts} });
                break;
            case Events::will_enter_background:
                if (!getListeners<appWillEnterBackground>().empty()) emit(appWillEnterBackground{ {type, ts} });
                break;
            case Events::did_enter_background:
                if (!getListeners<appDidEnterBackground>().empty()) emit(appDidEnterBackground{ {type, ts} });
                break;
            case Events::will_enter_foreground:
                if (!getListeners<appWillEnterForeground>().empty()) emit(appWillEnterForeground{ {type, ts} });
                break;
            case Events::did_enter_foreground:
                if (!getListeners<appDidEnterForeground>().empty()) emit(appDidEnterForeground{ {type, ts} });
                break;
            case Events::locale_changed:
                if (!getListeners<appLocaleChanged>().empty()) emit(appLocaleChanged{ {type, ts} });
                break;
            case Events::system_theme_changed:
                if (!getListeners<appSystemThemeChanged>().empty()) emit(appSystemThemeChanged{ {type, ts} });
                break;

                // --- Display Events ---
            case Events::display_orientation:
                if (!getListeners<displayOrientation>().empty()) emit(displayOrientation{ {{type, ts}, e.display.displayID}, static_cast<display_orientation>(e.display.data1) });
                break;
            case Events::display_added:
                if (!getListeners<displayAdded>().empty()) emit(displayAdded{ {{type, ts}, e.display.displayID} });
                break;
            case Events::display_removed:
                if (!getListeners<displayRemoved>().empty()) emit(displayRemoved{ {{type, ts}, e.display.displayID} });
                break;
            case Events::display_moved:
                if (!getListeners<displayMoved>().empty()) emit(displayMoved{ {{type, ts}, e.display.displayID} });
                break;
            case Events::display_desktop_mode_changed:
                if (!getListeners<displayDesktopModeChanged>().empty()) emit(displayDesktopModeChanged{ {{type, ts}, e.display.displayID} });
                break;
            case Events::display_current_mode_changed:
                if (!getListeners<displayCurrentModeChanged>().empty()) emit(displayCurrentModeChanged{ {{type, ts}, e.display.displayID} });
                break;
            case Events::display_content_scale_changed:
                if (!getListeners<displayContentScaleChanged>().empty()) emit(displayContentScaleChanged{ {{type, ts}, e.display.displayID}, (float)e.display.data1 });
                break;

                // --- Window Events ---
            case Events::window_shown:
                if (!getListeners<windowShown>().empty()) emit(windowShown{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_hidden:
                if (!getListeners<windowHidden>().empty()) emit(windowHidden{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_exposed:
                if (!getListeners<windowExposed>().empty()) emit(windowExposed{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_moved:
                if (!getListeners<windowMoved>().empty()) emit(windowMoved{ {{type, ts}, e.window.windowID}, {e.window.data1, e.window.data2} });
                break;
            case Events::window_resized:
                if (!getListeners<windowResized>().empty()) emit(windowResized{ {{type, ts}, e.window.windowID}, {e.window.data1, e.window.data2} });
                break;
            case Events::window_pixel_size_changed:
                if (!getListeners<windowPixelSizeChanged>().empty()) emit(windowPixelSizeChanged{ {{type, ts}, e.window.windowID}, {e.window.data1, e.window.data2} });
                break;
            case Events::window_metal_view_resized:
                if (!getListeners<windowMetalViewResized>().empty()) emit(windowMetalViewResized{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_minimized:
                if (!getListeners<windowMinimized>().empty()) emit(windowMinimized{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_maximized:
                if (!getListeners<windowMaximized>().empty()) emit(windowMaximized{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_restored:
                if (!getListeners<windowRestored>().empty()) emit(windowRestored{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_mouse_enter:
                if (!getListeners<windowMouseEnter>().empty()) emit(windowMouseEnter{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_mouse_leave:
                if (!getListeners<windowMouseLeave>().empty()) emit(windowMouseLeave{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_focus_gained:
                if (!getListeners<windowFocusGained>().empty()) emit(windowFocusGained{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_focus_lost:
                if (!getListeners<windowFocusLost>().empty()) emit(windowFocusLost{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_close_request:
                if (!getListeners<windowCloseRequest>().empty()) emit(windowCloseRequest{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_hit_test:
                if (!getListeners<windowHitTest>().empty()) emit(windowHitTest{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_display_changed:
                if (!getListeners<windowDisplayChanged>().empty()) emit(windowDisplayChanged{ {{type, ts}, e.window.windowID}, static_cast<uint32_t>(e.window.data1) });
                break;
            case Events::window_display_scale_changed:
                if (!getListeners<windowDisplayScaleChanged>().empty()) emit(windowDisplayScaleChanged{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_safe_area_changed:
                if (!getListeners<windowSafeAreaChanged>().empty()) emit(windowSafeAreaChanged{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_occluded:
                if (!getListeners<windowOccluded>().empty()) emit(windowOccluded{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_enter_fullscreen:
                if (!getListeners<windowEnterFullscreen>().empty()) emit(windowEnterFullscreen{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_leave_fullscreen:
                if (!getListeners<windowLeaveFullscreen>().empty()) emit(windowLeaveFullscreen{ {{type, ts}, e.window.windowID} });
                break;
            case Events::window_destroyed:
                if (!getListeners<windowDestroyed>().empty()) emit(windowDestroyed{ {{type, ts}, e.window.windowID} });
                break;

                // --- Keyboard Events ---
            case Events::key_down:
                if (!getListeners<keyDown>().empty()) {
                    emit(keyDown{
                        {{type, ts}, e.key.windowID}, e.key.which,
                        static_cast<scancode>(e.key.scancode), Keycode{e.key.key},
                        static_cast<keymod>(e.key.mod), e.key.repeat != 0, e.key.down != 0
                        });
                }
                break;
            case Events::key_up:
                if (!getListeners<keyUp>().empty()) {
                    emit(keyUp{
                        {{type, ts}, e.key.windowID}, e.key.which,
                        static_cast<scancode>(e.key.scancode), Keycode{e.key.key},
                        static_cast<keymod>(e.key.mod), e.key.repeat != 0, e.key.down != 0
                        });
                }
                break;
            case Events::text_editing:
                if (!getListeners<textEditing>().empty()) {
                    emit(textEditing{
                        {{type, ts}, e.edit.windowID}, e.edit.text ? e.edit.text : "",
                        e.edit.start, e.edit.length
                        });
                }
                break;
            case Events::text_input:
                if (!getListeners<textInput>().empty()) emit(textInput{ {{type, ts}, e.text.windowID}, e.text.text ? e.text.text : "" });
                break;
            case Events::keymap_changed:
                if (!getListeners<keymapChanged>().empty()) emit(keymapChanged{ {type, ts} });
                break;

                // --- Mouse Events ---
            case Events::mouse_motion:
                if (!getListeners<mouseMotion>().empty()) {
                    emit(mouseMotion{
                        {{type, ts}, e.motion.windowID}, e.motion.which,
                        {e.motion.x, e.motion.y}, {e.motion.xrel, e.motion.yrel}, e.motion.state
                        });
                }
                break;
            case Events::mouse_button_down:
                if (!getListeners<mouseButtonDown>().empty()) {
                    emit(mouseButtonDown{
                        {{type, ts}, e.button.windowID}, e.button.which,
                        e.button.button, e.button.clicks, e.button.down != 0, {e.button.x, e.button.y}
                        });
                }
                break;
            case Events::mouse_button_up:
                if (!getListeners<mouseButtonUp>().empty()) {
                    emit(mouseButtonUp{
                        {{type, ts}, e.button.windowID}, e.button.which,
                        e.button.button, e.button.clicks, e.button.down != 0, {e.button.x, e.button.y}
                        });
                }
                break;
            case Events::mouse_wheel:
                if (!getListeners<mouseWheel>().empty()) {
                    emit(mouseWheel{
                        {{type, ts}, e.wheel.windowID}, e.wheel.which,
                        {e.wheel.x, e.wheel.y}, {e.wheel.mouse_x, e.wheel.mouse_y},
                        {e.wheel.integer_x, e.wheel.integer_y}, static_cast<wheel_direction>(e.wheel.direction)
                        });
                }
                break;

                // --- Joystick Events ---
            case Events::joystick_axis_motion:
                if (!getListeners<joystickAxisMoved>().empty()) emit(joystickAxisMoved{ {type, ts}, e.jaxis.which, e.jaxis.axis, e.jaxis.value });
                break;
            case Events::joystick_ball_motion:
                if (!getListeners<joystickBallMoved>().empty()) emit(joystickBallMoved{ {type, ts}, e.jball.which, e.jball.ball, {e.jball.xrel, e.jball.yrel} });
                break;
            case Events::joystick_hat_motion:
                if (!getListeners<joystickHatPressed>().empty()) emit(joystickHatPressed{ {type, ts}, e.jhat.which, e.jhat.hat, static_cast<hat_position>(e.jhat.value) });
                break;
            case Events::joystick_button_down:
                if (!getListeners<joystickButtonDown>().empty()) emit(joystickButtonDown{ {type, ts}, e.jbutton.which, e.jbutton.button, e.jbutton.down != 0 });
                break;
            case Events::joystick_button_up:
                if (!getListeners<joystickButtonUp>().empty()) emit(joystickButtonUp{ {type, ts}, e.jbutton.which, e.jbutton.button, e.jbutton.down != 0 });
                break;
            case Events::joystick_added:
                if (!getListeners<joystickAdded>().empty()) emit(joystickAdded{ {type, ts}, e.jdevice.which });
                break;
            case Events::joystick_removed:
                if (!getListeners<joystickRemoved>().empty()) emit(joystickRemoved{ {type, ts}, e.jdevice.which });
                break;
            case Events::joystick_battery_updated:
                if (!getListeners<joystickBattery>().empty()) emit(joystickBattery{ {type, ts}, e.jbattery.which, static_cast<power_state>(e.jbattery.state), (float)e.jbattery.percent });
                break;

                // --- Gamepad Events ---
            case Events::gamepad_axis_motion:
                if (!getListeners<gamepadAxis>().empty()) emit(gamepadAxis{ {type, ts}, e.gaxis.which, static_cast<gamepad_axis>(e.gaxis.axis), e.gaxis.value });
                break;
            case Events::gamepad_button_down:
                if (!getListeners<gamepadButtonDown>().empty()) emit(gamepadButtonDown{ {type, ts}, e.gbutton.which, static_cast<gamepad_button>(e.gbutton.button), e.gbutton.down != 0 });
                break;
            case Events::gamepad_button_up:
                if (!getListeners<gamepadButtonUp>().empty()) emit(gamepadButtonUp{ {type, ts}, e.gbutton.which, static_cast<gamepad_button>(e.gbutton.button), e.gbutton.down != 0 });
                break;
            case Events::gamepad_added:
                if (!getListeners<gamepadAdded>().empty()) emit(gamepadAdded{ {type, ts}, e.gdevice.which });
                break;
            case Events::gamepad_removed:
                if (!getListeners<gamepadRemoved>().empty()) emit(gamepadRemoved{ {type, ts}, e.gdevice.which });
                break;
            case Events::gamepad_remapped:
                if (!getListeners<gamepadRemapped>().empty()) emit(gamepadRemapped{ {type, ts}, e.gdevice.which });
                break;
            case Events::gamepad_touchpad_down:
                if (!getListeners<gamepadTouchpadDown>().empty()) emit(gamepadTouchpadDown{ {type, ts}, e.gtouchpad.which, e.gtouchpad.touchpad, e.gtouchpad.finger, {e.gtouchpad.x, e.gtouchpad.y}, e.gtouchpad.pressure });
                break;
            case Events::gamepad_touchpad_motion:
                if (!getListeners<gamepadTouchpadMotion>().empty()) emit(gamepadTouchpadMotion{ {type, ts}, e.gtouchpad.which, e.gtouchpad.touchpad, e.gtouchpad.finger, {e.gtouchpad.x, e.gtouchpad.y}, e.gtouchpad.pressure });
                break;
            case Events::gamepad_touchpad_up:
                if (!getListeners<gamepadTouchpadUp>().empty()) emit(gamepadTouchpadUp{ {type, ts}, e.gtouchpad.which, e.gtouchpad.touchpad, e.gtouchpad.finger, {e.gtouchpad.x, e.gtouchpad.y}, e.gtouchpad.pressure });
                break;
            case Events::gamepad_sensor_update:
                if (!getListeners<gamepadSensor>().empty()) emit(gamepadSensor{ {type, ts}, e.gsensor.which, static_cast<sensor_type>(e.gsensor.sensor), {e.gsensor.data[0], e.gsensor.data[1], e.gsensor.data[2]}, e.gsensor.sensor_timestamp });
                break;

                // --- Touch Events ---
            case Events::finger_down:
                if (!getListeners<fingerDown>().empty()) emit(fingerDown{ {type, ts}, e.tfinger.touchID, e.tfinger.fingerID, e.tfinger.windowID, {e.tfinger.x, e.tfinger.y}, {e.tfinger.dx, e.tfinger.dy}, e.tfinger.pressure });
                break;
            case Events::finger_motion:
                if (!getListeners<fingerMotion>().empty()) emit(fingerMotion{ {type, ts}, e.tfinger.touchID, e.tfinger.fingerID, e.tfinger.windowID, {e.tfinger.x, e.tfinger.y}, {e.tfinger.dx, e.tfinger.dy}, e.tfinger.pressure });
                break;
            case Events::finger_up:
                if (!getListeners<fingerUp>().empty()) emit(fingerUp{ {type, ts}, e.tfinger.touchID, e.tfinger.fingerID, e.tfinger.windowID, {e.tfinger.x, e.tfinger.y}, {e.tfinger.dx, e.tfinger.dy}, e.tfinger.pressure });
                break;

                // --- Drop Events ---
            case Events::drop_file:
                if (!getListeners<dropFile>().empty()) emit(dropFile{ {{type, ts}, e.drop.windowID}, {e.drop.x, e.drop.y}, e.drop.source ? e.drop.source : "", e.drop.data ? e.drop.data : "" });
                break;
            case Events::drop_text:
                if (!getListeners<dropText>().empty()) emit(dropText{ {{type, ts}, e.drop.windowID}, {e.drop.x, e.drop.y}, e.drop.source ? e.drop.source : "", e.drop.data ? e.drop.data : "" });
                break;
            case Events::drop_begin:
                if (!getListeners<dropBegin>().empty()) emit(dropBegin{ {{type, ts}, e.drop.windowID}, {e.drop.x, e.drop.y}, e.drop.source ? e.drop.source : "" });
                break;
            case Events::drop_complete:
                if (!getListeners<dropComplete>().empty()) emit(dropComplete{ {{type, ts}, e.drop.windowID}, {e.drop.x, e.drop.y}, e.drop.source ? e.drop.source : "" });
                break;
            case Events::drop_position:
                if (!getListeners<dropPosition>().empty()) emit(dropPosition{ {{type, ts}, e.drop.windowID}, {e.drop.x, e.drop.y}, e.drop.source ? e.drop.source : "", e.drop.data ? e.drop.data : "" });
                break;

                // --- Audio Device Events ---
            case Events::audio_device_added:
                if (!getListeners<audioDeviceAdded>().empty()) emit(audioDeviceAdded{ {type, ts}, e.adevice.which, e.adevice.recording != 0 });
                break;
            case Events::audio_device_removed:
                if (!getListeners<audioDeviceRemoved>().empty()) emit(audioDeviceRemoved{ {type, ts}, e.adevice.which, e.adevice.recording != 0 });
                break;
            case Events::audio_device_format_changed:
                if (!getListeners<audioDeviceFormatChanged>().empty()) emit(audioDeviceFormatChanged{ {type, ts}, e.adevice.which, e.adevice.recording != 0 });
                break;

                // --- Sensor Events ---
            case Events::sensor_update:
                if (!getListeners<sensorEvent>().empty()) {
                    sensorEvent se{ {type, ts}, e.sensor.which, {}, e.sensor.sensor_timestamp };
                    std::copy(std::begin(e.sensor.data), std::end(e.sensor.data), std::begin(se.data));
                    emit(se);
                }
                break;

                // --- Pen Events ---
            case Events::pen_proximity_in:
                if (!getListeners<penProximityIn>().empty()) emit(penProximityIn{ {type, ts}, e.pmotion.which, e.pmotion.windowID, {e.pmotion.x, e.pmotion.y} });
                break;
            case Events::pen_proximity_out:
                if (!getListeners<penProximityOut>().empty()) emit(penProximityOut{ {type, ts}, e.pmotion.which, e.pmotion.windowID, {e.pmotion.x, e.pmotion.y} });
                break;
            case Events::pen_motion:
                if (!getListeners<penMotion>().empty()) emit(penMotion{ {type, ts}, e.pmotion.which, e.pmotion.windowID, {e.pmotion.x, e.pmotion.y} });
                break;

                // --- Camera Device Events ---
            case Events::camera_device_added:
                if (!getListeners<cameraDeviceAdded>().empty()) emit(cameraDeviceAdded{ {type, ts}, e.cdevice.which });
                break;
            case Events::camera_device_removed:
                if (!getListeners<cameraDeviceRemoved>().empty()) emit(cameraDeviceRemoved{ {type, ts}, e.cdevice.which });
                break;
            case Events::camera_device_approved:
                if (!getListeners<cameraDeviceApproved>().empty()) emit(cameraDeviceApproved{ {type, ts}, e.cdevice.which });
                break;
            case Events::camera_device_denied:
                if (!getListeners<cameraDeviceDenied>().empty()) emit(cameraDeviceDenied{ {type, ts}, e.cdevice.which });
                break;

                // --- Render Events ---
            case Events::render_targets_reset:
                if (!getListeners<renderTargetsReset>().empty()) emit(renderTargetsReset{ {{type, ts}, e.window.windowID} });
                break;
            case Events::render_device_reset:
                if (!getListeners<renderDeviceReset>().empty()) emit(renderDeviceReset{ {{type, ts}, e.window.windowID} });
                break;

            default:
                break;
            }
        }
    };

    // Облегченная глобальная функция для кадра: только прокачивает очередь SDL3.
    // Нам больше не нужно самим бегать циклом по событиям ради инпутов — watcher все сделает за нас внутри SDL_PumpEvents / SDL_PollEvent.
    inline void pollEvents()
    {
        SDL_PumpEvents();
    }
}

// ===== File: window.hpp =====
#pragma once
#include "utilities.hpp"
#include "pixel_format.hpp"
#include "rectangle.hpp"
#include "surface.hpp"
#include "event.hpp"

#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include <string>
#include <string_view>
#include <memory>

namespace mngl
{
	// Предварительные объявления (Forward declarations)
	class window;
	class display;
	struct display_mode;

	class window_manager
	{
	public:

		static std::vector<mngl::window*> get_all_windows()
		{
			std::shared_lock lg(mut);
			std::vector<mngl::window*> wins;
			wins.reserve(elements.size()); // Избегаем лишних реаллокаций памяти
			for (const auto& [id, win] : elements)
				wins.push_back(win);
			return wins;
		}

		static mngl::window& get_window(SDL_Window* win)
		{
			std::shared_lock lg(mut);
			auto id = SDL_GetWindowID(win);
			return *elements.at(id);
		}

		static mngl::window& get_window(SDL_WindowID id)
		{
			std::shared_lock lg(mut);
			return *elements.at(id);
		}

		static mngl::window& parent(SDL_Window* win)
		{
			std::shared_lock lg(mut);
			auto prnt = SDL_GetWindowParent(win);
			if (!prnt) throw sdl_error("window does not have a parent!");
			return *elements.at(SDL_GetWindowID(prnt));
		}

		static mngl::window& parent(SDL_WindowID id)
		{
			std::shared_lock lg(mut);
			auto win = SDL_GetWindowFromID(id);
			auto prnt = SDL_GetWindowParent(win);
			if (!prnt) throw sdl_error("window does not have a parent!");
			return *elements.at(SDL_GetWindowID(prnt));
		}

	private:
		friend class window;
		static inline std::unordered_map<SDL_WindowID, mngl::window*> elements;
		static inline std::shared_mutex mut;

		static void add_window(SDL_Window* win, mngl::window* data)
		{
			std::unique_lock lg(mut);
			auto id = SDL_GetWindowID(win);
			elements.insert({ id, data });
		}

		static void remove_window(SDL_Window* win)
		{
			std::unique_lock lg(mut);
			auto id = SDL_GetWindowID(win);
			elements.erase(id);
		}

		window_manager(const window_manager&) = delete;
		window_manager& operator=(const window_manager&) = delete;
		window_manager(window_manager&&) = delete;
		window_manager& operator=(window_manager&&) = delete;
		window_manager() = default;
		~window_manager() = default;
	};

	class display
	{
	public:
		~display() = default;
		display(const display& other) : disp(other.disp) {}
		display(display&& other) noexcept
		{
			std::swap(other.disp, disp);
		}
		display& operator=(display other)
		{
			std::swap(other.disp, disp);
			return *this;
		}

		SDL_DisplayID raw() const
		{
			return disp;
		}

		static std::vector<display> displays()
		{
			std::lock_guard lg(mut);
			int cnt = 0;
			std::unique_ptr<SDL_DisplayID[], decltype(&SDL_free)> guard(SDL_GetDisplays(&cnt), &SDL_free);
			auto* d = guard.get();
			if (d == nullptr) throw sdl_error("error getting displays: ");
			std::vector<display> res;
			res.reserve(cnt);
			for (; cnt > 0; cnt--, d++)
			{
				res.push_back(*d);
			}
			return res; // RVO отлично сработает и без std::move
		}

		static display primary()
		{
			std::lock_guard lg(mut);
			auto d = SDL_GetPrimaryDisplay();
			if (d == 0) throw sdl_error("failed to get priary display: ");
			return d;
		}

		std::string_view name() const
		{
			auto* n = SDL_GetDisplayName(disp);
			if (!n) throw sdl_error("failed to get display name: ");
			return n;
		}

		mngl::rectangle bounds() const
		{
			mngl::rectangle rect;
			if (!SDL_GetDisplayBounds(disp, mngl::rectangleProxy(rect))) throw sdl_error("failed to get disply bounds: ");
			return rect;
		}

		mngl::rectangle usable_bounds() const
		{
			mngl::rectangle rect;
			if (!SDL_GetDisplayUsableBounds(disp, mngl::rectangleProxy(rect))) throw sdl_error("failed to get disply bounds: ");
			return rect;
		}

		float scale() const
		{
			float s = SDL_GetDisplayContentScale(disp);
			if (s == 0.0f) throw sdl_error("failed to get content scale of display: ");
			return s;
		}

		mngl::display_orientation orientation() const
		{
			mngl::display_orientation orient =
				static_cast<mngl::display_orientation>(SDL_GetNaturalDisplayOrientation(disp));
#ifdef MNGL_DISPLAYORIENTATION_UNKNOWS_IS_ERROR
			if (orient == mngl::display_orientation::unknown) throw std::runtime_error("display orientation is unavaible!");
#endif
			return orient;
		}

		mngl::display_mode current_mode() const;
		std::vector<mngl::display_mode> fullscreen_modes() const;
		mngl::display_mode closest_mode(glm::ivec2 size, float fps, bool include_hdpi = true) const;

		static display for_point(glm::ivec2 pnt)
		{
			std::lock_guard lg(mut);
			SDL_Point point{ pnt.x, pnt.y }; // Безопасное создание структуры вместо reinterpret_cast
			auto d = SDL_GetDisplayForPoint(&point);
			if (!d) throw sdl_error("failed to find such display for point " + to_string(pnt) + ": ");
			return d;
		}

		static display for_rect(mngl::rectangle rect)
		{
			std::lock_guard lg(mut);
			auto d = SDL_GetDisplayForRect(mngl::rectangleProxy(rect));
			if (!d) throw sdl_error("failed to find such display for rectangle at position " + to_string(rect.pos) + " and size " + to_string(rect.size) + ": ");
			return d;
		}

		static display for_window(const mngl::window& win);

	private:
		display() = default;
		display(SDL_DisplayID id) : disp(id) {}
		SDL_DisplayID disp = 0;
		static inline std::mutex mut;

		friend class display_mode;
	};

	struct display_mode
	{
		display_mode(SDL_DisplayMode mode) :
			disp(mode.displayID),
			format(static_cast<mngl::pixel_format>(mode.format)),
			size({ mode.w, mode.h }),
			pixel_density(mode.pixel_density),
			fps(mode.refresh_rate),
			fps_rational({ mode.refresh_rate_numerator, mode.refresh_rate_denominator })
		{}
		~display_mode() = default;

		mngl::display disp;
		mngl::pixel_format format;
		glm::ivec2 size;
		float pixel_density;
		float fps;
		glm::ivec2 fps_rational;

		operator SDL_DisplayMode() const
		{
			return SDL_DisplayMode{
				disp.raw(),
				static_cast<SDL_PixelFormat>(format),
				size.x,
				size.y,
				pixel_density,
				fps,
				fps_rational.x,
				fps_rational.y,
				sdl_internal
			};
		}

	private:
		SDL_DisplayModeData* sdl_internal = nullptr; // не трогать, я не ебу что оно делает
		display_mode() = default; // не разрешено делать долбаёбам
		friend class display;
	};

	// Реализация отложенных методов display (после объявления display_mode)
	inline mngl::display_mode display::current_mode() const
	{
		const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(disp);
		if (!mode) throw sdl_error("failed to get display mode: ");
		return *mode;
	}

	inline std::vector<mngl::display_mode> display::fullscreen_modes() const
	{
		int cnt = 0;
		auto raw = SDL_GetFullscreenDisplayModes(disp, &cnt);
		auto deleter = [](SDL_DisplayMode** p) { if (p) SDL_free(p); };
		std::unique_ptr<SDL_DisplayMode*, decltype(deleter)> modes(raw, deleter);
		std::vector<mngl::display_mode> view;
		view.reserve(cnt);
		if (!modes && cnt > 0) throw sdl_error("error getting fullscreen modes: ");
		for (auto i = modes.get(); i < modes.get() + cnt; i++)
		{
			if (*i) view.push_back(**i);
		}
		return view;
	}

	inline mngl::display_mode display::closest_mode(glm::ivec2 size, float fps, bool include_hdpi) const
	{
		SDL_DisplayMode out;
		if (!SDL_GetClosestFullscreenDisplayMode(disp, size.x, size.y, fps, include_hdpi, &out)) throw sdl_error("error finding closest display mode: ");
		return out;
	}

	class window
	{
	public:

		struct flags
		{
			bool fullscreen : 1 = false;
			bool hidden : 1 = false;
			bool borderless : 1 = false;
			bool resizeable : 1 = false;
			bool minimized : 1 = false;
			bool maximized : 1 = false;
			bool mouse_grabbed : 1 = false;
			bool modal : 1 = false;
			bool high_pixel_density : 1 = false;
			bool always_on_top : 1 = false;
			bool utility : 1 = false;
			bool tooltip : 1 = false;
			bool popup_menu : 1 = false;
			bool keyboard_grabbed : 1 = false;
			bool transparent : 1 = false;
			bool not_focusable : 1 = false;

			enum class backends : uint8_t
			{
				none,
				opengl,
				vulkan,
				metal,
			} backend = backends::none;

			operator SDL_WindowFlags() const noexcept
			{
				SDL_WindowFlags f = 0;

				if (fullscreen)         f |= SDL_WINDOW_FULLSCREEN;
				if (hidden)             f |= SDL_WINDOW_HIDDEN;
				if (borderless)         f |= SDL_WINDOW_BORDERLESS;
				if (resizeable)         f |= SDL_WINDOW_RESIZABLE;
				if (minimized)          f |= SDL_WINDOW_MINIMIZED;
				if (maximized)          f |= SDL_WINDOW_MAXIMIZED;
				if (mouse_grabbed)      f |= SDL_WINDOW_MOUSE_GRABBED;
				if (modal)              f |= SDL_WINDOW_MODAL;
				if (high_pixel_density) f |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
				if (always_on_top)      f |= SDL_WINDOW_ALWAYS_ON_TOP;
				if (utility)            f |= SDL_WINDOW_UTILITY;
				if (tooltip)            f |= SDL_WINDOW_TOOLTIP;
				if (popup_menu)         f |= SDL_WINDOW_POPUP_MENU;
				if (keyboard_grabbed)   f |= SDL_WINDOW_KEYBOARD_GRABBED;
				if (transparent)        f |= SDL_WINDOW_TRANSPARENT;
				if (not_focusable)      f |= SDL_WINDOW_NOT_FOCUSABLE;

				switch (backend)
				{
				case backends::opengl: f |= SDL_WINDOW_OPENGL; break;
				case backends::vulkan: f |= SDL_WINDOW_VULKAN; break;
				case backends::metal:  f |= SDL_WINDOW_METAL;  break;
				case backends::none:   break;
				}

				return f;
			}

			flags() = default;
			flags(SDL_WindowFlags fl)
			{
				fullscreen = (fl & SDL_WINDOW_FULLSCREEN) != 0;
				hidden = (fl & SDL_WINDOW_HIDDEN) != 0;
				borderless = (fl & SDL_WINDOW_BORDERLESS) != 0;
				resizeable = (fl & SDL_WINDOW_RESIZABLE) != 0;
				minimized = (fl & SDL_WINDOW_MINIMIZED) != 0;
				maximized = (fl & SDL_WINDOW_MAXIMIZED) != 0;
				mouse_grabbed = (fl & SDL_WINDOW_MOUSE_GRABBED) != 0;
				modal = (fl & SDL_WINDOW_MODAL) != 0;
				high_pixel_density = (fl & SDL_WINDOW_HIGH_PIXEL_DENSITY) != 0;
				always_on_top = (fl & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
				utility = (fl & SDL_WINDOW_UTILITY) != 0;
				tooltip = (fl & SDL_WINDOW_TOOLTIP) != 0;
				popup_menu = (fl & SDL_WINDOW_POPUP_MENU) != 0;
				keyboard_grabbed = (fl & SDL_WINDOW_KEYBOARD_GRABBED) != 0;
				transparent = (fl & SDL_WINDOW_TRANSPARENT) != 0;
				not_focusable = (fl & SDL_WINDOW_NOT_FOCUSABLE) != 0;

				if ((fl & SDL_WINDOW_OPENGL) != 0)
					backend = backends::opengl;
				else if ((fl & SDL_WINDOW_VULKAN) != 0)
					backend = backends::vulkan;
				else if ((fl & SDL_WINDOW_METAL) != 0)
					backend = backends::metal;
				else
					backend = backends::none;
			}
		};

		window(const std::string& name, glm::uvec2 size, flags f = flags())
		{
			win = SDL_CreateWindow(name.data(), size.x, size.y, f);
			if (!win) throw sdl_error("failed to create window: ");
			logger.debug("created window with parametrs: title = ", name, "; size = {", size.x, ",", size.y, "}; flags = ", SDL_WindowFlags(f));
			logger.debug("window located in ", win);
			window_manager::add_window(win, this);
		}

		window(const window&) = delete;
		window& operator=(const window&) = delete;

		window(window&& other) noexcept
		{
			win = other.win;
			other.win = nullptr;
			if (win)
			{
				window_manager::remove_window(other.win);
				window_manager::add_window(win, this);
				logger.debug("moved window ", win);
			}
		}

		window& operator=(window&& other) noexcept
		{
			if (this == &other) return *this;
			if (win)
			{
				SDL_DestroyWindow(win);
				window_manager::remove_window(win);
				logger.debug("destroyed old window ", win, " due to move assignment");
			}
			window_manager::remove_window(other.win);
			win = other.win;
			other.win = nullptr;
			window_manager::add_window(win, this);
			logger.debug("moved window ", win);
			return *this;
		}

		~window()
		{
			if (win)
			{
				SDL_DestroyWindow(win);
				window_manager::remove_window(win);
				logger.debug("destroyed window ", win);
			}
		}

		SDL_WindowID id() const
		{
			return SDL_GetWindowID(win);
		}

		operator SDL_WindowID() const
		{
			return id();
		}

		SDL_Window* raw() const
		{
			return win;
		}

		explicit operator SDL_Window* () const
		{
			return raw();
		}

		// геттеры и сеттеры

		// заголовок окна
		std::string title() const
		{
			auto* title = SDL_GetWindowTitle(win);
			if (!title) throw sdl_error("failed to get window title: ");
			return title;
		}

		void title(const std::string& new_title)
		{
			// ФИКС БАГА SDL3: проверка через оператор отрицания (!)
			if (!SDL_SetWindowTitle(win, new_title.data())) throw sdl_error("failed to set window title: ");
			logger.debug("changed window ", win, " title to ", new_title);
		}

		// размер окна
		glm::uvec2 size() const
		{
			int w, h;
			if (!SDL_GetWindowSize(win, &w, &h)) throw sdl_error("failed to get window size: ");
			return { static_cast<unsigned int>(w), static_cast<unsigned int>(h) };
		}

		void size(glm::uvec2 new_size)
		{
			SDL_SetWindowSize(win, new_size.x, new_size.y);
			logger.debug("changed window ", win, " size to {", new_size.x, ",", new_size.y, "}");
		}

		// позиция окна
		glm::ivec2 position() const
		{
			int x, y;
			if (!SDL_GetWindowPosition(win, &x, &y)) throw sdl_error("failed to get window position: ");
			return { x, y };
		}

		void position(glm::ivec2 new_pos)
		{
			if (!SDL_SetWindowPosition(win, new_pos.x, new_pos.y)) throw sdl_error("failed to set window position: ");
			logger.debug("changed window ", win, " position to {", new_pos.x, ",", new_pos.y, "}");
		}

		void move(glm::ivec2 delta)
		{
			auto pos = position();
			position(pos + delta);
		}

		// минимальный / максимальный размер окна
		glm::uvec2 min_size() const
		{
			int w, h;
			if (!SDL_GetWindowMinimumSize(win, &w, &h)) throw sdl_error("failed to get window minimum size: ");
			return { static_cast<unsigned int>(w), static_cast<unsigned int>(h) };
		}

		void min_size(glm::uvec2 new_min_size)
		{
			if (!SDL_SetWindowMinimumSize(win, new_min_size.x, new_min_size.y)) throw sdl_error("failed to set window minimum size: ");
			logger.debug("changed window ", win, " minimum size to {", new_min_size.x, ",", new_min_size.y, "}");
		}

		glm::uvec2 max_size() const
		{
			int w, h;
			if (!SDL_GetWindowMaximumSize(win, &w, &h)) throw sdl_error("failed to get window maximum size: ");
			return { static_cast<unsigned int>(w), static_cast<unsigned int>(h) };
		}

		void max_size(glm::uvec2 new_max_size)
		{
			if (!SDL_SetWindowMaximumSize(win, new_max_size.x, new_max_size.y)) throw sdl_error("failed to set window maximum size: ");
			logger.debug("changed window ", win, " maximum size to {", new_max_size.x, ",", new_max_size.y, "}");
		}

		struct window_borders
		{
			int top = 0;
			int left = 0;
			int bottom = 0;
			int right = 0;
		};

		window_borders borders() const
		{
			window_borders res;
			if (!SDL_GetWindowBordersSize(win, &res.top, &res.left, &res.bottom, &res.right))
				throw sdl_error("failed to get window borders size: ");
			return res;
		}

		// флаги
		flags get_flags() const
		{
			return SDL_GetWindowFlags(win);
		}

		void show()
		{
			if (!SDL_ShowWindow(win)) throw sdl_error("failed to show window: ");
			logger.debug("shown window ", win);
		}

		void hide()
		{
			if (!SDL_HideWindow(win)) throw sdl_error("failed to hide windows: ");
			logger.debug("hidden window ", win);
		}

		void raise()
		{
			if (!SDL_RaiseWindow(win)) throw sdl_error("failed to raise window: ");
			logger.debug("raised window ", win);
		}

		void maximize()
		{
			if (!SDL_MaximizeWindow(win)) throw sdl_error("failed to maximize window: ");
			logger.debug("maximized window ", win);
		}

		void minimize()
		{
			if (!SDL_MinimizeWindow(win)) throw sdl_error("failed to minimize window: ");
			logger.debug("minimized window ", win);
		}

		void restore()
		{
			if (!SDL_RestoreWindow(win)) throw sdl_error("failed to restore window: ");
			logger.debug("restored window ", win);
		}

		// полный экран
		void fullscreen(bool to = true)
		{
			if (!SDL_SetWindowFullscreen(win, to ? SDL_WINDOW_FULLSCREEN : 0)) throw sdl_error("failed to set window fullscreen state: ");
			logger.debug((to ? "enabled" : "disabled"), " fullscreen for window ", win);
		}

		void unfullscreen()
		{
			fullscreen(false);
		}

		bool fullscreened() const
		{
			return (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
		}

		mngl::display_mode mode() const
		{
			return mngl::display::for_window(*this).current_mode();
		}

		mngl::display attached_display() const
		{
			return mngl::display::for_window(*this);
		}

		mngl::rectangle safe_area() const
		{
			mngl::rectangle res;
			if (!SDL_GetWindowSafeArea(win, mngl::rectangleProxy(res))) throw sdl_error("failed to get safe area for window: ");
			return res;
		}

		void parent(const window& other)
		{
			if (!SDL_SetWindowParent(win, other.win)) throw sdl_error("failed to set parent for window: ");
		}

		window& parent()
		{
			return mngl::window_manager::parent(win);
		}

		const window& parent() const
		{
			return mngl::window_manager::parent(win);
		}

		void modal(bool to)
		{
			if (!SDL_SetWindowModal(win, to)) throw sdl_error("failed to set window modality to " + std::to_string(to) + ": ");
		}

		// другая поебень
		glm::uvec2 pixel_size() const
		{
			int w, h;
			if (!SDL_GetWindowSizeInPixels(win, &w, &h))
				throw sdl_error("failed to get window pixel size: ");
			return { static_cast<unsigned int>(w), static_cast<unsigned int>(h) };
		}

		void aspect_ratio(float min_aspect, float max_aspect)
		{
			if (!SDL_SetWindowAspectRatio(win, min_aspect, max_aspect))
				throw sdl_error("failed to set window aspect ratio: ");
		}

		void mouse_grab(bool enable)
		{
			if (!SDL_SetWindowMouseGrab(win, enable))
				throw sdl_error("failed to set mouse grab: ");
		}

		bool mouse_grab() const
		{
			return SDL_GetWindowMouseGrab(win);
		}

		bool has_keyboard_focus() const
		{
			return (SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS) != 0;
		}

		void clipboard_text(const char* text)
		{
			if (!SDL_SetClipboardText(text)) throw sdl_error("failed to set clipboard text: ");
		}

		std::string clipboard_text() const
		{
			auto* text = SDL_GetClipboardText();
			if (!text) return "";
			std::string str(text);
			SDL_free(text);
			return str;
		}

		void flash(bool enable = true)
		{
			if (!SDL_FlashWindow(win, enable ? SDL_FLASH_UNTIL_FOCUSED : SDL_FLASH_BRIEFLY)) throw sdl_error("failed to flash window: ");
		}

		float opacity() const
		{
			// ФИКС БАГА С ПРИОРИТЕТОМ: разделили вызов функции и проверку на ошибку
			float val = SDL_GetWindowOpacity(win);
			if (val < 0.0f) throw sdl_error("failed to get window opacity: ");
			return val;
		}

		void opacity(float factor)
		{
			if (!SDL_SetWindowOpacity(win, factor)) throw sdl_error("failed to set window opacity: ");
			logger.debug("changed window ", win, " opacity to ", factor);
		}

		void icon(mngl::surface& icon)
		{
			if (!SDL_SetWindowIcon(win, icon.raw())) throw sdl_error("failed to set icon!");

			if (icon.animated())
			{
				SDL_Window* raw_win = this->win;
				icon.on_frame_changed = [raw_win](SDL_Surface* new_frame)
				{
					if (!SDL_SetWindowIcon(raw_win, new_frame)) throw sdl_error("failed to set icon frame!");
				};
			}

			logger.debug("changed window ", win, " icon");
		}

		void borderless(bool enable)
		{
			// В SDL3 передаем !enable, так как функция ожидает параметр "bordered" (наличие рамки)
			if (!SDL_SetWindowBordered(win, !enable))
				throw sdl_error("failed to set window borderless state: ");
			logger.debug((enable ? "enabled" : "disabled"), " borderless mode for window ", win);
		}

		bool borderless() const
		{
			return (SDL_GetWindowFlags(win) & SDL_WINDOW_BORDERLESS) != 0;
		}

		bool transparent() const
		{
			return (SDL_GetWindowFlags(win) & SDL_WINDOW_TRANSPARENT) != 0;
		}

	private:
		SDL_Window* win = nullptr;
	};

	// Реализация display::for_window (после того как класс window полностью объявлен)
	inline display display::for_window(const mngl::window& win)
	{
		std::lock_guard lg(mut);
		auto d = SDL_GetDisplayForWindow(win.raw());
		if (!d) throw sdl_error("failed to find display for window " + std::to_string(reinterpret_cast<uintptr_t>(win.raw())) + ": ");
		return d;
	}
}

// ===== File: device.hpp =====
#pragma once
#include "utilities.hpp"
#include "window.hpp"

#include <SDL3/SDL_gpu.h>

namespace mngl::gpu
{

	enum class texture_format
	{
		invalid = SDL_GPU_TEXTUREFORMAT_INVALID,

		/* Unsigned Normalized Float Color Formats */
		a8_unorm = SDL_GPU_TEXTUREFORMAT_A8_UNORM,
		r8_unorm = SDL_GPU_TEXTUREFORMAT_R8_UNORM,
		r8g8_unorm = SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,
		r8g8b8a8_unorm = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		r16_unorm = SDL_GPU_TEXTUREFORMAT_R16_UNORM,
		r16g16_unorm = SDL_GPU_TEXTUREFORMAT_R16G16_UNORM,
		r16g16b16a16_unorm = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM,
		r10g10b10a2_unorm = SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM,
		b5g6r5_unorm = SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM,
		b5g5r5a1_unorm = SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM,
		b4g4r4a4_unorm = SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM,
		b8g8r8a8_unorm = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,

		/* Compressed Unsigned Normalized Float Color Formats */
		bc1_rgba_unorm = SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM,
		bc2_rgba_unorm = SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM,
		bc3_rgba_unorm = SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM,
		bc4_r_unorm = SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM,
		bc5_rg_unorm = SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM,
		bc7_rgba_unorm = SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM,

		/* Compressed Signed Float Color Formats */
		bc6h_rgb_float = SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT,

		/* Compressed Unsigned Float Color Formats */
		bc6h_rgb_ufloat = SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT,

		/* Signed Normalized Float Color Formats  */
		r8_snorm = SDL_GPU_TEXTUREFORMAT_R8_SNORM,
		r8g8_snorm = SDL_GPU_TEXTUREFORMAT_R8G8_SNORM,
		r8g8b8a8_snorm = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
		r16_snorm = SDL_GPU_TEXTUREFORMAT_R16_SNORM,
		r16g16_snorm = SDL_GPU_TEXTUREFORMAT_R16G16_SNORM,
		r16g16b16a16_snorm = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SNORM,

		/* Signed Float Color Formats */
		r16_float = SDL_GPU_TEXTUREFORMAT_R16_FLOAT,
		r16g16_float = SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT,
		r16g16b16a16_float = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
		r32_float = SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
		r32g32_float = SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT,
		r32g32b32a32_float = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,

		/* Unsigned Float Color Formats */
		r11g11b10_ufloat = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,

		/* Unsigned Integer Color Formats */
		r8_uint = SDL_GPU_TEXTUREFORMAT_R8_UINT,
		r8g8_uint = SDL_GPU_TEXTUREFORMAT_R8G8_UINT,
		r8g8b8a8_uint = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT,
		r16_uint = SDL_GPU_TEXTUREFORMAT_R16_UINT,
		r16g16_uint = SDL_GPU_TEXTUREFORMAT_R16G16_UINT,
		r16g16b16a16_uint = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT,
		r32_uint = SDL_GPU_TEXTUREFORMAT_R32_UINT,
		r32g32_uint = SDL_GPU_TEXTUREFORMAT_R32G32_UINT,
		r32g32b32a32_uint = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT,

		/* Signed Integer Color Formats */
		r8_int = SDL_GPU_TEXTUREFORMAT_R8_INT,
		r8g8_int = SDL_GPU_TEXTUREFORMAT_R8G8_INT,
		r8g8b8a8_int = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_INT,
		r16_int = SDL_GPU_TEXTUREFORMAT_R16_INT,
		r16g16_int = SDL_GPU_TEXTUREFORMAT_R16G16_INT,
		r16g16b16a16_int = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_INT,
		r32_int = SDL_GPU_TEXTUREFORMAT_R32_INT,
		r32g32_int = SDL_GPU_TEXTUREFORMAT_R32G32_INT,
		r32g32b32a32_int = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT,

		/* SRGB Unsigned Normalized Color Formats */
		r8g8b8a8_unorm_srgb = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
		b8g8r8a8_unorm_srgb = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,

		/* Compressed SRGB Unsigned Normalized Color Formats */
		bc1_rgba_unorm_srgb = SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB,
		bc2_rgba_unorm_srgb = SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM_SRGB,
		bc3_rgba_unorm_srgb = SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB,
		bc7_rgba_unorm_srgb = SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB,

		/* Depth Formats */
		d16_unorm = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
		d24_unorm = SDL_GPU_TEXTUREFORMAT_D24_UNORM,
		d32_float = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
		d24_unorm_s8_uint = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
		d32_float_s8_uint = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,

		/* Compressed ASTC Normalized Float Color Formats */
		astc_4x4_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM,
		astc_5x4_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM,
		astc_5x5_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM,
		astc_6x5_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM,
		astc_6x6_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM,
		astc_8x5_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM,
		astc_8x6_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM,
		astc_8x8_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM,
		astc_10x5_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM,
		astc_10x6_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM,
		astc_10x8_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM,
		astc_10x10_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM,
		astc_12x10_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM,
		astc_12x12_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM,

		/* Compressed SRGB ASTC Normalized Float Color Formats */
		astc_4x4_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM_SRGB,
		astc_5x4_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM_SRGB,
		astc_5x5_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM_SRGB,
		astc_6x5_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM_SRGB,
		astc_6x6_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM_SRGB,
		astc_8x5_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM_SRGB,
		astc_8x6_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM_SRGB,
		astc_8x8_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM_SRGB,
		astc_10x5_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM_SRGB,
		astc_10x6_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM_SRGB,
		astc_10x8_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM_SRGB,
		astc_10x10_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM_SRGB,
		astc_12x10_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM_SRGB,
		astc_12x12_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM_SRGB,

		/* Compressed ASTC Signed Float Color Formats */
		astc_4x4_float = SDL_GPU_TEXTUREFORMAT_ASTC_4x4_FLOAT,
		astc_5x4_float = SDL_GPU_TEXTUREFORMAT_ASTC_5x4_FLOAT,
		astc_5x5_float = SDL_GPU_TEXTUREFORMAT_ASTC_5x5_FLOAT,
		astc_6x5_float = SDL_GPU_TEXTUREFORMAT_ASTC_6x5_FLOAT,
		astc_6x6_float = SDL_GPU_TEXTUREFORMAT_ASTC_6x6_FLOAT,
		astc_8x5_float = SDL_GPU_TEXTUREFORMAT_ASTC_8x5_FLOAT,
		astc_8x6_float = SDL_GPU_TEXTUREFORMAT_ASTC_8x6_FLOAT,
		astc_8x8_float = SDL_GPU_TEXTUREFORMAT_ASTC_8x8_FLOAT,
		astc_10x5_float = SDL_GPU_TEXTUREFORMAT_ASTC_10x5_FLOAT,
		astc_10x6_float = SDL_GPU_TEXTUREFORMAT_ASTC_10x6_FLOAT,
		astc_10x8_float = SDL_GPU_TEXTUREFORMAT_ASTC_10x8_FLOAT,
		astc_10x10_float = SDL_GPU_TEXTUREFORMAT_ASTC_10x10_FLOAT,
		astc_12x10_float = SDL_GPU_TEXTUREFORMAT_ASTC_12x10_FLOAT,
		astc_12x12_float = SDL_GPU_TEXTUREFORMAT_ASTC_12x12_FLOAT
	};

	struct shader_formats
	{
		constexpr static inline int spirv = SDL_GPU_SHADERFORMAT_SPIRV;
		constexpr static inline int dxil = SDL_GPU_SHADERFORMAT_DXIL;
		constexpr static inline int msl = SDL_GPU_SHADERFORMAT_MSL;
		constexpr static inline int dxbc = SDL_GPU_SHADERFORMAT_DXBC;
		constexpr static inline int metallib = SDL_GPU_SHADERFORMAT_METALLIB;
		constexpr static inline int invalid = SDL_GPU_SHADERFORMAT_INVALID;
	};

	typedef uint32_t shader_format;

#ifdef SDL_PLATFORM_WINDOWS
#define MNGL_DEFAULT_SHADER_FORMAT shader_formats::dxil
#elifdef SDL_PLATFORM_MACOS
#define MNGL_DEFAULT_SHADER_FORMAT shader_formats::msl
#else
#define MNGL_DEFAULT_SHADER_FORMAT shader_formats::spirv
#endif

	class device
	{
	public:
		constexpr static inline SDL_GPUShaderFormat base_formats =
			shader_formats::spirv |
			shader_formats::dxil |
			shader_formats::msl;

		SDL_GPUDevice* raw() const
		{
			return handle;
		}

		void wait() const
		{
			SDL_WaitForGPUIdle(handle);
		}

		device(const device&) = delete;
		device& operator=(const device&) = delete;

		device(device&& o) noexcept : handle(o.handle) { o.handle = nullptr; }

		device& operator=(device&& o) noexcept
		{
			if (this != &o)
			{
				if (handle)
				{
					wait();
					SDL_DestroyGPUDevice(handle);
				}
				handle = o.handle;
				o.handle = nullptr;
			}
			return *this;
		}

		device(SDL_GPUShaderFormat allowed_formats = -1)
		{
			if (allowed_formats == -1) allowed_formats = base_formats;
			TRY_SDL(handle = SDL_CreateGPUDevice(allowed_formats, debug_mode, nullptr));
		}

		~device()
		{
			if (handle)
			{
				wait();
				SDL_DestroyGPUDevice(handle);
			}
		}

		SDL_GPUShaderFormat shader_format() const
		{
			return SDL_GetGPUShaderFormats(handle);
		}

		void claim_window(window& win)
		{
			TRY_SDL(SDL_ClaimWindowForGPUDevice(handle, win.raw()));
		}
		void release_window(window& win)
		{
			SDL_ReleaseWindowFromGPUDevice(handle, win.raw());
		}

		enum class hdr_mode
		{
			sdr = SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
			sdr_linear = SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
			hdr = SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR,
			hdr10 = SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2084
		};

		enum class vsync_mode
		{
			vsync = SDL_GPU_PRESENTMODE_VSYNC,
			immediate = SDL_GPU_PRESENTMODE_IMMEDIATE,
			mailbox = SDL_GPU_PRESENTMODE_MAILBOX
		};
		
		void swapchain_parametrs(window& win, hdr_mode hdr, vsync_mode vsync)
		{
			TRY_SDL(SDL_SetGPUSwapchainParameters(
				handle,
				win.raw(),
				(SDL_GPUSwapchainComposition)hdr,
				(SDL_GPUPresentMode)vsync));
		}

		texture_format swapchain_texture_format(const window& win) const
		{
			return (texture_format)SDL_GetGPUSwapchainTextureFormat(handle, win.raw());
		}
	private:
		SDL_GPUDevice* handle;
	};
}

// ===== File: texture.hpp =====
#pragma once

#include "utilities.hpp"
#include "device.hpp"

namespace mngl::gpu
{
	class texture
	{
	public:
		texture() {}
		~texture()
		{
			if (handle && !is_swapchain) SDL_ReleaseGPUTexture(dev, handle);
		}
		texture(const texture& other) = delete;
		texture& operator=(const texture& other) = delete;
		texture(texture&& other)
		{
			swap(*this, other);
		}
		texture& operator=(texture&& other)
		{
			swap(*this, other);
			return *this;
		}

		texture(const device& device, glm::uvec2 texture_size, texture_format format, uint32_t flags)
			: dev(device.raw()), fmt(format), usage_flag(flags), size(texture_size), is_swapchain(false)
		{
			SDL_GPUTextureCreateInfo create_info{};
			create_info.type = SDL_GPU_TEXTURETYPE_2D;
			create_info.format = static_cast<SDL_GPUTextureFormat>(format);
			create_info.usage = flags;
			create_info.width = size.x;
			create_info.height = size.y;
			create_info.layer_count_or_depth = 1;
			create_info.num_levels = 1; // Для начала без мип-мапов

			TRY_SDL(handle = SDL_CreateGPUTexture(dev, &create_info));
		}

		struct usage_flags
		{
			constexpr static inline auto sampler			= SDL_GPU_TEXTUREUSAGE_SAMPLER;
			constexpr static inline auto color_target		= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
			constexpr static inline auto stencil_target	= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
			constexpr static inline auto graphics_read	= SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ;
			constexpr static inline auto compute_read		= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
			constexpr static inline auto compute_write	= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
			constexpr static inline auto compute_rw		= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE;
		};

		// Проверка на валидность
		bool valid() const { return handle != nullptr; }
		explicit operator bool() const { return valid(); }

		// Доступ к сырым объектам SDL
		SDL_GPUTexture* raw() const { return handle; }
		SDL_GPUDevice* device_raw() const { return dev; }

		// Параметры текстуры
		glm::uvec2 get_size() const { return size; }
		uint32_t width() const { return size.x; }
		uint32_t height() const { return size.y; }
		texture_format format() const { return fmt; }

		// Проверка флагов использования (для ассертов)
		bool has_usage(uint32_t flag) const { return (usage_flag & flag) != 0; }
		bool is_swapchain_texture() const { return is_swapchain; }

	private:
		SDL_GPUTexture* handle	= nullptr;
		SDL_GPUDevice* dev		= nullptr;
		texture_format fmt		= texture_format::invalid;
		uint32_t usage_flag		= 0;
		glm::uvec2 size			= { 0, 0 };
		bool is_swapchain		= false;

		friend class command_buffer;

		texture(SDL_GPUDevice* device, SDL_GPUTexture* swapchain_handle, texture_format format, glm::uvec2 texture_size)
			: handle(swapchain_handle), dev(device), fmt(format),
			usage_flag(SDL_GPU_TEXTUREUSAGE_COLOR_TARGET), size(texture_size), is_swapchain(true)
		{}

		friend inline void swap(mngl::gpu::texture& l, mngl::gpu::texture& r)
		{
			using std::swap;
			swap(l.handle, r.handle);
			swap(l.dev, r.dev);
			swap(l.fmt, r.fmt);
			swap(l.usage_flag, r.usage_flag);
			swap(l.size, r.size);
			swap(l.is_swapchain, r.is_swapchain);
		}
	};
}

// ===== File: command_buffer.hpp =====
#pragma once
#include "utilities.hpp"
#include "device.hpp"
#include "texture.hpp"


namespace mngl::gpu
{
	class command_buffer
	{
	public:
		explicit command_buffer(SDL_GPUCommandBuffer* cb) : handle(cb) {}
		
		command_buffer(const device& dev)
		{
			TRY_SDL(handle = SDL_AcquireGPUCommandBuffer(dev.raw()));
			this->dev = dev.raw();
		}

		~command_buffer()
		{
			if (handle) submit();
		}

		// Запрещаем копирование
		command_buffer(const command_buffer&) = delete;
		command_buffer& operator=(const command_buffer&) = delete;

		// Разрешаем перемещение
		command_buffer(command_buffer&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
		command_buffer& operator=(command_buffer&& o) noexcept
		{
			if (this != &o)
			{
				handle = o.handle;
				o.handle = nullptr;
			}
			return *this;
		}

		SDL_GPUCommandBuffer* raw() { return handle; }
		const SDL_GPUCommandBuffer* raw() const { return handle; }
		bool valid() const { return handle != nullptr; }

		// Явный сабмит буфера в очередь GPU
		void submit()
		{
			if (handle)
			{
				SDL_SubmitGPUCommandBuffer(handle);
				handle = nullptr; // обнуляем, так как буфер улетел и больше нам не принадлежит
			}
		}

		texture acquire_swapchain_texture(window& win)
		{
			SDL_GPUTexture* tex_handle = nullptr;
			glm::uvec2 size = { 0, 0 };
			TRY_SDL(SDL_AcquireGPUSwapchainTexture(handle, win.raw(), &tex_handle, &size.x, &size.y));
			auto fmt = static_cast<texture_format>(SDL_GetGPUSwapchainTextureFormat(dev, win.raw()));
			return texture(dev, tex_handle, fmt, size);
		}

	private:
		SDL_GPUCommandBuffer* handle = nullptr;
		SDL_GPUDevice* dev;
	};
}

// ===== File: input.hpp =====
#pragma once

#include "utilities.hpp"
#include <bitset>

#include "event.hpp"
#include "iupdate.h"
#include <set>

namespace mngl
{
	enum class mouse_button : uint8_t
	{
		left = 1, // SDL_BUTTON_LEFT
		middle = 2, // SDL_BUTTON_MIDDLE
		right = 3, // SDL_BUTTON_RIGHT
		x1 = 4, // SDL_BUTTON_X1
		x2 = 5  // SDL_BUTTON_X2
	};

	class Input
	{
	private:
		static inline double dt = 0.1;
		static void update_dt()
		{
			static uint64_t last_time = 0;
			if (!last_time) [[unlikely]]
			{
				last_time = performance_counter();
				return;
			}
			auto current_time = performance_counter();
			dt = std::min((current_time - last_time) / static_cast<double>(performance_frequency()), 0.1);
			last_time = current_time;
		}
		static void init_all()
		{
			static [[maybe_unused]] bool app_quit_inited = [&]() -> bool
			{
				event_manager::on<appQuit>([](const appQuit& e) { should_work = false; });
				event_manager::on<appTerminating>([](const appTerminating& e) { should_work = false; });
				event_manager::on<windowCloseRequest>([](const windowCloseRequest& e) { should_work = false; });
				return true;
			}();

			static [[maybe_unused]] bool kboard_inited = [&]() -> bool
			{
				event_manager::on<keyDown>([](const keyDown& e) { if(!e.repeat) [[likely]] pressed_keys.insert(e.code); });
				event_manager::on<keyUp>([](const keyUp& e) { released_keys.insert(e.code); holded_keys.erase(e.code); });
				return true;
			}();

			static [[maybe_unused]] bool mouse_inited = [&]() -> bool
			{
				event_manager::on<mouseMotion>([](const mouseMotion& e) { mdelta += e.position - mpos; mpos = e.position; });
				event_manager::on<mouseButtonDown>([](const mouseButtonDown& e) { mpressed |= 1 << (e.button - 1); });
				event_manager::on<mouseButtonUp>([](const mouseButtonUp& e) { mreleased |= 1 << (e.button - 1); mholded &= ~(1 << (e.button - 1)); });
				event_manager::on<mouseWheel>([](const mouseWheel& e) { mscrolldelta += e.scroll; });
				return true;
			}();
		}

		static inline std::set<scancode> pressed_keys;
		static inline std::set<scancode> holded_keys;
		static inline std::set<scancode> released_keys;

		static inline glm::vec2 mpos;
		static inline glm::vec2 mdelta;
		static inline glm::vec2 mscrolldelta;

		static inline uint32_t mpressed;
		static inline uint32_t mholded;
		static inline uint32_t mreleased;

		static void update_info()
		{
			holded_keys.insert(pressed_keys.begin(), pressed_keys.end());
			pressed_keys.clear();
			released_keys.clear();
			mholded |= mpressed;
			mreleased = 0;
			mpressed = 0;
			mdelta = {0, 0};
			mscrolldelta = {0, 0};
		}

		static void pool_events()
		{
			update_info();
			SDL_PumpEvents();
			SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
		}
	public:
		static uint64_t performance_counter()
		{
			return SDL_GetPerformanceCounter();
		}
		static uint64_t performance_frequency()
		{
			return SDL_GetPerformanceFrequency();
		}
		static double get_dt()
		{
			return dt;
		}
		static bool continue_work()
		{
			init_all();
			pool_events();
			if (!should_work) [[unlikely]]
				return false;
			//перерасчёт всех дельт, и тому подобного
			update_dt();
			update_manager::tick(dt);
			return true;
		}

		static bool is_key_pressed(scancode code)
		{
			return pressed_keys.find(code) != pressed_keys.end();
		}
		static bool is_key_holded(scancode code)
		{
			return holded_keys.find(code) != holded_keys.end();
		}
		static bool is_key_released(scancode code)
		{
			return released_keys.find(code) != released_keys.end();
		}

		static glm::vec2 get_mouse_position()
		{
			return mpos;
		}
		static glm::vec2 get_mouse_delta()
		{
			return mdelta;
		}
		static glm::vec2 get_mouse_scroll_delta()
		{
			return mscrolldelta;
		}

		static bool is_mouse_button_pressed(mouse_button button)
		{
			return mpressed & (1 << (static_cast<uint8_t>(button) - 1));
		}
		static bool is_mouse_button_holded(mouse_button button)
		{
			return mholded & (1 << (static_cast<uint8_t>(button) - 1));
		}
		static bool is_mouse_button_released(mouse_button button)
		{
			return mreleased & (1 << (static_cast<uint8_t>(button) - 1));
		}



		inline static bool should_work = true;
	};
}

// ===== File: MNGL.cpp =====
﻿#include <iostream>

int main()
{
    std::cout << "Hello World!\n";
}

// ===== File: render_pass.hpp =====
#pragma once

#include "utilities.hpp"
#include "color.hpp"
#include "command_buffer.hpp"
#include "texture.hpp"

namespace mngl::gpu
{
	class render_pass
	{
	public:

		enum class loader
		{
			clear =		SDL_GPU_LOADOP_CLEAR,
			load =		SDL_GPU_LOADOP_LOAD,
			dont_care =	SDL_GPU_LOADOP_DONT_CARE
		};

		enum class storer
		{
			store =		SDL_GPU_STOREOP_STORE,
			dont_care =	SDL_GPU_STOREOP_DONT_CARE
		};

		struct color_attachment
		{
			texture& target;
			color col = color(0);
			loader load = loader::clear;
			storer store = storer::store;
		};

		struct depth_attachment
		{
			texture& target;
			float depth = 1;
			uint8_t stencil = 0;
			loader load = loader::clear;
			storer store = storer::store;
		};

		//ez constructor
		render_pass(command_buffer& cmd, texture& target, const color& col = color(0), loader ld = loader::clear, storer st = storer::store, const depth_attachment* depth = nullptr) :
			render_pass(cmd, {{target, col, ld, st}}, depth)
		{		}

		render_pass(command_buffer& cmd, std::initializer_list<color_attachment> color_targets, const depth_attachment* depth = nullptr)
		{
			if (color_targets.empty()) throw std::invalid_argument("render pass must have to have at least one target!");

			std::vector<SDL_GPUColorTargetInfo> sdl_color_targets;
			sdl_color_targets.reserve(color_targets.size());

			for (auto& attachment : color_targets)
			{
				if (!attachment.target.has_usage(texture::usage_flags::color_target)) throw std::invalid_argument("target texture must have to have color_target usage flag!");
				SDL_GPUColorTargetInfo info{};
				info.texture = attachment.target.raw();
				info.clear_color = SDL_FColor{ attachment.col.r, attachment.col.g, attachment.col.b, attachment.col.a };
				info.load_op = static_cast<SDL_GPULoadOp>(attachment.load);
				info.store_op = static_cast<SDL_GPUStoreOp>(attachment.store);

				sdl_color_targets.push_back(info);
			}

			SDL_GPUDepthStencilTargetInfo sdl_depth_info{};
			SDL_GPUDepthStencilTargetInfo* depth_ptr = nullptr;

			if (depth)
			{
				auto& attachment = *depth;
				if (!attachment.target.has_usage(texture::usage_flags::stencil_target)) throw std::invalid_argument("target texture must have to have color_target usage flag!");
				sdl_depth_info.texture = depth->target.raw();
				sdl_depth_info.clear_depth = depth->depth;
				sdl_depth_info.clear_stencil = depth->stencil;
				sdl_depth_info.load_op = static_cast<SDL_GPULoadOp>(depth->load);
				sdl_depth_info.store_op = static_cast<SDL_GPUStoreOp>(depth->store);
				sdl_depth_info.stencil_load_op = static_cast<SDL_GPULoadOp>(depth->load);
				sdl_depth_info.stencil_store_op = static_cast<SDL_GPUStoreOp>(depth->store);

				depth_ptr = &sdl_depth_info;
			}

			handle = SDL_BeginGPURenderPass(
				cmd.raw(),
				sdl_color_targets.data(),
				static_cast<uint32_t>(sdl_color_targets.size()),
				depth_ptr
			);
		}

		~render_pass()
		{
			if (handle)
			{
				SDL_EndGPURenderPass(handle);
			}
		}

		render_pass(const render_pass&) = delete;
		render_pass& operator=(const render_pass&) = delete;

		// Разрешаем перемещение
		render_pass(render_pass&& o) noexcept { swap(*this, o); }
		render_pass& operator=(render_pass&& o) noexcept
		{
			swap(*this, o);
			return *this;
		}

		SDL_GPURenderPass* raw() const { return handle; }
		bool valid() const { return handle != nullptr; }

		void end_pass()
		{
			if (handle)
			{
				SDL_EndGPURenderPass(handle);
			}
			handle = nullptr;
		}

	private:
		SDL_GPURenderPass* handle = nullptr;

		friend inline void swap(render_pass& l, render_pass& r) noexcept
		{
			using std::swap;
			swap(l.handle, r.handle);
		}
	};
}

// ===== File: shader.hpp =====
#pragma once
#include "utilities.hpp"
#include "device.hpp"

namespace mngl::gpu
{
	enum class graphics_shader_stage
	{
		vertex = SDL_GPU_SHADERSTAGE_FRAGMENT,
		fragment = SDL_GPU_SHADERSTAGE_VERTEX,
	};

    struct graphics_shader_asset
    {
        graphics_shader_stage stage;
        std::vector<uint8_t> bytecode;
        std::string entrypoint = "main";

        // SDL-специфичные метаданные (отражение ресурсов)
        uint32_t samplers = 0;
        uint32_t storage_textures = 0;
        uint32_t storage_buffers = 0;
        uint32_t uniform_buffers = 0;

        // Сам скомпилированный GPU объект (создается лениво в основном потоке)
        SDL_GPUShader* gpu_handle = nullptr;
    };


}

