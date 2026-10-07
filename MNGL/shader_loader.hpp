#pragma once

#include "graphics_shader.hpp"
#include "utilities.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <format>
#include <iterator>
#include <regex>
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
		// Reflect the MNGL HLSL fragment layout at b0/space3 and resources at space2.
		bool reflect_uniforms = false;
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

			const auto reflected_uniforms = reflect_uniforms_if_requested(options, source_path);

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
			apply_uniform_reflection(asset, options, reflected_uniforms);
			return asset;
		}

		static graphics_shader_asset load_binary(
			const std::filesystem::path& bytecode_path,
			const std::filesystem::path& reflection_source = {},
			const shader_compile_options& options = {}
		)
		{
			if (!valid_shader_stage(options.stage) || !valid_shader_format(options.format) ||
				!valid_entrypoint(options.entrypoint))
			{
				throw std::invalid_argument("Cannot load shader bytecode with invalid shader options");
			}
			validate_source_file(bytecode_path);

			auto bytes = read_binary_file(bytecode_path);
			if (bytes.empty())
			{
				throw std::invalid_argument(std::format(
					"Shader bytecode is empty: '{}'",
					bytecode_path.string()));
			}

			uniform_reflection reflected_uniforms{};
			if (options.reflect_uniforms)
			{
				if (reflection_source.empty())
				{
					throw std::invalid_argument(
						"Named-uniform reflection requires the matching HLSL source path");
				}
				reflected_uniforms = reflect_uniforms_if_requested(options, reflection_source);
			}

			graphics_shader_asset asset{};
			asset.stage = options.stage;
			asset.code.assign(options.format, std::move(bytes));
			asset.entrypoint = options.entrypoint;
			asset.resources = options.resources;
			asset.debug_name = bytecode_path.filename().string();
			apply_uniform_reflection(asset, options, reflected_uniforms);
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
		struct uniform_reflection
		{
			uint32_t block_size = 0;
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

		static uniform_reflection reflect_uniforms_if_requested(
			const shader_compile_options& options,
			const std::filesystem::path& source_path
		)
		{
			if (!options.reflect_uniforms)
			{
				return {};
			}
			if (options.source_language != shader_source_language::hlsl ||
				options.stage != graphics_shader_stage::fragment)
			{
				throw std::invalid_argument(
					"MNGL named uniform/resource reflection currently supports HLSL fragment shaders only");
			}
			validate_source_file(source_path);
			return reflect_hlsl_uniforms(source_path);
		}

		static void apply_uniform_reflection(
			graphics_shader_asset& asset,
			const shader_compile_options& options,
			const uniform_reflection& reflected_uniforms
		)
		{
			if (!options.reflect_uniforms)
			{
				return;
			}

			const auto validate_count = [](uint32_t requested, uint32_t reflected, std::string_view name)
			{
				if (requested != 0 && requested != reflected)
				{
					throw std::invalid_argument(std::format(
						"Shader {} count does not match the reflected HLSL layout",
						name));
				}
			};
			validate_count(options.resources.samplers, reflected_uniforms.resource_counts.samplers, "sampler");
			validate_count(
				options.resources.storage_textures,
				reflected_uniforms.resource_counts.storage_textures,
				"storage texture");
			validate_count(
				options.resources.storage_buffers,
				reflected_uniforms.resource_counts.storage_buffers,
				"storage buffer");
			validate_count(
				options.resources.uniform_buffers,
				reflected_uniforms.resource_counts.uniform_buffers,
				"uniform buffer");

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

		static uniform_reflection reflect_hlsl_uniforms(const std::filesystem::path& source_path)
		{
			std::ifstream source_file(source_path, std::ios::binary);
			if (!source_file)
			{
				throw std::runtime_error(std::format(
					"Failed to read HLSL source for named-uniform reflection: '{}'",
					source_path.string()));
			}
			std::string source{
				std::istreambuf_iterator<char>(source_file),
				std::istreambuf_iterator<char>() };
			if (source_file.bad())
			{
				throw std::runtime_error(std::format(
					"Failed while reading HLSL source for named-uniform reflection: '{}'",
					source_path.string()));
			}

			static const std::regex comments(R"(//[^\r\n]*|/\*[\s\S]*?\*/)");
			static const std::regex cbuffer_keyword(R"(\bcbuffer\b)");
			static const std::regex cbuffer(
				R"(\bcbuffer\s+\w+\s*:\s*register\s*\(\s*b(\d+)\s*,\s*space(\d+)\s*\)\s*\{([^}]*)\})");
			static const std::regex uniform_field(
				R"((?:(row_major|column_major)\s+)?(int64_t|uint64_t|float16_t|int16_t|uint16_t|int8_t|uint8_t|min16float|min10float|min16int|min16uint|double|float|half|int|uint|bool|dword)([1-4]?)(?:x([1-4]))?\s+([A-Za-z_]\w*)\s*(?:\[\s*(\d+)\s*\])?\s*;)");
			static const std::regex texture_resource(
				R"(\bTexture2D(?:\s*<[^>]+>)?\s+([A-Za-z_]\w*)\s*:\s*register\s*\(\s*t(\d+)\s*,\s*space\s*(\d+)\s*\)\s*;)");
			static const std::regex buffer_resource(
				R"(\bStructuredBuffer\s*<[^>]+>\s+([A-Za-z_]\w*)\s*:\s*register\s*\(\s*t(\d+)\s*,\s*space\s*(\d+)\s*\)\s*;)");
			static const std::regex sampler_resource(
				R"(\bSamplerState\s+([A-Za-z_]\w*)\s*:\s*register\s*\(\s*s(\d+)\s*,\s*space\s*(\d+)\s*\)\s*;)");
			static const std::regex resource_register(
				R"(\bregister\s*\(\s*[ts]\s*\d+)");
			static const std::regex row_major_default(
				R"(#\s*pragma\s+pack_matrix\s*\(\s*row_major\s*\))");

			source = std::regex_replace(source, comments, "");
			const bool default_row_major = std::regex_search(source, row_major_default);
			std::size_t keyword_count = 0;
			for (auto it = std::sregex_iterator(source.begin(), source.end(), cbuffer_keyword);
				it != std::sregex_iterator();
				++it)
			{
				++keyword_count;
			}
			if (keyword_count > 1)
			{
				throw std::invalid_argument(
					"MNGL reflection supports at most one fragment cbuffer");
			}

			uniform_reflection result{};
			if (keyword_count == 1)
			{
				std::smatch block_match;
				if (!std::regex_search(source, block_match, cbuffer) ||
					std::stoul(block_match[1].str()) != 0 ||
					std::stoul(block_match[2].str()) != 3)
				{
					throw std::invalid_argument(
						"MNGL fragment cbuffer must use register(b0, space3)");
				}

				const std::string body = block_match[3].str();
				const std::string remaining = std::regex_replace(body, uniform_field, "");
				if (remaining.find_first_not_of(" \t\r\n") != std::string::npos)
				{
					throw std::invalid_argument(
						"MNGL cbuffer reflection supports scalar, vector, and matrix fields only; arrays and structs are not supported");
				}

				uint32_t byte_offset = 0;
				for (auto it = std::sregex_iterator(body.begin(), body.end(), uniform_field);
					it != std::sregex_iterator();
					++it)
				{
					const std::smatch& field_match = *it;
					const std::string scalar_type = field_match[2].str();
					shader_uniform_field_layout field{};
					field.name = field_match[5].str();
					if (field_match[6].matched)
					{
						const auto array_count = std::stoull(field_match[6].str());
						if (array_count == 0 || array_count > std::numeric_limits<uint32_t>::max())
						{
							throw std::invalid_argument("HLSL uniform array length is outside the supported range");
						}
						field.is_array = true;
						field.array_count = static_cast<uint32_t>(array_count);
					}
					field.rows = field_match[3].str().empty()
						? 1
						: static_cast<uint8_t>(std::stoul(field_match[3].str()));
					field.columns = field_match[4].str().empty()
						? 1
						: static_cast<uint8_t>(std::stoul(field_match[4].str()));
					field.is_matrix = !field_match[4].str().empty();
					field.row_major = field_match[1].matched
						? field_match[1].str() == "row_major"
						: default_row_major;
					if (!field.is_matrix && !field_match[1].str().empty())
					{
						throw std::invalid_argument("row_major/column_major only applies to HLSL matrices");
					}

					if (scalar_type == "bool")
					{
						field.scalar_size = 4;
						field.is_boolean = true;
					}
					else if (scalar_type == "int8_t")
					{
						field.scalar_size = 1;
						field.is_signed = true;
					}
					else if (scalar_type == "uint8_t")
					{
						field.scalar_size = 1;
					}
					else if (scalar_type == "int16_t")
					{
						field.scalar_size = 2;
						field.is_signed = true;
					}
					else if (scalar_type == "uint16_t")
					{
						field.scalar_size = 2;
					}
					else if (scalar_type == "int64_t")
					{
						field.scalar_size = 8;
						field.is_signed = true;
					}
					else if (scalar_type == "uint64_t" || scalar_type == "dword")
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
						field.scalar_size = scalar_type == "float16_t"
							? 2
							: scalar_type == "double" ? 8 : 4;
						field.is_floating_point = true;
					}

					if (field.is_matrix)
					{
						byte_offset = (byte_offset + 15) & ~uint32_t(15);
						const uint32_t vector_components = field.row_major
							? field.columns
							: field.rows;
						const uint32_t vector_count = field.row_major
							? field.rows
							: field.columns;
						const uint32_t stride =
							(vector_components * field.scalar_size + 15) & ~uint32_t(15);
						field.byte_offset = byte_offset;
						field.element_size = vector_count * stride;
						field.array_stride = field.is_array
							? field.element_size
							: 0;
						if (field.is_array &&
							field.array_count > std::numeric_limits<uint32_t>::max() / field.array_stride)
						{
							throw std::length_error("HLSL matrix array exceeds the supported uniform block size");
						}
						field.storage_size = field.is_array
							? field.array_count * field.array_stride
							: field.element_size;
						if (field.storage_size > std::numeric_limits<uint32_t>::max() - byte_offset)
						{
							throw std::length_error("HLSL cbuffer exceeds the supported size");
						}
						byte_offset += field.storage_size;
					}
					else
					{
						const uint32_t field_size =
							static_cast<uint32_t>(field.rows) * field.scalar_size;
						if (field.is_array)
						{
							byte_offset = (byte_offset + 15) & ~uint32_t(15);
							field.byte_offset = byte_offset;
							field.element_size = field_size;
							field.array_stride = (field_size + 15) & ~uint32_t(15);
							if (field.array_count >
								std::numeric_limits<uint32_t>::max() / field.array_stride)
							{
								throw std::length_error("HLSL uniform array exceeds the supported block size");
							}
							field.storage_size = field.array_count * field.array_stride;
							if (field.storage_size > std::numeric_limits<uint32_t>::max() - byte_offset)
							{
								throw std::length_error("HLSL cbuffer exceeds the supported size");
							}
							byte_offset += field.storage_size;
						}
						else
						{
							if (byte_offset % field.scalar_size != 0)
							{
								byte_offset =
									(byte_offset + field.scalar_size - 1) &
									~static_cast<uint32_t>(field.scalar_size - 1);
							}
							if (byte_offset % 16 + field_size > 16)
							{
								byte_offset = (byte_offset + 15) & ~uint32_t(15);
							}
							field.byte_offset = byte_offset;
							field.element_size = field_size;
							field.storage_size = field_size;
							if (field.storage_size > std::numeric_limits<uint32_t>::max() - byte_offset)
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
					throw std::invalid_argument("MNGL fragment cbuffer must declare at least one uniform field");
				}
				result.block_size = (byte_offset + 15) & ~uint32_t(15);
				result.resource_counts.uniform_buffers = 1;
			}

			struct resource_declaration
			{
				std::string name;
				uint32_t slot = 0;
				uint32_t space = 0;
			};
			std::vector<resource_declaration> textures;
			std::vector<resource_declaration> buffers;
			std::vector<resource_declaration> samplers;
			auto collect_resources = [](const std::string& text, const std::regex& pattern)
			{
				std::vector<resource_declaration> found;
				for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern);
					it != std::sregex_iterator();
					++it)
				{
					const std::smatch& match = *it;
					const auto slot = std::stoull(match[2].str());
					const auto space = std::stoull(match[3].str());
					if (slot > std::numeric_limits<uint32_t>::max() ||
						space > std::numeric_limits<uint32_t>::max())
					{
						throw std::out_of_range("HLSL resource register exceeds the supported range");
					}
					found.push_back({
						.name = match[1].str(),
						.slot = static_cast<uint32_t>(slot),
						.space = static_cast<uint32_t>(space)
					});
				}
				return found;
			};
			textures = collect_resources(source, texture_resource);
			buffers = collect_resources(source, buffer_resource);
			samplers = collect_resources(source, sampler_resource);

			std::size_t reflected_resource_registers = 0;
			for (auto it = std::sregex_iterator(source.begin(), source.end(), resource_register);
				it != std::sregex_iterator();
				++it)
			{
				++reflected_resource_registers;
			}
			if (reflected_resource_registers != textures.size() + buffers.size() + samplers.size())
			{
				throw std::invalid_argument(
					"MNGL resource reflection supports Texture2D, StructuredBuffer, and SamplerState at space2");
			}

			auto require_space2 = [](const std::vector<resource_declaration>& declarations)
			{
				for (const auto& declaration : declarations)
				{
					if (declaration.space != 2)
					{
						throw std::invalid_argument("MNGL fragment resources must use HLSL register space2");
					}
				}
			};
			require_space2(textures);
			require_space2(buffers);
			require_space2(samplers);

			auto contiguous_slot_count = [](std::vector<uint32_t> slots, std::string_view kind)
			{
				std::sort(slots.begin(), slots.end());
				for (uint32_t index = 0; index < slots.size(); ++index)
				{
					if (slots[index] != index)
					{
						throw std::invalid_argument(std::format(
							"MNGL {} resource registers must be contiguous from slot zero",
							kind));
					}
				}
				return static_cast<uint32_t>(slots.size());
			};

			std::vector<uint32_t> sampled_texture_slots;
			std::vector<uint32_t> storage_texture_slots;
			std::vector<uint32_t> storage_buffer_slots;
			std::vector<uint32_t> sampler_slots;
			for (const auto& declaration : buffers)
			{
				storage_buffer_slots.push_back(declaration.slot);
				result.resources.push_back({
					.name = declaration.name,
					.kind = shader_resource_kind::storage_buffer,
					.slot = declaration.slot
				});
			}
			for (const auto& declaration : textures)
			{
				const auto matching_sampler = std::find_if(
					samplers.begin(),
					samplers.end(),
					[&](const resource_declaration& sampler) {
						return sampler.slot == declaration.slot;
					});
				const bool sampled = matching_sampler != samplers.end();
				auto& slots = sampled ? sampled_texture_slots : storage_texture_slots;
				slots.push_back(declaration.slot);
				result.resources.push_back({
					.name = declaration.name,
					.kind = sampled
						? shader_resource_kind::sampled_texture
						: shader_resource_kind::storage_texture,
					.slot = declaration.slot
				});
			}
			for (const auto& declaration : samplers)
			{
				if (std::none_of(
					textures.begin(),
					textures.end(),
					[&](const resource_declaration& texture) {
						return texture.slot == declaration.slot;
					}))
				{
					throw std::invalid_argument(
						"Each MNGL SamplerState must pair with a Texture2D at the same register index");
				}
				sampler_slots.push_back(declaration.slot);
			}

			result.resource_counts.samplers =
				contiguous_slot_count(std::move(sampled_texture_slots), "sampled texture");
			if (contiguous_slot_count(std::move(sampler_slots), "sampler") !=
				result.resource_counts.samplers)
			{
				throw std::invalid_argument("MNGL sampled textures and samplers must use matching register indices");
			}
			result.resource_counts.storage_textures =
				contiguous_slot_count(std::move(storage_texture_slots), "storage texture");
			result.resource_counts.storage_buffers =
				contiguous_slot_count(std::move(storage_buffer_slots), "storage buffer");
			return result;
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
