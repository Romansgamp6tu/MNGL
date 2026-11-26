#include <MNGL/shader_loader.hpp>

#include <exception>
#include <iostream>
#include <string>

int main()
{
	try
	{
		mngl::gpu::shader_compile_options options{};
		options.format = mngl::gpu::shader_format::msl;
		const std::string shader_directory = MNGL_SHADER_SOURCE_DIRECTORY;
		const auto vertex = mngl::gpu::shader_loader::compile_vertex(
		    shader_directory + "/batcher2d.vert.hlsl", options);
		const auto fragment = mngl::gpu::shader_loader::compile_fragment(
		    shader_directory + "/batcher2d.frag.hlsl", options);
		options.resources = {.storage_buffers = 1, .uniform_buffers = 1};
		const auto svg_gradient = mngl::gpu::shader_loader::compile_fragment(
		    shader_directory + "/svg_gradient.frag.hlsl", options);
		options.resources = {};
		const auto svg_filter_vertex = mngl::gpu::shader_loader::compile_vertex(
		    shader_directory + "/svg_filter.vert.hlsl", options);
		options.resources = {.samplers = 2, .uniform_buffers = 1};
		const auto svg_filter_fragment = mngl::gpu::shader_loader::compile_fragment(
		    shader_directory + "/svg_filter.frag.hlsl", options);
		if (!vertex.valid() || !fragment.valid() || !svg_gradient.valid() ||
		    !svg_filter_vertex.valid() || !svg_filter_fragment.valid())
		{
			std::cerr << "MNGL built-in or SVG shaders did not compile to MSL.\n";
			return 1;
		}
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
