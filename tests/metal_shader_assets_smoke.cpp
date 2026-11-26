#define APP_NAME "MNGL Metal shader asset smoke"
#define APP_VERSION 1
#define APP_FLAGS 0

#include <MNGL/detail/two_d/batcher_types.hpp>
#include <MNGL/detail/two_d/svg_effect_renderer.hpp>
#include <MNGL/paths.hpp>
#include <MNGL/runtime.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	void require_msl(const mngl::gpu::graphics_shader_asset& shader, const char* name)
	{
		require(shader.valid(), name);
		require(shader.code.format == mngl::gpu::shader_format::msl, name);
	}
} // namespace

int main()
{
	try
	{
		mngl::runtime::ensure();
		const auto staged_shaders = mngl::paths::application_directory() / "shaders";
		for (const auto* name : {"batcher2d.vert.msl", "batcher2d.frag.msl", "svg_gradient.frag.msl",
		                         "svg_filter.vert.msl", "svg_filter.frag.msl"})
		{
			require(std::filesystem::is_regular_file(staged_shaders / name),
			        "MSL asset is missing from the application's shader staging directory");
		}

		const auto batcher = mngl::gpu::batcher2d_shader_assets::load_default(mngl::gpu::shader_format::msl);
		require_msl(batcher.vertex, "Default 2D vertex shader did not load as MSL");
		require_msl(batcher.fragment, "Default 2D fragment shader did not load as MSL");

		mngl::gpu::shader_compile_options options{};
		options.format = mngl::gpu::shader_format::msl;
		options.shadercross_executable = "mngl-forbidden-shadercross";
		options.glslang_validator_executable = "mngl-forbidden-glslangValidator";
		auto directory = mngl::paths::shader_directory("svg_gradient.frag.hlsl");
		options.stage = mngl::gpu::graphics_shader_stage::fragment;
		options.resources.storage_buffers = 1;
		options.resources.uniform_buffers = 1;
		require_msl(mngl::two_d::detail::svg::load_svg_shader(directory / "svg_gradient.frag.hlsl", options),
		            "SVG gradient shader did not load as MSL");

		directory = mngl::paths::shader_directory("svg_filter.vert.hlsl");
		options.stage = mngl::gpu::graphics_shader_stage::vertex;
		options.resources = {};
		require_msl(mngl::two_d::detail::svg::load_svg_shader(directory / "svg_filter.vert.hlsl", options),
		            "SVG filter vertex shader did not load as MSL");

		directory = mngl::paths::shader_directory("svg_filter.frag.hlsl");
		options.stage = mngl::gpu::graphics_shader_stage::fragment;
		options.resources.samplers = 2;
		options.resources.uniform_buffers = 1;
		require_msl(mngl::two_d::detail::svg::load_svg_shader(directory / "svg_filter.frag.hlsl", options),
		            "SVG filter fragment shader did not load as MSL");
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
