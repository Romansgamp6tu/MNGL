#pragma once

#include <atomic>

namespace mngl::gpu
{
	namespace detail
	{
		inline std::atomic_bool gpu_culling_enabled_globally = false;
		inline std::atomic_bool cpu_culling_enabled_globally = true;
		inline std::atomic_bool order_independent_batch_sorting_enabled_globally = true;
		inline std::atomic_bool scene_culling_enabled_globally = true;
		inline std::atomic_bool dirty_region_updates_enabled_globally = true;
	} // namespace detail

	// These process-wide 2D rendering settings take effect on later frames.
	inline void enable_gpu_culling() noexcept
	{
		detail::gpu_culling_enabled_globally.store(true, std::memory_order_relaxed);
	}

	inline void disable_gpu_culling() noexcept
	{
		detail::gpu_culling_enabled_globally.store(false, std::memory_order_relaxed);
	}

	[[nodiscard]] inline bool gpu_culling_enabled() noexcept
	{
		return detail::gpu_culling_enabled_globally.load(std::memory_order_relaxed);
	}

	inline void enable_cpu_culling() noexcept
	{
		detail::cpu_culling_enabled_globally.store(true, std::memory_order_relaxed);
	}

	inline void disable_cpu_culling() noexcept
	{
		detail::cpu_culling_enabled_globally.store(false, std::memory_order_relaxed);
	}

	[[nodiscard]] inline bool cpu_culling_enabled() noexcept
	{
		return detail::cpu_culling_enabled_globally.load(std::memory_order_relaxed);
	}

	inline void enable_order_independent_batch_sorting() noexcept
	{
		detail::order_independent_batch_sorting_enabled_globally.store(true, std::memory_order_relaxed);
	}

	inline void disable_order_independent_batch_sorting() noexcept
	{
		detail::order_independent_batch_sorting_enabled_globally.store(false, std::memory_order_relaxed);
	}

	[[nodiscard]] inline bool order_independent_batch_sorting_enabled() noexcept
	{
		return detail::order_independent_batch_sorting_enabled_globally.load(std::memory_order_relaxed);
	}

	inline void enable_scene_culling() noexcept
	{
		detail::scene_culling_enabled_globally.store(true, std::memory_order_relaxed);
	}

	inline void disable_scene_culling() noexcept
	{
		detail::scene_culling_enabled_globally.store(false, std::memory_order_relaxed);
	}

	[[nodiscard]] inline bool scene_culling_enabled() noexcept
	{
		return detail::scene_culling_enabled_globally.load(std::memory_order_relaxed);
	}

	inline void enable_dirty_region_updates() noexcept
	{
		detail::dirty_region_updates_enabled_globally.store(true, std::memory_order_relaxed);
	}

	inline void disable_dirty_region_updates() noexcept
	{
		detail::dirty_region_updates_enabled_globally.store(false, std::memory_order_relaxed);
	}

	[[nodiscard]] inline bool dirty_region_updates_enabled() noexcept
	{
		return detail::dirty_region_updates_enabled_globally.load(std::memory_order_relaxed);
	}
} // namespace mngl::gpu
