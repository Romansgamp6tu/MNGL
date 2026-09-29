#pragma once

#include "utilities.hpp"
#include "device.hpp"
#include "command_buffer.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace mngl::gpu
{
	enum class buffer_usage : uint8_t
	{
		vertex = SDL_GPU_BUFFERUSAGE_VERTEX,
		index = SDL_GPU_BUFFERUSAGE_INDEX,
		indirect = SDL_GPU_BUFFERUSAGE_INDIRECT,
		graphics_storage_read = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
		compute_storage_read = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
		compute_storage_write = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
	};

	class buffer
	{
	public:
		buffer() noexcept = default;
		
		buffer(SDL_GPUDevice* dev, uint32_t size, buffer_usage usage)
			: m_device(dev), m_size(size), m_usage(usage)
		{
			if (!m_size) return;
			if (!m_device)
			{
				throw std::invalid_argument("Cannot create a GPU buffer with a null device");
			}

			SDL_GPUBufferCreateInfo info{};
			info.usage = static_cast<SDL_GPUBufferUsageFlags>(m_usage);
			info.size = m_size;
			
			m_handle = SDL_CreateGPUBuffer(m_device, &info);
			if (!m_handle) throw sdl_error("Failed to create GPU buffer: ");
		}

		~buffer() { destroy(); }

		buffer(const buffer&) = delete;
		buffer& operator=(const buffer&) = delete;

		buffer(buffer&& other) noexcept
			: m_device(std::exchange(other.m_device, nullptr))
			, m_handle(std::exchange(other.m_handle, nullptr))
			, m_transfer_buffer(std::exchange(other.m_transfer_buffer, nullptr))
			, m_size(std::exchange(other.m_size, 0))
			, m_transfer_capacity(std::exchange(other.m_transfer_capacity, 0))
			, m_usage(other.m_usage)
		{ }

		buffer& operator=(buffer&& other) noexcept
		{
			if (this != &other)
			{
				destroy();
				m_device = std::exchange(other.m_device, nullptr);
				m_handle = std::exchange(other.m_handle, nullptr);
				m_transfer_buffer = std::exchange(other.m_transfer_buffer, nullptr);
				m_size = std::exchange(other.m_size, 0);
				m_transfer_capacity = std::exchange(other.m_transfer_capacity, 0);
				m_usage = other.m_usage;
			}
			return *this;
		}

		[[nodiscard]] SDL_GPUBuffer* raw() const noexcept { return m_handle; }
		[[nodiscard]] SDL_GPUDevice* device_raw() const noexcept { return m_device; }
		[[nodiscard]] uint32_t size() const noexcept { return m_size; }
		[[nodiscard]] buffer_usage usage() const noexcept { return m_usage; }
		[[nodiscard]] bool valid() const noexcept { return m_handle != nullptr; }
		explicit operator bool() const noexcept { return valid(); }

		void destroy() noexcept
		{
			if (m_device && m_handle) SDL_ReleaseGPUBuffer(m_device, m_handle);
			m_handle = nullptr;
			release_transfer_buffer();
			m_device = nullptr;
			m_size = 0;
		}

		template<typename T>
		void upload(SDL_GPUDevice* device, command_buffer& cmd, std::span<const T> data, bool cycle = false)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Data must be trivially copyable!");

			const uint32_t upload_size = checked_size(data.size_bytes());
			if (upload_size == 0) return;
			if (!device)
			{
				throw std::invalid_argument("Cannot upload GPU buffer data with a null device");
			}
			if (m_device && m_device != device)
			{
				throw std::invalid_argument("GPU buffer cannot be uploaded using a different device");
			}
			if (!cmd.valid())
			{
				throw std::invalid_argument("Cannot upload GPU buffer data with an invalid command buffer");
			}
			const auto* command_device = cmd.device_raw();
			if (command_device && command_device != device)
			{
				throw std::invalid_argument("GPU buffer and command buffer must use the same device");
			}

			// Grow geometrically so a changing workload does not recreate the GPU buffer every frame.
			if (upload_size > m_size || !m_handle)
			{
				const uint32_t capacity = grow_capacity(m_size, upload_size);
				SDL_GPUBufferCreateInfo info{};
				info.usage = static_cast<SDL_GPUBufferUsageFlags>(m_usage);
				info.size = capacity;

				auto* new_handle = SDL_CreateGPUBuffer(device, &info);
				if (!new_handle)
				{
					throw sdl_error("Failed to allocate GPU buffer: ");
				}

				if (m_handle) SDL_ReleaseGPUBuffer(m_device, m_handle);
				m_device = device;
				m_handle = new_handle;
				m_size = capacity;
			}

			if (!m_transfer_buffer || upload_size > m_transfer_capacity)
			{
				const uint32_t capacity = grow_capacity(m_transfer_capacity, upload_size);
				SDL_GPUTransferBufferCreateInfo transfer_info{};
				transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
				transfer_info.size = capacity;

				auto* new_transfer_buffer = SDL_CreateGPUTransferBuffer(m_device, &transfer_info);
				if (!new_transfer_buffer)
				{
					throw sdl_error("Failed to create GPU transfer buffer: ");
				}

				release_transfer_buffer();
				m_transfer_buffer = new_transfer_buffer;
				m_transfer_capacity = capacity;
			}

			void* mapped = SDL_MapGPUTransferBuffer(m_device, m_transfer_buffer, true);
			if (!mapped)
			{
				throw sdl_error("Failed to map GPU transfer buffer: ");
			}

			std::memcpy(mapped, data.data(), upload_size);
			SDL_UnmapGPUTransferBuffer(m_device, m_transfer_buffer);

			SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd.raw());
			if (!copy_pass)
			{
				throw sdl_error("Failed to begin GPU copy pass: ");
			}

			SDL_GPUTransferBufferLocation src_location{
				.transfer_buffer = m_transfer_buffer,
				.offset = 0
			};

			SDL_GPUBufferRegion dst_region{
				.buffer = m_handle,
				.offset = 0,
				.size = upload_size
			};

			SDL_UploadToGPUBuffer(copy_pass, &src_location, &dst_region, cycle);
			SDL_EndGPUCopyPass(copy_pass);
		}

		template<typename T>
		static buffer create_with_data(
			SDL_GPUDevice* device,
			command_buffer& cmd,
			std::span<const T> data,
			buffer_usage usage
		)
		{
			buffer buf(device, checked_size(data.size_bytes()), usage);
			buf.upload(device, cmd, data);
			buf.release_transfer_buffer();
			return buf;
		}

	private:
		static uint32_t grow_capacity(uint32_t current, uint32_t required)
		{
			const uint64_t maximum = std::numeric_limits<uint32_t>::max();
			uint64_t capacity = current ? current : 1;
			while (capacity < required)
			{
				capacity = std::min(capacity * 2, maximum);
			}
			return static_cast<uint32_t>(capacity);
		}

		static uint32_t checked_size(std::size_t size)
		{
			if (size > std::numeric_limits<uint32_t>::max())
			{
				throw std::length_error("GPU buffer data exceeds SDL's 32-bit size limit");
			}
			return static_cast<uint32_t>(size);
		}

		void release_transfer_buffer() noexcept
		{
			if (m_device && m_transfer_buffer)
			{
				SDL_ReleaseGPUTransferBuffer(m_device, m_transfer_buffer);
			}
			m_transfer_buffer = nullptr;
			m_transfer_capacity = 0;
		}

		SDL_GPUDevice* m_device = nullptr;
		SDL_GPUBuffer* m_handle = nullptr;
		SDL_GPUTransferBuffer* m_transfer_buffer = nullptr;
		uint32_t m_size = 0;
		uint32_t m_transfer_capacity = 0;
		buffer_usage m_usage = buffer_usage::vertex;
	};
}