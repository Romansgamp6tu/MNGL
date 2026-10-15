#pragma once

#include "color.hpp"

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

template<typename T>
concept Streamable = requires(std::ostream& os, T value)
{
	{ os << value } -> std::same_as<std::ostream&>;
};

class Logger
{
public:
	enum class severity
	{
		log,
		debug,
		debug_warning,
		info,
		success,
		warning,
		error,
		critical
	};

	struct entry
	{
		std::string timestamp;
		severity level;
		std::string message;
	};

	static constexpr std::size_t recent_entry_limit = 1000;

	Logger(std::ostream& output_stream, std::string out_file = "last.log")
		: output(output_stream),
		file(std::move(out_file)),
		color_output_enabled_(detect_color_support(output_stream))
	{
	}

	~Logger() = default;
	Logger() = delete;
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;
	Logger(Logger&&) = delete;

	template<Streamable... T>
	Logger& operator()(T&&... out)
	{
		std::ostringstream message;
		((message << std::forward<T>(out)), ...);
		const auto text = message.str();

		std::lock_guard lock(mutex);
		output << text << '\n';
		file << text << '\n';
		return *this;
	}

	template<Streamable... T>
	void log(T&&... out)
	{
		write_entry(severity::log, std::forward<T>(out)...);
	}

	template<Streamable... T>
	void debug(T&&... out)
	{
		write_entry(severity::debug, std::forward<T>(out)...);
	}

	template<Streamable... T>
	void debug_warning(T&&... out)
	{
		write_entry(severity::debug_warning, std::forward<T>(out)...);
	}

	template<Streamable... T>
	void info(T&&... out)
	{
		write_entry(severity::info, std::forward<T>(out)...);
	}

	template<Streamable... T>
	void success(T&&... out)
	{
		write_entry(severity::success, std::forward<T>(out)...);
	}

	template<Streamable... T>
	void warning(T&&... out)
	{
		write_entry(severity::warning, std::forward<T>(out)...);
	}

	template<Streamable... T>
	void error(T&&... out)
	{
		write_entry(severity::error, std::forward<T>(out)...);
	}

	template<Streamable... T>
	void critical(T&&... out)
	{
		write_entry(severity::critical, std::forward<T>(out)...);
	}

	void setColor(mngl::color color)
	{
		std::lock_guard lock(mutex);
		write_color(color);
	}

	void clearColor()
	{
		std::lock_guard lock(mutex);
		reset_color();
	}

	std::vector<entry> snapshot() const
	{
		std::lock_guard lock(mutex);
		return std::vector<entry>(recent_entries.begin(), recent_entries.end());
	}

	void clear_entries()
	{
		std::lock_guard lock(mutex);
		recent_entries.clear();
	}

	bool color_output_enabled() const noexcept
	{
		return color_output_enabled_;
	}

	void flush()
	{
		std::lock_guard lock(mutex);
		output.flush();
		file.flush();
	}

	void enableDebug()
	{
		std::lock_guard lock(mutex);
		debug_output = true;
	}

	void disableDebug()
	{
		std::lock_guard lock(mutex);
		debug_output = false;
	}

	void setDebug(bool to)
	{
		std::lock_guard lock(mutex);
		debug_output = to;
	}

	bool debugged() const
	{
		std::lock_guard lock(mutex);
		return debug_output;
	}

	static std::string_view level_name(severity level) noexcept
	{
		switch (level)
		{
		case severity::log: return "LOG";
		case severity::debug: return "DEBUG";
		case severity::debug_warning: return "DEBUG/WARNING";
		case severity::info: return "INFO";
		case severity::success: return "SUCCESS";
		case severity::warning: return "WARNING";
		case severity::error: return "ERROR";
		case severity::critical: return "CRITICAL";
		}
		return "LOG";
	}

	static mngl::color level_color(severity level) noexcept
	{
		switch (level)
		{
		case severity::log:
		case severity::debug:
			return mngl::color(0x9C, 0xA3, 0xAF);
		case severity::debug_warning:
		case severity::warning:
			return mngl::color(0xF5, 0x9E, 0x0B);
		case severity::info:
			return mngl::color(0x3B, 0x82, 0xF6);
		case severity::success:
			return mngl::color(0x22, 0xC5, 0x5E);
		case severity::error:
			return mngl::color(0xEF, 0x44, 0x44);
		case severity::critical:
			return mngl::color(0xDC, 0x26, 0x26);
		}
		return mngl::color(0xD1, 0xD5, 0xDB);
	}

private:
	static bool environment_disables_color() noexcept
	{
		return inspect_environment_variable(
				"NO_COLOR",
				[](std::string_view value)
				{
					return !value.empty();
				}) ||
			inspect_environment_variable(
				"TERM",
				[](std::string_view value)
				{
					return value == "dumb";
				});
	}

	static bool environment_forces_color() noexcept
	{
		return inspect_environment_variable(
			"FORCE_COLOR",
			[](std::string_view value)
			{
				return !value.empty() && value != "0";
			});
	}

	template<typename Predicate>
	static bool inspect_environment_variable(
		const char* name,
		Predicate&& predicate) noexcept
	{
#if defined(_WIN32)
		char* value = nullptr;
		std::size_t size = 0;
		const auto result = _dupenv_s(&value, &size, name);
		if (result != 0 || !value)
		{
			std::free(value);
			return false;
		}

		const bool matches = predicate(std::string_view(value));
		std::free(value);
		return matches;
#else
		const char* value = std::getenv(name);
		return value && predicate(std::string_view(value));
#endif
	}

#if defined(_WIN32)
	static HANDLE standard_console_handle(std::ostream& stream) noexcept
	{
		if (&stream == &std::cout)
		{
			return GetStdHandle(STD_OUTPUT_HANDLE);
		}
		if (&stream == &std::cerr)
		{
			return GetStdHandle(STD_ERROR_HANDLE);
		}
		return INVALID_HANDLE_VALUE;
	}
#elif defined(__unix__) || defined(__APPLE__)
	static int standard_stream_descriptor(std::ostream& stream) noexcept
	{
		if (&stream == &std::cout)
		{
			return STDOUT_FILENO;
		}
		if (&stream == &std::cerr)
		{
			return STDERR_FILENO;
		}
		return -1;
	}
#endif

	static bool detect_color_support(std::ostream& stream) noexcept
	{
		if (environment_disables_color())
		{
			return false;
		}

		const bool force_color = environment_forces_color();
#if defined(_WIN32)
		const HANDLE handle = standard_console_handle(stream);
		if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		DWORD mode = 0;
		if (!GetConsoleMode(handle, &mode))
		{
			return force_color;
		}
		if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
		{
			return true;
		}

		const bool enabled = SetConsoleMode(
			handle,
			mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
		return enabled || force_color;
#elif defined(__unix__) || defined(__APPLE__)
		const int descriptor = standard_stream_descriptor(stream);
		return descriptor >= 0 &&
			(force_color || ::isatty(descriptor) == 1);
#else
		(void)stream;
		return false;
#endif
	}

	template<Streamable... T>
	void write_entry(severity level, T&&... out)
	{
		std::lock_guard lock(mutex);
		if ((level == severity::debug || level == severity::debug_warning) &&
			!debug_output)
		{
			return;
		}

		std::ostringstream message;
		((message << std::forward<T>(out)), ...);
		const auto text = message.str();
		const auto timestamp = time();

		if (recent_entries.size() == recent_entry_limit)
		{
			recent_entries.pop_front();
		}
		recent_entries.push_back({ timestamp, level, text });

		const auto name = level_name(level);
		output << timestamp << ' ';
		if (color_output_enabled_)
		{
			write_color(level_color(level));
		}
		output << '[' << name << ']';
		if (color_output_enabled_)
		{
			reset_color();
		}
		output << ' ' << text << '\n';

		file << timestamp << " [" << name << "] " << text << '\n';
	}

	void write_color(mngl::color color)
	{
		if (!color_output_enabled_)
		{
			return;
		}

		const auto rgba = color.rgba_255();
		output << "\x1b[38;2;"
			<< rgba.r << ';' << rgba.g << ';' << rgba.b << 'm';
	}

	void reset_color()
	{
		if (color_output_enabled_)
		{
			output << "\x1b[0m";
		}
	}

	static std::string time()
	{
		const auto now = std::chrono::system_clock::now();
		const auto local_time = std::chrono::zoned_time{
			std::chrono::current_zone(),
			now
		}.get_local_time();
		const auto day = std::chrono::floor<std::chrono::days>(local_time);
		const auto time_of_day = local_time - day;
		const auto hms = std::chrono::hh_mm_ss{ time_of_day };
		const auto milliseconds =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				hms.subseconds()).count();

		return std::format(
			"[{:02}:{:02}:{:02}:{:03}]",
			hms.hours().count(),
			hms.minutes().count(),
			hms.seconds().count(),
			milliseconds);
	}

	std::ostream& output;
	std::ofstream file;
	mutable std::mutex mutex;
	std::deque<entry> recent_entries;
	bool color_output_enabled_;
	bool debug_output =
#ifdef _DEBUG
		true;
#else
		false;
#endif
};

inline Logger logger(std::cout);
