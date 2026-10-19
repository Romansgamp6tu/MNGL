#pragma once

#include "utilities.hpp"
#include "graphics_shader.hpp"
#include "vertex_layout.hpp"
#include "device.hpp"
#include "gpu_texture.hpp"

namespace mngl::gpu
{
	enum class primitive_type : uint8_t
	{
		triangle_list = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		triangle_strip = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
		line_list = SDL_GPU_PRIMITIVETYPE_LINELIST,
		line_strip = SDL_GPU_PRIMITIVETYPE_LINESTRIP,
		point_list = SDL_GPU_PRIMITIVETYPE_POINTLIST
	};

	enum class cull_mode : uint8_t
	{
		none = SDL_GPU_CULLMODE_NONE,
		front = SDL_GPU_CULLMODE_FRONT,
		back = SDL_GPU_CULLMODE_BACK
	};

	enum class front_face : uint8_t
	{
		clockwise = SDL_GPU_FRONTFACE_CLOCKWISE,
		counter_clockwise = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
	};

	enum class fill_mode : uint8_t
	{
		fill = SDL_GPU_FILLMODE_FILL,
		line = SDL_GPU_FILLMODE_LINE
	};

	enum class compare_op : uint8_t
	{
		never = SDL_GPU_COMPAREOP_NEVER,
		less = SDL_GPU_COMPAREOP_LESS,
		equal = SDL_GPU_COMPAREOP_EQUAL,
		less_or_equal = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
		greater = SDL_GPU_COMPAREOP_GREATER,
		not_equal = SDL_GPU_COMPAREOP_NOT_EQUAL,
		greater_or_equal = SDL_GPU_COMPAREOP_GREATER_OR_EQUAL,
		always = SDL_GPU_COMPAREOP_ALWAYS
	};

	class graphics_pipeline
	{
	public:
		graphics_pipeline() noexcept = default;
		graphics_pipeline(SDL_GPUDevice* dev, SDL_GPUGraphicsPipeline* handle) noexcept
			: m_device(dev), m_handle(handle)
		{ }

		~graphics_pipeline() { destroy(); }

		graphics_pipeline(const graphics_pipeline&) = delete;
		graphics_pipeline& operator=(const graphics_pipeline&) = delete;

		graphics_pipeline(graphics_pipeline&& other) noexcept
			: m_device(std::exchange(other.m_device, nullptr))
			, m_handle(std::exchange(other.m_handle, nullptr))
		{ }

		graphics_pipeline& operator=(graphics_pipeline&& other) noexcept
		{
			if (this != &other)
			{
				destroy();
				m_device = std::exchange(other.m_device, nullptr);
				m_handle = std::exchange(other.m_handle, nullptr);
			}
			return *this;
		}

		[[nodiscard]] SDL_GPUGraphicsPipeline* handle() const noexcept { return m_handle; }
		[[nodiscard]] bool valid() const noexcept { return m_handle != nullptr; }
		explicit operator bool() const noexcept { return valid(); }

		void destroy() noexcept
		{
			if (m_device && m_handle)
			{
				SDL_ReleaseGPUGraphicsPipeline(m_device, m_handle);
				m_handle = nullptr;
			}
		}

	private:
		SDL_GPUDevice* m_device = nullptr;
		SDL_GPUGraphicsPipeline* m_handle = nullptr;
	};

	class graphics_pipeline_builder
	{
	public:

		graphics_pipeline_builder() = default;

		graphics_pipeline_builder& set_vertex_shader(SDL_GPUShader* shader) noexcept
		{
			m_vert_shader = shader;
			return *this;
		}

		graphics_pipeline_builder& set_vertex_shader(const graphics_shader& shader) noexcept
		{
			return set_vertex_shader(shader.handle());
		}

		graphics_pipeline_builder& set_fragment_shader(SDL_GPUShader* shader) noexcept
		{
			m_frag_shader = shader;
			return *this;
		}

		graphics_pipeline_builder& set_fragment_shader(const graphics_shader& shader) noexcept
		{
			return set_fragment_shader(shader.handle());
		}

		graphics_pipeline_builder& set_vertex_layout(const vertex_layout& layout)
		{
			m_layout = layout;
			return *this;
		}

		graphics_pipeline_builder& set_primitive_type(primitive_type type) noexcept
		{
			m_primitive_type = type;
			return *this;
		}

		graphics_pipeline_builder& set_cull_mode(cull_mode mode) noexcept
		{
			m_cull_mode = mode;
			return *this;
		}

		graphics_pipeline_builder& set_front_face(front_face face) noexcept
		{
			m_front_face = face;
			return *this;
		}

		graphics_pipeline_builder& set_fill_mode(fill_mode mode) noexcept
		{
			m_fill_mode = mode;
			return *this;
		}

		graphics_pipeline_builder& enable_depth_test(
			compare_op op = compare_op::less_or_equal,
			bool write_enable = true
		) noexcept
		{
			m_enable_depth_test = true;
			m_enable_depth_write = write_enable;
			m_depth_compare_op = op;
			return *this;
		}

		graphics_pipeline_builder& disable_depth_test() noexcept
		{
			m_enable_depth_test = false;
			m_enable_depth_write = false;
			return *this;
		}

		graphics_pipeline_builder& add_color_target(
			SDL_GPUTextureFormat format,
			SDL_GPUColorTargetBlendState blend_state = {}
		)
		{
			m_color_targets.push_back(SDL_GPUColorTargetDescription{
				.format = format,
				.blend_state = blend_state
									  });
			return *this;
		}

		graphics_pipeline_builder& add_color_target(
			texture_format format,
			SDL_GPUColorTargetBlendState blend_state = {}
		)
		{
			return add_color_target(static_cast<SDL_GPUTextureFormat>(format), blend_state);
		}

		graphics_pipeline_builder& add_alpha_blend_target(SDL_GPUTextureFormat format)
		{
			SDL_GPUColorTargetBlendState blend{};
			blend.enable_blend = true;
			blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
			blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
			blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
			blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
			blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

			return add_color_target(format, blend);
		}

		graphics_pipeline_builder& add_alpha_blend_target(texture_format format)
		{
			return add_alpha_blend_target(static_cast<SDL_GPUTextureFormat>(format));
		}

		graphics_pipeline_builder& set_depth_stencil_format(SDL_GPUTextureFormat format) noexcept
		{
			m_depth_stencil_format = format;
			m_has_depth_target = (format != SDL_GPU_TEXTUREFORMAT_INVALID);
			return *this;
		}

		graphics_pipeline_builder& set_depth_stencil_format(texture_format format) noexcept
		{
			return set_depth_stencil_format(static_cast<SDL_GPUTextureFormat>(format));
		}

		graphics_pipeline build(SDL_GPUDevice* device) const
		{
			if (!device)
			{
				throw std::runtime_error("Cannot build graphics_pipeline with null device");
			}
			if (!m_vert_shader || !m_frag_shader)
			{
				throw std::runtime_error("Graphics pipeline requires both vertex and fragment shaders");
			}

			// 1. Преобразуем vertex_layout в C-структуры SDL
			std::vector<SDL_GPUVertexBufferDescription> sdl_buffers;
			sdl_buffers.reserve(m_layout.buffers().size());
			for (const auto& buf : m_layout.buffers())
			{
				sdl_buffers.push_back(buf.to_sdl());
			}

			std::vector<SDL_GPUVertexAttribute> sdl_attrs;
			sdl_attrs.reserve(m_layout.attributes().size());
			for (const auto& attr : m_layout.attributes())
			{
				sdl_attrs.push_back(attr.to_sdl());
			}

			// 2. Заполняем описания
			SDL_GPUGraphicsPipelineCreateInfo info{};
			info.vertex_shader = m_vert_shader;
			info.fragment_shader = m_frag_shader;

			info.vertex_input_state = SDL_GPUVertexInputState{
				.vertex_buffer_descriptions = sdl_buffers.data(),
				.num_vertex_buffers = static_cast<uint32_t>(sdl_buffers.size()),
				.vertex_attributes = sdl_attrs.data(),
				.num_vertex_attributes = static_cast<uint32_t>(sdl_attrs.size())
			};

			info.primitive_type = static_cast<SDL_GPUPrimitiveType>(m_primitive_type);

			info.rasterizer_state = SDL_GPURasterizerState{
				.fill_mode = static_cast<SDL_GPUFillMode>(m_fill_mode),
				.cull_mode = static_cast<SDL_GPUCullMode>(m_cull_mode),
				.front_face = static_cast<SDL_GPUFrontFace>(m_front_face)
			};

			info.depth_stencil_state = SDL_GPUDepthStencilState{
				.compare_op = static_cast<SDL_GPUCompareOp>(m_depth_compare_op),
				.enable_depth_test = m_enable_depth_test,
				.enable_depth_write = m_enable_depth_write
			};

			info.target_info = SDL_GPUGraphicsPipelineTargetInfo{
				.color_target_descriptions = m_color_targets.data(),
				.num_color_targets = static_cast<uint32_t>(m_color_targets.size()),
				.depth_stencil_format = m_depth_stencil_format,
				.has_depth_stencil_target = m_has_depth_target
			};

			SDL_GPUGraphicsPipeline* handle = SDL_CreateGPUGraphicsPipeline(device, &info);
			if (!handle)
			{
				throw sdl_error("Failed to create SDL_GPUGraphicsPipeline: ");
			}

			return graphics_pipeline{device, handle};
		}

		graphics_pipeline build(const mngl::gpu::device& dev) const
		{
			return build(dev.raw());
		}

	private:
		SDL_GPUShader* m_vert_shader = nullptr;
		SDL_GPUShader* m_frag_shader = nullptr;
		vertex_layout m_layout;

		primitive_type m_primitive_type = primitive_type::triangle_list;
		cull_mode m_cull_mode = cull_mode::none;
		front_face m_front_face = front_face::counter_clockwise;
		fill_mode m_fill_mode = fill_mode::fill;

		bool m_enable_depth_test = false;
		bool m_enable_depth_write = false;
		compare_op m_depth_compare_op = compare_op::less_or_equal;

		std::vector<SDL_GPUColorTargetDescription> m_color_targets;
		SDL_GPUTextureFormat m_depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID;
		bool m_has_depth_target = false;
	};
}