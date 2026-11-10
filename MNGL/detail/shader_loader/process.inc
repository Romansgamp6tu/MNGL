		static process_result run_process(const std::filesystem::path& executable,
		                                  const std::vector<std::filesystem::path>& args)
		{
			if (executable.empty()) [[unlikely]]
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
			explicit windows_handle(HANDLE handle = nullptr) noexcept : m_handle(handle) {}
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
			explicit windows_child_process(HANDLE handle) noexcept : m_handle(handle) {}
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

		static std::filesystem::path resolve_windows_executable(const std::filesystem::path& executable)
		{
			if (executable.has_parent_path() || executable.is_absolute())
			{
				return executable;
			}

			// Resolve app-local tools first so placing one beside the application works from any cwd.
			std::vector<wchar_t> module_path(32768);
			const DWORD module_length = GetModuleFileNameW(nullptr, module_path.data(),
			                                               static_cast<DWORD>(module_path.size()));
			if (module_length != 0 && module_length < module_path.size())
			{
				std::filesystem::path local_name = executable;
				if (local_name.extension().empty())
				{
					local_name += L".exe";
				}
				const auto local_executable =
				    std::filesystem::path(std::wstring(module_path.data(), module_length)).parent_path() /
				    local_name;
				std::error_code error;
				if (std::filesystem::is_regular_file(local_executable, error) && !error)
				{
					return local_executable;
				}
			}

			// Supplying lpApplicationName to CreateProcessW does not resolve a bare name via PATH.
			// SearchPathW gives the remaining system directories and PATH a chance before launching.
			std::vector<wchar_t> buffer(32768);
			const DWORD length = SearchPathW(nullptr, executable.c_str(), L".exe",
			                                 static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
			if (length == 0 || length >= buffer.size()) [[unlikely]]
			{
				return executable;
			}
			return std::filesystem::path(std::wstring(buffer.data(), length));
		}

		static process_result run_process_windows(const std::filesystem::path& executable,
		                                          const std::vector<std::filesystem::path>& args)
		{
			SECURITY_ATTRIBUTES security_attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
			HANDLE raw_read_pipe = nullptr;
			HANDLE raw_write_pipe = nullptr;
			if (!CreatePipe(&raw_read_pipe, &raw_write_pipe, &security_attributes, 0)) [[unlikely]]
			{
				throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
				                        "Failed to create shader compiler output pipe");
			}
			windows_handle read_pipe(raw_read_pipe);
			windows_handle write_pipe(raw_write_pipe);
			if (!SetHandleInformation(read_pipe.get(), HANDLE_FLAG_INHERIT, 0)) [[unlikely]]
			{
				throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
				                        "Failed to configure shader compiler output pipe");
			}

			const HANDLE raw_stdin =
			    CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security_attributes,
				            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (raw_stdin == INVALID_HANDLE_VALUE) [[unlikely]]
			{
				throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
				                        "Failed to open null input for shader compiler");
			}
			windows_handle standard_input(raw_stdin);

			const auto resolved_executable = resolve_windows_executable(executable);
			std::wstring command_line = quote_windows_argument(resolved_executable.native());
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
			if (!CreateProcessW(resolved_executable.c_str(), mutable_command_line.data(), nullptr, nullptr, TRUE,
			                    CREATE_NO_WINDOW, nullptr, nullptr, &startup_info, &process_info)) [[unlikely]]
			{
				throw std::system_error(
				    static_cast<int>(GetLastError()), std::system_category(),
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
				if (!ReadFile(read_pipe.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read,
				              nullptr))
				{
					const DWORD error = GetLastError();
					if (error == ERROR_BROKEN_PIPE)
					{
						break;
					}
					throw std::system_error(static_cast<int>(error), std::system_category(),
					                        "Failed to read shader compiler output");
				}
				if (bytes_read == 0)
				{
					break;
				}
				result.output.append(buffer.data(), bytes_read);
			}

			const DWORD wait_result = WaitForSingleObject(child.get(), INFINITE);
			if (wait_result != WAIT_OBJECT_0) [[unlikely]]
			{
				throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
				                        "Failed to wait for shader compiler");
			}
			child.mark_completed();

			DWORD exit_code = 0;
			if (!GetExitCodeProcess(child.get(), &exit_code)) [[unlikely]]
			{
				throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
				                        "Failed to retrieve shader compiler exit code");
			}
			result.exit_code = static_cast<int>(exit_code);
			return result;
		}
#else
		class posix_file_descriptor
		{
		public:
			explicit posix_file_descriptor(int descriptor = -1) noexcept : m_descriptor(descriptor) {}
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
			explicit posix_child_process(pid_t pid) noexcept : m_pid(pid) {}
			posix_child_process(const posix_child_process&) = delete;
			posix_child_process& operator=(const posix_child_process&) = delete;
			~posix_child_process() noexcept
			{
				if (m_pid > 0 && !m_completed)
				{
					::kill(m_pid, SIGKILL);
					int status = 0;
					while (::waitpid(m_pid, &status, 0) < 0 && errno == EINTR)
					{
					}
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
			if (duplicate < 0) [[unlikely]]
			{
				throw std::system_error(errno, std::generic_category(),
				                        "Failed to duplicate shader compiler output pipe");
			}
			descriptor.reset(duplicate);
		}

		static process_result run_process_posix(const std::filesystem::path& executable,
		                                        const std::vector<std::filesystem::path>& args)
		{
			int pipe_descriptors[2]{};
			if (::pipe(pipe_descriptors) != 0) [[unlikely]]
			{
				throw std::system_error(errno, std::generic_category(),
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
			if (init_result != 0) [[unlikely]]
			{
				throw std::system_error(init_result, std::generic_category(),
				                        "Failed to initialize shader compiler process");
			}
			struct actions_guard
			{
				posix_spawn_file_actions_t* value;
				~actions_guard() { ::posix_spawn_file_actions_destroy(value); }
			} cleanup_actions{&actions};

			auto check_action = [](int result, const char* operation)
			{
				if (result != 0) [[unlikely]]
				{
					throw std::system_error(result, std::generic_category(), operation);
				}
			};
			check_action(::posix_spawn_file_actions_adddup2(&actions, write_pipe.get(), STDOUT_FILENO),
			             "Failed to redirect shader compiler output");
			check_action(::posix_spawn_file_actions_adddup2(&actions, write_pipe.get(), STDERR_FILENO),
			             "Failed to redirect shader compiler errors");
			check_action(::posix_spawn_file_actions_addclose(&actions, read_pipe.get()),
			             "Failed to close shader compiler input pipe");
			check_action(::posix_spawn_file_actions_addclose(&actions, write_pipe.get()),
			             "Failed to close shader compiler output pipe");

			pid_t pid = 0;
			const int spawn_result =
			    ::posix_spawnp(&pid, executable.native().c_str(), &actions, nullptr, argv.data(), environ);
			if (spawn_result != 0) [[unlikely]]
			{
				throw std::system_error(
				    spawn_result, std::generic_category(),
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
					throw std::system_error(errno, std::generic_category(),
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
			if (wait_result < 0) [[unlikely]]
			{
				throw std::system_error(errno, std::generic_category(), "Failed to wait for shader compiler");
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
