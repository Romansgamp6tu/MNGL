#pragma once

#include "graphics_shader.hpp"
#include "utilities.hpp"

#include <filesystem>
#include <format>
#include <random>
#include <stdexcept>
#include <system_error>

namespace mngl::gpu
{
	struct shader_compile_options
	{
		graphics_shader_stage stage = graphics_shader_stage::vertex;
		shader_format format = shader_formats::spirv;
		std::string entrypoint = "main";
		shader_resource_layout resources{};
	};

	class shader_loader
	{
	public:
		static graphics_shader_asset compile_with_shadercross(
			const std::filesystem::path& hlsl_path,
			const shader_compile_options& options = {}
		)
		{
			const auto temp_dir = std::filesystem::temp_directory_path();
			const auto compiled_path = temp_dir / std::format("mngl_temp_{}.spv", std::random_device{}());

			const std::string cmd = std::format(
				"shadercross \"{}\" -o \"{}\" -e {}",
				hlsl_path.string(),
				compiled_path.string(),
				options.entrypoint
			);

			// 1. Компилируем и захватываем stdout/stderr
			const auto [exit_code, output] = run_command(cmd);

			if (exit_code != 0)
			{
				throw std::runtime_error(std::format(
					"Shadercross failed for '{}' (code {}):\n"
					"----------------------------------------\n"
					"{}\n"
					"----------------------------------------",
					hlsl_path.string(),
					exit_code,
					output.empty() ? "<no output>" : output
				));
			}

			// 2. Читаем байты и гарантированно удаляем временный файл
			std::vector<uint8_t> bytes;
			try
			{
				bytes = read_binary_file(compiled_path);
			}
			catch (...)
			{
				std::error_code ec;
				std::filesystem::remove(compiled_path, ec);
				throw;
			}

			std::error_code ec;
			std::filesystem::remove(compiled_path, ec);

			// 3. Собираем CPU-ассет
			graphics_shader_asset asset{};
			asset.stage = options.stage;
			asset.code.assign(options.format, std::move(bytes));
			asset.entrypoint = options.entrypoint;
			asset.resources = options.resources;
			asset.debug_name = hlsl_path.filename().string();

			return asset;
		}

		static graphics_shader_asset compile_with_shadercross(
			const std::filesystem::path& hlsl_path,
			graphics_shader_stage stage,
			shader_format format = shader_formats::spirv,
			const std::string& entrypoint = "main"
		)
		{
			return compile_with_shadercross(hlsl_path, shader_compile_options{
				.stage = stage,
				.format = format,
				.entrypoint = entrypoint
											});
		}
	};
}