#pragma once

#include "runtime.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace mngl::paths
{
	namespace detail
	{
		// SDL returns UTF-8 paths. Constructing a path from char8_t data keeps
		// non-ASCII paths correct on Windows as well as POSIX platforms.
		inline std::filesystem::path from_utf8(std::string_view value)
		{
			std::u8string encoded;
			encoded.reserve(value.size());
			for (const char byte : value)
			{
				encoded.push_back(static_cast<char8_t>(static_cast<unsigned char>(byte)));
			}
			return std::filesystem::path(std::move(encoded));
		}
	} // namespace detail

	enum class user_folder
	{
		home,
		desktop,
		documents,
		downloads,
		music,
		pictures,
		public_share,
		saved_games,
		screenshots,
		templates,
		videos
	};

	// Directory containing application resources. On macOS this can be the
	// Resources directory inside the application's bundle.
	[[nodiscard]] inline std::expected<std::filesystem::path, sdl_error> try_application_directory()
	{
		const char* path = SDL_GetBasePath();
		if (!path)
		{
			return std::unexpected(sdl_error("Failed to get application directory: "));
		}
		return detail::from_utf8(path);
	}

	// Persistent, application-specific directory. SDL creates it when needed.
	[[nodiscard]] inline std::expected<std::filesystem::path, sdl_error> try_user_data_directory()
	{
		char* raw_path = SDL_GetPrefPath(APP_ORGANIZATION, APP_STORAGE_NAME);
		if (!raw_path)
		{
			return std::unexpected(sdl_error("Failed to get user data directory: "));
		}

		std::unique_ptr<char, decltype(&SDL_free)> path(raw_path, &SDL_free);
		return detail::from_utf8(path.get());
	}

	// A user-managed standard folder such as Downloads or Documents.
	[[nodiscard]] inline std::expected<std::filesystem::path, sdl_error> try_user_folder_path(
	    user_folder folder)
	{
		SDL_Folder sdl_folder{};
		switch (folder)
		{
		case user_folder::home:
			sdl_folder = SDL_FOLDER_HOME;
			break;
		case user_folder::desktop:
			sdl_folder = SDL_FOLDER_DESKTOP;
			break;
		case user_folder::documents:
			sdl_folder = SDL_FOLDER_DOCUMENTS;
			break;
		case user_folder::downloads:
			sdl_folder = SDL_FOLDER_DOWNLOADS;
			break;
		case user_folder::music:
			sdl_folder = SDL_FOLDER_MUSIC;
			break;
		case user_folder::pictures:
			sdl_folder = SDL_FOLDER_PICTURES;
			break;
		case user_folder::public_share:
			sdl_folder = SDL_FOLDER_PUBLICSHARE;
			break;
		case user_folder::saved_games:
			sdl_folder = SDL_FOLDER_SAVEDGAMES;
			break;
		case user_folder::screenshots:
			sdl_folder = SDL_FOLDER_SCREENSHOTS;
			break;
		case user_folder::templates:
			sdl_folder = SDL_FOLDER_TEMPLATES;
			break;
		case user_folder::videos:
			sdl_folder = SDL_FOLDER_VIDEOS;
			break;
		default:
			return std::unexpected(sdl_error::from_message("invalid user folder"));
		}

		const char* path = SDL_GetUserFolder(sdl_folder);
		if (!path)
		{
			return std::unexpected(sdl_error("Failed to get user folder: "));
		}
		return detail::from_utf8(path);
	}
} // namespace mngl::paths

namespace mngl::paths
{
	namespace detail
	{
		inline std::filesystem::path value_or_throw(std::expected<std::filesystem::path, sdl_error> value)
		{
			if (!value) [[unlikely]]
				throw value.error();
			return std::move(*value);
		}
	} // namespace detail
	[[nodiscard]] inline std::filesystem::path application_directory()
	{
		return detail::value_or_throw(try_application_directory());
	}
	[[nodiscard]] inline std::filesystem::path user_data_directory()
	{
		return detail::value_or_throw(try_user_data_directory());
	}
	[[nodiscard]] inline std::filesystem::path user_folder_path(user_folder folder)
	{
		if (folder < user_folder::home || folder > user_folder::videos) [[unlikely]]
			throw std::invalid_argument("invalid user folder");
		return detail::value_or_throw(try_user_folder_path(folder));
	}
} // namespace mngl::paths
