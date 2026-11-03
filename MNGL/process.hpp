#pragma once

#include "utilities.hpp"

#include <SDL3/SDL_process.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mngl::process
{
	enum class stdio_route
	{
		inherited,
		discard,
		pipe
	};

	struct launch_options
	{
		std::filesystem::path working_directory;
		stdio_route standard_input = stdio_route::discard;
		stdio_route standard_output = stdio_route::pipe;
		stdio_route standard_error = stdio_route::pipe;
		bool merge_standard_error = false;
	};

	enum class stream_state
	{
		ready,
		not_ready,
		end_of_stream,
		failed
	};

	struct stream_read_result
	{
		std::vector<std::byte> data;
		stream_state state = stream_state::ready;
		std::string error;
	};

	struct command_result
	{
		// Combined stdout and stderr, represented as bytes in a std::string.
		std::string output;
		int exit_code = 0;
	};

	namespace detail
	{
		struct properties_guard
		{
			SDL_PropertiesID id = SDL_CreateProperties();

			~properties_guard() noexcept
			{
				if (id)
				{
					SDL_DestroyProperties(id);
				}
			}
		};

		struct sdl_free_deleter
		{
			void operator()(void* value) const noexcept
			{
				SDL_free(value);
			}
		};

		inline std::string path_to_utf8(const std::filesystem::path& path)
		{
			const auto encoded = path.u8string();
			std::string result(
				reinterpret_cast<const char*>(encoded.data()),
				encoded.size());
			if (result.find('\0') != std::string::npos)
			{
				throw std::invalid_argument("process working directory cannot contain a null character");
			}
			return result;
		}

		inline SDL_ProcessIO to_sdl_route(stdio_route route)
		{
			switch (route)
			{
			case stdio_route::inherited: return SDL_PROCESS_STDIO_INHERITED;
			case stdio_route::discard: return SDL_PROCESS_STDIO_NULL;
			case stdio_route::pipe: return SDL_PROCESS_STDIO_APP;
			default: throw std::invalid_argument("invalid process standard I/O route");
			}
		}

		inline void check_property(bool success, const char* message)
		{
			if (!success)
			{
				throw sdl_error(message);
			}
		}

		inline std::string error_text()
		{
			const char* error = SDL_GetError();
			return error && *error ? error : "SDL process stream error";
		}
	}

	class Process
	{
	public:
		Process(const Process&) = delete;
		Process& operator=(const Process&) = delete;

		Process(Process&& other) noexcept
			: m_process(std::exchange(other.m_process, nullptr)),
			  m_stdin(std::exchange(other.m_stdin, nullptr)),
			  m_stdout(std::exchange(other.m_stdout, nullptr)),
			  m_stderr(std::exchange(other.m_stderr, nullptr)),
			  m_exit_code(std::exchange(other.m_exit_code, std::nullopt))
		{
		}

		Process& operator=(Process&& other) noexcept
		{
			if (this != &other)
			{
				destroy_handle();
				m_process = std::exchange(other.m_process, nullptr);
				m_stdin = std::exchange(other.m_stdin, nullptr);
				m_stdout = std::exchange(other.m_stdout, nullptr);
				m_stderr = std::exchange(other.m_stderr, nullptr);
				m_exit_code = std::exchange(other.m_exit_code, std::nullopt);
			}
			return *this;
		}

		~Process() noexcept
		{
			destroy_handle();
		}

		[[nodiscard]] static Process launch(
			std::vector<std::string> arguments,
			const launch_options& options = {})
		{
			if (arguments.empty() || arguments.front().empty())
			{
				throw std::invalid_argument("process arguments must include a non-empty executable path at index 0");
			}
			for (const auto& argument : arguments)
			{
				if (argument.find('\0') != std::string::npos)
				{
					throw std::invalid_argument("process arguments cannot contain null characters");
				}
			}
			if (options.merge_standard_error && options.standard_error != stdio_route::inherited)
			{
				throw std::invalid_argument(
					"set standard_error to inherited when merging it into standard_output");
			}

			std::vector<char*> argv;
			argv.reserve(arguments.size() + 1);
			for (auto& argument : arguments)
			{
				argv.push_back(argument.data());
			}
			argv.push_back(nullptr);

			detail::properties_guard properties;
			if (!properties.id)
			{
				throw sdl_error("failed to create process properties: ");
			}

			detail::check_property(
				SDL_SetPointerProperty(
					properties.id,
					SDL_PROP_PROCESS_CREATE_ARGS_POINTER,
					argv.data()),
				"failed to set process arguments: ");

			if (!options.working_directory.empty())
			{
				const auto working_directory = detail::path_to_utf8(options.working_directory);
				detail::check_property(
					SDL_SetStringProperty(
						properties.id,
						SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING,
						working_directory.c_str()),
					"failed to set process working directory: ");
			}

			detail::check_property(
				SDL_SetNumberProperty(
					properties.id,
					SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,
					static_cast<Sint64>(detail::to_sdl_route(options.standard_input))),
				"failed to set process standard input: ");
			detail::check_property(
				SDL_SetNumberProperty(
					properties.id,
					SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,
					static_cast<Sint64>(detail::to_sdl_route(options.standard_output))),
				"failed to set process standard output: ");

			if (options.merge_standard_error)
			{
				detail::check_property(
					SDL_SetBooleanProperty(
						properties.id,
						SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN,
						true),
					"failed to merge process standard error: ");
			}
			else
			{
				detail::check_property(
					SDL_SetNumberProperty(
						properties.id,
						SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,
						static_cast<Sint64>(detail::to_sdl_route(options.standard_error))),
					"failed to set process standard error: ");
			}

			auto* process = SDL_CreateProcessWithProperties(properties.id);
			if (!process)
			{
				throw sdl_error("failed to launch process: ");
			}

			Process result(process);
			try
			{
				const auto process_properties = SDL_GetProcessProperties(process);
				if (!process_properties)
				{
					throw sdl_error("failed to get process properties: ");
				}
				if (options.standard_input == stdio_route::pipe)
				{
					result.m_stdin = SDL_GetProcessInput(process);
					if (!result.m_stdin)
					{
						throw sdl_error("failed to get process standard input: ");
					}
				}
				if (options.standard_output == stdio_route::pipe)
				{
					result.m_stdout = SDL_GetProcessOutput(process);
					if (!result.m_stdout)
					{
						throw sdl_error("failed to get process standard output: ");
					}
				}
				if (!options.merge_standard_error && options.standard_error == stdio_route::pipe)
				{
					result.m_stderr = static_cast<SDL_IOStream*>(SDL_GetPointerProperty(
						process_properties,
						SDL_PROP_PROCESS_STDERR_POINTER,
						nullptr));
					if (!result.m_stderr)
					{
						throw sdl_error("failed to get process standard error: ");
					}
				}
			}
			catch (...)
			{
				// SDL_DestroyProcess only releases the tracking object. Avoid leaving
				// a child running if setup failed before its handle could be returned.
				(void)SDL_KillProcess(process, true);
				throw;
			}
			return result;
		}

		[[nodiscard]] bool valid() const noexcept
		{
			return m_process != nullptr;
		}

		[[nodiscard]] SDL_Process* raw() const noexcept
		{
			return m_process;
		}

		[[nodiscard]] std::optional<std::uint64_t> id() const
		{
			ensure_valid();
			const auto properties = SDL_GetProcessProperties(m_process);
			if (!properties)
			{
				throw sdl_error("failed to get process properties: ");
			}
			const auto value = SDL_GetNumberProperty(properties, SDL_PROP_PROCESS_PID_NUMBER, -1);
			if (value < 0)
			{
				return std::nullopt;
			}
			return static_cast<std::uint64_t>(value);
		}

		// Returns an exit code once the process has finished; otherwise nullopt.
		[[nodiscard]] std::optional<int> poll()
		{
			ensure_valid();
			if (m_exit_code)
			{
				return m_exit_code;
			}
			int exit_code = 0;
			if (SDL_WaitProcess(m_process, false, &exit_code))
			{
				m_exit_code = exit_code;
			}
			return m_exit_code;
		}

		// Drain piped output before blocking here; an undrained full pipe can
		// prevent the child process from exiting.
		[[nodiscard]] int wait()
		{
			ensure_valid();
			if (m_exit_code)
			{
				return *m_exit_code;
			}
			int exit_code = 0;
			if (!SDL_WaitProcess(m_process, true, &exit_code))
			{
				throw sdl_error("failed while waiting for process: ");
			}
			m_exit_code = exit_code;
			return exit_code;
		}

		[[nodiscard]] bool terminate(bool force = true)
		{
			ensure_valid();
			return SDL_KillProcess(m_process, force);
		}

		[[nodiscard]] stream_read_result read_stdout(std::size_t max_bytes = 4096)
		{
			return read_stream(m_stdout, max_bytes, "standard output was not piped");
		}

		[[nodiscard]] stream_read_result read_stderr(std::size_t max_bytes = 4096)
		{
			return read_stream(m_stderr, max_bytes, "standard error was not piped");
		}

		// Writes one non-blocking chunk. Check the returned count and retry the
		// remaining bytes if the process input pipe is full.
		[[nodiscard]] std::size_t write_stdin(std::span<const std::byte> bytes)
		{
			ensure_valid();
			if (!m_stdin)
			{
				throw std::logic_error("process standard input was not piped");
			}
			if (bytes.empty())
			{
				return 0;
			}

			const auto written = SDL_WriteIO(m_stdin, bytes.data(), bytes.size());
			if (SDL_GetIOStatus(m_stdin) == SDL_IO_STATUS_ERROR)
			{
				throw sdl_error("failed to write to process standard input: ");
			}
			return written;
		}

		[[nodiscard]] std::size_t write_stdin(std::string_view text)
		{
			return write_stdin(std::as_bytes(std::span(text.data(), text.size())));
		}

		[[nodiscard]] static command_result run(
			std::vector<std::string> arguments,
			std::filesystem::path working_directory = {})
		{
			launch_options options;
			options.working_directory = std::move(working_directory);
			options.standard_input = stdio_route::discard;
			options.standard_output = stdio_route::pipe;
			options.standard_error = stdio_route::inherited;
			options.merge_standard_error = true;

			auto process = launch(std::move(arguments), options);
			size_t size = 0;
			int exit_code = 0;
			std::unique_ptr<void, detail::sdl_free_deleter> output(
				SDL_ReadProcess(process.m_process, &size, &exit_code));
			if (!output)
			{
				sdl_error error("failed to read process output: ");
				(void)process.terminate();
				throw error;
			}

			const auto* chars = static_cast<const char*>(output.get());
			return { std::string(chars, size), exit_code };
		}

	private:
		explicit Process(SDL_Process* process)
			: m_process(process)
		{
		}

		void ensure_valid() const
		{
			if (!m_process)
			{
				throw std::logic_error("process handle is empty");
			}
		}

		[[nodiscard]] stream_read_result read_stream(
			SDL_IOStream* stream,
			std::size_t max_bytes,
			const char* unavailable_message)
		{
			ensure_valid();
			if (!stream)
			{
				throw std::logic_error(unavailable_message);
			}
			if (max_bytes == 0)
			{
				return {};
			}

			stream_read_result result;
			result.data.resize(max_bytes);
			const auto bytes_read = SDL_ReadIO(stream, result.data.data(), result.data.size());
			result.data.resize(bytes_read);
			switch (SDL_GetIOStatus(stream))
			{
			case SDL_IO_STATUS_READY:
				result.state = stream_state::ready;
				break;
			case SDL_IO_STATUS_NOT_READY:
				result.state = stream_state::not_ready;
				break;
			case SDL_IO_STATUS_EOF:
				result.state = stream_state::end_of_stream;
				break;
			default:
				result.state = stream_state::failed;
				result.error = detail::error_text();
				break;
			}
			return result;
		}

		void destroy_handle() noexcept
		{
			if (m_process)
			{
				SDL_DestroyProcess(std::exchange(m_process, nullptr));
			}
			m_stdin = nullptr;
			m_stdout = nullptr;
			m_stderr = nullptr;
			m_exit_code.reset();
		}

		SDL_Process* m_process = nullptr;
		SDL_IOStream* m_stdin = nullptr;
		SDL_IOStream* m_stdout = nullptr;
		SDL_IOStream* m_stderr = nullptr;
		std::optional<int> m_exit_code;
	};
}
