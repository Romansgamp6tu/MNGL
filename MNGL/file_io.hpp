#pragma once

#include <cstddef>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace mngl
{
	struct process_result
	{
		int exit_code = 0;
		std::string output;
	};

	inline std::vector<std::uint8_t> read_binary_file(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file.is_open()) [[unlikely]]
		{
			throw std::runtime_error(std::format("Failed to open file: {}", path.string()));
		}

		const auto size = static_cast<std::streamoff>(file.tellg());
		if (size < 0) [[unlikely]]
		{
			throw std::runtime_error(std::format("Failed to determine file size: {}", path.string()));
		}
		const auto unsigned_size = static_cast<std::uintmax_t>(size);
		if (unsigned_size > std::numeric_limits<std::size_t>::max() ||
		    unsigned_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) [[unlikely]]
		{
			throw std::length_error(std::format("File is too large to read: {}", path.string()));
		}
		if (!file.seekg(0, std::ios::beg)) [[unlikely]]
		{
			throw std::runtime_error(std::format("Failed to seek in file: {}", path.string()));
		}

		std::vector<std::uint8_t> buffer(static_cast<std::size_t>(unsigned_size));
		if (!buffer.empty() &&
		    !file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()))) [[unlikely]]
		{
			throw std::runtime_error(std::format("Failed to read binary file: {}", path.string()));
		}

		return buffer;
	}

} // namespace mngl
