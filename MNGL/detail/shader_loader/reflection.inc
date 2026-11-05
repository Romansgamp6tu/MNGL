		struct uniform_reflection
		{
			std::uint32_t block_size = 0;
			std::vector<shader_uniform_field_layout> fields;
			std::vector<shader_resource_field_layout> resources;
			shader_resource_layout resource_counts{};
		};

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
					throw std::filesystem::filesystem_error("Failed to locate a temporary directory", ec);
				}

				static std::atomic<std::uint64_t> next_serial{0};
				for (unsigned attempt = 0; attempt < 32; ++attempt)
				{
					const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
					const auto serial = next_serial.fetch_add(1, std::memory_order_relaxed);
					const auto candidate =
					    root / std::format("mngl_shader_{}_{}_{}", current_process_id(), ticks, serial);

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
			temporary_directory(temporary_directory&& other) noexcept : m_path(std::move(other.m_path))
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
						SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
						            "Failed to remove shader temp directory '%s': %s",
						            m_path.string().c_str(), ec.message().c_str());
					}
				}
			}

			[[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

		private:
			explicit temporary_directory(std::filesystem::path path) noexcept : m_path(std::move(path)) {}

			std::filesystem::path m_path;
		};

		static std::uint64_t current_process_id() noexcept
		{
#if defined(_WIN32)
			return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
			return static_cast<std::uint64_t>(::getpid());
#endif
		}

		static output_format output_format_for(shader_format format)
		{
			switch (format)
			{
			case shader_format::spirv:
				return {"SPIRV", "spv"};
			case shader_format::dxbc:
				return {"DXBC", "dxbc"};
			case shader_format::dxil:
				return {"DXIL", "dxil"};
			case shader_format::msl:
				return {"MSL", "msl"};
			default:
				throw std::invalid_argument("shadercross does not support the requested output format");
			}
		}

		static std::string_view stage_name_for(shader_stage stage)
		{
			switch (stage)
			{
			case shader_stage::vertex:
				return "vertex";
			case shader_stage::fragment:
				return "fragment";
			case shader_stage::compute:
				return "compute";
			default:
				throw std::invalid_argument("Unsupported shader stage");
			}
		}

		static std::string_view glsl_stage_name_for(shader_stage stage)
		{
			switch (stage)
			{
			case shader_stage::vertex:
				return "vert";
			case shader_stage::fragment:
				return "frag";
			case shader_stage::compute:
				return "comp";
			default:
				throw std::invalid_argument("glslangValidator does not support the requested shader stage");
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
					throw std::filesystem::filesystem_error("Failed to inspect shader source file",
					                                        source_path, ec);
				}
				throw std::invalid_argument(
				    std::format("Shader source is not a regular file: '{}'", source_path.string()));
			}
		}

		static uniform_reflection reflect_uniforms_if_requested(const shader_compile_options& options,
		                                                        const std::filesystem::path& source_path)
		{
			if (!options.reflect_uniforms)
			{
				return {};
			}
			if (options.source_language != shader_source_language::hlsl ||
			    (options.stage != shader_stage::fragment && options.stage != shader_stage::compute))
			{
				throw std::invalid_argument("MNGL named uniform/resource reflection currently supports HLSL "
				                            "fragment and compute shaders only");
			}
			validate_source_file(source_path);
			return reflect_hlsl_uniforms(source_path, options.stage);
		}

		static void apply_uniform_reflection(shader_asset& asset, const shader_compile_options& options,
		                                     const uniform_reflection& reflected_uniforms)
		{
			if (!options.reflect_uniforms)
			{
				return;
			}

			const auto validate_count =
			    [](std::uint32_t requested, std::uint32_t reflected, std::string_view name)
			{
				if (requested != 0 && requested != reflected)
				{
					throw std::invalid_argument(
					    std::format("Shader {} count does not match the reflected HLSL layout", name));
				}
			};
			validate_count(options.resources.samplers, reflected_uniforms.resource_counts.samplers,
			               "sampler");
			validate_count(options.resources.storage_textures,
			               reflected_uniforms.resource_counts.storage_textures, "storage texture");
			validate_count(options.resources.storage_buffers,
			               reflected_uniforms.resource_counts.storage_buffers, "storage buffer");
			validate_count(options.resources.uniform_buffers,
			               reflected_uniforms.resource_counts.uniform_buffers, "uniform buffer");
			validate_count(options.resources.read_write_storage_textures,
			               reflected_uniforms.resource_counts.read_write_storage_textures,
			               "read-write storage texture");
			validate_count(options.resources.read_write_storage_buffers,
			               reflected_uniforms.resource_counts.read_write_storage_buffers,
			               "read-write storage buffer");

			asset.resources = reflected_uniforms.resource_counts;
			if (reflected_uniforms.block_size != 0)
			{
				asset.set_uniform_layout(reflected_uniforms.block_size, reflected_uniforms.fields);
			}
			if (!reflected_uniforms.resources.empty())
			{
				asset.set_resource_layout(reflected_uniforms.resources);
			}
		}

		static uniform_reflection reflect_hlsl_uniforms(const std::filesystem::path& source_path,
		                                                shader_stage stage)
		{
			std::ifstream source_file(source_path, std::ios::binary);
			if (!source_file)
			{
				throw std::runtime_error(std::format(
				    "Failed to read HLSL source for named-uniform reflection: '{}'", source_path.string()));
			}
			std::string source{std::istreambuf_iterator<char>(source_file), std::istreambuf_iterator<char>()};
			if (source_file.bad())
			{
				throw std::runtime_error(
				    std::format("Failed while reading HLSL source for named-uniform reflection: '{}'",
					            source_path.string()));
			}

			static const std::regex comments(R"(//[^\r\n]*|/\*[\s\S]*?\*/)");
			static const std::regex cbuffer_keyword(R"(\bcbuffer\b)");
			static const std::regex cbuffer(
			    R"(\bcbuffer\s+\w+\s*:\s*register\s*\(\s*b(\d+)\s*,\s*space(\d+)\s*\)\s*\{([^}]*)\})");
			static const std::regex uniform_field(
			    R"((?:(row_major|column_major)\s+)?(std::int64_t|std::uint64_t|float16_t|std::int16_t|std::uint16_t|std::int8_t|std::uint8_t|min16float|min10float|min16int|min16uint|double|float|half|int|uint|bool|dword)([1-4]?)(?:x([1-4]))?\s+([A-Za-z_]\w*)\s*(?:\[\s*(\d+)\s*\])?\s*;)");
			static const std::regex texture_resource(
			    R"(\bTexture2D(?:\s*<[^>]+>)?\s+([A-Za-z_]\w*)\s*:\s*register\s*\(\s*t(\d+)\s*,\s*space\s*(\d+)\s*\)\s*;)");
			static const std::regex buffer_resource(
			    R"(\bStructuredBuffer\s*<[^>]+>\s+([A-Za-z_]\w*)\s*:\s*register\s*\(\s*t(\d+)\s*,\s*space\s*(\d+)\s*\)\s*;)");
			static const std::regex read_write_texture_resource(
			    R"(\bRWTexture2D(?:\s*<[^>]+>)?\s+([A-Za-z_]\w*)\s*:\s*register\s*\(\s*u(\d+)\s*,\s*space\s*(\d+)\s*\)\s*;)");
			static const std::regex read_write_buffer_resource(
			    R"(\bRWStructuredBuffer\s*<[^>]+>\s+([A-Za-z_]\w*)\s*:\s*register\s*\(\s*u(\d+)\s*,\s*space\s*(\d+)\s*\)\s*;)");
			static const std::regex sampler_resource(
			    R"(\bSamplerState\s+([A-Za-z_]\w*)\s*:\s*register\s*\(\s*s(\d+)\s*,\s*space\s*(\d+)\s*\)\s*;)");
			static const std::regex resource_register(R"(\bregister\s*\(\s*[tsu]\s*\d+)");
			static const std::regex row_major_default(R"(#\s*pragma\s+pack_matrix\s*\(\s*row_major\s*\))");

			source = std::regex_replace(source, comments, "");
			const bool default_row_major = std::regex_search(source, row_major_default);
			std::size_t keyword_count = 0;
			for (auto it = std::sregex_iterator(source.begin(), source.end(), cbuffer_keyword);
			     it != std::sregex_iterator(); ++it)
			{
				++keyword_count;
			}
			if (keyword_count > 1)
			{
				throw std::invalid_argument("MNGL reflection supports at most one HLSL cbuffer");
			}

			uniform_reflection result{};
			if (keyword_count == 1)
			{
				std::smatch block_match;
				if (!std::regex_search(source, block_match, cbuffer) ||
				    std::stoul(block_match[1].str()) != 0 || std::stoul(block_match[2].str()) != 3)
				{
					throw std::invalid_argument("MNGL HLSL cbuffer must use register(b0, space3)");
				}

				const std::string body = block_match[3].str();
				const std::string remaining = std::regex_replace(body, uniform_field, "");
				if (remaining.find_first_not_of(" \t\r\n") != std::string::npos)
				{
					throw std::invalid_argument("MNGL cbuffer reflection supports scalar, vector, and matrix "
					                            "fields only; arrays and structs are not supported");
				}

				std::uint32_t byte_offset = 0;
				for (auto it = std::sregex_iterator(body.begin(), body.end(), uniform_field);
				     it != std::sregex_iterator(); ++it)
				{
					const std::smatch& field_match = *it;
					const std::string scalar_type = field_match[2].str();
					shader_uniform_field_layout field{};
					field.name = field_match[5].str();
					if (field_match[6].matched)
					{
						const auto array_count = std::stoull(field_match[6].str());
						if (array_count == 0 || array_count > std::numeric_limits<std::uint32_t>::max())
						{
							throw std::invalid_argument(
							    "HLSL uniform array length is outside the supported range");
						}
						field.is_array = true;
						field.array_count = static_cast<std::uint32_t>(array_count);
					}
					field.rows = field_match[3].str().empty()
					                 ? 1
					                 : static_cast<std::uint8_t>(std::stoul(field_match[3].str()));
					field.columns = field_match[4].str().empty()
					                    ? 1
					                    : static_cast<std::uint8_t>(std::stoul(field_match[4].str()));
					field.is_matrix = !field_match[4].str().empty();
					field.row_major =
					    field_match[1].matched ? field_match[1].str() == "row_major" : default_row_major;
					if (!field.is_matrix && !field_match[1].str().empty())
					{
						throw std::invalid_argument("row_major/column_major only applies to HLSL matrices");
					}

					if (scalar_type == "bool")
					{
						field.scalar_size = 4;
						field.is_boolean = true;
					}
					else if (scalar_type == "std::int8_t")
					{
						field.scalar_size = 1;
						field.is_signed = true;
					}
					else if (scalar_type == "std::uint8_t")
					{
						field.scalar_size = 1;
					}
					else if (scalar_type == "std::int16_t")
					{
						field.scalar_size = 2;
						field.is_signed = true;
					}
					else if (scalar_type == "std::uint16_t")
					{
						field.scalar_size = 2;
					}
					else if (scalar_type == "std::int64_t")
					{
						field.scalar_size = 8;
						field.is_signed = true;
					}
					else if (scalar_type == "std::uint64_t" || scalar_type == "dword")
					{
						field.scalar_size = scalar_type == "dword" ? 4 : 8;
					}
					else if (scalar_type == "int" || scalar_type == "min16int")
					{
						field.scalar_size = 4;
						field.is_signed = true;
					}
					else if (scalar_type == "uint" || scalar_type == "min16uint")
					{
						field.scalar_size = 4;
					}
					else
					{
						field.scalar_size = scalar_type == "float16_t" ? 2 : scalar_type == "double" ? 8 : 4;
						field.is_floating_point = true;
					}

					if (field.is_matrix)
					{
						byte_offset = (byte_offset + 15) & ~std::uint32_t(15);
						const std::uint32_t vector_components = field.row_major ? field.columns : field.rows;
						const std::uint32_t vector_count = field.row_major ? field.rows : field.columns;
						const std::uint32_t stride =
						    (vector_components * field.scalar_size + 15) & ~std::uint32_t(15);
						field.byte_offset = byte_offset;
						field.element_size = vector_count * stride;
						field.array_stride = field.is_array ? field.element_size : 0;
						if (field.is_array && field.array_count > std::numeric_limits<std::uint32_t>::max() /
						                                              field.array_stride)
						{
							throw std::length_error(
							    "HLSL matrix array exceeds the supported uniform block size");
						}
						field.storage_size =
						    field.is_array ? field.array_count * field.array_stride : field.element_size;
						if (field.storage_size > std::numeric_limits<std::uint32_t>::max() - byte_offset)
						{
							throw std::length_error("HLSL cbuffer exceeds the supported size");
						}
						byte_offset += field.storage_size;
					}
					else
					{
						const std::uint32_t field_size =
						    static_cast<std::uint32_t>(field.rows) * field.scalar_size;
						if (field.is_array)
						{
							byte_offset = (byte_offset + 15) & ~std::uint32_t(15);
							field.byte_offset = byte_offset;
							field.element_size = field_size;
							field.array_stride = (field_size + 15) & ~std::uint32_t(15);
							if (field.array_count >
							    std::numeric_limits<std::uint32_t>::max() / field.array_stride)
							{
								throw std::length_error(
								    "HLSL uniform array exceeds the supported block size");
							}
							field.storage_size = field.array_count * field.array_stride;
							if (field.storage_size > std::numeric_limits<std::uint32_t>::max() - byte_offset)
							{
								throw std::length_error("HLSL cbuffer exceeds the supported size");
							}
							byte_offset += field.storage_size;
						}
						else
						{
							if (byte_offset % field.scalar_size != 0)
							{
								byte_offset = (byte_offset + field.scalar_size - 1) &
								              ~static_cast<std::uint32_t>(field.scalar_size - 1);
							}
							if (byte_offset % 16 + field_size > 16)
							{
								byte_offset = (byte_offset + 15) & ~std::uint32_t(15);
							}
							field.byte_offset = byte_offset;
							field.element_size = field_size;
							field.storage_size = field_size;
							if (field.storage_size > std::numeric_limits<std::uint32_t>::max() - byte_offset)
							{
								throw std::length_error("HLSL cbuffer exceeds the supported size");
							}
							byte_offset += field_size;
						}
					}
					result.fields.push_back(std::move(field));
				}
				if (result.fields.empty())
				{
					throw std::invalid_argument("MNGL HLSL cbuffer must declare at least one uniform field");
				}
				result.block_size = (byte_offset + 15) & ~std::uint32_t(15);
				result.resource_counts.uniform_buffers = 1;
			}

			struct resource_declaration
			{
				std::string name;
				std::uint32_t slot = 0;
				std::uint32_t space = 0;
			};
			std::vector<resource_declaration> textures;
			std::vector<resource_declaration> buffers;
			std::vector<resource_declaration> read_write_textures;
			std::vector<resource_declaration> read_write_buffers;
			std::vector<resource_declaration> samplers;
			auto collect_resources = [](const std::string& text, const std::regex& pattern)
			{
				std::vector<resource_declaration> found;
				for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern);
				     it != std::sregex_iterator(); ++it)
				{
					const std::smatch& match = *it;
					const auto slot = std::stoull(match[2].str());
					const auto space = std::stoull(match[3].str());
					if (slot > std::numeric_limits<std::uint32_t>::max() ||
					    space > std::numeric_limits<std::uint32_t>::max())
					{
						throw std::out_of_range("HLSL resource register exceeds the supported range");
					}
					found.push_back({.name = match[1].str(),
					                 .slot = static_cast<std::uint32_t>(slot),
					                 .space = static_cast<std::uint32_t>(space)});
				}
				return found;
			};
			textures = collect_resources(source, texture_resource);
			buffers = collect_resources(source, buffer_resource);
			read_write_textures = collect_resources(source, read_write_texture_resource);
			read_write_buffers = collect_resources(source, read_write_buffer_resource);
			samplers = collect_resources(source, sampler_resource);

			std::size_t reflected_resource_registers = 0;
			for (auto it = std::sregex_iterator(source.begin(), source.end(), resource_register);
			     it != std::sregex_iterator(); ++it)
			{
				++reflected_resource_registers;
			}
			if (reflected_resource_registers != textures.size() + buffers.size() +
			                                        read_write_textures.size() + read_write_buffers.size() +
			                                        samplers.size())
			{
				throw std::invalid_argument("MNGL resource reflection supports Texture2D, StructuredBuffer, "
				                            "RWTexture2D, RWStructuredBuffer, and SamplerState at space2");
			}
			if (stage != shader_stage::compute &&
			    (!read_write_textures.empty() || !read_write_buffers.empty()))
			{
				throw std::invalid_argument(
				    "HLSL read-write storage resources are only supported in compute shaders");
			}

			auto require_space2 = [](const std::vector<resource_declaration>& declarations)
			{
				for (const auto& declaration : declarations)
				{
					if (declaration.space != 2)
					{
						throw std::invalid_argument("MNGL HLSL resources must use register space2");
					}
				}
			};
			require_space2(textures);
			require_space2(buffers);
			require_space2(read_write_textures);
			require_space2(read_write_buffers);
			require_space2(samplers);

			auto contiguous_slot_count = [](std::vector<std::uint32_t> slots, std::string_view kind)
			{
				std::sort(slots.begin(), slots.end());
				for (std::uint32_t index = 0; index < slots.size(); ++index)
				{
					if (slots[index] != index)
					{
						throw std::invalid_argument(std::format(
						    "MNGL {} resource registers must be contiguous from slot zero", kind));
					}
				}
				return static_cast<std::uint32_t>(slots.size());
			};

			std::vector<std::uint32_t> sampled_texture_slots;
			std::vector<std::uint32_t> storage_texture_slots;
			std::vector<std::uint32_t> storage_buffer_slots;
			std::vector<std::uint32_t> read_write_storage_texture_slots;
			std::vector<std::uint32_t> read_write_storage_buffer_slots;
			std::vector<std::uint32_t> sampler_slots;
			for (const auto& declaration : buffers)
			{
				storage_buffer_slots.push_back(declaration.slot);
				result.resources.push_back({.name = declaration.name,
				                            .kind = shader_resource_kind::storage_buffer,
				                            .slot = declaration.slot});
			}
			for (const auto& declaration : read_write_buffers)
			{
				read_write_storage_buffer_slots.push_back(declaration.slot);
				result.resources.push_back({.name = declaration.name,
				                            .kind = shader_resource_kind::read_write_storage_buffer,
				                            .slot = declaration.slot});
			}
			for (const auto& declaration : textures)
			{
				const auto matching_sampler =
				    std::find_if(samplers.begin(), samplers.end(), [&](const resource_declaration& sampler)
					             { return sampler.slot == declaration.slot; });
				const bool sampled = matching_sampler != samplers.end();
				auto& slots = sampled ? sampled_texture_slots : storage_texture_slots;
				slots.push_back(declaration.slot);
				result.resources.push_back({.name = declaration.name,
				                            .kind = sampled ? shader_resource_kind::sampled_texture
				                                            : shader_resource_kind::storage_texture,
				                            .slot = declaration.slot});
			}
			for (const auto& declaration : read_write_textures)
			{
				read_write_storage_texture_slots.push_back(declaration.slot);
				result.resources.push_back({.name = declaration.name,
				                            .kind = shader_resource_kind::read_write_storage_texture,
				                            .slot = declaration.slot});
			}
			for (const auto& declaration : samplers)
			{
				if (std::none_of(textures.begin(), textures.end(), [&](const resource_declaration& texture)
				                 { return texture.slot == declaration.slot; }))
				{
					throw std::invalid_argument(
					    "Each MNGL SamplerState must pair with a Texture2D at the same register index");
				}
				sampler_slots.push_back(declaration.slot);
			}

			result.resource_counts.samplers =
			    contiguous_slot_count(std::move(sampled_texture_slots), "sampled texture");
			if (contiguous_slot_count(std::move(sampler_slots), "sampler") != result.resource_counts.samplers)
			{
				throw std::invalid_argument(
				    "MNGL sampled textures and samplers must use matching register indices");
			}
			result.resource_counts.storage_textures =
			    contiguous_slot_count(std::move(storage_texture_slots), "storage texture");
			result.resource_counts.storage_buffers =
			    contiguous_slot_count(std::move(storage_buffer_slots), "storage buffer");
			result.resource_counts.read_write_storage_textures = contiguous_slot_count(
			    std::move(read_write_storage_texture_slots), "read-write storage texture");
			result.resource_counts.read_write_storage_buffers = contiguous_slot_count(
			    std::move(read_write_storage_buffer_slots), "read-write storage buffer");
			return result;
		}

		static glm::uvec3 resolve_compute_threadgroup_size(const std::filesystem::path& source_path,
		                                                   const shader_compile_options& options)
		{
			const auto requested = options.compute_threadgroup_size;
			const bool any_requested = requested.x != 0 || requested.y != 0 || requested.z != 0;
			if (any_requested)
			{
				if (requested.x == 0 || requested.y == 0 || requested.z == 0)
				{
					throw std::invalid_argument("Compute threadgroup dimensions must all be non-zero");
				}
			}

			validate_source_file(source_path);
			std::ifstream source_file(source_path, std::ios::binary);
			if (!source_file)
			{
				throw std::runtime_error(
				    std::format("Failed to read shader source for compute threadgroup reflection: '{}'",
					            source_path.string()));
			}
			std::string source{std::istreambuf_iterator<char>(source_file), std::istreambuf_iterator<char>()};
			if (source_file.bad())
			{
				throw std::runtime_error(
				    std::format("Failed while reading shader source for compute threadgroup reflection: '{}'",
					            source_path.string()));
			}

			static const std::regex comments(R"(//[^\r\n]*|/\*[\s\S]*?\*/)");
			source = std::regex_replace(source, comments, "");

			auto parse_dimension = [](const std::string& value)
			{
				const auto parsed = std::stoull(value);
				if (parsed == 0 || parsed > std::numeric_limits<std::uint32_t>::max())
				{
					throw std::invalid_argument(
					    "Compute threadgroup dimensions must fit in a non-zero 32-bit integer");
				}
				return static_cast<std::uint32_t>(parsed);
			};

			if (options.source_language == shader_source_language::hlsl)
			{
				static const std::regex numthreads(
				    R"(\[\s*numthreads\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)\s*\]\s*\w+\s+([A-Za-z_]\w*)\s*\()");
				glm::uvec3 reflected{0, 0, 0};
				std::size_t match_count = 0;
				for (auto it = std::sregex_iterator(source.begin(), source.end(), numthreads);
				     it != std::sregex_iterator(); ++it)
				{
					const std::smatch& match = *it;
					if (match[4].str() != options.entrypoint)
					{
						continue;
					}
					reflected = {parse_dimension(match[1].str()), parse_dimension(match[2].str()),
					             parse_dimension(match[3].str())};
					++match_count;
				}
				if (match_count == 0 && any_requested)
				{
					return requested;
				}
				if (match_count != 1)
				{
					throw std::invalid_argument(
					    std::format("Could not infer one HLSL [numthreads] declaration for entrypoint '{}'; "
						            "set compute_threadgroup_size explicitly",
						            options.entrypoint));
				}
				if (any_requested &&
				    (requested.x != reflected.x || requested.y != reflected.y || requested.z != reflected.z))
				{
					throw std::invalid_argument(
					    "compute_threadgroup_size does not match the HLSL [numthreads] declaration");
				}
				return any_requested ? requested : reflected;
			}

			if (options.source_language == shader_source_language::glsl)
			{
				static const std::regex local_size_layout(R"(layout\s*\(([^)]*)\)\s*in\s*;)");
				static const std::regex local_size_x(R"(local_size_x\s*=\s*(\d+))");
				static const std::regex local_size_y(R"(local_size_y\s*=\s*(\d+))");
				static const std::regex local_size_z(R"(local_size_z\s*=\s*(\d+))");
				glm::uvec3 reflected{0, 0, 0};
				std::size_t match_count = 0;
				for (auto it = std::sregex_iterator(source.begin(), source.end(), local_size_layout);
				     it != std::sregex_iterator(); ++it)
				{
					const std::string layout = (*it)[1].str();
					std::smatch x_match;
					if (!std::regex_search(layout, x_match, local_size_x))
					{
						continue;
					}
					std::smatch y_match;
					std::smatch z_match;
					reflected = {
					    parse_dimension(x_match[1].str()),
					    std::regex_search(layout, y_match, local_size_y) ? parse_dimension(y_match[1].str())
						                                                 : 1u,
					    std::regex_search(layout, z_match, local_size_z) ? parse_dimension(z_match[1].str())
						                                                 : 1u};
					++match_count;
				}
				if (match_count == 0 && any_requested)
				{
					return requested;
				}
				if (match_count != 1)
				{
					throw std::invalid_argument("Could not infer one GLSL local_size layout; set "
					                            "compute_threadgroup_size explicitly");
				}
				if (any_requested &&
				    (requested.x != reflected.x || requested.y != reflected.y || requested.z != reflected.z))
				{
					throw std::invalid_argument(
					    "compute_threadgroup_size does not match the GLSL local_size layout");
				}
				return any_requested ? requested : reflected;
			}

			throw std::invalid_argument("Unsupported shader source language");
		}
