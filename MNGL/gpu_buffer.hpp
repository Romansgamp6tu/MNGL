#pragma once

#include <cstdint>

#include "runtime.hpp"
#include "device.hpp"
#include "command_buffer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace mngl::two_d
{
	class mesh;
}

namespace mngl::gpu
{
	class batcher2d;

	enum class buffer_usage : std::uint8_t
	{
		vertex = SDL_GPU_BUFFERUSAGE_VERTEX,
		index = SDL_GPU_BUFFERUSAGE_INDEX,
		indirect = SDL_GPU_BUFFERUSAGE_INDIRECT,
		graphics_storage_read = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
		compute_storage_read = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
		compute_storage_write = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
	};

	[[nodiscard]] constexpr buffer_usage operator|(buffer_usage left, buffer_usage right) noexcept
	{
		return static_cast<buffer_usage>(static_cast<std::uint32_t>(left) |
		                                 static_cast<std::uint32_t>(right));
	}

	[[nodiscard]] constexpr bool has_usage(buffer_usage value, buffer_usage usage) noexcept
	{
		const auto flags = static_cast<std::uint32_t>(value);
		const auto requested = static_cast<std::uint32_t>(usage);
		return (flags & requested) == requested;
	}

	namespace detail
	{
		class gpu_copy_pass
		{
		public:
			explicit gpu_copy_pass(command_buffer& command)
			{
				command.begin_pass();
				m_command = &command;
				auto* command_handle = command.raw();
				if (!command_handle) [[unlikely]]
				{
					throw std::invalid_argument("Cannot begin a GPU copy pass with an invalid command buffer");
				}
				m_device = command.device_raw();
				m_handle = SDL_BeginGPUCopyPass(command_handle);
				if (!m_handle) [[unlikely]]
				{
					command.end_pass();
					m_command = nullptr;
					throw sdl_error("Failed to begin GPU copy pass: ");
				}
			}

			gpu_copy_pass(const gpu_copy_pass&) = delete;
			gpu_copy_pass& operator=(const gpu_copy_pass&) = delete;
			~gpu_copy_pass() { end_pass(); }

			[[nodiscard]] SDL_GPUCopyPass* raw() const noexcept { return m_handle; }
			[[nodiscard]] SDL_GPUDevice* device_raw() const noexcept { return m_device; }
			[[nodiscard]] bool valid() const noexcept { return m_handle != nullptr; }

			void end_pass() noexcept
			{
				if (m_handle)
				{
					SDL_EndGPUCopyPass(m_handle);
					m_handle = nullptr;
				}
				if (m_command)
				{
					m_command->end_pass();
					m_command = nullptr;
				}
			}

		private:
			SDL_GPUDevice* m_device = nullptr;
			SDL_GPUCopyPass* m_handle = nullptr;
			command_buffer* m_command = nullptr;
		};
	} // namespace detail

	class buffer
	{
	public:
		buffer() noexcept = default;

		buffer(SDL_GPUDevice* dev, std::uint32_t size, buffer_usage usage)
		    : m_device(dev), m_size(size), m_usage(usage)
		{
			if (!m_size)
				return;
			if (!m_device) [[unlikely]]
			{
				throw std::invalid_argument("Cannot create a GPU buffer with a null device");
			}

			SDL_GPUBufferCreateInfo info{};
			info.usage = static_cast<SDL_GPUBufferUsageFlags>(m_usage);
			info.size = m_size;

			SDL_ClearError();
			m_handle = SDL_CreateGPUBuffer(m_device, &info);
			if (!m_handle) [[unlikely]]
			{
				const char* driver = SDL_GetGPUDeviceDriver(m_device);
				throw sdl_error(std::format("Failed to create GPU buffer on driver '{}': size={} bytes, usage=0x{:X}",
				                            driver ? driver : "<unknown>", m_size,
				                            static_cast<std::uint32_t>(m_usage)));
			}
		}

		buffer(const device& dev, std::uint32_t size, buffer_usage usage) : buffer(dev.raw(), size, usage)
		{
			m_device_owner = dev;
		}

		~buffer() { destroy(); }

		buffer(const buffer&) = delete;
		buffer& operator=(const buffer&) = delete;

		buffer(buffer&& other) noexcept
		    : m_device_owner(std::move(other.m_device_owner)),
		      m_device(std::exchange(other.m_device, nullptr)),
		      m_handle(std::exchange(other.m_handle, nullptr)),
		      m_transfer_buffer(std::exchange(other.m_transfer_buffer, nullptr)),
		      m_size(std::exchange(other.m_size, 0)),
		      m_transfer_capacity(std::exchange(other.m_transfer_capacity, 0)), m_usage(other.m_usage)
		{
		}

		buffer& operator=(buffer&& other) noexcept
		{
			if (this != &other)
			{
				destroy();
				m_device_owner = std::move(other.m_device_owner);
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
		[[nodiscard]] std::uint32_t size() const noexcept { return m_size; }
		[[nodiscard]] buffer_usage usage() const noexcept { return m_usage; }
		[[nodiscard]] bool valid() const noexcept { return m_handle != nullptr; }
		explicit operator bool() const noexcept { return valid(); }

		void destroy() noexcept
		{
			if (m_device && m_handle)
				SDL_ReleaseGPUBuffer(m_device, m_handle);
			m_handle = nullptr;
			release_transfer_buffer();
			m_device = nullptr;
			m_device_owner.reset();
			m_size = 0;
		}

		template <typename T>
		void upload(const device& dev, command_buffer& cmd, std::span<const T> data, bool cycle = false)
		{
			if (m_device && m_device != dev.raw()) [[unlikely]]
			{
				throw std::invalid_argument("GPU buffer cannot be uploaded using a different device");
			}
			if (m_device_owner && m_device_owner->raw() != dev.raw()) [[unlikely]]
			{
				throw std::invalid_argument("GPU buffer cannot be uploaded using a different device");
			}
			m_device_owner = dev;
			upload(dev.raw(), cmd, data, cycle);
		}

		template <typename T>
		void upload(SDL_GPUDevice* device, command_buffer& cmd, std::span<const T> data, bool cycle = false)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Data must be trivially copyable!");

			const std::uint32_t upload_size = checked_size(data.size_bytes());
			if (upload_size == 0)
				return;
			if (!device) [[unlikely]]
			{
				throw std::invalid_argument("Cannot upload GPU buffer data with a null device");
			}
			if (m_device && m_device != device) [[unlikely]]
			{
				throw std::invalid_argument("GPU buffer cannot be uploaded using a different device");
			}
			if (!cmd.valid()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot upload GPU buffer data with an invalid command buffer");
			}
			const auto* command_device = cmd.device_raw();
			if (command_device && command_device != device) [[unlikely]]
			{
				throw std::invalid_argument("GPU buffer and command buffer must use the same device");
			}
			const std::uint32_t staged_size = stage_upload(device, std::as_bytes(data));
			if (staged_size == 0)
			{
				return;
			}
			detail::gpu_copy_pass copy_pass(cmd);
			record_upload(copy_pass, staged_size, cycle);
		}

		template <typename T>
		void upload_range(const device& dev, command_buffer& cmd, std::size_t first_element,
		                  std::span<const T> data)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Data must be trivially copyable!");
			if (data.empty())
			{
				return;
			}
			if (!dev.raw()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot update a GPU buffer with a null device");
			}
			if (m_device && m_device != dev.raw()) [[unlikely]]
			{
				throw std::invalid_argument("GPU buffer cannot be updated using a different device");
			}
			if (!m_handle) [[unlikely]]
			{
				throw std::logic_error("Cannot update a GPU buffer before it has been allocated");
			}
			if (!cmd.valid()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot update a GPU buffer with an invalid command buffer");
			}
			const auto* command_device = cmd.device_raw();
			if (command_device && command_device != dev.raw()) [[unlikely]]
			{
				throw std::invalid_argument("GPU buffer and command buffer must use the same device");
			}
			if (first_element > std::numeric_limits<std::uint32_t>::max() / sizeof(T)) [[unlikely]]
			{
				throw std::length_error("GPU buffer update offset exceeds SDL's 32-bit limit");
			}
			const auto destination_offset = static_cast<std::uint32_t>(first_element * sizeof(T));
			const auto upload_size = checked_size(data.size_bytes());
			if (destination_offset > m_size || upload_size > m_size - destination_offset) [[unlikely]]
			{
				throw std::out_of_range("GPU buffer update range exceeds the allocated buffer size");
			}
			if (m_device_owner && m_device_owner->raw() != dev.raw()) [[unlikely]]
			{
				throw std::invalid_argument("GPU buffer cannot be updated using a different device");
			}
			m_device_owner = dev;
			const auto staged_size = stage_upload(dev.raw(), std::as_bytes(data));
			if (staged_size == 0)
			{
				return;
			}
			detail::gpu_copy_pass copy_pass(cmd);
			// A partial update cannot cycle the destination: SDL may give the command a
			// fresh buffer whose untouched bytes have never been initialized. Without
			// cycling, cancellation leaves the previously submitted contents intact.
			record_upload(copy_pass, staged_size, false, destination_offset);
		}

		template <typename T>
		static buffer create_with_data(SDL_GPUDevice* device, command_buffer& cmd, std::span<const T> data,
		                               buffer_usage usage)
		{
			buffer buf(device, checked_size(data.size_bytes()), usage);
			buf.upload(device, cmd, data);
			buf.release_transfer_buffer();
			return buf;
		}

		template <typename T>
		static buffer create_with_data(const device& device, command_buffer& cmd, std::span<const T> data,
		                               buffer_usage usage)
		{
			buffer buf(device, checked_size(data.size_bytes()), usage);
			buf.upload(device, cmd, data);
			buf.release_transfer_buffer();
			return buf;
		}

	private:
		friend class batcher2d;
		friend class ::mngl::two_d::mesh;

		[[nodiscard]] std::uint32_t stage_upload(SDL_GPUDevice* device, std::span<const std::byte> data)
		{
			const std::uint32_t upload_size = checked_size(data.size_bytes());
			if (upload_size == 0)
			{
				return 0;
			}
			if (!device) [[unlikely]]
			{
				throw std::invalid_argument("Cannot upload GPU buffer data with a null device");
			}
			if (m_device && m_device != device) [[unlikely]]
			{
				throw std::invalid_argument("GPU buffer cannot be uploaded using a different device");
			}

			// Grow geometrically so a changing workload does not recreate the GPU buffer every frame.
			if (upload_size > m_size || !m_handle)
			{
				const std::uint32_t capacity = grow_capacity(m_size, upload_size);
				SDL_GPUBufferCreateInfo info{};
				info.usage = static_cast<SDL_GPUBufferUsageFlags>(m_usage);
				info.size = capacity;

				auto* new_handle = SDL_CreateGPUBuffer(device, &info);
				if (!new_handle) [[unlikely]]
				{
					throw sdl_error("Failed to allocate GPU buffer: ");
				}

				if (m_handle)
					SDL_ReleaseGPUBuffer(m_device, m_handle);
				m_device = device;
				m_handle = new_handle;
				m_size = capacity;
			}

			if (!m_transfer_buffer || upload_size > m_transfer_capacity)
			{
				const std::uint32_t capacity = grow_capacity(m_transfer_capacity, upload_size);
				SDL_GPUTransferBufferCreateInfo transfer_info{};
				transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
				transfer_info.size = capacity;

				auto* new_transfer_buffer = SDL_CreateGPUTransferBuffer(m_device, &transfer_info);
				if (!new_transfer_buffer) [[unlikely]]
				{
					throw sdl_error("Failed to create GPU transfer buffer: ");
				}

				release_transfer_buffer();
				m_transfer_buffer = new_transfer_buffer;
				m_transfer_capacity = capacity;
			}

			void* mapped = SDL_MapGPUTransferBuffer(m_device, m_transfer_buffer, true);
			if (!mapped) [[unlikely]]
			{
				throw sdl_error("Failed to map GPU transfer buffer: ");
			}

			std::memcpy(mapped, data.data(), upload_size);
			SDL_UnmapGPUTransferBuffer(m_device, m_transfer_buffer);
			return upload_size;
		}

		void record_upload(detail::gpu_copy_pass& copy_pass, std::uint32_t upload_size, bool cycle,
		                   std::uint32_t destination_offset = 0, std::uint32_t source_offset = 0)
		{
			if (upload_size == 0)
			{
				return;
			}
			if (!copy_pass.valid()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot upload GPU buffer data with an invalid copy pass");
			}
			if (copy_pass.device_raw() && copy_pass.device_raw() != m_device) [[unlikely]]
			{
				throw std::invalid_argument("GPU buffer and copy pass must use the same device");
			}
			if (!m_handle || !m_transfer_buffer || source_offset > m_transfer_capacity ||
			    upload_size > m_transfer_capacity - source_offset || destination_offset > m_size ||
			    upload_size > m_size - destination_offset) [[unlikely]]
			{
				throw std::logic_error("GPU buffer upload was not staged before recording");
			}

			SDL_GPUTransferBufferLocation src_location{.transfer_buffer = m_transfer_buffer, .offset = source_offset};
			SDL_GPUBufferRegion dst_region{.buffer = m_handle, .offset = destination_offset, .size = upload_size};
			SDL_UploadToGPUBuffer(copy_pass.raw(), &src_location, &dst_region, cycle);
		}

		static std::uint32_t grow_capacity(std::uint32_t current, std::uint32_t required)
		{
			const std::uint64_t maximum = std::numeric_limits<std::uint32_t>::max();
			std::uint64_t capacity = current ? current : 1;
			while (capacity < required)
			{
				capacity = std::min(capacity * 2, maximum);
			}
			return static_cast<std::uint32_t>(capacity);
		}

		static std::uint32_t checked_size(std::size_t size)
		{
			if (size > std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
			{
				throw std::length_error("GPU buffer data exceeds SDL's 32-bit size limit");
			}
			return static_cast<std::uint32_t>(size);
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

		std::optional<device> m_device_owner;
		SDL_GPUDevice* m_device = nullptr;
		SDL_GPUBuffer* m_handle = nullptr;
		SDL_GPUTransferBuffer* m_transfer_buffer = nullptr;
		std::uint32_t m_size = 0;
		std::uint32_t m_transfer_capacity = 0;
		buffer_usage m_usage = buffer_usage::vertex;
	};

	template <typename T> class vertex_buffer
	{
	public:
		vertex_buffer() = default;

		template <typename Range>
		    requires std::ranges::input_range<const Range>
		vertex_buffer(const device& device, command_buffer& cmd, const Range& vertices)
		    : m_buffer(device, 0, buffer_usage::vertex)
		{
			upload(device, cmd, vertices);
		}

		vertex_buffer(const vertex_buffer&) = delete;
		vertex_buffer& operator=(const vertex_buffer&) = delete;
		vertex_buffer(vertex_buffer&&) noexcept = default;
		vertex_buffer& operator=(vertex_buffer&&) noexcept = default;

		template <typename Range>
		    requires std::ranges::input_range<const Range>
		void upload(const device& device, command_buffer& cmd, const Range& vertices, bool cycle = false)
		{
			using element_type = std::remove_cv_t<std::ranges::range_value_t<const Range>>;
			static_assert(std::is_same_v<element_type, T>,
			              "Vertex range element type must match vertex_buffer<T>");
			static_assert(std::is_trivially_copyable_v<T>, "Vertex data must be trivially copyable");

			if constexpr (std::ranges::contiguous_range<const Range> && std::ranges::sized_range<const Range>)
			{
				const std::size_t count = std::ranges::size(vertices);
				validate_count(count);
				const std::span<const T> data(std::ranges::data(vertices), count);
				m_buffer.upload(device, cmd, data, cycle);
				m_count = static_cast<std::uint32_t>(count);
			}
			else
			{
				std::vector<T> packed;
				if constexpr (std::ranges::sized_range<const Range>)
				{
					packed.reserve(std::ranges::size(vertices));
				}
				for (const auto& vertex : vertices)
				{
					packed.push_back(vertex);
				}
				validate_count(packed.size());
				m_buffer.upload(device, cmd, std::span<const T>(packed), cycle);
				m_count = static_cast<std::uint32_t>(packed.size());
			}
		}

		[[nodiscard]] const buffer& gpu_buffer() const noexcept { return m_buffer; }
		[[nodiscard]] buffer& gpu_buffer() noexcept { return m_buffer; }
		[[nodiscard]] std::uint32_t size() const noexcept { return m_count; }
		[[nodiscard]] bool empty() const noexcept { return m_count == 0; }

	private:
		static void validate_count(std::size_t count)
		{
			if (count == 0) [[unlikely]]
			{
				throw std::invalid_argument("A vertex buffer cannot be uploaded from an empty range");
			}
			if (count > std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
			{
				throw std::length_error("Vertex count exceeds SDL's 32-bit limit");
			}
		}

		buffer m_buffer;
		std::uint32_t m_count = 0;
	};
} // namespace mngl::gpu
