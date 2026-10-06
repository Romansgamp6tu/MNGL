#pragma once

#include "device.hpp"
#include "utilities.hpp"
#include "graphics_pipeline.hpp" // Для compare_op

#include <utility>
#include <stdexcept>

namespace mngl::gpu
{
	enum class filter_mode : uint32_t
	{
		nearest = SDL_GPU_FILTER_NEAREST,
		linear = SDL_GPU_FILTER_LINEAR
	};

	enum class sampler_mipmap_mode : uint32_t
	{
		nearest = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		linear = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
	};

	enum class sampler_address_mode : uint32_t
	{
		repeat = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		mirrored_repeat = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT,
		clamp_to_edge = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
	};

	// RAII-обёртка над SDL_GPUSampler
	class sampler
	{
	public:
		sampler() noexcept = default;

		sampler(SDL_GPUDevice* device, SDL_GPUSampler* handle) noexcept
			: m_device(device), m_handle(handle)
		{ }

		~sampler() { destroy(); }

		sampler(const sampler&) = delete;
		sampler& operator=(const sampler&) = delete;

		sampler(sampler&& other) noexcept
			: m_device(std::exchange(other.m_device, nullptr))
			, m_handle(std::exchange(other.m_handle, nullptr))
		{ }

		sampler& operator=(sampler&& other) noexcept
		{
			if (this != &other)
			{
				destroy();
				m_device = std::exchange(other.m_device, nullptr);
				m_handle = std::exchange(other.m_handle, nullptr);
			}
			return *this;
		}

		[[nodiscard]] SDL_GPUSampler* raw() const noexcept { return m_handle; }
		[[nodiscard]] SDL_GPUDevice* device_raw() const noexcept { return m_device; }
		[[nodiscard]] bool valid() const noexcept { return m_handle != nullptr; }
		explicit operator bool() const noexcept { return valid(); }

		void destroy() noexcept
		{
			if (m_device && m_handle)
			{
				SDL_ReleaseGPUSampler(m_device, m_handle);
				m_handle = nullptr;
			}
		}

	private:
		SDL_GPUDevice* m_device = nullptr;
		SDL_GPUSampler* m_handle = nullptr;
	};

	// Билдер для гибкого создания сэмплеров
	class sampler_builder
	{
	public:
		sampler_builder() = default;

		sampler_builder& set_filters(filter_mode min_filter, filter_mode mag_filter) noexcept
		{
			m_min_filter = min_filter;
			m_mag_filter = mag_filter;
			return *this;
		}

		sampler_builder& set_filter(filter_mode filter) noexcept
		{
			return set_filters(filter, filter);
		}

		sampler_builder& set_address_mode(sampler_address_mode u, sampler_address_mode v, sampler_address_mode w) noexcept
		{
			m_address_u = u;
			m_address_v = v;
			m_address_w = w;
			return *this;
		}

		sampler_builder& set_address_mode(sampler_address_mode mode) noexcept
		{
			return set_address_mode(mode, mode, mode);
		}

		sampler_builder& set_mipmap_mode(sampler_mipmap_mode mode) noexcept
		{
			m_mipmap_mode = mode;
			return *this;
		}

		sampler_builder& set_lod_bias(float bias) noexcept
		{
			m_lod_bias = bias;
			return *this;
		}

		sampler_builder& enable_anisotropy(float max_anisotropy = 16.0f) noexcept
		{
			m_enable_anisotropy = true;
			m_max_anisotropy = max_anisotropy;
			return *this;
		}

		sampler_builder& enable_compare(compare_op op) noexcept
		{
			m_enable_compare = true;
			m_compare_op = op;
			return *this;
		}

		sampler build(SDL_GPUDevice* device) const
		{
			if (!device)
			{
				throw std::runtime_error("Cannot create sampler with null device");
			}

			SDL_GPUSamplerCreateInfo info{};
			info.min_filter = static_cast<SDL_GPUFilter>(m_min_filter);
			info.mag_filter = static_cast<SDL_GPUFilter>(m_mag_filter);
			info.mipmap_mode = static_cast<SDL_GPUSamplerMipmapMode>(m_mipmap_mode);
			info.address_mode_u = static_cast<SDL_GPUSamplerAddressMode>(m_address_u);
			info.address_mode_v = static_cast<SDL_GPUSamplerAddressMode>(m_address_v);
			info.address_mode_w = static_cast<SDL_GPUSamplerAddressMode>(m_address_w);
			info.mip_lod_bias = m_lod_bias;
			info.enable_anisotropy = m_enable_anisotropy;
			info.max_anisotropy = m_max_anisotropy;
			info.enable_compare = m_enable_compare;
			info.compare_op = static_cast<SDL_GPUCompareOp>(m_compare_op);

			SDL_GPUSampler* handle = SDL_CreateGPUSampler(device, &info);
			if (!handle)
			{
				throw sdl_error("Failed to create GPU sampler: ");
			}

			return sampler{device, handle};
		}

		// --- Пресеты ---

		static sampler create_point_clamp(SDL_GPUDevice* device)
		{
			return sampler_builder{}
				.set_filter(filter_mode::nearest)
				.set_address_mode(sampler_address_mode::clamp_to_edge)
				.build(device);
		}

		static sampler create_point_repeat(SDL_GPUDevice* device)
		{
			return sampler_builder{}
				.set_filter(filter_mode::nearest)
				.set_address_mode(sampler_address_mode::repeat)
				.build(device);
		}

		static sampler create_linear_clamp(SDL_GPUDevice* device)
		{
			return sampler_builder{}
				.set_filter(filter_mode::linear)
				.set_mipmap_mode(sampler_mipmap_mode::linear)
				.set_address_mode(sampler_address_mode::clamp_to_edge)
				.build(device);
		}

		static sampler create_linear_repeat(SDL_GPUDevice* device)
		{
			return sampler_builder{}
				.set_filter(filter_mode::linear)
				.set_mipmap_mode(sampler_mipmap_mode::linear)
				.set_address_mode(sampler_address_mode::repeat)
				.build(device);
		}

	private:
		filter_mode m_min_filter = filter_mode::linear;
		filter_mode m_mag_filter = filter_mode::linear;
		sampler_mipmap_mode m_mipmap_mode = sampler_mipmap_mode::linear;

		sampler_address_mode m_address_u = sampler_address_mode::clamp_to_edge;
		sampler_address_mode m_address_v = sampler_address_mode::clamp_to_edge;
		sampler_address_mode m_address_w = sampler_address_mode::clamp_to_edge;

		float m_lod_bias = 0.0f;
		bool m_enable_anisotropy = false;
		float m_max_anisotropy = 1.0f;

		bool m_enable_compare = false;
		compare_op m_compare_op = compare_op::never;
	};
}