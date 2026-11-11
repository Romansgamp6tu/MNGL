#pragma once

#include <cstdint>

#include "runtime.hpp"
#include "color.hpp"
#include "command_buffer.hpp"
#include "gpu_buffer.hpp"
#include "gpu_texture.hpp"
#include "graphics_pipeline.hpp"

#include <array>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace mngl::gpu
{
	class render_pass
	{
	public:
		enum class loader
		{
			clear = SDL_GPU_LOADOP_CLEAR,
			load = SDL_GPU_LOADOP_LOAD,
			dont_care = SDL_GPU_LOADOP_DONT_CARE
		};

		enum class storer
		{
			store = SDL_GPU_STOREOP_STORE,
			dont_care = SDL_GPU_STOREOP_DONT_CARE
		};

		struct color_attachment
		{
			texture& target;
			color col = color(0);
			loader load = loader::clear;
			storer store = storer::store;
		};

		struct depth_attachment
		{
			texture& target;
			float depth = 1.0f;
			std::uint8_t stencil = 0;
			loader load = loader::clear;
			storer store = storer::store;
		};

		render_pass() noexcept = default;

		// Быстрый конструктор для 1 таргета
		render_pass(command_buffer& cmd, texture& target, const color& col = color(0),
		            loader ld = loader::clear, storer st = storer::store,
		            const depth_attachment* depth = nullptr)
		    : render_pass(cmd, std::array{color_attachment{target, col, ld, st}}, depth)
		{
		}

		// Универсальный конструктор через std::span (принимает и { a, b }, и std::vector, и std::array)
		render_pass(command_buffer& cmd, std::span<const color_attachment> color_targets,
		            const depth_attachment* depth = nullptr)
		{
			if (!cmd.valid()) [[unlikely]]
			{
				throw std::invalid_argument("Render pass requires a valid command buffer");
			}
			if (color_targets.empty() && !depth) [[unlikely]]
			{
				throw std::invalid_argument("Render pass requires at least one color or depth target");
			}
			if (color_targets.size() > 4) [[unlikely]]
			{
				throw std::invalid_argument("Render pass supports at most four color targets");
			}

			std::array<SDL_GPUColorTargetInfo, 4> sdl_color_targets{};
			std::uint32_t color_target_count = 0;
			const auto* command_device = cmd.device_raw();

			for (const auto& attachment : color_targets)
			{
				if (!attachment.target.has_usage(texture::usage_flags::color_target)) [[unlikely]]
				{
					throw std::invalid_argument("Target texture must have color_target usage flag!");
				}
				if (command_device && attachment.target.device_raw() != command_device) [[unlikely]]
				{
					throw std::invalid_argument("Render target belongs to a different GPU device");
				}

				SDL_GPUColorTargetInfo info{};
				info.texture = attachment.target.raw();
				info.clear_color =
				    SDL_FColor{attachment.col.r, attachment.col.g, attachment.col.b, attachment.col.a};
				info.load_op = static_cast<SDL_GPULoadOp>(attachment.load);
				info.store_op = static_cast<SDL_GPUStoreOp>(attachment.store);

				sdl_color_targets[color_target_count++] = info;
			}

			SDL_GPUDepthStencilTargetInfo sdl_depth_info{};
			SDL_GPUDepthStencilTargetInfo* depth_ptr = nullptr;

			if (depth)
			{
				// Исправлено со stencil_target на depth_stencil_target и корректный текст ошибки
				if (!depth->target.has_usage(texture::usage_flags::stencil_target)) [[unlikely]]
				{
					throw std::invalid_argument(
					    "Depth target texture must have depth_stencil_target usage flag!");
				}
				if (command_device && depth->target.device_raw() != command_device) [[unlikely]]
				{
					throw std::invalid_argument("Depth target belongs to a different GPU device");
				}

				sdl_depth_info.texture = depth->target.raw();
				sdl_depth_info.clear_depth = depth->depth;
				sdl_depth_info.clear_stencil = depth->stencil;
				sdl_depth_info.load_op = static_cast<SDL_GPULoadOp>(depth->load);
				sdl_depth_info.store_op = static_cast<SDL_GPUStoreOp>(depth->store);
				sdl_depth_info.stencil_load_op = static_cast<SDL_GPULoadOp>(depth->load);
				sdl_depth_info.stencil_store_op = static_cast<SDL_GPUStoreOp>(depth->store);

				depth_ptr = &sdl_depth_info;
			}

			const auto* color_target_data = color_target_count ? sdl_color_targets.data() : nullptr;
			m_commands = cmd.raw();
			handle = SDL_BeginGPURenderPass(m_commands, color_target_data, color_target_count, depth_ptr);

			if (!handle) [[unlikely]]
			{
				throw sdl_error("Failed to begin GPU render pass: ");
			}
		}

		~render_pass() { end_pass(); }

		render_pass(const render_pass&) = delete;
		render_pass& operator=(const render_pass&) = delete;

		render_pass(render_pass&& o) noexcept { swap(*this, o); }

		render_pass& operator=(render_pass&& o) noexcept
		{
			if (this != &o)
			{
				end_pass(); // Завершаем текущий проход перед принятием нового
				swap(*this, o);
			}
			return *this;
		}

		[[nodiscard]] SDL_GPURenderPass* raw() const noexcept { return handle; }
		[[nodiscard]] bool valid() const noexcept { return handle != nullptr; }
		explicit operator bool() const noexcept { return valid(); }

		void end_pass() noexcept
		{
			if (handle)
			{
				SDL_EndGPURenderPass(handle);
				handle = nullptr;
			}
			m_commands = nullptr;
		}

		// =========================================================================
		// МЕТОДЫ ОТРИСОВКИ И СВЯЗЫВАНИЯ (binding / drawing commands)
		// =========================================================================

		void bind_pipeline(const graphics_pipeline& pipeline) const noexcept
		{
			SDL_BindGPUGraphicsPipeline(handle, pipeline.raw());
		}

		void draw_primitives(std::uint32_t num_vertices, std::uint32_t num_instances = 1,
		                     std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0) const noexcept
		{
			SDL_DrawGPUPrimitives(handle, num_vertices, num_instances, first_vertex, first_instance);
		}

		void draw_indexed_primitives(std::uint32_t num_indices, std::uint32_t num_instances = 1,
		                             std::uint32_t first_index = 0, std::int32_t vertex_offset = 0,
		                             std::uint32_t first_instance = 0) const noexcept
		{
			SDL_DrawGPUIndexedPrimitives(handle, num_indices, num_instances, first_index, vertex_offset,
			                             first_instance);
		}

		template <typename T> void push_vertex_uniform(std::uint32_t slot, const T& data) const
		{
			static_assert(std::is_trivially_copyable_v<T>, "Uniform data must be trivially copyable!");
			ensure_active();
			SDL_PushGPUVertexUniformData(m_commands, slot, &data, static_cast<std::uint32_t>(sizeof(T)));
		}

		template <typename T> void push_fragment_uniform(std::uint32_t slot, const T& data) const
		{
			static_assert(std::is_trivially_copyable_v<T>, "Uniform data must be trivially copyable!");
			ensure_active();
			SDL_PushGPUFragmentUniformData(m_commands, slot, &data, static_cast<std::uint32_t>(sizeof(T)));
		}

		void bind_vertex_buffers(std::uint32_t first_slot,
		                         std::span<const SDL_GPUBufferBinding> bindings) const noexcept
		{
			SDL_BindGPUVertexBuffers(handle, first_slot, bindings.data(),
			                         static_cast<std::uint32_t>(bindings.size()));
		}

		void bind_vertex_buffer(std::uint32_t slot, SDL_GPUBuffer* buffer,
		                        std::uint32_t offset = 0) const noexcept
		{
			const SDL_GPUBufferBinding binding{.buffer = buffer, .offset = offset};
			SDL_BindGPUVertexBuffers(handle, slot, &binding, 1);
		}

		void bind_vertex_buffer(std::uint32_t slot, const buffer& buffer, std::uint32_t offset = 0) const
		{
			if (!buffer.valid() || !has_usage(buffer.usage(), buffer_usage::vertex)) [[unlikely]]
			{
				throw std::invalid_argument("Vertex binding requires a valid vertex-usage GPU buffer");
			}
			bind_vertex_buffer(slot, buffer.raw(), offset);
		}

		template <typename T>
		void bind_vertex_buffer(std::uint32_t slot, const vertex_buffer<T>& buffer,
		                        std::uint32_t offset = 0) const
		{
			bind_vertex_buffer(slot, buffer.gpu_buffer(), offset);
		}

		// Связывание индексного буфера (16-bit или 32-bit)
		void bind_index_buffer(SDL_GPUBuffer* buffer,
		                       SDL_GPUIndexElementSize index_element_size = SDL_GPU_INDEXELEMENTSIZE_16BIT,
		                       std::uint32_t offset = 0) const noexcept
		{
			const SDL_GPUBufferBinding binding{.buffer = buffer, .offset = offset};
			SDL_BindGPUIndexBuffer(handle, &binding, index_element_size);
		}

		void bind_fragment_samplers(std::uint32_t first_slot,
		                            std::span<const SDL_GPUTextureSamplerBinding> bindings) const noexcept
		{
			SDL_BindGPUFragmentSamplers(handle, first_slot, bindings.data(),
			                            static_cast<std::uint32_t>(bindings.size()));
		}

		void bind_fragment_sampler(std::uint32_t slot, SDL_GPUTexture* texture,
		                           SDL_GPUSampler* sampler) const noexcept
		{
			const SDL_GPUTextureSamplerBinding binding{.texture = texture, .sampler = sampler};
			SDL_BindGPUFragmentSamplers(handle, slot, &binding, 1);
		}

	private:
		void ensure_active() const
		{
			if (!handle || !m_commands) [[unlikely]]
			{
				throw std::logic_error("Cannot push uniform data without an active render pass");
			}
		}

		SDL_GPURenderPass* handle = nullptr;
		SDL_GPUCommandBuffer* m_commands = nullptr;

		friend inline void swap(render_pass& l, render_pass& r) noexcept
		{
			using std::swap;
			swap(l.handle, r.handle);
			swap(l.m_commands, r.m_commands);
		}
	};
} // namespace mngl::gpu
