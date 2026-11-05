#pragma once
#include "color.hpp"
#include <fstream>
#include <iostream>
#include <array>
#include <chrono>
#include <string>
#include <format>
#include <mutex>

namespace mngl
{
	template <typename T>
	concept streamable = requires(std::ostream& os, T t) {
		{ os << t } -> std::same_as<std::ostream&>;
	};

	class logger
	{
	public:
		void set_color(mngl::color col)
		{
			auto rgba = col.rgba_255();
			cout << "\x1b[38;2;" << (int)rgba.r << ";" << (int)rgba.g << ";" << (int)rgba.b << "m"
			     << std::flush;
		}
		void clear_color() { cout << "\x1b[0m"; }

		logger(std::ostream& cout, std::string out_file = "last.log") : cout(cout), fout(out_file) {}
		~logger() = default;
		logger() = delete;
		logger(const logger&) = delete;
		logger& operator=(const logger&) = delete;
		logger(logger&&) = delete;

		template <streamable... T> logger& operator()(T&&... out)
		{
			((cout << std::forward<T>(out)), ...);
			((fout << std::forward<T>(out)), ...);
			cout << '\n';
			fout << '\n';
			return *this;
		}

		template <streamable... T> void log(T&&... out)
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			cout << time() << "[LOG] ";
			this->operator()(out...);
		}

		template <streamable... T> void debug(T&&... out)
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			if (debug_output)
			{
				cout << time() << "[DEBUG] ";
				this->operator()(out...);
			}
		}

		template <streamable... T> void debug_warning(T&&... out)
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			if (debug_output)
			{
				this->set_color(mngl::color(0xF5, 0x9E, 0x0B));
				cout << time() << "[DEBUG/WARNING] ";
				this->operator()(out...);
				this->clear_color();
			}
		}

		template <streamable... T> void info(T&&... out)
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			this->set_color(mngl::color(0x3B, 0x82, 0xF6));
			cout << time() << "[INFO] ";
			this->operator()(std::forward<T>(out)...);
			this->clear_color();
		}

		template <streamable... T> void success(T&&... out)
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			this->set_color(mngl::color(0x22, 0xC5, 0x5E));
			cout << time() << "[SUCCESS] ";
			this->operator()(std::forward<T>(out)...);
			this->clear_color();
		}

		template <streamable... T> void warning(T&&... out)
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			this->set_color(mngl::color(0xF5, 0x9E, 0x0B));
			cout << time() << "[WARNING] ";
			this->operator()(std::forward<T>(out)...);
			this->clear_color();
		}

		template <streamable... T> void error(T&&... out)
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			this->set_color(mngl::color(0xEF, 0x44, 0x44));
			cout << time() << "[ERROR] ";
			this->operator()(std::forward<T>(out)...);
			this->clear_color();
		}

		template <streamable... T> void critical(T&&... out)
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			this->set_color(mngl::color(0xDC, 0x26, 0x26));
			cout << time() << "[CRITICAL] ";
			this->operator()(std::forward<T>(out)...);
			this->clear_color();
		}

		void flush()
		{
			cout.flush();
			fout.flush();
		}

		void enable_debug()
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			debug_output = true;
		}

		void disable_debug()
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			debug_output = false;
		}

		void set_debug(bool to)
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			debug_output = to;
		}

		bool debug_enabled() const
		{
			std::lock_guard<std::mutex> _lock_(mutex);
			return debug_output;
		}

		void setColor(mngl::color value) { set_color(value); }
		void clearColor() { clear_color(); }
		void enableDebug() { enable_debug(); }
		void disableDebug() { disable_debug(); }
		void setDebug(bool value) { set_debug(value); }
		[[nodiscard]] bool debugged() const { return debug_enabled(); }

	private:
		std::string time()
		{
			auto now = std::chrono::system_clock::now();
			auto local_time = std::chrono::zoned_time{std::chrono::current_zone(), now}.get_local_time();
			auto dp = std::chrono::floor<std::chrono::days>(local_time);
			auto time_of_day = local_time - dp;
			auto hms = std::chrono::hh_mm_ss{time_of_day};
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(hms.subseconds()).count();

			// Используем {:03} для миллисекунд
			return std::format("[{:02}:{:02}:{:02}:{:03}]", hms.hours().count(), hms.minutes().count(),
			                   hms.seconds().count(), ms);
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

	inline logger log(std::cout);
} // namespace mngl
using Logger = mngl::logger;
template <typename T>
concept Streamable = mngl::streamable<T>;
inline auto& logger = mngl::log;