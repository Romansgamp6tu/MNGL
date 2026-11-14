#include <MNGL/shader_loader.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	class temporary_shader_files
	{
	public:
		temporary_shader_files()
		{
			const auto serial = std::chrono::steady_clock::now().time_since_epoch().count();
			m_path = std::filesystem::temp_directory_path() /
			         ("mngl_shader_reflection_regression_" + std::to_string(serial));
			std::filesystem::create_directories(m_path);
		}

		~temporary_shader_files()
		{
			std::error_code error;
			std::filesystem::remove_all(m_path, error);
		}

		[[nodiscard]] std::filesystem::path source(std::string_view name, std::string_view contents) const
		{
			const auto path = m_path / (std::string(name) + ".hlsl");
			std::ofstream file(path, std::ios::binary);
			file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
			if (!file)
				throw std::runtime_error("failed to write shader test source");
			return path;
		}

		[[nodiscard]] std::filesystem::path bytecode(std::string_view name) const
		{
			const auto path = m_path / (std::string(name) + ".dxil");
			const std::array<std::uint8_t, 4> bytes{0x01, 0x02, 0x03, 0x04};
			std::ofstream file(path, std::ios::binary);
			file.write(reinterpret_cast<const char*>(bytes.data()),
			           static_cast<std::streamsize>(bytes.size()));
			if (!file)
				throw std::runtime_error("failed to write shader test bytecode");
			return path;
		}

	private:
		std::filesystem::path m_path;
	};

	[[nodiscard]] mngl::gpu::graphics_shader_asset reflect_fragment(const temporary_shader_files& files,
	                                                                std::string_view name,
	                                                                std::string_view source)
	{
		mngl::gpu::shader_compile_options options{};
		options.stage = mngl::gpu::shader_stage::fragment;
		options.format = mngl::gpu::shader_format::dxil;
		options.reflect_uniforms = true;
		return mngl::gpu::shader_loader::load_binary(files.bytecode(name), files.source(name, source),
		                                             options);
	}

	[[nodiscard]] mngl::gpu::compute_shader_asset reflect_compute(const temporary_shader_files& files,
	                                                              std::string_view name,
	                                                              std::string_view source)
	{
		mngl::gpu::shader_compile_options options{};
		options.stage = mngl::gpu::shader_stage::compute;
		options.format = mngl::gpu::shader_format::dxil;
		options.reflect_uniforms = true;
		return mngl::gpu::shader_loader::load_compute_binary(files.bytecode(name), files.source(name, source),
		                                                     options);
	}

	[[nodiscard]] const mngl::gpu::shader_resource_field_layout& find_resource(
	    const mngl::gpu::shader_asset& shader, std::string_view name)
	{
		const auto& layout = shader.resource_layout();
		for (const auto& resource : layout)
		{
			if (resource.name == name)
				return resource;
		}
		throw std::runtime_error("reflected resource was missing: " + std::string(name));
	}

	template <typename Function> void expect_invalid_argument(Function&& function, std::string_view message)
	{
		try
		{
			function();
		}
		catch (const std::invalid_argument& error)
		{
			if (std::string_view(error.what()).find(message) == std::string_view::npos)
				throw std::runtime_error("invalid_argument did not explain the reflection failure");
			return;
		}
		throw std::runtime_error("expected std::invalid_argument from HLSL reflection");
	}

	template <typename T>
	[[nodiscard]] T read_value(const std::vector<std::uint8_t>& bytes, std::size_t offset)
	{
		if (offset + sizeof(T) > bytes.size())
			throw std::runtime_error("uniform test read exceeded its block");
		T value{};
		std::memcpy(&value, bytes.data() + offset, sizeof(T));
		return value;
	}

	void test_mixed_fragment_registers(const temporary_shader_files& files)
	{
		const auto shader = reflect_fragment(files, "mixed_fragment",
		                                     "Texture2D<float4> image : register(t0, space2);\n"
		                                     "SamplerState image_sampler : register(s0, space2);\n"
		                                     "StructuredBuffer<float4> factors : register(t1, space2);\n"
		                                     "Texture2D<float4> history : register(t2, space2);\n"
		                                     "StructuredBuffer<uint> indices : register(t3, space2);\n");

		if (shader.resources.samplers != 1 || shader.resources.storage_textures != 1 ||
		    shader.resources.storage_buffers != 2)
			throw std::runtime_error("mixed fragment resource counts were incorrect");
		if (find_resource(shader, "image").slot != 0 || find_resource(shader, "history").slot != 0 ||
		    find_resource(shader, "factors").slot != 0 || find_resource(shader, "indices").slot != 1)
			throw std::runtime_error("HLSL t-registers were not translated to per-kind SDL slots");
	}

	void test_mixed_compute_registers(const temporary_shader_files& files)
	{
		const auto shader =
		    reflect_compute(files, "mixed_compute",
			                "Texture2D<float4> image : register(t0, space0);\n"
			                "SamplerState image_sampler : register(s0, space0);\n"
			                "StructuredBuffer<float4> factors : register(t1, space0);\n"
			                "RWTexture2D<float4> output : register(u0, space1);\n"
			                "RWStructuredBuffer<uint> indices : register(u1, space1);\n"
			                "[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {}\n");

		if (shader.resources.samplers != 1 || shader.resources.storage_buffers != 1 ||
		    shader.resources.read_write_storage_textures != 1 ||
		    shader.resources.read_write_storage_buffers != 1)
			throw std::runtime_error("mixed compute resource counts were incorrect");
		if (find_resource(shader, "image").slot != 0 || find_resource(shader, "factors").slot != 0 ||
		    find_resource(shader, "output").slot != 0 || find_resource(shader, "indices").slot != 0)
			throw std::runtime_error("HLSL t/u registers were not translated to per-kind SDL slots");
	}

	void test_register_gaps_are_rejected(const temporary_shader_files& files)
	{
		expect_invalid_argument(
		    [&]
		    {
			    (void)reflect_fragment(files, "t_register_gap",
				                       "Texture2D<float4> image : register(t0, space2);\n"
				                       "StructuredBuffer<uint> values : register(t2, space2);\n");
		    },
		    "t-register resource registers must be contiguous");
		expect_invalid_argument(
		    [&]
		    {
			    (void)reflect_compute(files, "u_register_gap",
				                      "RWTexture2D<float4> output : register(u0, space1);\n"
				                      "RWStructuredBuffer<uint> values : register(u2, space1);\n"
				                      "[numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) {}\n");
		    },
		    "u-register resource registers must be contiguous");
	}

	void test_preprocessing_is_rejected(const temporary_shader_files& files)
	{
		expect_invalid_argument(
		    [&] { (void)reflect_fragment(files, "include_fragment", "#include \"shared.hlsli\"\n"); },
		    "does not preprocess '#include'");
		expect_invalid_argument(
		    [&]
		    {
			    (void)reflect_fragment(files, "conditional_fragment",
				                       "#if 0\nTexture2D<float4> hidden : register(t0, space2);\n#endif\n");
		    },
		    "does not preprocess '#if'");
		expect_invalid_argument(
		    [&]
		    {
			    (void)reflect_compute(files, "include_compute",
				                      "#include \"threads.hlsli\"\n"
				                      "void main(uint3 id : SV_DispatchThreadID) {}\n");
		    },
		    "does not preprocess '#include'");
	}

	void test_matrix_and_array_packing(const temporary_shader_files& files)
	{
		auto matrix = reflect_fragment(files, "matrix_packing",
		                               "cbuffer Params : register(b0, space3) { column_major float3x2 "
		                               "transform_value; float tail; };\n");
		const glm::mat2x3 transform{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
		matrix["transform_value"] = transform;
		matrix["tail"] = 91.25f;
		const auto matrix_blocks = matrix.uniform_block_data();
		if (matrix_blocks.size() != 1 || read_value<float>(matrix_blocks.front(), 24) != transform[1][2] ||
		    read_value<float>(matrix_blocks.front(), 28) != 91.25f)
			throw std::runtime_error("column-major float3x2 packing did not leave the final lane available");

		auto array =
		    reflect_fragment(files, "array_packing",
			                 "cbuffer Params : register(b0, space3) { float2 values[2]; float tail; };\n");
		const std::array<glm::vec2, 2> values{glm::vec2{1.0f, 2.0f}, glm::vec2{3.0f, 4.0f}};
		array["values"] = values;
		array["tail"] = 17.5f;
		const auto array_blocks = array.uniform_block_data();
		if (array_blocks.size() != 1 || read_value<float>(array_blocks.front(), 16) != 3.0f ||
		    read_value<float>(array_blocks.front(), 20) != 4.0f ||
		    read_value<float>(array_blocks.front(), 24) != 17.5f)
			throw std::runtime_error("float2 array packing did not place the following scalar at byte 24");
	}

	void test_hlsl_fixed_width_integer_types(const temporary_shader_files& files)
	{
		auto shader =
		    reflect_fragment(files, "integer_types",
			                 "cbuffer Params : register(b0, space3) { uint64_t wide; int64_t signed_wide; "
			                 "uint16_t small; int16_t signed_small; };\n");
		constexpr std::uint64_t wide = 0x0123456789abcdefull;
		constexpr std::int64_t signed_wide = -0x1234567890ll;
		constexpr std::uint16_t small_value = 0xfedc;
		constexpr std::int16_t signed_small = -1234;
		shader["wide"] = wide;
		shader["signed_wide"] = signed_wide;
		shader["small"] = small_value;
		shader["signed_small"] = signed_small;
		const auto blocks = shader.uniform_block_data();
		if (blocks.size() != 1 || read_value<std::uint64_t>(blocks.front(), 0) != wide ||
		    read_value<std::int64_t>(blocks.front(), 8) != signed_wide ||
		    read_value<std::uint16_t>(blocks.front(), 16) != small_value ||
		    read_value<std::int16_t>(blocks.front(), 18) != signed_small)
			throw std::runtime_error(
			    "fixed-width HLSL integer uniforms were not encoded at the expected offsets");
	}
} // namespace

int main()
{
	try
	{
		const temporary_shader_files files;
		test_mixed_fragment_registers(files);
		test_mixed_compute_registers(files);
		test_register_gaps_are_rejected(files);
		test_preprocessing_is_rejected(files);
		test_matrix_and_array_packing(files);
		test_hlsl_fixed_width_integer_types(files);
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
