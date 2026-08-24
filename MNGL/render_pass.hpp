#pragma once

#include "utilities.hpp"
#include "color.hpp"
#include "command_buffer.hpp"
#include "gpu_texture.hpp"
#include "graphics_pipeline.hpp"

#include <span>
#include <vector>
#include <stdexcept>

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
			uint8_t stencil = 0;
			loader load = loader::clear;
			storer store = storer::store;
		};

		render_pass() noexcept = default;

		// Быстрый конструктор для 1 таргета
		render_pass(
			command_buffer& cmd,
			texture& target,
			const color& col = color(0),
			loader ld = loader::clear,
			storer st = storer::store,
			const depth_attachment* depth = nullptr
		) : render_pass(cmd, std::array{color_attachment{ target, col, ld, st }}, depth)
		{ }

		// Универсальный конструктор через std::span (принимает и { a, b }, и std::vector, и std::array)
		render_pass(
			command_buffer& cmd,
			std::span<const color_attachment> color_targets,
			const depth_attachment* depth = nullptr
		)
		{
			if (color_targets.empty())
			{
				throw std::invalid_argument("Render pass must have at least one color target!");
			}

			std::vector<SDL_GPUColorTargetInfo> sdl_color_targets;
			sdl_color_targets.reserve(color_targets.size());

			for (const auto& attachment : color_targets)
			{
				if (!attachment.target.has_usage(texture::usage_flags::color_target))
				{
					throw std::invalid_argument("Target texture must have color_target usage flag!");
				}

				SDL_GPUColorTargetInfo info{};
				info.texture = attachment.target.raw();
				info.clear_color = SDL_FColor{attachment.col.r, attachment.col.g, attachment.col.b, attachment.col.a};
				info.load_op = static_cast<SDL_GPULoadOp>(attachment.load);
				info.store_op = static_cast<SDL_GPUStoreOp>(attachment.store);

				sdl_color_targets.push_back(info);
			}

			SDL_GPUDepthStencilTargetInfo sdl_depth_info{};
			SDL_GPUDepthStencilTargetInfo* depth_ptr = nullptr;

			if (depth)
			{
				// Исправлено со stencil_target на depth_stencil_target и корректный текст ошибки
				if (!depth->target.has_usage(texture::usage_flags::stencil_target))
				{
					throw std::invalid_argument("Depth target texture must have depth_stencil_target usage flag!");
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

			handle = SDL_BeginGPURenderPass(
				cmd.raw(),
				sdl_color_targets.data(),
				static_cast<uint32_t>(sdl_color_targets.size()),
				depth_ptr
			);

			if (!handle)
			{
				throw sdl_error("Failed to begin GPU render pass: ");
			}
		}

		~render_pass()
		{
			end_pass();
		}

		render_pass(const render_pass&) = delete;
		render_pass& operator=(const render_pass&) = delete;

		render_pass(render_pass&& o) noexcept
		{
			swap(*this, o);
		}

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
		}

		// =========================================================================
		// МЕТОДЫ ОТРИСОВКИ И СВЯЗЫВАНИЯ (binding / drawing commands)
		// =========================================================================

		void bind_pipeline(const graphics_pipeline& pipeline) const noexcept
		{
			SDL_BindGPUGraphicsPipeline(handle, pipeline.handle());
		}

		void draw_primitives(
			uint32_t num_vertices,
			uint32_t num_instances = 1,
			uint32_t first_vertex = 0,
			uint32_t first_instance = 0
		) const noexcept
		{
			SDL_DrawGPUPrimitives(handle, num_vertices, num_instances, first_vertex, first_instance);
		}

		void draw_indexed_primitives(
			uint32_t num_indices,
			uint32_t num_instances = 1,
			uint32_t first_index = 0,
			int32_t vertex_offset = 0,
			uint32_t first_instance = 0
		) const noexcept
		{
			SDL_DrawGPUIndexedPrimitives(handle, num_indices, num_instances, first_index, vertex_offset, first_instance);
		}

		template<typename T>
		void push_vertex_uniform(uint32_t slot, const T& data) const noexcept
		{
			static_assert(std::is_trivially_copyable_v<T>, "Uniform data must be trivially copyable!");
			SDL_PushGPUVertexUniformData(handle, slot, &data, static_cast<uint32_t>(sizeof(T)));
		}

		template<typename T>
		void push_fragment_uniform(uint32_t slot, const T& data) const noexcept
		{
			static_assert(std::is_trivially_copyable_v<T>, "Uniform data must be trivially copyable!");
			SDL_PushGPUFragmentUniformData(handle, slot, &data, static_cast<uint32_t>(sizeof(T)));
		}

		void bind_vertex_buffers(
			uint32_t first_slot,
			std::span<const SDL_GPUBufferBinding> bindings
		) const noexcept
		{
			SDL_BindGPUVertexBuffers(handle, first_slot, bindings.data(), static_cast<uint32_t>(bindings.size()));
		}

		void bind_vertex_buffer(uint32_t slot, SDL_GPUBuffer* buffer, uint32_t offset = 0) const noexcept
		{
			const SDL_GPUBufferBinding binding{.buffer = buffer, .offset = offset};
			SDL_BindGPUVertexBuffers(handle, slot, &binding, 1);
		}

		// Связывание индексного буфера (16-bit или 32-bit)
		void bind_index_buffer(
			SDL_GPUBuffer* buffer,
			SDL_GPUIndexElementSize index_element_size = SDL_GPU_INDEXELEMENTSIZE_16BIT,
			uint32_t offset = 0
		) const noexcept
		{
			const SDL_GPUBufferBinding binding{.buffer = buffer, .offset = offset};
			SDL_BindGPUIndexBuffer(handle, &binding, index_element_size);
		}

		void bind_fragment_samplers(
			uint32_t first_slot,
			std::span<const SDL_GPUTextureSamplerBinding> bindings
		) const noexcept
		{
			SDL_BindGPUFragmentSamplers(handle, first_slot, bindings.data(), static_cast<uint32_t>(bindings.size()));
		}

		void bind_fragment_sampler(uint32_t slot, SDL_GPUTexture* texture, SDL_GPUSampler* sampler) const noexcept
		{
			const SDL_GPUTextureSamplerBinding binding{.texture = texture, .sampler = sampler};
			SDL_BindGPUFragmentSamplers(handle, slot, &binding, 1);
		}

	private:
		SDL_GPURenderPass* handle = nullptr;

		friend inline void swap(render_pass& l, render_pass& r) noexcept
		{
			using std::swap;
			swap(l.handle, r.handle);
		}
	};
}