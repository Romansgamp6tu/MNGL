#pragma once

#include "runtime.hpp"

#include <SDL3/SDL_clipboard.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mngl::clipboard
{
	using data_provider = std::function<std::optional<std::vector<std::byte>>(std::string_view mime_type)>;

	namespace detail
	{
		inline void validate_mime_type(std::string_view mime_type)
		{
			if (mime_type.empty()) [[unlikely]]
			{
				throw std::invalid_argument("clipboard MIME type cannot be empty");
			}
			if (mime_type.find('\0') != std::string_view::npos) [[unlikely]]
			{
				throw std::invalid_argument("clipboard MIME type cannot contain a null character");
			}
		}

		inline void validate_text(std::string_view text)
		{
			if (text.find('\0') != std::string_view::npos) [[unlikely]]
			{
				throw std::invalid_argument("clipboard text cannot contain a null character");
			}
		}

		struct sdl_free_deleter
		{
			void operator()(void* value) const noexcept { SDL_free(value); }
		};

		struct offer_state
		{
			std::vector<std::string> mime_types;
			data_provider provide;
			std::mutex provider_mutex;
			std::vector<std::optional<std::vector<std::byte>>> cached_data;
			std::vector<bool> requested;
		};

		inline const void* SDLCALL provide_data(void* userdata, const char* mime_type,
		                                        std::size_t* size) noexcept
		{
			if (size)
			{
				*size = 0;
			}
			if (!userdata || !mime_type || !size)
			{
				return nullptr;
			}

			auto& state = *static_cast<offer_state*>(userdata);
			const auto mime_type_iter =
			    std::find(state.mime_types.begin(), state.mime_types.end(), mime_type);
			if (mime_type_iter == state.mime_types.end())
			{
				return nullptr;
			}
			const auto index = static_cast<std::size_t>(mime_type_iter - state.mime_types.begin());

			try
			{
				std::lock_guard lock(state.provider_mutex);
				if (!state.requested[index])
				{
					// Cache even an empty result: each MIME type is a snapshot for this offer.
					state.requested[index] = true;
					state.cached_data[index] = state.provide(mime_type);
				}
				const auto& data = state.cached_data[index];
				if (!data || data->empty())
				{
					return nullptr;
				}

				*size = data->size();
				return data->data();
			}
			catch (...)
			{
				// C++ exceptions must not escape into SDL.
				return nullptr;
			}
		}

		inline void SDLCALL cleanup_offer(void* userdata) noexcept
		{
			delete static_cast<offer_state*>(userdata);
		}
	} // namespace detail

	// Clipboard text is UTF-8. SDL owns and frees its returned buffer via SDL_free.
	inline void set_text(std::string_view text)
	{
		detail::validate_text(text);
		const std::string terminated(text);
		if (!SDL_SetClipboardText(terminated.c_str())) [[unlikely]]
		{
			throw sdl_error("failed to set clipboard text: ");
		}
	}

	[[nodiscard]] inline std::string get_text()
	{
		std::unique_ptr<char, detail::sdl_free_deleter> text(SDL_GetClipboardText());
		if (!text) [[unlikely]]
		{
			throw sdl_error("failed to get clipboard text: ");
		}
		return std::string(text.get());
	}

	[[nodiscard]] inline bool has_text()
	{
		return SDL_HasClipboardText();
	}

	// The primary selection is separate from the clipboard and is primarily
	// available on X11 and Wayland systems.
	namespace primary_selection
	{
		inline void set_text(std::string_view text)
		{
			detail::validate_text(text);
			const std::string terminated(text);
			if (!SDL_SetPrimarySelectionText(terminated.c_str())) [[unlikely]]
			{
				throw sdl_error("failed to set primary selection text: ");
			}
		}

		[[nodiscard]] inline std::string get_text()
		{
			std::unique_ptr<char, detail::sdl_free_deleter> text(SDL_GetPrimarySelectionText());
			if (!text) [[unlikely]]
			{
				throw sdl_error("failed to get primary selection text: ");
			}
			return std::string(text.get());
		}

		[[nodiscard]] inline bool has_text()
		{
			return SDL_HasPrimarySelectionText();
		}
	} // namespace primary_selection

	// Returns all formats currently offered by the system clipboard.
	[[nodiscard]] inline std::vector<std::string> mime_types()
	{
		std::size_t count = 0;
		std::unique_ptr<char*, detail::sdl_free_deleter> types(SDL_GetClipboardMimeTypes(&count));
		if (!types) [[unlikely]]
		{
			throw sdl_error("failed to get clipboard MIME types: ");
		}

		std::vector<std::string> result;
		result.reserve(count);
		for (std::size_t index = 0; index < count; ++index)
		{
			result.emplace_back(types.get()[index]);
		}
		return result;
	}

	[[nodiscard]] inline bool has_data(std::string_view mime_type)
	{
		detail::validate_mime_type(mime_type);
		const std::string terminated(mime_type);
		return SDL_HasClipboardData(terminated.c_str());
	}

	[[nodiscard]] inline std::vector<std::byte> get_data(std::string_view mime_type)
	{
		detail::validate_mime_type(mime_type);
		const std::string terminated(mime_type);
		std::size_t size = 0;
		std::unique_ptr<void, detail::sdl_free_deleter> data(SDL_GetClipboardData(terminated.c_str(), &size));
		if (!data) [[unlikely]]
		{
			throw sdl_error("failed to get clipboard data: ");
		}

		const auto* first = static_cast<const std::byte*>(data.get());
		return {first, first + size};
	}

	// Offers formats to other applications. Data is generated on the first request
	// per MIME type and remains alive until SDL invokes its cleanup callback. Each
	// format is a snapshot for this offer; call set_data() again to replace it.
	inline void set_data(std::vector<std::string> mime_types, data_provider provide)
	{
		if (mime_types.empty()) [[unlikely]]
		{
			throw std::invalid_argument("clipboard offer must provide at least one MIME type");
		}
		if (!provide) [[unlikely]]
		{
			throw std::invalid_argument("clipboard data provider cannot be empty");
		}
		for (std::size_t index = 0; index < mime_types.size(); ++index)
		{
			detail::validate_mime_type(mime_types[index]);
			if (std::find(mime_types.begin(), mime_types.begin() + index, mime_types[index]) !=
			    mime_types.begin() + index) [[unlikely]]
			{
				throw std::invalid_argument("clipboard offer contains a duplicate MIME type");
			}
		}

		auto state = std::make_unique<detail::offer_state>();
		state->mime_types = std::move(mime_types);
		state->provide = std::move(provide);
		state->cached_data.resize(state->mime_types.size());
		state->requested.resize(state->mime_types.size(), false);

		std::vector<const char*> mime_type_names;
		mime_type_names.reserve(state->mime_types.size());
		for (const auto& type : state->mime_types)
		{
			mime_type_names.push_back(type.c_str());
		}

		if (!SDL_SetClipboardData(detail::provide_data, detail::cleanup_offer, state.get(),
		                          mime_type_names.data(), mime_type_names.size())) [[unlikely]]
		{
			throw sdl_error("failed to set clipboard data: ");
		}
		state.release();
	}

	inline void clear()
	{
		if (!SDL_ClearClipboardData()) [[unlikely]]
		{
			throw sdl_error("failed to clear clipboard: ");
		}
	}
} // namespace mngl::clipboard

