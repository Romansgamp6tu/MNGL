#include <mod_loader.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
	[[nodiscard]] std::string json_escape(std::string_view value)
	{
		std::string result;
		result.reserve(value.size());
		for (const char character : value)
		{
			if (character == '"' || character == '\\')
			{
				result.push_back('\\');
			}
			if (static_cast<unsigned char>(character) < 0x20)
			{
				throw std::invalid_argument("manifest value contains a control character");
			}
			result.push_back(character);
		}
		return result;
	}
}

int main(int argc, char** argv)
{
	try
	{
		if (argc != 3)
		{
			throw std::invalid_argument(
				"usage: write_b_plus_manifest <package-directory> <module-filename>");
		}
		const std::filesystem::path package_directory(argv[1]);
		const std::filesystem::path module_filename(argv[2]);
		if (module_filename.empty() || module_filename.has_parent_path() ||
			module_filename.filename() != module_filename)
		{
			throw std::invalid_argument("module filename must be a single path component");
		}

		std::filesystem::create_directories(package_directory / "bin");
		const auto binary_path = std::filesystem::path("bin") / module_filename;
		const auto manifest_path = package_directory / "mod.json";
		std::ofstream manifest(manifest_path, std::ios::binary | std::ios::trunc);
		if (!manifest)
		{
			throw std::runtime_error("cannot create manifest: " + manifest_path.string());
		}
		manifest << "{\n"
			<< "  \"schema_version\": 1,\n"
			<< "  \"id\": \"mngl.tree-practicum.b-plus\",\n"
			<< "  \"name\": \"MNGL B+ tree algorithm\",\n"
			<< "  \"version\": \"1.0.0\",\n"
			<< "  \"api_version\": \"1.0\",\n"
			<< "  \"binary_abi\": \""
			<< json_escape(mngl::mods::native_binary_abi_tag()) << "\",\n"
			<< "  \"binary\": \""
			<< json_escape(binary_path.generic_string()) << "\"\n"
			<< "}\n";
		if (!manifest)
		{
			throw std::runtime_error("failed while writing manifest: " + manifest_path.string());
		}
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
