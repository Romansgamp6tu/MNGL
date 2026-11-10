#pragma once

#include "graphics_shader.hpp"
#include "file_io.hpp"
#include "runtime.hpp"

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
#include <sstream>
#include <source_location>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

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

#if !defined(_WIN32)
extern "C" char** environ;
#endif

namespace mngl::gpu
{
	enum class shader_source_language : std::uint8_t
	{
		hlsl,
		glsl
	};

	// GLSL is first compiled to SPIR-V by glslangValidator; shadercross handles other targets.
	// Executable paths may be absolute or names resolved through PATH.
	struct shader_compile_options
	{
		shader_stage stage = shader_stage::vertex;
		shader_format format = static_cast<shader_format>(::mngl::gpu::default_shader_format);
		std::string entrypoint = "main";
		shader_resource_layout resources{};
		shader_source_language source_language = shader_source_language::hlsl;
		std::filesystem::path shadercross_executable = "shadercross";
		std::filesystem::path glslang_validator_executable = "glslangValidator";
		// Reflect the MNGL HLSL layout at b0/space3 and resources at space2.
		bool reflect_uniforms = false;
		// Zero means infer the literal group size from HLSL [numthreads] or GLSL local_size.
		glm::uvec3 compute_threadgroup_size{0, 0, 0};
	};

	class shader_loader
	{
	public:
		static graphics_shader_asset compile(const std::filesystem::path& source_path,
		                                     const shader_compile_options& options = {})
		{
			if (!valid_graphics_shader_stage(options.stage)) [[unlikely]]
			{
				throw std::invalid_argument("shader_loader::compile accepts only vertex and fragment "
				                            "shaders; use compile_compute for compute shaders");
			}

			const auto reflected_uniforms = reflect_uniforms_if_requested(options, source_path);
			auto bytes = compile_shader_bytecode(source_path, options, options.stage);

			graphics_shader_asset asset{};
			asset.stage = options.stage;
			asset.code.assign(options.format, std::move(bytes));
			asset.entrypoint = options.entrypoint;
			asset.resources = options.resources;
			asset.debug_name = source_path.filename().string();
			apply_uniform_reflection(asset, options, reflected_uniforms);
			if (!asset.valid()) [[unlikely]]
			{
				throw std::runtime_error("Shader compilation produced an invalid graphics shader asset");
			}
			return asset;
		}

		static graphics_shader_asset compile_vertex(const std::filesystem::path& source_path,
		                                            shader_compile_options options = {})
		{
			options.stage = graphics_shader_stage::vertex;
			return compile(source_path, options);
		}

		static graphics_shader_asset compile_fragment(const std::filesystem::path& source_path,
		                                              shader_compile_options options = {})
		{
			options.stage = graphics_shader_stage::fragment;
			if (options.source_language == shader_source_language::hlsl)
			{
				options.reflect_uniforms = true;
			}
			return compile(source_path, options);
		}

		static compute_shader_asset compile_compute(const std::filesystem::path& source_path,
		                                            shader_compile_options options = {})
		{
			options.stage = shader_stage::compute;
			if (options.source_language == shader_source_language::hlsl)
			{
				options.reflect_uniforms = true;
			}
			if (options.source_language == shader_source_language::glsl && options.reflect_uniforms) [[unlikely]]
			{
				throw std::invalid_argument(
				    "MNGL named uniform/resource reflection currently supports HLSL compute shaders only");
			}

			const auto threadgroup_size = resolve_compute_threadgroup_size(source_path, options);
			const auto reflected_uniforms = reflect_uniforms_if_requested(options, source_path);
			auto bytes = compile_shader_bytecode(source_path, options, shader_stage::compute);

			compute_shader_asset asset{};
			asset.stage = shader_stage::compute;
			asset.code.assign(options.format, std::move(bytes));
			asset.entrypoint = options.entrypoint;
			asset.resources = options.resources;
			asset.debug_name = source_path.filename().string();
			asset.threadgroup_size = threadgroup_size;
			apply_uniform_reflection(asset, options, reflected_uniforms);
			if (!asset.valid()) [[unlikely]]
			{
				throw std::runtime_error("Shader compilation produced an invalid compute shader asset");
			}
			return asset;
		}

		static graphics_shader_asset load_binary(const std::filesystem::path& bytecode_path,
		                                         const std::filesystem::path& reflection_source = {},
		                                         const shader_compile_options& options = {})
		{
			if (!valid_graphics_shader_stage(options.stage) || !valid_shader_format(options.format) ||
			    !valid_entrypoint(options.entrypoint)) [[unlikely]]
			{
				throw std::invalid_argument("Cannot load shader bytecode with invalid shader options");
			}
			validate_source_file(bytecode_path);

			auto bytes = read_binary_file(bytecode_path);
			if (bytes.empty()) [[unlikely]]
			{
				throw std::invalid_argument(
				    std::format("Shader bytecode is empty: '{}'", bytecode_path.string()));
			}

			uniform_reflection reflected_uniforms{};
			if (options.reflect_uniforms)
			{
				if (reflection_source.empty()) [[unlikely]]
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

		static compute_shader_asset load_compute_binary(const std::filesystem::path& bytecode_path,
		                                                const std::filesystem::path& reflection_source = {},
		                                                const shader_compile_options& options = {})
		{
			if (!valid_shader_format(options.format) || !valid_entrypoint(options.entrypoint)) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Cannot load compute shader bytecode with invalid shader options");
			}
			validate_source_file(bytecode_path);

			auto bytes = read_binary_file(bytecode_path);
			if (bytes.empty()) [[unlikely]]
			{
				throw std::invalid_argument(
				    std::format("Shader bytecode is empty: '{}'", bytecode_path.string()));
			}

			glm::uvec3 threadgroup_size = options.compute_threadgroup_size;
			if (threadgroup_size.x == 0 && threadgroup_size.y == 0 && threadgroup_size.z == 0)
			{
				if (reflection_source.empty()) [[unlikely]]
				{
					throw std::invalid_argument("Loading compute bytecode requires compute_threadgroup_size "
					                            "or the matching source file");
				}
				threadgroup_size = resolve_compute_threadgroup_size(reflection_source, options);
			}
			else if (threadgroup_size.x == 0 || threadgroup_size.y == 0 || threadgroup_size.z == 0) [[unlikely]]
			{
				throw std::invalid_argument("Compute threadgroup dimensions must all be non-zero");
			}

			uniform_reflection reflected_uniforms{};
			if (options.reflect_uniforms)
			{
				if (reflection_source.empty()) [[unlikely]]
				{
					throw std::invalid_argument(
					    "Named-uniform reflection requires the matching HLSL source path");
				}
				auto reflection_options = options;
				reflection_options.stage = shader_stage::compute;
				reflected_uniforms = reflect_uniforms_if_requested(reflection_options, reflection_source);
			}

			compute_shader_asset asset{};
			asset.stage = shader_stage::compute;
			asset.code.assign(options.format, std::move(bytes));
			asset.entrypoint = options.entrypoint;
			asset.resources = options.resources;
			asset.debug_name = bytecode_path.filename().string();
			asset.threadgroup_size = threadgroup_size;
			apply_uniform_reflection(asset, options, reflected_uniforms);
			if (!asset.valid()) [[unlikely]]
			{
				throw std::runtime_error("Loaded bytecode produced an invalid compute shader asset");
			}
			return asset;
		}

		static graphics_shader_asset compile_glsl(const std::filesystem::path& glsl_path,
		                                          shader_compile_options options = {})
		{
			options.source_language = shader_source_language::glsl;
			return compile(glsl_path, options);
		}

		// Kept for source compatibility; source_language may also select the GLSL pipeline.
		static graphics_shader_asset compile_with_shadercross(const std::filesystem::path& source_path,
		                                                      const shader_compile_options& options = {})
		{
			return compile(source_path, options);
		}

		static graphics_shader_asset compile_with_shadercross(
		    const std::filesystem::path& hlsl_path, graphics_shader_stage stage,
		    shader_format format = static_cast<shader_format>(::mngl::gpu::default_shader_format),
		    const std::string& entrypoint = "main")
		{
			return compile(hlsl_path, shader_compile_options{
			                              .stage = stage, .format = format, .entrypoint = entrypoint});
		}

	private:
#include "detail/shader_loader/reflection.inc"
#include "detail/shader_loader/compiler.inc"
#include "detail/shader_loader/process.inc"
	};
} // namespace mngl::gpu
