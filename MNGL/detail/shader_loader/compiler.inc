		static std::filesystem::path argument(std::string_view value)
		{
			return std::filesystem::path(std::string(value));
		}

		static std::vector<std::filesystem::path> shadercross_arguments(
		    const std::filesystem::path& input_path, std::string_view source_format, std::string_view stage,
		    output_format destination_format, const std::filesystem::path& output_path,
		    std::string_view entrypoint)
		{
			return {input_path,
			        argument("-s"),
			        argument(source_format),
			        argument("-d"),
			        argument(destination_format.name),
			        argument("-t"),
			        argument(stage),
			        argument("-o"),
			        output_path,
			        argument("-e"),
			        argument(entrypoint)};
		}

		static void run_shadercross(const std::filesystem::path& input_path,
		                            const std::filesystem::path& display_source,
		                            std::string_view source_format, std::string_view stage,
		                            output_format destination_format,
		                            const std::filesystem::path& output_path,
		                            const shader_compile_options& options)
		{
			const auto args = shadercross_arguments(input_path, source_format, stage, destination_format,
			                                        output_path, options.entrypoint);
			run_compiler(options.shadercross_executable, args, "shadercross", display_source);
		}

		static void run_glslang(const std::filesystem::path& source_path,
		                        const std::filesystem::path& output_path,
		                        const shader_compile_options& options, bool hlsl_input = false)
		{
			std::vector<std::filesystem::path> args;
			args.reserve(hlsl_input ? 11 : 8);
			if (hlsl_input)
			{
				args.push_back(argument("-D"));
				args.push_back(argument("--hlsl-offsets"));
				args.push_back(argument("--hlsl-iomap"));
			}
			args.insert(args.end(), {argument("-V"),
			                         argument("-S"),
			                         argument(glsl_stage_name_for(options.stage)),
			                         argument("-e"),
			                         argument(options.entrypoint),
			                         argument("-o"),
			                         output_path,
			                         source_path});
			run_compiler(options.glslang_validator_executable, args, "glslangValidator", source_path);
		}

		static void run_compiler(const std::filesystem::path& executable,
		                         const std::vector<std::filesystem::path>& args, std::string_view tool_name,
		                         const std::filesystem::path& source_path)
		{
			const auto result = run_process(executable, args);
			if (result.exit_code != 0)
			{
				throw std::runtime_error(std::format("{} failed for '{}' (exit code {}):\n"
				                                     "----------------------------------------\n"
				                                     "{}\n"
				                                     "----------------------------------------",
				                                     tool_name, source_path.string(), result.exit_code,
				                                     result.output.empty() ? "<no output>" : result.output));
			}
		}

		static std::vector<std::uint8_t> read_shader_output(const std::filesystem::path& path,
		                                                    std::string_view tool_name,
		                                                    const std::filesystem::path& source_path)
		{
			auto bytes = read_binary_file(path);
			if (bytes.empty())
			{
				throw std::runtime_error(
				    std::format("{} produced an empty shader for '{}'", tool_name, source_path.string()));
			}
			return bytes;
		}

		static std::vector<std::uint8_t> compile_shader_bytecode(const std::filesystem::path& source_path,
		                                                         const shader_compile_options& options,
		                                                         shader_stage stage)
		{
			const auto output_format = output_format_for(options.format);
			const auto stage_name = stage_name_for(stage);
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

			auto compile_options = options;
			compile_options.stage = stage;
			auto temp_dir = temporary_directory::create();
			const auto compiled_path = temp_dir.path() / std::format("compiled.{}", output_format.extension);

			if (options.source_language == shader_source_language::hlsl)
			{
				// SDL_shadercross's prebuilt DXC is only shipped for Windows and Linux.
				// On macOS, compile HLSL to SPIR-V first, then translate it to MSL.
#if defined(SDL_PLATFORM_MACOS)
				if (options.format == shader_format::spirv || options.format == shader_format::msl)
				{
					const auto spirv_path = temp_dir.path() / "source.spv";
					run_glslang(source_path, spirv_path, compile_options, true);
					if (options.format == shader_format::spirv)
					{
						return read_shader_output(spirv_path, "glslangValidator", source_path);
					}
					run_shadercross(spirv_path, source_path, "SPIRV", stage_name, output_format,
					                compiled_path, compile_options);
					return read_shader_output(compiled_path, "shadercross", source_path);
				}
#endif
				run_shadercross(source_path, source_path, "HLSL", stage_name, output_format, compiled_path,
				                compile_options);
				return read_shader_output(compiled_path, "shadercross", source_path);
			}

			const auto spirv_path = temp_dir.path() / "source.spv";
			run_glslang(source_path, spirv_path, compile_options);
			if (options.format == shader_format::spirv)
			{
				return read_shader_output(spirv_path, "glslangValidator", source_path);
			}

			run_shadercross(spirv_path, source_path, "SPIRV", stage_name, output_format, compiled_path,
			                compile_options);
			return read_shader_output(compiled_path, "shadercross", source_path);
		}

		static bool valid_entrypoint(std::string_view entrypoint) noexcept
		{
			if (entrypoint.empty())
			{
				return false;
			}

			const auto is_letter = [](char ch)
			{ return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'); };
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
