#pragma once

#include "paths.hpp"
#include "window.hpp"

#include <SDL3/SDL_dialog.h>

#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mngl::dialog
{
	enum class request_status
	{
		pending,
		selected,
		cancelled,
		failed
	};

	struct result
	{
		request_status status = request_status::pending;
		std::vector<std::filesystem::path> paths;
		std::optional<std::size_t> selected_filter;
		std::string error;
	};

	struct file_filter
	{
		std::string name;
		// Semicolon-separated extensions, e.g. "png;jpg", or "*" for all files.
		std::string pattern;
	};

	struct file_dialog_options
	{
		const mngl::window* parent = nullptr;
		std::optional<std::filesystem::path> default_location;
		std::vector<file_filter> filters;
	};

	struct folder_dialog_options
	{
		const mngl::window* parent = nullptr;
		std::optional<std::filesystem::path> default_location;
	};

	namespace detail
	{
		struct request_state
		{
			mutable std::mutex mutex;
			result value;
		};

		struct callback_context
		{
			std::shared_ptr<request_state> state;
			std::vector<std::string> filter_names;
			std::vector<std::string> filter_patterns;
			std::vector<SDL_DialogFileFilter> sdl_filters;
			std::string default_location;
		};

		inline std::string path_to_utf8(const std::filesystem::path& path)
		{
			const auto encoded = path.u8string();
			return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
		}

		inline void complete(const std::shared_ptr<request_state>& state, result value) noexcept
		{
			try
			{
				std::lock_guard lock(state->mutex);
				state->value = std::move(value);
			}
			catch (...)
			{
				// Never allow a C++ exception to escape through SDL's C callback.
			}
		}

		inline void SDLCALL file_dialog_callback(void* userdata, const char* const* filelist,
		                                         int filter) noexcept
		{
			std::unique_ptr<callback_context> context(static_cast<callback_context*>(userdata));
			if (!context)
			{
				return;
			}

			result completed;
			try
			{
				if (!filelist)
				{
					completed.status = request_status::failed;
					const char* message = SDL_GetError();
					completed.error = message && *message ? message : "SDL file dialog failed";
				}
				else if (!*filelist)
				{
					completed.status = request_status::cancelled;
				}
				else
				{
					completed.status = request_status::selected;
					for (auto path = filelist; *path; ++path)
					{
						completed.paths.push_back(paths::detail::from_utf8(*path));
					}
					if (filter >= 0)
					{
						completed.selected_filter = static_cast<std::size_t>(filter);
					}
				}
			}
			catch (const std::exception& error)
			{
				completed.status = request_status::failed;
				try
				{
					completed.error = error.what();
				}
				catch (...)
				{
				}
			}
			catch (...)
			{
				completed.status = request_status::failed;
				try
				{
					completed.error = "Unknown error while processing SDL dialog result";
				}
				catch (...)
				{
				}
			}

			complete(context->state, std::move(completed));
		}

		inline std::shared_ptr<request_state> make_state()
		{
			return std::make_shared<request_state>();
		}

		inline std::shared_ptr<request_state> failed_state(std::string message)
		{
			auto state = make_state();
			state->value.status = request_status::failed;
			state->value.error = std::move(message);
			return state;
		}

		inline std::shared_ptr<request_state> launch_file_dialog(file_dialog_options options, bool allow_many,
		                                                         bool save)
		{
			if (options.parent && !options.parent->raw())
			{
				return failed_state("The parent window is not initialized");
			}

			auto context = std::make_unique<callback_context>();
			context->state = make_state();

			if (options.default_location && !options.default_location->empty())
			{
				context->default_location = path_to_utf8(*options.default_location);
			}

			context->filter_names.reserve(options.filters.size());
			context->filter_patterns.reserve(options.filters.size());
			for (auto& filter : options.filters)
			{
				context->filter_names.push_back(std::move(filter.name));
				context->filter_patterns.push_back(std::move(filter.pattern));
			}

			context->sdl_filters.reserve(options.filters.size());
			for (std::size_t index = 0; index < options.filters.size(); ++index)
			{
				context->sdl_filters.push_back(
				    {context->filter_names[index].c_str(), context->filter_patterns[index].c_str()});
			}

			const auto raw_window = options.parent ? options.parent->raw() : nullptr;
			const auto raw_filters = context->sdl_filters.empty() ? nullptr : context->sdl_filters.data();
			const auto filter_count = static_cast<int>(context->sdl_filters.size());
			const auto default_location =
			    context->default_location.empty() ? nullptr : context->default_location.c_str();
			auto state = context->state;
			auto* userdata = context.release();

			if (save)
			{
				SDL_ShowSaveFileDialog(file_dialog_callback, userdata, raw_window, raw_filters, filter_count,
				                       default_location);
			}
			else
			{
				SDL_ShowOpenFileDialog(file_dialog_callback, userdata, raw_window, raw_filters, filter_count,
				                       default_location, allow_many);
			}

			return state;
		}

		inline std::shared_ptr<request_state> launch_folder_dialog(folder_dialog_options options)
		{
			if (options.parent && !options.parent->raw())
			{
				return failed_state("The parent window is not initialized");
			}

			auto context = std::make_unique<callback_context>();
			context->state = make_state();
			if (options.default_location && !options.default_location->empty())
			{
				context->default_location = path_to_utf8(*options.default_location);
			}

			const auto raw_window = options.parent ? options.parent->raw() : nullptr;
			const auto default_location =
			    context->default_location.empty() ? nullptr : context->default_location.c_str();
			auto state = context->state;
			auto* userdata = context.release();

			SDL_ShowOpenFolderDialog(file_dialog_callback, userdata, raw_window, default_location, false);

			return state;
		}
	} // namespace detail

	class request
	{
	public:
		request(const request&) = default;
		request(request&&) noexcept = default;
		request& operator=(const request&) = default;
		request& operator=(request&&) noexcept = default;
		[[nodiscard]] bool valid() const noexcept { return static_cast<bool>(m_state); }
		explicit operator bool() const noexcept { return valid(); }

		// Returns a thread-safe snapshot. The result remains available on later polls.
		[[nodiscard]] mngl::dialog::result result() const
		{
			if (!m_state)
			{
				return {request_status::failed, {}, {}, "Dialog request is empty"};
			}
			std::lock_guard lock(m_state->mutex);
			return m_state->value;
		}

		[[nodiscard]] bool ready() const { return result().status != request_status::pending; }
		[[nodiscard]] mngl::dialog::result poll() const { return result(); }

	private:
		explicit request(std::shared_ptr<detail::request_state> state) : m_state(std::move(state)) {}

		std::shared_ptr<detail::request_state> m_state;

		friend request open_file(file_dialog_options options);
		friend request open_files(file_dialog_options options);
		friend request save_file(file_dialog_options options);
		friend request open_folder(folder_dialog_options options);
	};

	[[nodiscard]] inline request open_file(file_dialog_options options = {})
	{
		return request(detail::launch_file_dialog(std::move(options), false, false));
	}

	[[nodiscard]] inline request open_files(file_dialog_options options = {})
	{
		return request(detail::launch_file_dialog(std::move(options), true, false));
	}

	[[nodiscard]] inline request save_file(file_dialog_options options = {})
	{
		return request(detail::launch_file_dialog(std::move(options), false, true));
	}

	[[nodiscard]] inline request open_folder(folder_dialog_options options = {})
	{
		return request(detail::launch_folder_dialog(std::move(options)));
	}
} // namespace mngl::dialog
