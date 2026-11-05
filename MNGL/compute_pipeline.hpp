#pragma once

#include "device.hpp"
#include "graphics_shader.hpp"
#include "utilities.hpp"

#include <utility>
#include <stdexcept>
#include <optional>

namespace mngl::gpu
{
	class compute_pipeline_builder;

	class compute_pipeline
	{
	public:
		compute_pipeline() noexcept = default;
		~compute_pipeline() { destroy(); }

		compute_pipeline(const compute_pipeline&) = delete;
		compute_pipeline& operator=(const compute_pipeline&) = delete;

		compute_pipeline(compute_pipeline&& other) noexcept
		    : m_device_owner(std::move(other.m_device_owner)),
		      m_device(std::exchange(other.m_device, nullptr)),
		      m_handle(std::exchange(other.m_handle, nullptr)),
		      m_resources(std::exchange(other.m_resources, shader_resource_layout{})),
		      m_threadgroup_size(std::exchange(other.m_threadgroup_size, glm::uvec3{0, 0, 0}))
		{
		}

		compute_pipeline& operator=(compute_pipeline&& other) noexcept
		{
			if (this != &other)
			{
				destroy();
				m_device_owner = std::move(other.m_device_owner);
				m_device = std::exchange(other.m_device, nullptr);
				m_handle = std::exchange(other.m_handle, nullptr);
				m_resources = std::exchange(other.m_resources, shader_resource_layout{});
				m_threadgroup_size = std::exchange(other.m_threadgroup_size, glm::uvec3{0, 0, 0});
			}
			return *this;
		}

		[[nodiscard]] SDL_GPUComputePipeline* raw() const noexcept { return m_handle; }
		[[nodiscard]] SDL_GPUDevice* device_raw() const noexcept { return m_device; }
		[[nodiscard]] const shader_resource_layout& resources() const noexcept { return m_resources; }
		[[nodiscard]] glm::uvec3 threadgroup_size() const noexcept { return m_threadgroup_size; }
		[[nodiscard]] bool valid() const noexcept { return m_handle != nullptr; }
		explicit operator bool() const noexcept { return valid(); }

		void destroy() noexcept
		{
			if (m_device && m_handle)
			{
				SDL_ReleaseGPUComputePipeline(m_device, m_handle);
			}
			m_device = nullptr;
			m_handle = nullptr;
			m_device_owner.reset();
			m_resources = {};
			m_threadgroup_size = {0, 0, 0};
		}

	private:
		friend class compute_pipeline_builder;

		compute_pipeline(SDL_GPUDevice* device, SDL_GPUComputePipeline* handle,
		                 shader_resource_layout resources, glm::uvec3 threadgroup_size) noexcept
		    : m_device(device), m_handle(handle), m_resources(resources), m_threadgroup_size(threadgroup_size)
		{
		}

		std::optional<device> m_device_owner;
		SDL_GPUDevice* m_device = nullptr;
		SDL_GPUComputePipeline* m_handle = nullptr;
		shader_resource_layout m_resources{};
		glm::uvec3 m_threadgroup_size{0, 0, 0};
	};

	class compute_pipeline_builder
	{
	public:
		compute_pipeline_builder& set_compute_shader(const compute_shader_asset& shader) noexcept
		{
			m_shader = &shader;
			return *this;
		}

		compute_pipeline build(SDL_GPUDevice* device) const
		{
			if (!device)
			{
				throw std::invalid_argument("Cannot create a compute pipeline with a null device");
			}
			if (!m_shader || !m_shader->valid())
			{
				throw std::invalid_argument(
				    "Cannot create a compute pipeline without a valid compute shader asset");
			}

			const auto format = static_cast<SDL_GPUShaderFormat>(m_shader->code.format);
			if ((SDL_GetGPUShaderFormats(device) & format) == 0)
			{
				throw std::invalid_argument("GPU device does not support the compute shader asset's format");
			}

			SDL_GPUComputePipelineCreateInfo info{};
			info.code_size = m_shader->code.bytes.size();
			info.code = m_shader->code.bytes.data();
			info.entrypoint = m_shader->entrypoint.c_str();
			info.format = format;
			info.num_samplers = m_shader->resources.samplers;
			info.num_readonly_storage_textures = m_shader->resources.storage_textures;
			info.num_readonly_storage_buffers = m_shader->resources.storage_buffers;
			info.num_readwrite_storage_textures = m_shader->resources.read_write_storage_textures;
			info.num_readwrite_storage_buffers = m_shader->resources.read_write_storage_buffers;
			info.num_uniform_buffers = m_shader->resources.uniform_buffers;
			info.threadcount_x = m_shader->threadgroup_size.x;
			info.threadcount_y = m_shader->threadgroup_size.y;
			info.threadcount_z = m_shader->threadgroup_size.z;

			auto* handle = SDL_CreateGPUComputePipeline(device, &info);
			if (!handle)
			{
				throw sdl_error("Failed to create GPU compute pipeline: ");
			}

			return compute_pipeline{device, handle, m_shader->resources, m_shader->threadgroup_size};
		}

		compute_pipeline build(const mngl::gpu::device& device) const
		{
			auto result = build(device.raw());
			result.m_device_owner = device;
			return result;
		}

	private:
		const compute_shader_asset* m_shader = nullptr;
	};
} // namespace mngl::gpu
