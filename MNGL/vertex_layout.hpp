#pragma once

#include <cstdint>

#include "utilities.hpp"
#include "device.hpp"

#include <limits>
#include <type_traits>

namespace mngl::gpu
{
	enum class vertex_elemetn_fmt : std::uint8_t
	{
		invalid = 0,

		int1 = SDL_GPU_VERTEXELEMENTFORMAT_INT,
		int2 = SDL_GPU_VERTEXELEMENTFORMAT_INT2,
		int3 = SDL_GPU_VERTEXELEMENTFORMAT_INT3,
		int4 = SDL_GPU_VERTEXELEMENTFORMAT_INT4,

		uint1 = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
		uint2 = SDL_GPU_VERTEXELEMENTFORMAT_UINT2,
		uint3 = SDL_GPU_VERTEXELEMENTFORMAT_UINT3,
		uint4 = SDL_GPU_VERTEXELEMENTFORMAT_UINT4,

		float1 = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,
		float2 = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
		float3 = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
		float4 = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,

		byte4_norm = SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM,
		ubyte4_norm = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM
	};

	enum class vertex_input_rate : std::uint8_t
	{
		vertex = SDL_GPU_VERTEXINPUTRATE_VERTEX,
		instance = SDL_GPU_VERTEXINPUTRATE_INSTANCE
	};

	struct vertex_attribute
	{
		std::uint32_t location = 0;
		std::uint32_t buffer_slot = 0;
		vertex_elemetn_fmt format = vertex_elemetn_fmt::invalid;
		std::uint32_t offset = 0;

		SDL_GPUVertexAttribute to_sdl() const noexcept
		{
			SDL_GPUVertexAttribute attr{};
			attr.location = location;
			attr.buffer_slot = buffer_slot;
			attr.format = static_cast<SDL_GPUVertexElementFormat>(format);
			attr.offset = offset;
			return attr;
		}
	};

	struct vertex_buffer_description
	{
		std::uint32_t slot = 0;
		std::uint32_t pitch = 0;
		vertex_input_rate input_rate = vertex_input_rate::vertex;
		std::uint32_t instance_step_rate = 0;

		SDL_GPUVertexBufferDescription to_sdl() const noexcept
		{
			SDL_GPUVertexBufferDescription desc{};
			desc.slot = slot;
			desc.pitch = pitch;
			desc.input_rate = static_cast<SDL_GPUVertexInputRate>(input_rate);
			desc.instance_step_rate = instance_step_rate;
			return desc;
		}
	};

	class vertex_layout
	{
	public:
		vertex_layout() = default;

		vertex_layout& add_buffer(std::uint32_t slot, std::uint32_t pitch,
		                          vertex_input_rate input_rate = vertex_input_rate::vertex,
		                          std::uint32_t instance_step_rate = 0)
		{
			m_buffers.push_back({slot, pitch, input_rate, instance_step_rate});
			return *this;
		}

		template <typename Vertex>
		vertex_layout& add_buffer(std::uint32_t slot,
		                          vertex_input_rate input_rate = vertex_input_rate::vertex,
		                          std::uint32_t instance_step_rate = 0)
		{
			static_assert(std::is_trivially_copyable_v<Vertex>, "Vertex type must be trivially copyable");
			static_assert(sizeof(Vertex) <= std::numeric_limits<std::uint32_t>::max(),
			              "Vertex type is too large");
			return add_buffer(slot, static_cast<std::uint32_t>(sizeof(Vertex)), input_rate,
			                  instance_step_rate);
		}

		vertex_layout& add_attribute(std::uint32_t location, std::uint32_t buffer_slot,
		                             vertex_elemetn_fmt format, std::uint32_t offset)
		{
			m_attributes.push_back({location, buffer_slot, format, offset});
			return *this;
		}

		vertex_layout& add_attribute(std::uint32_t location, vertex_elemetn_fmt format, std::uint32_t offset)
		{
			m_attributes.push_back({location, 0, format, offset});
			return *this;
		}

		[[nodiscard]] std::span<const vertex_buffer_description> buffers() const noexcept
		{
			return m_buffers;
		}
		[[nodiscard]] std::span<const vertex_attribute> attributes() const noexcept { return m_attributes; }

		[[nodiscard]] bool empty() const noexcept { return m_buffers.empty() && m_attributes.empty(); }

		void clear() noexcept
		{
			m_buffers.clear();
			m_attributes.clear();
		}

	private:
		std::vector<vertex_buffer_description> m_buffers;
		std::vector<vertex_attribute> m_attributes;
	};
} // namespace mngl::gpu