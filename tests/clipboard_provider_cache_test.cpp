#include <MNGL/clipboard.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
		{
			throw std::runtime_error(message);
		}
	}

	void test_repeated_requests_reuse_one_buffer()
	{
		constexpr std::size_t payload_size = 1024 * 1024;
		constexpr int request_count = 32;
		mngl::clipboard::detail::offer_state state;
		state.mime_types.emplace_back("application/x-mngl-test");
		state.cached_data.resize(state.mime_types.size());
		state.requested.resize(state.mime_types.size(), false);

		std::size_t provider_calls = 0;
		state.provide = [&provider_calls,
		                 payload_size](std::string_view) -> std::optional<std::vector<std::byte>>
		{
			++provider_calls;
			return std::vector<std::byte>(payload_size, static_cast<std::byte>(provider_calls));
		};

		const void* first_pointer = nullptr;
		for (int request = 0; request < request_count; ++request)
		{
			std::size_t size = 0;
			const void* data =
			    mngl::clipboard::detail::provide_data(&state, "application/x-mngl-test", &size);
			require(data != nullptr, "provider returned no data");
			require(size == payload_size, "cached payload size changed");
			if (request == 0)
			{
				first_pointer = data;
			}
			require(data == first_pointer, "repeated request did not reuse its retained buffer");
			require(static_cast<const std::byte*>(data)[0] == std::byte{1},
			        "repeated request did not return the first response snapshot");
		}

		require(provider_calls == 1, "provider was called more than once for one MIME type");
		require(state.cached_data.size() == 1 && state.cached_data[0] &&
		            state.cached_data[0]->size() == payload_size,
		        "offer retained more than one payload for the MIME type");
	}

	void test_missing_data_is_cached_and_clears_size()
	{
		constexpr int request_count = 32;
		mngl::clipboard::detail::offer_state state;
		state.mime_types.emplace_back("application/x-mngl-empty");
		state.cached_data.resize(state.mime_types.size());
		state.requested.resize(state.mime_types.size(), false);

		std::size_t provider_calls = 0;
		state.provide = [&provider_calls](std::string_view) -> std::optional<std::vector<std::byte>>
		{
			++provider_calls;
			return std::nullopt;
		};

		for (int request = 0; request < request_count; ++request)
		{
			std::size_t size = 123;
			const void* data =
			    mngl::clipboard::detail::provide_data(&state, "application/x-mngl-empty", &size);
			require(data == nullptr && size == 0, "missing payload was not returned as empty data");
		}
		require(provider_calls == 1, "empty provider response was not cached");
	}
} // namespace

int main()
{
	try
	{
		test_repeated_requests_reuse_one_buffer();
		test_missing_data_is_cached_and_clears_size();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
