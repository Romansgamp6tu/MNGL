#pragma once


#include "utilities.hpp"
#include "device.hpp"

namespace mngl::gpu
{
	enum class vertex_elemetn_fmt : uint8_t
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

	enum class vertex_input_rate : uint8_t
	{
		vertex = SDL_GPU_VERTEXINPUTRATE_VERTEX,
		instance = SDL_GPU_VERTEXINPUTRATE_INSTANCE
	};

	struct vertex_attribute
	{
		uint32_t location = 0;
		uint32_t buffer_slot = 0;
		vertex_elemetn_fmt format = vertex_elemetn_fmt::invalid;
		uint32_t offset = 0;
		
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
		uint32_t slot = 0;
		uint32_t pitch = 0;
		vertex_input_rate input_rate = vertex_input_rate::vertex;
		uint32_t instance_step_rate = 0;

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

		vertex_layout& add_buffer(
			uint32_t slot,
			uint32_t pitch,
			vertex_input_rate input_rate = vertex_input_rate::vertex,
			uint32_t instance_step_rate = 0
		)
		{
			m_buffers.push_back({slot, pitch, input_rate, instance_step_rate});
		}

		vertex_layout& add_attribute(
			uint32_t location,
			uint32_t buffer_slot,
			vertex_elemetn_fmt format,
			uint32_t offset
		)
		{
			m_attributes.push_back({location, buffer_slot, format, offset});
		}

		vertex_layout& add_attribute(
			uint32_t location,
			vertex_elemetn_fmt format,
			uint32_t offset
		)
		{
			m_attributes.push_back({location, 0, format, offset});
		}

		[[nodiscard]] std::span<const vertex_buffer_description> buffers() const noexcept { return m_buffers; }
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
}