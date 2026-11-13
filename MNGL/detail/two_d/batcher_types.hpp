#pragma once

#include "../../color.hpp"
#include "../../command_buffer.hpp"
#include "../../gpu_buffer.hpp"
#include "../../graphics_pipeline.hpp"
#include "../../graphics_shader.hpp"
#include "../../gpu_texture.hpp"
#include "../../paths.hpp"
#include "../../rectangle.hpp"
#include "../../render_pass.hpp"
#include "../../sampler.hpp"
#include "../../shader_loader.hpp"
#include "../../texture.hpp"
#include "../../transform.hpp"
#include "../../vertex_layout.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace mngl::two_d
{
	class renderer;
}

namespace mngl::two_d::detail
{
	struct resident_mesh_vertex
	{
		float x;
		float y;
	};

	struct resident_mesh_block
	{
		std::uint32_t first_vertex = 0;
		std::uint32_t vertex_count = 0;
		glm::vec2 bounds_min{};
		glm::vec2 bounds_max{};
	};

	inline constexpr std::size_t resident_mesh_block_vertex_capacity = 3 * 64;

	struct resident_mesh_storage
	{
		explicit resident_mesh_storage(const gpu::device& owner) : device(owner) {}

		gpu::device device;
		gpu::buffer vertex_buffer;
		mutable std::mutex mutex;
		std::vector<resident_mesh_block> blocks;
		std::uint32_t vertex_count = 0;
		glm::vec2 bounds_min{};
		glm::vec2 bounds_max{};
	};
} // namespace mngl::two_d::detail

namespace mngl::gpu
{
	namespace detail
	{
		[[nodiscard]] inline shader_format select_2d_shader_format(SDL_GPUShaderFormat supported_formats)
		{
			// Prefer the packaged DXIL shaders, then native formats used by Metal and Vulkan.
			if ((supported_formats & shader_formats::dxil) != 0)
				return shader_format::dxil;
			if ((supported_formats & shader_formats::msl) != 0)
				return shader_format::msl;
			if ((supported_formats & shader_formats::spirv) != 0)
				return shader_format::spirv;
			if ((supported_formats & shader_formats::dxbc) != 0)
				return shader_format::dxbc;
			throw std::invalid_argument(
			    "GPU device does not support an MNGL 2D shader format (DXIL, MSL, SPIR-V, or DXBC)");
		}
	} // namespace detail

	enum class sprite_blend_mode : std::uint8_t
	{
		alpha,
		premultiplied_alpha, // Source RGB is already multiplied by source alpha.
		opaque,
		additive
	};

	enum class quad_effect : std::uint8_t
	{
		textured,
		circle,
		ellipse,
		rounded_rectangle
	};

	struct material_handle
	{
		constexpr static std::uint32_t invalid_value = std::numeric_limits<std::uint32_t>::max();
		std::uint32_t value = invalid_value;
		std::uint32_t generation = 0;
		[[nodiscard]] constexpr bool valid() const noexcept
		{
			return value != invalid_value && generation != 0;
		}
		constexpr bool operator==(const material_handle&) const noexcept = default;
	};

	struct material_instance_handle
	{
		constexpr static std::uint32_t invalid_value = std::numeric_limits<std::uint32_t>::max();
		std::uint32_t value = invalid_value;
		std::uint32_t generation = 0;
		[[nodiscard]] constexpr bool valid() const noexcept
		{
			return value != invalid_value && generation != 0;
		}
		constexpr bool operator==(const material_instance_handle&) const noexcept = default;
	};

	struct fragment_resource_bindings
	{
		// Bindings are copied, but the referenced GPU resources must outlive the material instance.
		void add_sampler(const texture& source, const sampler& state)
		{
			if (!source.valid() || !source.has_usage(texture::usage_flags::sampler) || !state.valid())
			    [[unlikely]]
			{
				throw std::invalid_argument(
				    "Fragment sampler binding requires a valid sampler texture and sampler");
			}
			require_device(source.device_raw());
			if (state.device_raw() != m_device) [[unlikely]]
			{
				throw std::invalid_argument("Fragment texture and sampler must use the same GPU device");
			}
			samplers.push_back({.texture = source.raw(), .sampler = state.raw()});
		}

		void add_storage_texture(const texture& source)
		{
			if (!source.valid() || !source.has_usage(texture::usage_flags::graphics_read)) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Fragment storage texture must be valid and graphics-storage-readable");
			}
			require_device(source.device_raw());
			storage_textures.push_back(source.raw());
		}

		void add_storage_buffer(const buffer& source)
		{
			if (!source.valid() || !has_usage(source.usage(), buffer_usage::graphics_storage_read))
			    [[unlikely]]
			{
				throw std::invalid_argument(
				    "Fragment storage buffer must be valid and graphics-storage-readable");
			}
			require_device(source.device_raw());
			storage_buffers.push_back(source.raw());
		}

		template <typename T> void add_uniform_block(const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Uniform data must be trivially copyable");
			static_assert(sizeof(T) % 16 == 0, "Uniform blocks must be padded to a multiple of 16 bytes");
			std::vector<std::uint8_t> block(sizeof(T));
			std::memcpy(block.data(), &value, sizeof(T));
			uniform_blocks.push_back(std::move(block));
		}

		void add_uniform_block_bytes(std::span<const std::uint8_t> block)
		{
			if (block.empty() || block.size() % 16 != 0 ||
			    block.size() > std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Fragment uniform blocks must be non-empty, 16-byte aligned, and fit in a 32-bit size");
			}
			uniform_blocks.emplace_back(block.begin(), block.end());
		}

		void add_uniform_blocks(const graphics_shader_asset& shader)
		{
			const auto blocks = shader.uniform_block_data();
			if (blocks.size() != shader.resources.uniform_buffers) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Shader asset does not provide named data for every fragment uniform buffer");
			}
			if (!uniform_blocks.empty() && !blocks.empty()) [[unlikely]]
			{
				throw std::logic_error(
				    "Uniform blocks cannot be supplied both by the shader asset and explicit bindings");
			}
			for (const auto& block : blocks)
			{
				add_uniform_block_bytes(block);
			}
		}

		void add_shader_resources(const graphics_shader_asset& shader, const sampler* default_sampler)
		{
			const auto& layout = shader.resource_layout();
			const auto& values = shader.resource_values();

			auto find_resource = [&](shader_resource_kind kind,
			                         std::uint32_t slot) -> const shader_resource_field_layout&
			{
				const auto found = std::find_if(layout.begin(), layout.end(),
				                                [kind, slot](const shader_resource_field_layout& field)
				                                { return field.kind == kind && field.slot == slot; });
				if (found == layout.end()) [[unlikely]]
				{
					throw std::invalid_argument(
					    "Reflected shader resource layout is missing a required slot");
				}
				return *found;
			};

			auto find_value =
			    [&](const shader_resource_field_layout& field) -> const shader_resource_assignment&
			{
				const auto found = values.find(field.name);
				if (found == values.end()) [[unlikely]]
				{
					throw std::invalid_argument("Shader resource '" + field.name + "' has not been assigned");
				}
				return found->second;
			};

			if (shader.resources.samplers != 0)
			{
				if (!samplers.empty()) [[unlikely]]
				{
					throw std::logic_error(
					    "Sampler resources cannot be bound both explicitly and by shader keys");
				}
				for (std::uint32_t slot = 0; slot < shader.resources.samplers; ++slot)
				{
					const auto& field = find_resource(shader_resource_kind::sampled_texture, slot);
					const auto& value = find_value(field);
					if (!value.texture) [[unlikely]]
					{
						throw std::invalid_argument("Sampled texture resource has no texture value");
					}
					const sampler* state = value.sampler_state ? value.sampler_state : default_sampler;
					if (!state) [[unlikely]]
					{
						throw std::logic_error("A default sampler is required for automatic texture binding");
					}
					add_sampler(value.texture->raw_gpu_texture(), *state);
				}
			}

			if (shader.resources.storage_textures != 0)
			{
				if (!storage_textures.empty()) [[unlikely]]
				{
					throw std::logic_error(
					    "store textures cannot be bound both explicitly and by shader keys");
				}
				for (std::uint32_t slot = 0; slot < shader.resources.storage_textures; ++slot)
				{
					const auto& field = find_resource(shader_resource_kind::storage_texture, slot);
					const auto& value = find_value(field);
					if (!value.texture) [[unlikely]]
					{
						throw std::invalid_argument("store texture resource has no texture value");
					}
					add_storage_texture(value.texture->raw_gpu_texture());
				}
			}

			if (shader.resources.storage_buffers != 0)
			{
				if (!storage_buffers.empty()) [[unlikely]]
				{
					throw std::logic_error(
					    "store buffers cannot be bound both explicitly and by shader keys");
				}
				for (std::uint32_t slot = 0; slot < shader.resources.storage_buffers; ++slot)
				{
					const auto& field = find_resource(shader_resource_kind::storage_buffer, slot);
					const auto& value = find_value(field);
					if (!value.storage_buffer) [[unlikely]]
					{
						throw std::invalid_argument("store buffer resource has no buffer value");
					}
					add_storage_buffer(*value.storage_buffer);
				}
			}
		}

	private:
		friend class batcher2d;

		void require_device(SDL_GPUDevice* device)
		{
			if (!device) [[unlikely]]
			{
				throw std::invalid_argument("Fragment resources require a valid GPU device");
			}
			if (m_device && m_device != device) [[unlikely]]
			{
				throw std::invalid_argument("Fragment resources must use the same GPU device");
			}
			m_device = device;
		}

		SDL_GPUDevice* m_device = nullptr;
		std::vector<SDL_GPUTextureSamplerBinding> samplers;
		std::vector<SDL_GPUTexture*> storage_textures;
		std::vector<SDL_GPUBuffer*> storage_buffers;
		std::vector<std::vector<std::uint8_t>> uniform_blocks;
	};

	struct texture_region_view
	{
		// Non-owning: the page must remain alive until its command buffer is submitted.
		const texture* page = nullptr;
		glm::vec4 uv_bounds{0.0f, 0.0f, 1.0f, 1.0f};
	};

	struct quad_transform
	{
		rectangle destination{};
		glm::vec2 pivot{0.5f, 0.5f};
		float rotation_radians = 0.0f;
		two_d::transform object_transform{};

		[[nodiscard]] const glm::mat4& matrix() const noexcept { return object_transform.matrix(); }

		quad_transform& matrix(const glm::mat4& value)
		{
			object_transform.matrix(value);
			return *this;
		}

		quad_transform& translate(glm::vec2 offset)
		{
			object_transform.translate(offset);
			return *this;
		}

		quad_transform& rotate(float radians, glm::vec2 origin = glm::vec2(0.0f))
		{
			object_transform.rotate(radians, origin);
			return *this;
		}

		quad_transform& scale(glm::vec2 factors, glm::vec2 origin = glm::vec2(0.0f))
		{
			object_transform.scale(factors, origin);
			return *this;
		}

		quad_transform& scale(float factor, glm::vec2 origin = glm::vec2(0.0f))
		{
			object_transform.scale(factor, origin);
			return *this;
		}

		quad_transform& reset() noexcept
		{
			object_transform.reset();
			return *this;
		}

	private:
		friend class batcher2d;

		struct affine_components
		{
			glm::vec2 transform_x{};
			glm::vec2 transform_y{};
			glm::vec2 translation{};
		};

		[[nodiscard]] affine_components model_components() const noexcept
		{
			// Sprite instances only consume the 2D affine basis and translation.
			const glm::vec2 pivot_offset = pivot * destination.size;
			const float cosine = rotation_radians == 0.0f ? 1.0f : std::cos(rotation_radians);
			const float sine = rotation_radians == 0.0f ? 0.0f : std::sin(rotation_radians);
			const glm::vec2 local_x{cosine * destination.size.x, sine * destination.size.x};
			const glm::vec2 local_y{-sine * destination.size.y, cosine * destination.size.y};
			const glm::vec2 rotated_pivot{cosine * pivot_offset.x - sine * pivot_offset.y,
			                              sine * pivot_offset.x + cosine * pivot_offset.y};
			const glm::vec2 local_translation = destination.pos + pivot_offset - rotated_pivot;

			const auto& object = object_transform.matrix();
			return {
			    .transform_x = {object[0][0] * local_x.x + object[1][0] * local_x.y,
				                object[0][1] * local_x.x + object[1][1] * local_x.y},
			    .transform_y = {object[0][0] * local_y.x + object[1][0] * local_y.y,
				                object[0][1] * local_y.x + object[1][1] * local_y.y},
			    .translation = {
			        object[0][0] * local_translation.x + object[1][0] * local_translation.y + object[3][0],
			        object[0][1] * local_translation.x + object[1][1] * local_translation.y + object[3][1]}};
		}

	public:
		[[nodiscard]] glm::mat4 model_matrix() const noexcept
		{
			const glm::vec2 pivot_offset = pivot * destination.size;
			glm::mat4 geometry(1.0f);
			geometry = glm::translate(geometry, glm::vec3(destination.pos + pivot_offset, 0.0f));
			geometry = glm::rotate(geometry, rotation_radians, glm::vec3(0.0f, 0.0f, 1.0f));
			geometry = glm::translate(geometry, glm::vec3(-pivot_offset, 0.0f));
			geometry = glm::scale(geometry, glm::vec3(destination.size, 1.0f));
			return object_transform.matrix() * geometry;
		}
	};

	struct quad_draw_state
	{
		filter_mode filter = filter_mode::linear;
		sprite_blend_mode blend = sprite_blend_mode::alpha;
		quad_effect effect = quad_effect::textured;
		glm::vec2 effect_parameters{0.0f, 0.0f};
		std::optional<SDL_Rect> scissor;
		// A custom material instance binds its own fragment resources and uniforms.
		material_instance_handle material{};
	};

	struct batcher2d_shader_assets
	{
		graphics_shader_asset vertex;
		graphics_shader_asset fragment;
		// Set only when the vertex shader recognizes the NaN sentinel in TEXCOORD4.y and clips it.
		bool supports_instance_culling = false;

		static batcher2d_shader_assets load_default(const device& gpu_device)
		{
			if (!gpu_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot load default 2D shaders for a null GPU device");
			}
			return load_default_format(detail::select_2d_shader_format(gpu_device.shader_format()));
		}

		// Retained for source compatibility; batcher2d uses the active-device overload above.
		static batcher2d_shader_assets load_default()
		{
			return load_default_format(static_cast<shader_format>(::mngl::gpu::default_shader_format));
		}

	private:
		static batcher2d_shader_assets load_default_format(shader_format format)
		{
			const auto base_path = paths::application_directory();
			const auto directory = base_path / "shaders";
			if (format == shader_format::dxil)
			{
				return load_dxil(directory);
			}

			shader_compile_options vertex_options{};
			vertex_options.stage = graphics_shader_stage::vertex;
			vertex_options.format = format;
			vertex_options.resources.uniform_buffers = 1;

			shader_compile_options fragment_options{};
			fragment_options.stage = graphics_shader_stage::fragment;
			fragment_options.format = format;
			fragment_options.resources.samplers = 1;
			fragment_options.resources.uniform_buffers = 1;

			return {shader_loader::compile(directory / "batcher2d.vert.hlsl", vertex_options),
			        shader_loader::compile(directory / "batcher2d.frag.hlsl", fragment_options), true};
		}

	public:
		static batcher2d_shader_assets load_dxil(const std::filesystem::path& directory)
		{
			batcher2d_shader_assets shaders{};
			shaders.vertex.stage = graphics_shader_stage::vertex;
			shaders.vertex.code.assign(shader_format::dxil,
			                           read_binary_file(directory / "batcher2d.vert.dxil"));
			shaders.vertex.entrypoint = "main";
			shaders.vertex.resources.uniform_buffers = 1;
			shaders.vertex.debug_name = "batcher2d.vert";
			shaders.supports_instance_culling = true;

			shaders.fragment.stage = graphics_shader_stage::fragment;
			shaders.fragment.code.assign(shader_format::dxil,
			                             read_binary_file(directory / "batcher2d.frag.dxil"));
			shaders.fragment.entrypoint = "main";
			shaders.fragment.resources.samplers = 1;
			shaders.fragment.resources.uniform_buffers = 1;
			shaders.fragment.debug_name = "batcher2d.frag";
			return shaders;
		}
	};
} // namespace mngl::gpu
