#pragma once

#include "runtime.hpp"

#include <SDL3/SDL_storage.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mngl::storage
{
	enum class entry_type
	{
		file,
		directory,
		other
	};

	struct entry
	{
		std::string name;
		std::string relative_path;
		entry_type type = entry_type::other;
		std::uint64_t size = 0;
	};

	namespace detail
	{
		inline std::string validate_relative_path(std::string_view path, bool allow_root = false)
		{
			if (path.find('\0') != std::string_view::npos)
			{
				throw std::invalid_argument("storage path cannot contain a null character");
			}
			if (path.empty())
			{
				if (allow_root)
				{
					return {};
				}
				throw std::invalid_argument("storage path cannot be empty");
			}
			if (path.front() == '/' || path.find('\\') != std::string_view::npos ||
			    (path.size() >= 2 &&
			     ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
			     path[1] == ':'))
			{
				throw std::invalid_argument("storage paths must be relative and use '/' separators");
			}

			std::string normalized(path);
			while (!normalized.empty() && normalized.back() == '/')
			{
				normalized.pop_back();
			}
			if (normalized.empty())
			{
				if (allow_root)
				{
					return {};
				}
				throw std::invalid_argument("storage path cannot refer to the root");
			}

			std::size_t component_begin = 0;
			while (component_begin < normalized.size())
			{
				const auto separator = normalized.find('/', component_begin);
				const auto component_end = separator == std::string::npos ? normalized.size() : separator;
				const auto component =
				    std::string_view(normalized).substr(component_begin, component_end - component_begin);
				if (component.empty() || component == "." || component == "..")
				{
					throw std::invalid_argument("storage paths cannot contain empty, '.' or '..' components");
				}
				if (separator == std::string::npos)
				{
					break;
				}
				component_begin = separator + 1;
			}
			return normalized;
		}

		inline std::string path_to_utf8(const std::filesystem::path& path)
		{
			const auto encoded = path.u8string();
			std::string result(reinterpret_cast<const char*>(encoded.data()), encoded.size());
			if (result.find('\0') != std::string::npos)
			{
				throw std::invalid_argument("storage root cannot contain a null character");
			}
			return result;
		}

		inline entry_type convert_type(SDL_PathType type) noexcept
		{
			switch (type)
			{
			case SDL_PATHTYPE_FILE:
				return entry_type::file;
			case SDL_PATHTYPE_DIRECTORY:
				return entry_type::directory;
			default:
				return entry_type::other;
			}
		}

		struct enumeration_state
		{
			std::vector<std::string> names;
			std::exception_ptr exception;
		};

		inline SDL_EnumerationResult SDLCALL collect_entry(void* userdata, const char*,
		                                                   const char* filename) noexcept
		{
			auto& state = *static_cast<enumeration_state*>(userdata);
			if (!filename)
			{
				return SDL_ENUM_CONTINUE;
			}
			try
			{
				state.names.emplace_back(filename);
				return SDL_ENUM_CONTINUE;
			}
			catch (...)
			{
				state.exception = std::current_exception();
				return SDL_ENUM_FAILURE;
			}
		}
	} // namespace detail

	class store
	{
	public:
		store(const store&) = delete;
		store& operator=(const store&) = delete;

		store(store&& other) noexcept : m_storage(std::exchange(other.m_storage, nullptr)) {}

		store& operator=(store&& other) noexcept
		{
			if (this != &other)
			{
				close_noexcept();
				m_storage = std::exchange(other.m_storage, nullptr);
			}
			return *this;
		}

		virtual ~store() noexcept { close_noexcept(); }

		[[nodiscard]] bool ready() const noexcept { return m_storage && SDL_StorageReady(m_storage); }

		[[nodiscard]] explicit operator bool() const noexcept { return m_storage != nullptr; }

		// Non-owning SDL handle for integrations that need lower-level access.
		[[nodiscard]] SDL_Storage* raw() const noexcept { return m_storage; }

		// Explicit close reports backend flush/close failures. The SDL handle is
		// released even when SDL_CloseStorage returns false.
		void close()
		{
			if (!m_storage)
			{
				return;
			}
			auto* storage = std::exchange(m_storage, nullptr);
			if (!SDL_CloseStorage(storage))
			{
				throw sdl_error("failed to close storage: ");
			}
		}

		[[nodiscard]] std::vector<std::byte> read_file(std::string_view relative_path) const
		{
			ensure_ready();
			const auto path = detail::validate_relative_path(relative_path);
			Uint64 length = 0;
			if (!SDL_GetStorageFileSize(m_storage, path.c_str(), &length))
			{
				throw sdl_error("failed to get storage file size: ");
			}
			if (length > static_cast<Uint64>(std::numeric_limits<std::size_t>::max()))
			{
				throw std::length_error("storage file is too large to fit in memory");
			}

			std::vector<std::byte> contents(static_cast<std::size_t>(length));
			if (length > 0 && !SDL_ReadStorageFile(m_storage, path.c_str(), contents.data(), length))
			{
				throw sdl_error("failed to read storage file: ");
			}
			return contents;
		}

		[[nodiscard]] std::string read_text_file(std::string_view relative_path) const
		{
			const auto bytes = read_file(relative_path);
			std::string text(bytes.size(), '\0');
			if (!bytes.empty())
			{
				std::memcpy(text.data(), bytes.data(), bytes.size());
			}
			return text;
		}

		void write_file(std::string_view relative_path, std::span<const std::byte> contents)
		{
			ensure_ready();
			const auto path = detail::validate_relative_path(relative_path);
			if (contents.size() > std::numeric_limits<Uint64>::max())
			{
				throw std::length_error("storage file is too large for SDL");
			}
			const std::byte empty_file = {};
			const void* source = contents.empty() ? &empty_file : contents.data();
			if (!SDL_WriteStorageFile(m_storage, path.c_str(), source, static_cast<Uint64>(contents.size())))
			{
				throw sdl_error("failed to write storage file: ");
			}
		}

		void write_text_file(std::string_view relative_path, std::string_view contents)
		{
			write_file(relative_path, std::as_bytes(std::span(contents.data(), contents.size())));
		}

		void create_directory(std::string_view relative_path)
		{
			ensure_ready();
			const auto path = detail::validate_relative_path(relative_path);
			std::size_t component_end = 0;
			while (component_end < path.size())
			{
				const auto separator = path.find('/', component_end);
				component_end = separator == std::string::npos ? path.size() : separator;
				const auto current_path = path.substr(0, component_end);

				SDL_PathInfo info{};
				if (SDL_GetStoragePathInfo(m_storage, current_path.c_str(), &info))
				{
					if (info.type != SDL_PATHTYPE_DIRECTORY)
					{
						throw std::runtime_error("storage path already exists and is not a directory: " +
						                         current_path);
					}
				}
				else
				{
					// A missing path is expected here; discard that query error so a
					// later mkdir failure reports its own SDL error.
					SDL_ClearError();
					if (!SDL_CreateStorageDirectory(m_storage, current_path.c_str()))
					{
						throw sdl_error("failed to create storage directory: ");
					}
				}

				if (separator == std::string::npos)
				{
					break;
				}
				component_end = separator + 1;
			}
		}

		[[nodiscard]] std::vector<entry> enumerate(std::string_view relative_directory = {}) const
		{
			ensure_ready();
			const auto directory = detail::validate_relative_path(relative_directory, true);
			detail::enumeration_state state;
			const char* path = directory.empty() ? nullptr : directory.c_str();
			if (!SDL_EnumerateStorageDirectory(m_storage, path, detail::collect_entry, &state))
			{
				if (state.exception)
				{
					std::rethrow_exception(state.exception);
				}
				throw sdl_error("failed to enumerate storage directory: ");
			}

			std::vector<entry> result;
			result.reserve(state.names.size());
			for (auto& name : state.names)
			{
				const auto full_path = directory.empty() ? name : directory + "/" + name;
				SDL_PathInfo info{};
				if (!SDL_GetStoragePathInfo(m_storage, full_path.c_str(), &info))
				{
					throw sdl_error("failed to get storage entry information: ");
				}
				result.push_back({std::move(name), full_path, detail::convert_type(info.type), info.size});
			}
			return result;
		}

	private:
		explicit store(SDL_Storage* storage, std::string_view open_error) : m_storage(storage)
		{
			if (!m_storage)
			{
				throw sdl_error(std::string(open_error));
			}
		}

		void ensure_ready() const
		{
			if (!m_storage)
			{
				throw std::logic_error("storage is closed");
			}
			if (!SDL_StorageReady(m_storage))
			{
				throw std::logic_error("storage is not ready; poll ready() before accessing files");
			}
		}

		void close_noexcept() noexcept
		{
			if (m_storage)
			{
				SDL_CloseStorage(std::exchange(m_storage, nullptr));
			}
		}

		SDL_Storage* m_storage = nullptr;

		friend class title_storage;
		friend class user_storage;
		friend class file_storage;
	};

	class title_storage final : public store
	{
	public:
		title_storage() : store(SDL_OpenTitleStorage(nullptr, 0), "failed to open title storage: ") {}

		explicit title_storage(const std::filesystem::path& root_override)
		    : store(open_override(root_override), "failed to open title storage: ")
		{
		}

	private:
		using store::create_directory;
		using store::write_file;
		using store::write_text_file;

		static SDL_Storage* open_override(const std::filesystem::path& root_override)
		{
			if (root_override.empty())
			{
				throw std::invalid_argument("title storage root override cannot be empty");
			}
			const auto utf8 = detail::path_to_utf8(root_override);
			return SDL_OpenTitleStorage(utf8.c_str(), 0);
		}
	};

	class user_storage final : public store
	{
	public:
		explicit user_storage(std::string organization = APP_ORGANIZATION,
		                      std::string application = APP_STORAGE_NAME)
		    : store(open_user(organization, application), "failed to open user storage: ")
		{
		}

	private:
		static SDL_Storage* open_user(const std::string& organization, const std::string& application)
		{
			if (organization.empty() || application.empty() || organization.find('\0') != std::string::npos ||
			    application.find('\0') != std::string::npos)
			{
				throw std::invalid_argument("user storage organization and application names must be "
				                            "non-empty and contain no null characters");
			}
			return SDL_OpenUserStorage(organization.c_str(), application.c_str(), 0);
		}
	};

	class file_storage final : public store
	{
	public:
		explicit file_storage(const std::filesystem::path& root)
		    : store(open_file(root), "failed to open file storage: ")
		{
		}

	private:
		static SDL_Storage* open_file(const std::filesystem::path& root)
		{
			if (root.empty())
			{
				throw std::invalid_argument("file storage root cannot be empty");
			}
			const auto utf8 = detail::path_to_utf8(root);
			return SDL_OpenFileStorage(utf8.c_str());
		}
	};
} // namespace mngl::storage

// Compatibility names; new code uses snake_case.
namespace mngl::storage
{
	using Storage = store;
	using TitleStorage = title_storage;
	using UserStorage = user_storage;
	using FileStorage = file_storage;
} // namespace mngl::storage
