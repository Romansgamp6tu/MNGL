#define NOMINMAX
#include <Windows.h>

#include <mod_loader.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
	std::string utf8_from_wide(std::wstring_view value)
	{
		if (value.empty())
		{
			return {};
		}

		const int required = WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			value.data(),
			static_cast<int>(value.size()),
			nullptr,
			0,
			nullptr,
			nullptr);
		if (required <= 0)
		{
			throw std::runtime_error("could not convert a manifest argument to UTF-8");
		}

		std::string result(static_cast<std::size_t>(required), '\0');
		if (WideCharToMultiByte(
				CP_UTF8,
				WC_ERR_INVALID_CHARS,
				value.data(),
				static_cast<int>(value.size()),
				result.data(),
				required,
				nullptr,
				nullptr) != required)
		{
			throw std::runtime_error("could not convert a manifest argument to UTF-8");
		}
		return result;
	}

	std::string json_escape(std::string_view value)
	{
		std::string result;
		result.reserve(value.size());
		for (const unsigned char ch : value)
		{
			switch (ch)
			{
			case '"': result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\b': result += "\\b"; break;
			case '\f': result += "\\f"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default:
				if (ch < 0x20u)
				{
					throw std::invalid_argument(
						"manifest value contains a control character");
				}
				result.push_back(static_cast<char>(ch));
			}
		}
		return result;
	}
}

int wmain(int argc, wchar_t* argv[])
{
	try
	{
		if (argc != 5)
		{
			throw std::invalid_argument(
				"usage: write_tree_mod_manifest <package-directory> "
				"<module-filename> <mod-id> <display-name>");
		}

		const std::filesystem::path package_directory(argv[1]);
		const std::filesystem::path module_filename(argv[2]);
		if (module_filename.empty() || module_filename.has_parent_path() ||
			module_filename.filename() != module_filename)
		{
			throw std::invalid_argument(
				"module filename must be a single path component");
		}

		const auto mod_id = utf8_from_wide(argv[3]);
		const auto display_name = utf8_from_wide(argv[4]);
		std::filesystem::create_directories(package_directory / "bin");
		const auto binary_path = std::filesystem::path("bin") / module_filename;
		const auto manifest_path = package_directory / "mod.json";
		std::ofstream manifest(manifest_path, std::ios::binary | std::ios::trunc);
		if (!manifest)
		{
			throw std::runtime_error(
				"cannot create manifest: " + manifest_path.string());
		}

		manifest << "{\n"
			<< "  \"schema_version\": 1,\n"
			<< "  \"id\": \"" << json_escape(mod_id) << "\",\n"
			<< "  \"name\": \"" << json_escape(display_name) << "\",\n"
			<< "  \"version\": \"1.0.0\",\n"
			<< "  \"api_version\": \"1.0\",\n"
			<< "  \"binary_abi\": \""
			<< json_escape(mngl::mods::native_binary_abi_tag()) << "\",\n"
			<< "  \"binary\": \""
			<< json_escape(binary_path.generic_string()) << "\"\n"
			<< "}\n";
		if (!manifest)
		{
			throw std::runtime_error(
				"failed while writing manifest: " + manifest_path.string());
		}
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
