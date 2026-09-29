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
		cout << '\n';
		fout << '\n';
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