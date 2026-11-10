#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace mngl::detail
{
	template <typename T, typename Allocator>
	void reserve_vector_growth(std::vector<T, Allocator>& values, std::size_t required,
	                           std::size_t maximum_capacity = std::numeric_limits<std::size_t>::max())
	{
		const std::size_t limit = std::min(values.max_size(), maximum_capacity);
		if (required > limit) [[unlikely]]
		{
			throw std::length_error("MNGL container capacity limit exceeded");
		}
		if (required <= values.capacity())
		{
			return;
		}

		const std::size_t current_capacity = values.capacity();
		const std::size_t grown_capacity =
		    current_capacity == 0 ? 1 : (current_capacity > limit / 2 ? limit : current_capacity * 2);
		values.reserve(std::max(required, grown_capacity));
	}

	template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
	void reserve_unordered_map_growth(std::unordered_map<Key, Value, Hash, Equal, Allocator>& values,
	                                  std::size_t required)
	{
		const std::size_t limit = values.max_size();
		if (required > limit) [[unlikely]]
		{
			throw std::length_error("MNGL container capacity limit exceeded");
		}

		const long double bucket_capacity =
		    static_cast<long double>(values.bucket_count()) * values.max_load_factor();
		if (static_cast<long double>(required) <= bucket_capacity)
		{
			return;
		}

		const std::size_t current_capacity = bucket_capacity >= static_cast<long double>(limit)
		                                         ? limit
		                                         : static_cast<std::size_t>(bucket_capacity);
		const std::size_t grown_capacity =
		    current_capacity == 0 ? 1 : (current_capacity > limit / 2 ? limit : current_capacity * 2);
		values.reserve(std::max(required, grown_capacity));
	}
} // namespace mngl::detail
