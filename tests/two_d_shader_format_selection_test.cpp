#include <MNGL/batcher2d.hpp>

#include <stdexcept>

int main()
{
	using mngl::gpu::shader_format;
	using mngl::gpu::shader_formats;
	using mngl::gpu::detail::select_2d_shader_format;

	if (select_2d_shader_format(shader_formats::spirv) != shader_format::spirv)
		throw std::runtime_error("SPIR-V-only GPU did not select SPIR-V shaders");
	if (select_2d_shader_format(shader_formats::dxil | shader_formats::spirv) != shader_format::dxil)
		throw std::runtime_error("DXIL was not preferred when available");
	if (select_2d_shader_format(shader_formats::msl | shader_formats::spirv) != shader_format::msl)
		throw std::runtime_error("MSL was not preferred over SPIR-V");
	if (select_2d_shader_format(shader_formats::dxbc) != shader_format::dxbc)
		throw std::runtime_error("DXBC-only GPU did not select DXBC shaders");

	try
	{
		(void)select_2d_shader_format(shader_formats::invalid);
	}
	catch (const std::invalid_argument&)
	{
		return 0;
	}
	throw std::runtime_error("GPU with no supported 2D shader format should be rejected");
}
