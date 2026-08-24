#pragma once

#include "utilities.hpp"
#include "device.hpp"
#include "command_buffer.hpp"

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
			, m_size(std::exchange(other.m_size, 0))
			, m_usage(other.m_usage)
		{ }

		buffer& operator=(buffer&& other) noexcept
		{
			if (this != &other)
			{
				destroy();
				m_device = std::exchange(other.m_device, nullptr);
				m_handle = std::exchange(other.m_handle, nullptr);
				m_size = std::exchange(other.m_size, 0);
				m_usage = other.m_usage;
			}
			return *this;
		}

		[[nodiscard]] SDL_GPUBuffer* raw() const noexcept { return m_handle; }
		[[nodiscard]] uint32_t size() const noexcept { return m_size; }
		[[nodiscard]] buffer_usage usage() const noexcept { return m_usage; }
		[[nodiscard]] bool valid() const noexcept { return m_handle != nullptr; }
		explicit operator bool() const noexcept { return valid(); }

		void destroy() noexcept
		{
			if (m_device && m_handle)
			{
				SDL_ReleaseGPUBuffer(m_device, m_handle);
				m_handle = nullptr;
				m_size = 0;
			}
		}

		template<typename T>
		void upload(SDL_GPUDevice* device, command_buffer& cmd, std::span<const T> data)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Data must be trivially copyable!");

			const uint32_t upload_size = static_cast<uint32_t>(data.size_bytes());
			if (upload_size == 0) return;

			// Пересоздаем буфер, если текущий размер слишком мал
			if (upload_size > m_size || !m_handle)
			{
				destroy();
				m_device = device;
				m_size = upload_size;

				SDL_GPUBufferCreateInfo info{};
				info.usage = static_cast<SDL_GPUBufferUsageFlags>(m_usage);
				info.size = m_size;

				m_handle = SDL_CreateGPUBuffer(m_device, &info);
				if (!m_handle)
				{
					throw sdl_error("Failed to allocate GPU buffer: ");
				}
			}

			// 1. Создаем временный Transfer Buffer в RAM
			SDL_GPUTransferBufferCreateInfo transfer_info{};
			transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			transfer_info.size = upload_size;

			SDL_GPUTransferBuffer* transfer_buf = SDL_CreateGPUTransferBuffer(m_device, &transfer_info);
			if (!transfer_buf)
			{
				throw sdl_error("Failed to create GPU transfer buffer: ");
			}

			// 2. Копируем данные из вектора/массива C++ в трансферный буфер
			void* mapped = SDL_MapGPUTransferBuffer(m_device, transfer_buf, false);
			if (!mapped)
			{
				SDL_ReleaseGPUTransferBuffer(m_device, transfer_buf);
				throw sdl_error("Failed to map GPU transfer buffer: ");
			}

			std::memcpy(mapped, data.data(), upload_size);
			SDL_UnmapGPUTransferBuffer(m_device, transfer_buf);

			// 3. Записываем команду копирования через Copy Pass в command_buffer
			SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd.raw());
			if (!copy_pass)
			{
				SDL_ReleaseGPUTransferBuffer(m_device, transfer_buf);
				throw sdl_error("Failed to begin GPU copy pass: ");
			}

			SDL_GPUTransferBufferLocation src_location{
				.transfer_buffer = transfer_buf,
				.offset = 0
			};

			SDL_GPUBufferRegion dst_region{
				.buffer = m_handle,
				.offset = 0,
				.size = upload_size
			};

			SDL_UploadToGPUBuffer(copy_pass, &src_location, &dst_region, false);
			SDL_EndGPUCopyPass(copy_pass);

			// 4. Освобождаем трансферный буфер (SDL3 сделает это безопасно после завершения передачи)
			SDL_ReleaseGPUTransferBuffer(m_device, transfer_buf);
		}

		template<typename T>
		static buffer create_with_data(
			SDL_GPUDevice* device,
			command_buffer& cmd,
			std::span<const T> data,
			buffer_usage usage
		)
		{
			buffer buf(device, static_cast<uint32_t>(data.size_bytes()), usage);
			buf.upload(device, cmd, data);
			return buf;
		}

	private:
		SDL_GPUDevice* m_device = nullptr;
		SDL_GPUBuffer* m_handle = nullptr;
		uint32_t m_size = 0;
		buffer_usage m_usage = buffer_usage::vertex;
	};
}