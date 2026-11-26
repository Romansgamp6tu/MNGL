#pragma once

#include "../../error.hpp"
#include "../../rectangle.hpp"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace mngl::two_d::detail::svg
{
	struct image_pixels
	{
		glm::uvec2 size{};
		std::vector<std::uint8_t> rgba;
	};

	inline constexpr std::size_t max_image_bytes = 64 * 1024 * 1024;

	[[nodiscard]] inline std::vector<std::uint8_t> decode_image_base64(std::string_view data)
	{
		constexpr std::string_view alphabet =
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::vector<std::uint8_t> bytes;
		bytes.reserve(std::min(data.size() / 4 * 3, max_image_bytes));
		std::uint32_t bits = 0;
		unsigned digits = 0;
		unsigned padding = 0;
		for (const char character : data)
		{
			if (character == ' ' || character == '\t' || character == '\r' || character == '\n')
			{
				continue;
			}
			if (character == '=')
			{
				if (++padding > 2)
				{
					throw std::invalid_argument("SVG image has invalid Base64 padding");
				}
				continue;
			}
			const auto value = alphabet.find(character);
			if (value == std::string_view::npos || padding != 0)
			{
				throw std::invalid_argument("SVG image has invalid Base64 data");
			}
			bits = (bits << 6) | static_cast<std::uint32_t>(value);
			if (++digits == 4)
			{
				if (max_image_bytes - bytes.size() < 3)
				{
					throw std::length_error("SVG image exceeds the encoded image size limit");
				}
				bytes.push_back(static_cast<std::uint8_t>(bits >> 16));
				bytes.push_back(static_cast<std::uint8_t>(bits >> 8));
				bytes.push_back(static_cast<std::uint8_t>(bits));
				digits = 0;
				bits = 0;
			}
		}
		if (digits == 1 || (padding != 0 && digits + padding != 4))
		{
			throw std::invalid_argument("SVG image has incomplete Base64 data");
		}
		if (digits >= 2)
		{
			if (max_image_bytes - bytes.size() < digits - 1)
			{
				throw std::length_error("SVG image exceeds the encoded image size limit");
			}
			bytes.push_back(static_cast<std::uint8_t>(bits >> (digits * 6 - 8)));
			if (digits == 3)
			{
				bytes.push_back(static_cast<std::uint8_t>(bits >> 2));
			}
		}
		return bytes;
	}

	[[nodiscard]] inline std::vector<std::uint8_t> read_svg_image_file(std::string_view href,
	                                                                   const std::filesystem::path& directory)
	{
		if (directory.empty())
		{
			throw std::invalid_argument("SVG image must be embedded unless local images are enabled");
		}
		std::string relative;
		for (std::size_t i = 0; i < href.size(); ++i)
		{
			char character = href[i];
			if (character == '%')
			{
				const auto hex = [](char value) -> unsigned
				{
					if (value >= '0' && value <= '9')
						return value - '0';
					if (value >= 'a' && value <= 'f')
						return value - 'a' + 10;
					if (value >= 'A' && value <= 'F')
						return value - 'A' + 10;
					throw std::invalid_argument("Invalid percent escape in SVG image path");
				};
				if (href.size() - i < 3)
				{
					throw std::invalid_argument("Incomplete percent escape in SVG image path");
				}
				character = static_cast<char>((hex(href[i + 1]) << 4) | hex(href[i + 2]));
				i += 2;
			}
			if (character == '\0' || character == ':' || character == '\\' || character == '?' ||
			    character == '#')
			{
				throw std::invalid_argument("SVG image requires a relative file path");
			}
			relative.push_back(character);
		}
		const std::filesystem::path path(std::u8string(relative.begin(), relative.end()));
		if (path.empty() || path.has_root_path())
		{
			throw std::invalid_argument("SVG image requires a relative file path");
		}
		const auto resolved = std::filesystem::canonical(directory / path);
		const auto within = resolved.lexically_relative(directory);
		if (within.empty() || *within.begin() == ".." || !std::filesystem::is_regular_file(resolved))
		{
			throw std::invalid_argument("SVG image must be a file inside the SVG directory");
		}
		const auto length = std::filesystem::file_size(resolved);
		if (length > max_image_bytes)
		{
			throw std::length_error("SVG image exceeds the encoded image size limit");
		}
		std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
		std::ifstream input(resolved, std::ios::binary);
		if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
		{
			throw std::runtime_error("Failed to read local SVG image");
		}
		return bytes;
	}

	[[nodiscard]] inline image_pixels decode_svg_image(std::string_view href,
	                                                   const std::filesystem::path& directory)
	{
		const auto data = [&]
		{
			if (!href.starts_with("data:"))
			{
				return read_svg_image_file(href, directory);
			}
			const auto comma = href.find(',');
			const auto header = href.substr(0, comma);
			if (comma == std::string_view::npos ||
			    (header != "data:image/png;base64" && header != "data:image/jpeg;base64" &&
			     header != "data:image/webp;base64"))
			{
				throw std::invalid_argument("SVG image data URI must contain Base64 PNG, JPEG, or WebP");
			}
			return decode_image_base64(href.substr(comma + 1));
		}();

		const auto matches = [&](std::size_t offset, std::string_view signature)
		{
			return data.size() >= offset + signature.size() &&
			       std::memcmp(data.data() + offset, signature.data(), signature.size()) == 0;
		};
		using decoder = SDL_Surface*(SDLCALL*)(SDL_IOStream*);
		decoder load = nullptr;
		if (matches(0, "\x89PNG\r\n\x1a\n"))
			load = IMG_LoadPNG_IO;
		else if (matches(0, "\xff\xd8\xff"))
			load = IMG_LoadJPG_IO;
		else if (matches(0, "RIFF") && matches(8, "WEBP"))
			load = IMG_LoadWEBP_IO;
		else
			throw std::invalid_argument("SVG images support only PNG, JPEG, and WebP");

		std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(
		    SDL_IOFromConstMem(data.data(), data.size()), SDL_CloseIO);
		if (!stream)
		{
			throw sdl_error("Failed to open SVG image bytes: ");
		}
		std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(load(stream.get()),
		                                                                    SDL_DestroySurface);
		if (!surface)
		{
			throw sdl_error("Failed to decode SVG image: ");
		}
		if (surface->w <= 0 || surface->h <= 0 || surface->w > 8192 || surface->h > 8192)
		{
			throw std::length_error("SVG image dimensions must be within 8192 pixels per axis");
		}
		std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> rgba(
		    SDL_ConvertSurface(surface.get(), SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
		if (!rgba)
		{
			throw sdl_error("Failed to convert SVG image to RGBA: ");
		}
		const auto row_bytes = static_cast<std::size_t>(rgba->w) * 4;
		image_pixels result{{static_cast<unsigned>(rgba->w), static_cast<unsigned>(rgba->h)},
		                    std::vector<std::uint8_t>(row_bytes * rgba->h)};
		for (int y = 0; y < rgba->h; ++y)
		{
			std::memcpy(result.rgba.data() + y * row_bytes,
			            static_cast<const std::uint8_t*>(rgba->pixels) +
			                static_cast<std::size_t>(y) * rgba->pitch,
			            row_bytes);
		}
		return result;
	}
} // namespace mngl::two_d::detail::svg
