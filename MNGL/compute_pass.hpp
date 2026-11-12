#pragma once

#include <cstdint>

#include "command_buffer.hpp"
#include "compute_pipeline.hpp"
#include "gpu_buffer.hpp"
#include "gpu_texture.hpp"
#include "sampler.hpp"
#include "texture.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mngl::gpu
{
	class compute_pass
	{
	public:
		struct storage_texture_write_binding
		{
			const texture& target;
			bool cycle = false;

			explicit storage_texture_write_binding(const texture& target_texture,
			                                       bool cycle_binding = false) noexcept
			    : target(target_texture), cycle(cycle_binding)
			{
			}

			explicit storage_texture_write_binding(const ::mngl::texture& target_texture,
			                                       bool cycle_binding = false) noexcept
			    : storage_texture_write_binding(target_texture.raw_gpu_texture(), cycle_binding)
			{
			}
		};

		struct storage_buffer_write_binding
		{
			const buffer& target;
			bool cycle = false;

			explicit storage_buffer_write_binding(const buffer& target_buffer,
			                                      bool cycle_binding = false) noexcept
			    : target(target_buffer), cycle(cycle_binding)
			{
			}
		};

		compute_pass() noexcept = default;

		compute_pass(command_buffer& commands, const compute_pipeline& pipeline,
		             std::span<const storage_texture_write_binding> write_textures = {},
		             std::span<const storage_buffer_write_binding> write_buffers = {})
		{
			begin(commands, pipeline, write_textures, write_buffers);
		}

		compute_pass(command_buffer& commands, const compute_pipeline& pipeline,
		             const compute_shader_asset& shader, const sampler* default_sampler = nullptr)
		{
			if (!shader.valid()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot begin a compute pass with an invalid shader asset");
			}
			if (!pipeline.valid()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot begin a compute pass with an invalid pipeline");
			}
			if (!(shader.resources == pipeline.resources()) ||
			    shader.threadgroup_size.x != pipeline.threadgroup_size().x ||
			    shader.threadgroup_size.y != pipeline.threadgroup_size().y ||
			    shader.threadgroup_size.z != pipeline.threadgroup_size().z) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Compute shader asset layout must match the pipeline used by the pass");
			}

			auto bindings = collect_write_bindings(shader);
			begin(commands, pipeline, bindings.textures, bindings.buffers);
			try
			{
				bind_reflected_resources(shader, default_sampler);
			}
			catch (...)
			{
				end_pass();
				throw;
			}
		}

		~compute_pass() { end_pass(); }

		compute_pass(const compute_pass&) = delete;
		compute_pass& operator=(const compute_pass&) = delete;

		compute_pass(compute_pass&& other) noexcept { swap(*this, other); }

		compute_pass& operator=(compute_pass&& other) noexcept
		{
			if (this != &other)
			{
				end_pass();
				swap(*this, other);
			}
			return *this;
		}

		[[nodiscard]] SDL_GPUComputePass* raw() const noexcept { return m_handle; }
		[[nodiscard]] bool valid() const noexcept { return m_handle != nullptr; }
		explicit operator bool() const noexcept { return valid(); }

		void bind_sampler(std::uint32_t slot, const texture& image, const sampler& state)
		{
			ensure_active();
			require_slot(m_bound_samplers, slot, "sampler");
			require_device(image.device_raw(), "Compute sampler texture");
			require_device(state.device_raw(), "Compute sampler");
			if (!image.valid() || !image.has_usage(texture::usage_flags::sampler)) [[unlikely]]
			{
				throw std::invalid_argument("Compute sampling requires a valid texture with sampler usage");
			}
			if (!state.valid()) [[unlikely]]
			{
				throw std::invalid_argument("Compute sampling requires a valid sampler");
			}

			const SDL_GPUTextureSamplerBinding binding{.texture = image.raw(), .sampler = state.raw()};
			SDL_BindGPUComputeSamplers(m_handle, slot, &binding, 1);
			m_bound_samplers[slot] = true;
		}

		void bind_sampler(std::uint32_t slot, const ::mngl::texture& image, const sampler& state)
		{
			bind_sampler(slot, image.raw_gpu_texture(), state);
		}

		void bind_storage_texture(std::uint32_t slot, const texture& image)
		{
			ensure_active();
			require_slot(m_bound_storage_textures, slot, "read-only storage texture");
			require_device(image.device_raw(), "Compute storage texture");
			if (!image.valid() || !image.has_usage(texture::usage_flags::compute_read)) [[unlikely]]
			{
				throw std::invalid_argument("Read-only compute texture binding requires compute_read usage");
			}

			SDL_GPUTexture* binding = image.raw();
			SDL_BindGPUComputeStorageTextures(m_handle, slot, &binding, 1);
			m_bound_storage_textures[slot] = true;
		}

		void bind_storage_texture(std::uint32_t slot, const ::mngl::texture& image)
		{
			bind_storage_texture(slot, image.raw_gpu_texture());
		}

		void bind_storage_buffer(std::uint32_t slot, const buffer& storage)
		{
			ensure_active();
			require_slot(m_bound_storage_buffers, slot, "read-only storage buffer");
			require_device(storage.device_raw(), "Compute storage buffer");
			if (!storage.valid() ||
			    !has_usage(storage.usage(), buffer_usage::compute_storage_read)) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Read-only compute buffer binding requires compute_storage_read usage");
			}

			SDL_GPUBuffer* binding = storage.raw();
			SDL_BindGPUComputeStorageBuffers(m_handle, slot, &binding, 1);
			m_bound_storage_buffers[slot] = true;
		}

		template <typename T> void push_uniform(std::uint32_t slot, const T& data)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Uniform data must be trivially copyable");
			push_uniform_bytes(slot, std::span{reinterpret_cast<const std::uint8_t*>(&data), sizeof(T)});
		}

		void push_uniform_bytes(std::uint32_t slot, std::span<const std::uint8_t> data)
		{
			ensure_active();
			require_slot(m_bound_uniform_buffers, slot, "uniform buffer");
			if (data.empty()) [[unlikely]]
			{
				throw std::invalid_argument("Compute uniform data cannot be empty");
			}
			if (data.size() > std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
			{
				throw std::length_error("Compute uniform data exceeds SDL's 32-bit size limit");
			}

			SDL_PushGPUComputeUniformData(m_command_buffer, slot, data.data(),
			                              static_cast<std::uint32_t>(data.size()));
			m_bound_uniform_buffers[slot] = true;
		}

		void dispatch(std::uint32_t groups_x, std::uint32_t groups_y = 1, std::uint32_t groups_z = 1)
		{
			ensure_all_resources_bound();
			if (groups_x == 0 || groups_y == 0 || groups_z == 0) [[unlikely]]
			{
				throw std::invalid_argument("Compute dispatch group counts must be non-zero");
			}
			SDL_DispatchGPUCompute(m_handle, groups_x, groups_y, groups_z);
		}

		void dispatch_for(glm::uvec3 thread_count)
		{
			ensure_active();
			if (thread_count.x == 0 || thread_count.y == 0 || thread_count.z == 0) [[unlikely]]
			{
				throw std::invalid_argument("Compute thread counts must be non-zero");
			}

			const auto local_size = m_threadgroup_size;
			dispatch(group_count(thread_count.x, local_size.x), group_count(thread_count.y, local_size.y),
			         group_count(thread_count.z, local_size.z));
		}

		void dispatch_indirect(const buffer& arguments, std::uint32_t offset = 0)
		{
			ensure_all_resources_bound();
			require_device(arguments.device_raw(), "Indirect dispatch buffer");
			if (!arguments.valid() || !has_usage(arguments.usage(), buffer_usage::indirect)) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Indirect compute dispatch requires a valid indirect-usage buffer");
			}
			if (offset % alignof(std::uint32_t) != 0 || offset > arguments.size() ||
			    sizeof(SDL_GPUIndirectDispatchCommand) > arguments.size() - offset) [[unlikely]]
			{
				throw std::out_of_range(
				    "Indirect compute dispatch command is outside the buffer or misaligned");
			}
			SDL_DispatchGPUComputeIndirect(m_handle, arguments.raw(), offset);
		}

		void end_pass() noexcept
		{
			if (m_handle)
			{
				SDL_EndGPUComputePass(m_handle);
				m_handle = nullptr;
			}
			m_command_buffer = nullptr;
			m_device = nullptr;
			m_resources = {};
			m_threadgroup_size = {0, 0, 0};
			m_bound_samplers.clear();
			m_bound_storage_textures.clear();
			m_bound_storage_buffers.clear();
			m_bound_write_storage_textures.clear();
			m_bound_write_storage_buffers.clear();
			m_bound_uniform_buffers.clear();
			m_pass_lease.reset();
		}

	private:
		struct writable_bindings
		{
			std::vector<storage_texture_write_binding> textures;
			std::vector<storage_buffer_write_binding> buffers;
		};

		void begin(command_buffer& commands, const compute_pipeline& pipeline,
		           std::span<const storage_texture_write_binding> write_textures,
		           std::span<const storage_buffer_write_binding> write_buffers)
		{
			if (!commands.valid()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot begin a compute pass with an invalid command buffer");
			}
			if (!pipeline.valid()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot begin a compute pass with an invalid pipeline");
			}
			if (write_textures.size() != pipeline.resources().read_write_storage_textures ||
			    write_buffers.size() != pipeline.resources().read_write_storage_buffers) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Compute pass writable resource counts must match the pipeline layout");
			}
			if (const auto command_device = commands.device_raw();
			    command_device && command_device != pipeline.device_raw()) [[unlikely]]
			{
				throw std::invalid_argument("Compute pipeline and command buffer must use the same device");
			}
			m_device = pipeline.device_raw();

			std::vector<SDL_GPUStorageTextureReadWriteBinding> sdl_write_textures;
			sdl_write_textures.reserve(write_textures.size());
			for (const auto& binding : write_textures)
			{
				require_device(binding.target.device_raw(), "Compute write texture");
				if (!binding.target.valid() ||
				    (!binding.target.has_usage(texture::usage_flags::compute_write) &&
				     !binding.target.has_usage(texture::usage_flags::compute_rw))) [[unlikely]]
				{
					throw std::invalid_argument(
					    "Writable compute texture binding requires compute_write or compute_rw usage");
				}
				sdl_write_textures.push_back(
				    {.texture = binding.target.raw(), .mip_level = 0, .layer = 0, .cycle = binding.cycle});
			}

			std::vector<SDL_GPUStorageBufferReadWriteBinding> sdl_write_buffers;
			sdl_write_buffers.reserve(write_buffers.size());
			for (const auto& binding : write_buffers)
			{
				require_device(binding.target.device_raw(), "Compute write buffer");
				if (!binding.target.valid() ||
				    !has_usage(binding.target.usage(), buffer_usage::compute_storage_write)) [[unlikely]]
				{
					throw std::invalid_argument(
					    "Writable compute buffer binding requires compute_storage_write usage");
				}
				sdl_write_buffers.push_back({.buffer = binding.target.raw(), .cycle = binding.cycle});
			}

			const auto resources = pipeline.resources();
			std::vector<bool> bound_samplers(resources.samplers, false);
			std::vector<bool> bound_storage_textures(resources.storage_textures, false);
			std::vector<bool> bound_storage_buffers(resources.storage_buffers, false);
			std::vector<bool> bound_write_storage_textures(resources.read_write_storage_textures, true);
			std::vector<bool> bound_write_storage_buffers(resources.read_write_storage_buffers, true);
			std::vector<bool> bound_uniform_buffers(resources.uniform_buffers, false);

			auto pass_lease = commands.begin_pass();
			auto* command_handle = commands.raw();
			auto* handle = SDL_BeginGPUComputePass(
			    command_handle, sdl_write_textures.empty() ? nullptr : sdl_write_textures.data(),
			    static_cast<std::uint32_t>(sdl_write_textures.size()),
			    sdl_write_buffers.empty() ? nullptr : sdl_write_buffers.data(),
			    static_cast<std::uint32_t>(sdl_write_buffers.size()));
			if (!handle) [[unlikely]]
			{
				throw sdl_error("Failed to begin GPU compute pass: ");
			}

			m_handle = handle;
			m_command_buffer = command_handle;
			m_pass_lease = std::move(pass_lease);
			m_device = pipeline.device_raw();
			m_resources = resources;
			m_threadgroup_size = pipeline.threadgroup_size();
			m_bound_samplers = std::move(bound_samplers);
			m_bound_storage_textures = std::move(bound_storage_textures);
			m_bound_storage_buffers = std::move(bound_storage_buffers);
			m_bound_write_storage_textures = std::move(bound_write_storage_textures);
			m_bound_write_storage_buffers = std::move(bound_write_storage_buffers);
			m_bound_uniform_buffers = std::move(bound_uniform_buffers);
			SDL_BindGPUComputePipeline(m_handle, pipeline.raw());
		}

		static writable_bindings collect_write_bindings(const compute_shader_asset& shader)
		{
			std::vector<std::pair<std::uint32_t, const texture*>> texture_slots;
			std::vector<std::pair<std::uint32_t, const buffer*>> buffer_slots;
			const auto& values = shader.resource_values();

			for (const auto& field : shader.resource_layout())
			{
				const auto value = values.find(field.name);
				if (value == values.end()) [[unlikely]]
				{
					throw std::invalid_argument("Compute shader resource '" + field.name +
					                            "' has not been assigned");
				}

				switch (field.kind)
				{
				case shader_resource_kind::read_write_storage_texture:
					if (!value->second.texture) [[unlikely]]
					{
						throw std::invalid_argument("Compute write texture resource '" + field.name +
						                            "' has no texture");
					}
					texture_slots.emplace_back(field.slot, &value->second.texture->raw_gpu_texture());
					break;
				case shader_resource_kind::read_write_storage_buffer:
					if (!value->second.storage_buffer) [[unlikely]]
					{
						throw std::invalid_argument("Compute write buffer resource '" + field.name +
						                            "' has no buffer");
					}
					buffer_slots.emplace_back(field.slot, value->second.storage_buffer);
					break;
				default:
					break;
				}
			}

			auto order_by_slot = [](auto& slots)
			{
				std::sort(slots.begin(), slots.end(),
				          [](const auto& left, const auto& right) { return left.first < right.first; });
				for (std::uint32_t slot = 0; slot < slots.size(); ++slot)
				{
					if (slots[slot].first != slot) [[unlikely]]
					{
						throw std::invalid_argument(
						    "Reflected compute write resources must be contiguous from slot zero");
					}
				}
			};
			order_by_slot(texture_slots);
			order_by_slot(buffer_slots);

			if (texture_slots.size() != shader.resources.read_write_storage_textures ||
			    buffer_slots.size() != shader.resources.read_write_storage_buffers) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Compute shader write resource reflection does not match its pipeline layout");
			}

			writable_bindings result{};
			result.textures.reserve(texture_slots.size());
			for (const auto& [slot, target] : texture_slots)
			{
				(void)slot;
				result.textures.emplace_back(*target);
			}
			result.buffers.reserve(buffer_slots.size());
			for (const auto& [slot, target] : buffer_slots)
			{
				(void)slot;
				result.buffers.emplace_back(*target);
			}
			return result;
		}

		void bind_reflected_resources(const compute_shader_asset& shader, const sampler* default_sampler)
		{
			const auto& values = shader.resource_values();
			for (const auto& field : shader.resource_layout())
			{
				const auto value = values.find(field.name);
				if (value == values.end()) [[unlikely]]
				{
					throw std::invalid_argument("Compute shader resource '" + field.name +
					                            "' has not been assigned");
				}

				switch (field.kind)
				{
				case shader_resource_kind::sampled_texture:
				{
					if (!value->second.texture) [[unlikely]]
					{
						throw std::invalid_argument("Compute sampled texture resource '" + field.name +
						                            "' has no texture");
					}
					const auto* state =
					    value->second.sampler_state ? value->second.sampler_state : default_sampler;
					if (!state) [[unlikely]]
					{
						throw std::invalid_argument(std::format(
						    "Compute sampled texture '{}' requires an explicit or default sampler",
						    field.name));
					}
					bind_sampler(field.slot, value->second.texture->raw_gpu_texture(), *state);
					break;
				}
				case shader_resource_kind::storage_texture:
					if (!value->second.texture) [[unlikely]]
					{
						throw std::invalid_argument("Compute storage texture resource '" + field.name +
						                            "' has no texture");
					}
					bind_storage_texture(field.slot, value->second.texture->raw_gpu_texture());
					break;
				case shader_resource_kind::storage_buffer:
					if (!value->second.storage_buffer) [[unlikely]]
					{
						throw std::invalid_argument("Compute storage buffer resource '" + field.name +
						                            "' has no buffer");
					}
					bind_storage_buffer(field.slot, *value->second.storage_buffer);
					break;
				case shader_resource_kind::read_write_storage_texture:
				case shader_resource_kind::read_write_storage_buffer:
					break;
				}
			}

			const auto uniform_blocks = shader.uniform_block_data();
			if (uniform_blocks.size() > m_resources.uniform_buffers) [[unlikely]]
			{
				throw std::invalid_argument("Reflected compute uniform blocks exceed the pipeline layout");
			}
			for (std::uint32_t slot = 0; slot < uniform_blocks.size(); ++slot)
			{
				push_uniform_bytes(slot, std::span<const std::uint8_t>(uniform_blocks[slot]));
			}
		}

		void ensure_active() const
		{
			if (!m_handle || !m_command_buffer) [[unlikely]]
			{
				throw std::logic_error("Compute pass is not active");
			}
		}

		void ensure_all_resources_bound() const
		{
			ensure_active();
			require_all_bound(m_bound_samplers, "sampler");
			require_all_bound(m_bound_storage_textures, "read-only storage texture");
			require_all_bound(m_bound_storage_buffers, "read-only storage buffer");
			require_all_bound(m_bound_write_storage_textures, "read-write storage texture");
			require_all_bound(m_bound_write_storage_buffers, "read-write storage buffer");
			require_all_bound(m_bound_uniform_buffers, "uniform buffer");
		}

		static void require_all_bound(const std::vector<bool>& bindings, std::string_view resource_name)
		{
			for (std::size_t slot = 0; slot < bindings.size(); ++slot)
			{
				if (!bindings[slot]) [[unlikely]]
				{
					throw std::logic_error(
					    std::format("Compute {} slot {} has not been bound", resource_name, slot));
				}
			}
		}

		static void require_slot(const std::vector<bool>& bindings, std::uint32_t slot,
		                         std::string_view resource_name)
		{
			if (slot >= bindings.size()) [[unlikely]]
			{
				throw std::out_of_range(
				    std::format("Compute {} slot {} is outside the pipeline layout", resource_name, slot));
			}
		}

		void require_device(SDL_GPUDevice* resource_device, std::string_view resource_name) const
		{
			if (resource_device != m_device) [[unlikely]]
			{
				throw std::invalid_argument(
				    std::format("{} and compute pipeline must use the same device", resource_name));
			}
		}

		static std::uint32_t group_count(std::uint32_t thread_count, std::uint32_t local_size)
		{
			return 1u + (thread_count - 1u) / local_size;
		}

		friend void swap(compute_pass& left, compute_pass& right) noexcept
		{
			using std::swap;
			swap(left.m_handle, right.m_handle);
			swap(left.m_command_buffer, right.m_command_buffer);
			swap(left.m_device, right.m_device);
			swap(left.m_pass_lease, right.m_pass_lease);
			swap(left.m_resources, right.m_resources);
			swap(left.m_threadgroup_size, right.m_threadgroup_size);
			swap(left.m_bound_samplers, right.m_bound_samplers);
			swap(left.m_bound_storage_textures, right.m_bound_storage_textures);
			swap(left.m_bound_storage_buffers, right.m_bound_storage_buffers);
			swap(left.m_bound_write_storage_textures, right.m_bound_write_storage_textures);
			swap(left.m_bound_write_storage_buffers, right.m_bound_write_storage_buffers);
			swap(left.m_bound_uniform_buffers, right.m_bound_uniform_buffers);
		}

		SDL_GPUComputePass* m_handle = nullptr;
		SDL_GPUCommandBuffer* m_command_buffer = nullptr;
		SDL_GPUDevice* m_device = nullptr;
		command_buffer::pass_lease m_pass_lease;
		shader_resource_layout m_resources{};
		glm::uvec3 m_threadgroup_size{0, 0, 0};
		std::vector<bool> m_bound_samplers;
		std::vector<bool> m_bound_storage_textures;
		std::vector<bool> m_bound_storage_buffers;
		std::vector<bool> m_bound_write_storage_textures;
		std::vector<bool> m_bound_write_storage_buffers;
		std::vector<bool> m_bound_uniform_buffers;
	};
} // namespace mngl::gpu

