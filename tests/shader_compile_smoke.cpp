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
		if (!vertex.valid() || !fragment.valid())
		{
			std::cerr << "MNGL built-in shaders did not compile to MSL.\n";
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
