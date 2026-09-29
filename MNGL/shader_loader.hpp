#pragma once

#include "graphics_shader.hpp"
#include "utilities.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace mngl::gpu
{
	enum class shader_source_language : uint8_t
	{
		hlsl,
		glsl
	};

	// GLSL is first compiled to SPIR-V by glslangValidator; shadercross handles other targets.
	// Executable paths may be absolute or names resolved through PATH.
	struct shader_compile_options
	{
		graphics_shader_stage stage = graphics_shader_stage::vertex;
		shader_format format = static_cast<shader_format>(MNGL_DEFAULT_SHADER_FORMAT);
		std::string entrypoint = "main";
		shader_resource_layout resources{};
		shader_source_language source_language = shader_source_language::hlsl;
		std::filesystem::path shadercross_executable = "shadercross";
		std::filesystem::path glslang_validator_executable = "glslangValidator";
	};

	class shader_loader
	{
	public:
		static graphics_shader_asset compile(
			const std::filesystem::path& source_path,
			const shader_compile_options& options = {}
		)
		{
			const auto output_format = output_format_for(options.format);
			const auto stage = stage_name_for(options.stage);
			if (!valid_entrypoint(options.entrypoint))
			{
				throw std::invalid_argument("Shader entrypoint must be a valid identifier");
			}
			if (options.source_language != shader_source_language::hlsl &&
				options.source_language != shader_source_language::glsl)
			{
				throw std::invalid_argument("Unsupported shader source language");
			}
			validate_source_file(source_path);

			auto temp_dir = temporary_directory::create();
			const auto compiled_path = temp_dir.path() /
				std::format("compiled.{}", output_format.extension);

			std::vector<uint8_t> bytes;
			if (options.source_language == shader_source_language::hlsl)
			{
				run_shadercross(
					source_path,
					source_path,
					"HLSL",
					stage,
					output_format,
					compiled_path,
					options);
				bytes = read_shader_output(compiled_path, "shadercross", source_path);
			}
			else
			{
				const auto spirv_path = temp_dir.path() / "source.spv";
				run_glslang(source_path, spirv_path, options);

				if (options.format == shader_format::spirv)
				{
					bytes = read_shader_output(spirv_path, "glslangValidator", source_path);
				}
				else
				{
					run_shadercross(
						spirv_path,
						source_path,
						"SPIRV",
						stage,
						output_format,
						compiled_path,
						options);
					bytes = read_shader_output(compiled_path, "shadercross", source_path);
				}
			}

			graphics_shader_asset asset{};
			asset.stage = options.stage;
			asset.code.assign(options.format, std::move(bytes));
			asset.entrypoint = options.entrypoint;
			asset.resources = options.resources;
			asset.debug_name = source_path.filename().string();
			return asset;
		}

		static graphics_shader_asset compile_glsl(
			const std::filesystem::path& glsl_path,
			shader_compile_options options = {}
		)
		{
			options.source_language = shader_source_language::glsl;
			return compile(glsl_path, options);
		}

		// Kept for source compatibility; source_language may also select the GLSL pipeline.
		static graphics_shader_asset compile_with_shadercross(
			const std::filesystem::path& source_path,
			const shader_compile_options& options = {}
		)
		{
			return compile(source_path, options);
		}

		static graphics_shader_asset compile_with_shadercross(
			const std::filesystem::path& hlsl_path,
			graphics_shader_stage stage,
			shader_format format = static_cast<shader_format>(MNGL_DEFAULT_SHADER_FORMAT),
			const std::string& entrypoint = "main"
		)
		{
			return compile(hlsl_path, shader_compile_options{
				.stage = stage,
				.format = format,
				.entrypoint = entrypoint
			});
		}

	private:
		struct output_format
		{
			std::string_view name;
			std::string_view extension;
		};

		class temporary_directory
		{
		public:
			static temporary_directory create()
			{
				std::error_code ec;
				const auto root = std::filesystem::temp_directory_path(ec);
				if (ec)
				{
					throw std::filesystem::filesystem_error(
						"Failed to locate a temporary directory", ec);
				}

				static std::atomic<uint64_t> next_serial{ 0 };
				for (unsigned attempt = 0; attempt < 32; ++attempt)
				{
					const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
					const auto serial = next_serial.fetch_add(1, std::memory_order_relaxed);
					const auto candidate = root / std::format(
						"mngl_shader_{}_{}_{}",
						current_process_id(),
						ticks,
						serial);

					ec.clear();
					if (std::filesystem::create_directory(candidate, ec))
					{
						return temporary_directory(candidate);
					}
					if (ec && ec != std::errc::file_exists)
					{
						throw std::filesystem::filesystem_error(
							"Failed to create a temporary shader directory", candidate, ec);
					}
				}

				throw std::runtime_error("Failed to create a unique temporary shader directory");
			}

			temporary_directory(const temporary_directory&) = delete;
			temporary_directory& operator=(const temporary_directory&) = delete;
			temporary_directory(temporary_directory&& other) noexcept
				: m_path(std::move(other.m_path))
			{
				other.m_path.clear();
			}
			temporary_directory& operator=(temporary_directory&&) = delete;

			~temporary_directory() noexcept
			{
				if (!m_path.empty())
				{
					std::error_code ec;
					std::filesystem::remove_all(m_path, ec);
					if (ec)
					{
						SDL_LogWarn(
							SDL_LOG_CATEGORY_APPLICATION,
							"Failed to remove shader temp directory '%s': %s",
							m_path.string().c_str(),
							ec.message().c_str());
					}
				}
			}

			[[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

		private:
			explicit temporary_directory(std::filesystem::path path) noexcept
				: m_path(std::move(path))
			{ }

			std::filesystem::path m_path;
		};

		static uint64_t current_process_id() noexcept
		{
#if defined(_WIN32)
			return static_cast<uint64_t>(GetCurrentProcessId());
#else
			return static_cast<uint64_t>(::getpid());
#endif
		}

		static output_format output_format_for(shader_format format)
		{
			switch (format)
			{
			case shader_format::spirv: return { "SPIRV", "spv" };
			case shader_format::dxbc: return { "DXBC", "dxbc" };
			case shader_format::dxil: return { "DXIL", "dxil" };
			case shader_format::msl: return { "MSL", "msl" };
			default:
				throw std::invalid_argument("shadercross does not support the requested output format");
			}
		}

		static std::string_view stage_name_for(graphics_shader_stage stage)
		{
			switch (stage)
			{
			case graphics_shader_stage::vertex: return "vertex";
			case graphics_shader_stage::fragment: return "fragment";
			default:
				throw std::invalid_argument("Unsupported graphics shader stage");
			}
		}

		static std::string_view glsl_stage_name_for(graphics_shader_stage stage)
		{
			switch (stage)
			{
			case graphics_shader_stage::vertex: return "vert";
			case graphics_shader_stage::fragment: return "frag";
			default:
				throw std::invalid_argument("glslangValidator does not support the requested graphics shader stage");
			}
		}

		static void validate_source_file(const std::filesystem::path& source_path)
		{
			if (source_path.empty())
			{
				throw std::invalid_argument("Shader source path cannot be empty");
			}

			std::error_code ec;
			if (!std::filesystem::is_regular_file(source_path, ec))
			{
				if (ec)
				{
					throw std::filesystem::filesystem_error(
						"Failed to inspect shader source file", source_path, ec);
				}
				throw std::invalid_argument(std::format(
					"Shader source is not a regular file: '{}'",
					source_path.string()));
			}
		}

		static std::filesystem::path argument(std::string_view value)
		{
			return std::filesystem::path(std::string(value));
		}

		static std::vector<std::filesystem::path> shadercross_arguments(
			const std::filesystem::path& input_path,
			std::string_view source_format,
			std::string_view stage,
			output_format destination_format,
			const std::filesystem::path& output_path,
			std::string_view entrypoint
		)
		{
			return {
				input_path,
				argument("-s"), argument(source_format),
				argument("-d"), argument(destination_format.name),
				argument("-t"), argument(stage),
				argument("-o"), output_path,
				argument("-e"), argument(entrypoint)
			};
		}

		static void run_shadercross(
			const std::filesystem::path& input_path,
			const std::filesystem::path& display_source,
			std::string_view source_format,
			std::string_view stage,
			output_format destination_format,
			const std::filesystem::path& output_path,
			const shader_compile_options& options
		)
		{
			const auto args = shadercross_arguments(
				input_path,
				source_format,
				stage,
				destination_format,
				output_path,
				options.entrypoint);
			run_compiler(
				options.shadercross_executable,
				args,
				"shadercross",
				display_source);
		}

		static void run_glslang(
			const std::filesystem::path& source_path,
			const std::filesystem::path& output_path,
			const shader_compile_options& options
		)
		{
			const std::vector<std::filesystem::path> args{
				argument("-V"),
				argument("-S"),
				argument(glsl_stage_name_for(options.stage)),
				argument("-e"),
				argument(options.entrypoint),
				argument("-o"),
				output_path,
				source_path
			};
			run_compiler(
				options.glslang_validator_executable,
				args,
				"glslangValidator",
				source_path);
		}

		static void run_compiler(
			const std::filesystem::path& executable,
			const std::vector<std::filesystem::path>& args,
			std::string_view tool_name,
			const std::filesystem::path& source_path
		)
		{
			const auto result = run_process(executable, args);
			if (result.exit_code != 0)
			{
				throw std::runtime_error(std::format(
					"{} failed for '{}' (exit code {}):\n"
					"----------------------------------------\n"
					"{}\n"
					"----------------------------------------",
					tool_name,
					source_path.string(),
					result.exit_code,
					result.output.empty() ? "<no output>" : result.output));
			}
		}

		static std::vector<uint8_t> read_shader_output(
			const std::filesystem::path& path,
			std::string_view tool_name,
			const std::filesystem::path& source_path
		)
		{
			auto bytes = read_binary_file(path);
			if (bytes.empty())
			{
				throw std::runtime_error(std::format(
					"{} produced an empty shader for '{}'",
					tool_name,
					source_path.string()));
			}
			return bytes;
		}

		static bool valid_entrypoint(std::string_view entrypoint) noexcept
		{
			if (entrypoint.empty())
			{
				return false;
			}

			const auto is_letter = [](char ch) {
				return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
			};
			if (!is_letter(entrypoint.front()) && entrypoint.front() != '_')
			{
				return false;
			}

			for (const char ch : entrypoint.substr(1))
			{
				if (!is_letter(ch) && (ch < '0' || ch > '9') && ch != '_')
				{
					return false;
				}
			}
			return true;
		}

		static process_result run_process(
			const std::filesystem::path& executable,
			const std::vector<std::filesystem::path>& args
		)
		{
			if (executable.empty())
			{
				throw std::invalid_argument("Shader compiler executable path cannot be empty");
			}

#if defined(_WIN32)
			return run_process_windows(executable, args);
#else
			return run_process_posix(executable, args);
#endif
		}

#if defined(_WIN32)
		class windows_handle
		{
		public:
			explicit windows_handle(HANDLE handle = nullptr) noexcept : m_handle(handle) { }
			windows_handle(const windows_handle&) = delete;
			windows_handle& operator=(const windows_handle&) = delete;
			~windows_handle() noexcept { reset(); }

			[[nodiscard]] HANDLE get() const noexcept { return m_handle; }

			void reset(HANDLE handle = nullptr) noexcept
			{
				if (m_handle && m_handle != INVALID_HANDLE_VALUE)
				{
					CloseHandle(m_handle);
				}
				m_handle = handle;
			}

		private:
			HANDLE m_handle;
		};

		class windows_child_process
		{
		public:
			explicit windows_child_process(HANDLE handle) noexcept : m_handle(handle) { }
			windows_child_process(const windows_child_process&) = delete;
			windows_child_process& operator=(const windows_child_process&) = delete;
			~windows_child_process() noexcept
			{
				if (m_handle)
				{
					if (!m_completed)
					{
						TerminateProcess(m_handle, 1);
						WaitForSingleObject(m_handle, INFINITE);
					}
					CloseHandle(m_handle);
				}
			}

			[[nodiscard]] HANDLE get() const noexcept { return m_handle; }
			void mark_completed() noexcept { m_completed = true; }

		private:
			HANDLE m_handle;
			bool m_completed = false;
		};

		static std::wstring quote_windows_argument(std::wstring_view value)
		{
			std::wstring quoted(1, L'"');
			std::size_t backslashes = 0;
			for (const wchar_t ch : value)
			{
				if (ch == L'\\')
				{
					++backslashes;
				}
				else if (ch == L'"')
				{
					quoted.append(backslashes * 2 + 1, L'\\');
					quoted.push_back(L'"');
					backslashes = 0;
				}
				else
				{
					quoted.append(backslashes, L'\\');
					quoted.push_back(ch);
					backslashes = 0;
				}
			}
			quoted.append(backslashes * 2, L'\\');
			quoted.push_back(L'"');
			return quoted;
		}

		static process_result run_process_windows(
			const std::filesystem::path& executable,
			const std::vector<std::filesystem::path>& args
		)
		{
			SECURITY_ATTRIBUTES security_attributes{
				sizeof(SECURITY_ATTRIBUTES),
				nullptr,
				TRUE
			};
			HANDLE raw_read_pipe = nullptr;
			HANDLE raw_write_pipe = nullptr;
			if (!CreatePipe(&raw_read_pipe, &raw_write_pipe, &security_attributes, 0))
			{
				throw std::system_error(
					static_cast<int>(GetLastError()),
					std::system_category(),
					"Failed to create shader compiler output pipe");
			}
			windows_handle read_pipe(raw_read_pipe);
			windows_handle write_pipe(raw_write_pipe);
			if (!SetHandleInformation(read_pipe.get(), HANDLE_FLAG_INHERIT, 0))
			{
				throw std::system_error(
					static_cast<int>(GetLastError()),
					std::system_category(),
					"Failed to configure shader compiler output pipe");
			}

			const HANDLE raw_stdin = CreateFileW(
				L"NUL",
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				&security_attributes,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr);
			if (raw_stdin == INVALID_HANDLE_VALUE)
			{
				throw std::system_error(
					static_cast<int>(GetLastError()),
					std::system_category(),
					"Failed to open null input for shader compiler");
			}
			windows_handle standard_input(raw_stdin);

			std::wstring command_line = quote_windows_argument(executable.native());
			for (const auto& arg : args)
			{
				command_line.push_back(L' ');
				command_line += quote_windows_argument(arg.native());
			}
			std::vector<wchar_t> mutable_command_line(command_line.begin(), command_line.end());
			mutable_command_line.push_back(L'\0');

			STARTUPINFOW startup_info{};
			startup_info.cb = sizeof(startup_info);
			startup_info.dwFlags = STARTF_USESTDHANDLES;
			startup_info.hStdInput = standard_input.get();
			startup_info.hStdOutput = write_pipe.get();
			startup_info.hStdError = write_pipe.get();

			PROCESS_INFORMATION process_info{};
			if (!CreateProcessW(
				executable.c_str(),
				mutable_command_line.data(),
				nullptr,
				nullptr,
				TRUE,
				CREATE_NO_WINDOW,
				nullptr,
				nullptr,
				&startup_info,
				&process_info))
			{
				throw std::system_error(
					static_cast<int>(GetLastError()),
					std::system_category(),
					std::format("Failed to start shader compiler '{}'", executable.string()));
			}

			windows_child_process child(process_info.hProcess);
			windows_handle thread(process_info.hThread);
			write_pipe.reset();

			process_result result{};
			std::array<char, 4096> buffer{};
			for (;;)
			{
				DWORD bytes_read = 0;
				if (!ReadFile(
					read_pipe.get(),
					buffer.data(),
					static_cast<DWORD>(buffer.size()),
					&bytes_read,
					nullptr))
				{
					const DWORD error = GetLastError();
					if (error == ERROR_BROKEN_PIPE)
					{
						break;
					}
					throw std::system_error(
						static_cast<int>(error),
						std::system_category(),
						"Failed to read shader compiler output");
				}
				if (bytes_read == 0)
				{
					break;
				}
				result.output.append(buffer.data(), bytes_read);
			}

			const DWORD wait_result = WaitForSingleObject(child.get(), INFINITE);
			if (wait_result != WAIT_OBJECT_0)
			{
				throw std::system_error(
					static_cast<int>(GetLastError()),
					std::system_category(),
					"Failed to wait for shader compiler");
			}
			child.mark_completed();

			DWORD exit_code = 0;
			if (!GetExitCodeProcess(child.get(), &exit_code))
			{
				throw std::system_error(
					static_cast<int>(GetLastError()),
					std::system_category(),
					"Failed to retrieve shader compiler exit code");
			}
			result.exit_code = static_cast<int>(exit_code);
			return result;
		}
#else
		class posix_file_descriptor
		{
		public:
			explicit posix_file_descriptor(int descriptor = -1) noexcept : m_descriptor(descriptor) { }
			posix_file_descriptor(const posix_file_descriptor&) = delete;
			posix_file_descriptor& operator=(const posix_file_descriptor&) = delete;
			~posix_file_descriptor() noexcept { reset(); }

			[[nodiscard]] int get() const noexcept { return m_descriptor; }

			void reset(int descriptor = -1) noexcept
			{
				if (m_descriptor >= 0)
				{
					::close(m_descriptor);
				}
				m_descriptor = descriptor;
			}

		private:
			int m_descriptor;
		};

		class posix_child_process
		{
		public:
			explicit posix_child_process(pid_t pid) noexcept : m_pid(pid) { }
			posix_child_process(const posix_child_process&) = delete;
			posix_child_process& operator=(const posix_child_process&) = delete;
			~posix_child_process() noexcept
			{
				if (m_pid > 0 && !m_completed)
				{
					::kill(m_pid, SIGKILL);
					int status = 0;
					while (::waitpid(m_pid, &status, 0) < 0 && errno == EINTR) { }
				}
			}

			[[nodiscard]] pid_t get() const noexcept { return m_pid; }
			void mark_completed() noexcept { m_completed = true; }

		private:
			pid_t m_pid;
			bool m_completed = false;
		};

		static void move_descriptor_above_standard(posix_file_descriptor& descriptor)
		{
			if (descriptor.get() >= STDERR_FILENO + 1)
			{
				return;
			}

			const int duplicate = ::fcntl(descriptor.get(), F_DUPFD, STDERR_FILENO + 1);
			if (duplicate < 0)
			{
				throw std::system_error(
					errno,
					std::generic_category(),
					"Failed to duplicate shader compiler output pipe");
			}
			descriptor.reset(duplicate);
		}

		static process_result run_process_posix(
			const std::filesystem::path& executable,
			const std::vector<std::filesystem::path>& args
		)
		{
			int pipe_descriptors[2]{};
			if (::pipe(pipe_descriptors) != 0)
			{
				throw std::system_error(
					errno,
					std::generic_category(),
					"Failed to create shader compiler output pipe");
			}
			posix_file_descriptor read_pipe(pipe_descriptors[0]);
			posix_file_descriptor write_pipe(pipe_descriptors[1]);
			move_descriptor_above_standard(read_pipe);
			move_descriptor_above_standard(write_pipe);

			std::vector<std::string> owned_args;
			owned_args.reserve(args.size() + 1);
			owned_args.emplace_back(executable.native());
			for (const auto& arg : args)
			{
				owned_args.emplace_back(arg.native());
			}
			std::vector<char*> argv;
			argv.reserve(owned_args.size() + 1);
			for (auto& arg : owned_args)
			{
				argv.push_back(arg.data());
			}
			argv.push_back(nullptr);

			posix_spawn_file_actions_t actions{};
			const int init_result = ::posix_spawn_file_actions_init(&actions);
			if (init_result != 0)
			{
				throw std::system_error(
					init_result,
					std::generic_category(),
					"Failed to initialize shader compiler process");
			}
			struct actions_guard
			{
				posix_spawn_file_actions_t* value;
				~actions_guard() { ::posix_spawn_file_actions_destroy(value); }
			} cleanup_actions{ &actions };

			auto check_action = [](int result, const char* operation) {
				if (result != 0)
				{
					throw std::system_error(result, std::generic_category(), operation);
				}
			};
			check_action(
				::posix_spawn_file_actions_adddup2(&actions, write_pipe.get(), STDOUT_FILENO),
				"Failed to redirect shader compiler output");
			check_action(
				::posix_spawn_file_actions_adddup2(&actions, write_pipe.get(), STDERR_FILENO),
				"Failed to redirect shader compiler errors");
			check_action(
				::posix_spawn_file_actions_addclose(&actions, read_pipe.get()),
				"Failed to close shader compiler input pipe");
			check_action(
				::posix_spawn_file_actions_addclose(&actions, write_pipe.get()),
				"Failed to close shader compiler output pipe");

			pid_t pid = 0;
			const int spawn_result = ::posix_spawnp(
				&pid,
				executable.native().c_str(),
				&actions,
				nullptr,
				argv.data(),
				environ);
			if (spawn_result != 0)
			{
				throw std::system_error(
					spawn_result,
					std::generic_category(),
					std::format("Failed to start shader compiler '{}'", executable.string()));
			}
			posix_child_process child(pid);
			write_pipe.reset();

			process_result result{};
			std::array<char, 4096> buffer{};
			for (;;)
			{
				const auto bytes_read = ::read(read_pipe.get(), buffer.data(), buffer.size());
				if (bytes_read < 0)
				{
					if (errno == EINTR)
					{
						continue;
					}
					throw std::system_error(
						errno,
						std::generic_category(),
						"Failed to read shader compiler output");
				}
				if (bytes_read == 0)
				{
					break;
				}
				result.output.append(buffer.data(), static_cast<std::size_t>(bytes_read));
			}

			int status = 0;
			pid_t wait_result = 0;
			do
			{
				wait_result = ::waitpid(child.get(), &status, 0);
			} while (wait_result < 0 && errno == EINTR);
			if (wait_result < 0)
			{
				throw std::system_error(
					errno,
					std::generic_category(),
					"Failed to wait for shader compiler");
			}
			child.mark_completed();

			if (WIFEXITED(status))
			{
				result.exit_code = WEXITSTATUS(status);
			}
			else if (WIFSIGNALED(status))
			{
				result.exit_code = 128 + WTERMSIG(status);
			}
			else
			{
				result.exit_code = -1;
			}
			return result;
		}
#endif
	};
}
