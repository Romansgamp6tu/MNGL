#pragma once

#include "color.hpp"
#include "command_buffer.hpp"
#include "gpu_buffer.hpp"
#include "graphics_pipeline.hpp"
#include "graphics_shader.hpp"
#include "gpu_texture.hpp"
#include "paths.hpp"
#include "rectangle.hpp"
#include "render_pass.hpp"
#include "sampler.hpp"
#include "shader_loader.hpp"
#include "texture.hpp"
#include "transform.hpp"
#include "vertex_layout.hpp"
#include "utilities.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <limits>
#include <memory>
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

namespace mngl::gpu
{
	enum class sprite_blend_mode : uint8_t
	{
		alpha,
		premultiplied_alpha, // Source RGB is already multiplied by source alpha.
		opaque,
		additive
	};

	enum class quad_effect : uint8_t
	{
		textured,
		circle,
		ellipse,
		rounded_rectangle
	};

	struct material_handle
	{
		constexpr static uint32_t invalid_value = std::numeric_limits<uint32_t>::max();
		uint32_t value = invalid_value;
		uint32_t generation = 0;
		[[nodiscard]] constexpr bool valid() const noexcept { return value != invalid_value && generation != 0; }
		constexpr bool operator==(const material_handle&) const noexcept = default;
	};

	struct material_instance_handle
	{
		constexpr static uint32_t invalid_value = std::numeric_limits<uint32_t>::max();
		uint32_t value = invalid_value;
		uint32_t generation = 0;
		[[nodiscard]] constexpr bool valid() const noexcept { return value != invalid_value && generation != 0; }
		constexpr bool operator==(const material_instance_handle&) const noexcept = default;
	};

	struct fragment_resource_bindings
	{
		// Bindings are copied, but the referenced GPU resources must outlive the material instance.
		void add_sampler(const texture& source, const sampler& state)
		{
			if (!source.valid() || !source.has_usage(texture::usage_flags::sampler) || !state.valid())
			{
				throw std::invalid_argument("Fragment sampler binding requires a valid sampler texture and sampler");
			}
			require_device(source.device_raw());
			if (state.device_raw() != m_device)
			{
				throw std::invalid_argument("Fragment texture and sampler must use the same GPU device");
			}
			samplers.push_back({ .texture = source.raw(), .sampler = state.raw() });
		}

		void add_storage_texture(const texture& source)
		{
			if (!source.valid() || !source.has_usage(texture::usage_flags::graphics_read))
			{
				throw std::invalid_argument("Fragment storage texture must be valid and graphics-storage-readable");
			}
			require_device(source.device_raw());
			storage_textures.push_back(source.raw());
		}

		void add_storage_buffer(const buffer& source)
		{
			if (!source.valid() || source.usage() != buffer_usage::graphics_storage_read)
			{
				throw std::invalid_argument("Fragment storage buffer must be valid and graphics-storage-readable");
			}
			require_device(source.device_raw());
			storage_buffers.push_back(source.raw());
		}

		template<typename T>
		void add_uniform_block(const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Uniform data must be trivially copyable");
			static_assert(sizeof(T) % 16 == 0, "Uniform blocks must be padded to a multiple of 16 bytes");
			std::vector<uint8_t> block(sizeof(T));
			std::memcpy(block.data(), &value, sizeof(T));
			uniform_blocks.push_back(std::move(block));
		}

		void add_uniform_block_bytes(std::span<const uint8_t> block)
		{
			if (block.empty() || block.size() % 16 != 0 ||
				block.size() > std::numeric_limits<uint32_t>::max())
			{
				throw std::invalid_argument(
					"Fragment uniform blocks must be non-empty, 16-byte aligned, and fit in a 32-bit size");
			}
			uniform_blocks.emplace_back(block.begin(), block.end());
		}

		void add_uniform_blocks(const graphics_shader_asset& shader)
		{
			const auto blocks = shader.uniform_block_data();
			if (blocks.size() != shader.resources.uniform_buffers)
			{
				throw std::invalid_argument(
					"Shader asset does not provide named data for every fragment uniform buffer");
			}
			if (!uniform_blocks.empty() && !blocks.empty())
			{
				throw std::logic_error(
					"Uniform blocks cannot be supplied both by the shader asset and explicit bindings");
			}
			for (const auto& block : blocks)
			{
				add_uniform_block_bytes(block);
			}
		}

		void add_shader_resources(
			const graphics_shader_asset& shader,
			const sampler* default_sampler
		)
		{
			const auto& layout = shader.resource_layout();
			const auto& values = shader.resource_values();

			auto find_resource = [&](shader_resource_kind kind, uint32_t slot)
				-> const shader_resource_field_layout&
			{
				const auto found = std::find_if(
					layout.begin(),
					layout.end(),
					[kind, slot](const shader_resource_field_layout& field) {
						return field.kind == kind && field.slot == slot;
					});
				if (found == layout.end())
				{
					throw std::invalid_argument("Reflected shader resource layout is missing a required slot");
				}
				return *found;
			};

			auto find_value = [&](const shader_resource_field_layout& field)
				-> const shader_resource_assignment&
			{
				const auto found = values.find(field.name);
				if (found == values.end())
				{
					throw std::invalid_argument(
						"Shader resource '" + field.name + "' has not been assigned");
				}
				return found->second;
			};

			if (shader.resources.samplers != 0)
			{
				if (!samplers.empty())
				{
					throw std::logic_error("Sampler resources cannot be bound both explicitly and by shader keys");
				}
				for (uint32_t slot = 0; slot < shader.resources.samplers; ++slot)
				{
					const auto& field = find_resource(shader_resource_kind::sampled_texture, slot);
					const auto& value = find_value(field);
					if (!value.texture)
					{
						throw std::invalid_argument("Sampled texture resource has no texture value");
					}
					const sampler* state = value.sampler_state ? value.sampler_state : default_sampler;
					if (!state)
					{
						throw std::logic_error("A default sampler is required for automatic texture binding");
					}
					add_sampler(value.texture->raw_gpu_texture(), *state);
				}
			}

			if (shader.resources.storage_textures != 0)
			{
				if (!storage_textures.empty())
				{
					throw std::logic_error(
						"Storage textures cannot be bound both explicitly and by shader keys");
				}
				for (uint32_t slot = 0; slot < shader.resources.storage_textures; ++slot)
				{
					const auto& field = find_resource(shader_resource_kind::storage_texture, slot);
					const auto& value = find_value(field);
					if (!value.texture)
					{
						throw std::invalid_argument("Storage texture resource has no texture value");
					}
					add_storage_texture(value.texture->raw_gpu_texture());
				}
			}

			if (shader.resources.storage_buffers != 0)
			{
				if (!storage_buffers.empty())
				{
					throw std::logic_error(
						"Storage buffers cannot be bound both explicitly and by shader keys");
				}
				for (uint32_t slot = 0; slot < shader.resources.storage_buffers; ++slot)
				{
					const auto& field = find_resource(shader_resource_kind::storage_buffer, slot);
					const auto& value = find_value(field);
					if (!value.storage_buffer)
					{
						throw std::invalid_argument("Storage buffer resource has no buffer value");
					}
					add_storage_buffer(*value.storage_buffer);
				}
			}
		}

	private:
		friend class batcher2d;

		void require_device(SDL_GPUDevice* device)
		{
			if (!device)
			{
				throw std::invalid_argument("Fragment resources require a valid GPU device");
			}
			if (m_device && m_device != device)
			{
				throw std::invalid_argument("Fragment resources must use the same GPU device");
			}
			m_device = device;
		}

		SDL_GPUDevice* m_device = nullptr;
		std::vector<SDL_GPUTextureSamplerBinding> samplers;
		std::vector<SDL_GPUTexture*> storage_textures;
		std::vector<SDL_GPUBuffer*> storage_buffers;
		std::vector<std::vector<uint8_t>> uniform_blocks;
	};

	struct texture_region_view
	{
		// Non-owning: the page must remain alive until its command buffer is submitted.
		const texture* page = nullptr;
		glm::vec4 uv_bounds{ 0.0f, 0.0f, 1.0f, 1.0f };
	};

	struct quad_transform
	{
		rectangle destination{};
		glm::vec2 pivot{ 0.5f, 0.5f };
		float rotation_radians = 0.0f;
		two_d::transform object_transform{};

		[[nodiscard]] const glm::mat4& matrix() const noexcept
		{
			return object_transform.matrix();
		}

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

		quad_transform& scale(
			glm::vec2 factors,
			glm::vec2 origin = glm::vec2(0.0f))
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

		[[nodiscard]] glm::mat4 model_matrix() const noexcept
		{
			const glm::vec2 pivot_offset = pivot * destination.size;
			glm::mat4 geometry(1.0f);
			geometry = glm::translate(
				geometry,
				glm::vec3(destination.pos + pivot_offset, 0.0f));
			geometry = glm::rotate(
				geometry,
				rotation_radians,
				glm::vec3(0.0f, 0.0f, 1.0f));
			geometry = glm::translate(
				geometry,
				glm::vec3(-pivot_offset, 0.0f));
			geometry = glm::scale(
				geometry,
				glm::vec3(destination.size, 1.0f));
			return object_transform.matrix() * geometry;
		}
	};

	struct quad_draw_state
	{
		filter_mode filter = filter_mode::linear;
		sprite_blend_mode blend = sprite_blend_mode::alpha;
		quad_effect effect = quad_effect::textured;
		glm::vec2 effect_parameters{ 0.0f, 0.0f };
		std::optional<SDL_Rect> scissor;
		// A custom material instance binds its own fragment resources and uniforms.
		material_instance_handle material{};
	};

	struct batcher2d_shader_assets
	{
		graphics_shader_asset vertex;
		graphics_shader_asset fragment;

		static batcher2d_shader_assets load_default()
		{
			const auto base_path = paths::application_directory();
			if (!base_path) throw base_path.error();

			const auto directory = *base_path / "shaders";
			const auto format = static_cast<shader_format>(MNGL_DEFAULT_SHADER_FORMAT);
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

			return {
				shader_loader::compile(directory / "batcher2d.vert.hlsl", vertex_options),
				shader_loader::compile(directory / "batcher2d.frag.hlsl", fragment_options)
			};
		}

		static batcher2d_shader_assets load_dxil(const std::filesystem::path& directory)
		{
			batcher2d_shader_assets shaders{};
			shaders.vertex.stage = graphics_shader_stage::vertex;
			shaders.vertex.code.assign(
				shader_format::dxil,
				read_binary_file(directory / "batcher2d.vert.dxil"));
			shaders.vertex.entrypoint = "main";
			shaders.vertex.resources.uniform_buffers = 1;
			shaders.vertex.debug_name = "batcher2d.vert";

			shaders.fragment.stage = graphics_shader_stage::fragment;
			shaders.fragment.code.assign(
				shader_format::dxil,
				read_binary_file(directory / "batcher2d.frag.dxil"));
			shaders.fragment.entrypoint = "main";
			shaders.fragment.resources.samplers = 1;
			shaders.fragment.resources.uniform_buffers = 1;
			shaders.fragment.debug_name = "batcher2d.frag";
			return shaders;
		}
	};

	class batcher2d
	{
	private:
		struct recording_frame_state
		{
			std::atomic<uint32_t> outstanding_contexts{ 0 };
		};

		struct quad_vertex
		{
			float x;
			float y;
		};

		struct sprite_instance
		{
			std::array<float, 2> transform_x;
			std::array<float, 2> transform_y;
			std::array<float, 2> transform_translation;
			std::array<float, 2> effect_parameters;
			std::array<float, 4> uv;
			std::array<uint8_t, 4> tint;
		};

		struct draw_batch
		{
			SDL_GPUTexture* texture = nullptr;
			filter_mode filter = filter_mode::linear;
			sprite_blend_mode blend = sprite_blend_mode::alpha;
			quad_effect effect = quad_effect::textured;
			std::optional<SDL_Rect> scissor;
			material_instance_handle material{};
			uint32_t first_instance = 0;
			uint32_t instance_count = 0;
		};

	public:
		class recording_context
		{
			friend class batcher2d;

		public:
			recording_context() noexcept = default;
			recording_context(const recording_context&) = delete;
			recording_context& operator=(const recording_context&) = delete;

			recording_context(recording_context&& other) noexcept
				: m_owner(std::exchange(other.m_owner, nullptr))
				, m_frame_generation(std::exchange(other.m_frame_generation, 0))
				, m_sealed(std::exchange(other.m_sealed, false))
				, m_frame_state(std::move(other.m_frame_state))
				, m_registered(std::exchange(other.m_registered, false))
				, m_instances(std::move(other.m_instances))
				, m_batches(std::move(other.m_batches))
			{ }

			recording_context& operator=(recording_context&& other) noexcept
			{
				if (this != &other)
				{
					release_registration();
					m_owner = std::exchange(other.m_owner, nullptr);
					m_frame_generation = std::exchange(other.m_frame_generation, 0);
					m_sealed = std::exchange(other.m_sealed, false);
					m_frame_state = std::move(other.m_frame_state);
					m_registered = std::exchange(other.m_registered, false);
					m_instances = std::move(other.m_instances);
					m_batches = std::move(other.m_batches);
				}
				return *this;
			}

			~recording_context() noexcept
			{
				release_registration();
			}

			[[nodiscard]] bool valid() const noexcept { return m_owner != nullptr; }
			[[nodiscard]] bool finished() const noexcept { return m_sealed; }

			void finish()
			{
				if (!m_owner)
				{
					throw std::logic_error("batcher2d recording context is no longer active");
				}
				m_owner->seal_recording_context(*this);
			}

		private:
			void release_registration() noexcept
			{
				if (m_registered && m_frame_state)
				{
					m_frame_state->outstanding_contexts.fetch_sub(
						1,
						std::memory_order_acq_rel);
				}
				m_registered = false;
				m_frame_state.reset();
			}

			const batcher2d* m_owner = nullptr;
			uint64_t m_frame_generation = 0;
			bool m_sealed = false;
			std::shared_ptr<recording_frame_state> m_frame_state;
			bool m_registered = false;
			std::vector<sprite_instance> m_instances;
			std::vector<draw_batch> m_batches;
		};

	private:
		struct custom_material
		{
			graphics_shader fragment_shader;
			graphics_pipeline pipeline;
			shader_resource_layout resources{};
			sprite_blend_mode blend = sprite_blend_mode::alpha;
		};

		struct custom_material_instance
		{
			material_handle material{};
			fragment_resource_bindings bindings;
		};

		struct custom_material_slot
		{
			uint32_t generation = 1;
			std::optional<custom_material> value;
		};

		struct custom_material_instance_slot
		{
			uint32_t generation = 1;
			std::optional<custom_material_instance> value;
		};

		static_assert(std::is_trivially_copyable_v<sprite_instance>);
		static_assert(std::is_standard_layout_v<sprite_instance>);
		static_assert(sizeof(sprite_instance) == 52);
		static_assert(offsetof(sprite_instance, transform_y) == 8);
		static_assert(offsetof(sprite_instance, transform_translation) == 16);
		static_assert(offsetof(sprite_instance, effect_parameters) == 24);
		static_assert(offsetof(sprite_instance, uv) == 32);
		static_assert(offsetof(sprite_instance, tint) == 48);
		static_assert(sizeof(glm::mat4) == 64);

	public:
		struct frame_stats
		{
			uint32_t sprites = 0;
			uint32_t batches = 0;
			uint32_t draw_calls = 0;
			uint32_t upload_bytes = 0;
		};

		batcher2d(
			const device& dev,
			texture_format target_format,
			const batcher2d_shader_assets& shaders,
			std::size_t initial_sprite_capacity = 1024
		)
			: m_device(dev)
			, m_target_format(target_format)
			, m_instance_buffer(dev.raw(), instance_buffer_size(initial_sprite_capacity), buffer_usage::vertex)
		{
			if (!m_device.raw())
			{
				throw std::invalid_argument("Cannot create batcher2d with a null GPU device");
			}
			if (m_target_format == texture_format::invalid)
			{
				throw std::invalid_argument("batcher2d requires a valid color target format");
			}
			if (shaders.vertex.stage != graphics_shader_stage::vertex ||
				shaders.fragment.stage != graphics_shader_stage::fragment)
			{
				throw std::invalid_argument("batcher2d requires vertex and fragment shader assets");
			}

			m_vertex_shader = graphics_shader(m_device.raw(), shaders.vertex);
			m_fragment_shader = graphics_shader(m_device.raw(), shaders.fragment);

			m_vertex_layout.add_buffer(0, sizeof(quad_vertex));
			m_vertex_layout.add_attribute(0, 0, vertex_elemetn_fmt::float2, offsetof(quad_vertex, x));
			m_vertex_layout.add_buffer(
				1,
				sizeof(sprite_instance),
				vertex_input_rate::instance);
			m_vertex_layout.add_attribute(1, 1, vertex_elemetn_fmt::float2, offsetof(sprite_instance, transform_x));
			m_vertex_layout.add_attribute(2, 1, vertex_elemetn_fmt::float2, offsetof(sprite_instance, transform_y));
			m_vertex_layout.add_attribute(3, 1, vertex_elemetn_fmt::float2, offsetof(sprite_instance, transform_translation));
			m_vertex_layout.add_attribute(4, 1, vertex_elemetn_fmt::float2, offsetof(sprite_instance, effect_parameters));
			m_vertex_layout.add_attribute(5, 1, vertex_elemetn_fmt::float4, offsetof(sprite_instance, uv));
			m_vertex_layout.add_attribute(6, 1, vertex_elemetn_fmt::ubyte4_norm, offsetof(sprite_instance, tint));

			for (uint8_t i = 0; i < static_cast<uint8_t>(sprite_blend_mode::additive) + 1; ++i)
			{
				const auto blend = static_cast<sprite_blend_mode>(i);
				m_pipelines[blend_index(blend)] = build_pipeline(m_fragment_shader, blend);
			}

			m_samplers[filter_index(filter_mode::linear)] =
				sampler_builder::create_linear_clamp(m_device.raw());
			m_samplers[filter_index(filter_mode::nearest)] =
				sampler_builder::create_point_clamp(m_device.raw());

			m_instances.reserve(initial_sprite_capacity);
			m_batches.reserve(std::min<std::size_t>(initial_sprite_capacity, 256));

			const std::array<quad_vertex, 4> vertices{{
				{0.0f, 0.0f},
				{1.0f, 0.0f},
				{0.0f, 1.0f},
				{1.0f, 1.0f}
			}};
			const std::array<uint16_t, 6> indices{ 0, 1, 2, 2, 1, 3 };

			command_buffer cmd(m_device);
			m_quad_vertices = buffer::create_with_data(
				m_device.raw(),
				cmd,
				std::span<const quad_vertex>(vertices),
				buffer_usage::vertex);
			m_quad_indices = buffer::create_with_data(
				m_device.raw(),
				cmd,
				std::span<const uint16_t>(indices),
				buffer_usage::index);
			cmd.submit();
		}

		explicit batcher2d(
			const device& dev,
			texture_format target_format,
			std::size_t initial_sprite_capacity = 1024
		)
			: batcher2d(dev, target_format, batcher2d_shader_assets::load_default(), initial_sprite_capacity)
		{ }

		batcher2d(
			const device& dev,
			texture_format target_format,
			const std::filesystem::path& shader_directory,
			std::size_t initial_sprite_capacity = 1024
		)
			: batcher2d(
				dev,
				target_format,
				batcher2d_shader_assets::load_dxil(shader_directory),
				initial_sprite_capacity)
		{ }

		batcher2d(const batcher2d&) = delete;
		batcher2d& operator=(const batcher2d&) = delete;
		batcher2d(batcher2d&&) = delete;
		batcher2d& operator=(batcher2d&&) = delete;

		[[nodiscard]] material_handle create_material(
			const graphics_shader_asset& fragment_shader,
			sprite_blend_mode blend = sprite_blend_mode::alpha
		)
		{
			ensure_resource_changes_allowed();
			return create_material_impl(fragment_shader, blend);
		}

		[[nodiscard]] material_instance_handle create_material_instance(
			material_handle material,
			fragment_resource_bindings bindings
		)
		{
			ensure_resource_changes_allowed();
			return create_material_instance_impl(material, std::move(bindings));
		}

		void update_material_instance(
			material_instance_handle instance,
			fragment_resource_bindings bindings
		)
		{
			ensure_resource_changes_allowed();
			auto& current = resolve_material_instance(instance);
			validate_material_bindings(current.material, bindings);
			current.bindings = std::move(bindings);
		}

		void destroy_material_instance(material_instance_handle instance)
		{
			ensure_resource_changes_allowed();
			(void)resolve_material_instance(instance);
			m_free_material_instance_slots.push_back(instance.value);
			auto& slot = m_custom_material_instances[instance.value];
			slot.value.reset();
			slot.generation = next_generation(slot.generation);
		}

		void destroy_material(material_handle material)
		{
			ensure_resource_changes_allowed();
			(void)resolve_material(material);
			for (const auto& instance : m_custom_material_instances)
			{
				if (instance.value && instance.value->material == material)
				{
					throw std::logic_error("Destroy material instances before destroying their material");
				}
			}
			m_free_material_slots.push_back(material.value);
			auto& slot = m_custom_materials[material.value];
			slot.value.reset();
			slot.generation = next_generation(slot.generation);
		}

		void begin_frame(const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f))
		{
			if (m_frame_active)
			{
				throw std::logic_error("batcher2d frame is already active");
			}

			auto recording_state = std::make_shared<recording_frame_state>();
			m_instances.clear();
			m_batches.clear();
			m_clear_color = clear_color;
			m_last_stats = {};
			if (++m_frame_generation == 0)
			{
				++m_frame_generation;
			}
			m_recording_state = std::move(recording_state);
			m_frame_active = true;
		}

		[[nodiscard]] recording_context create_recording_context(
			std::size_t expected_draw_count = 0
		) const
		{
			ensure_frame_active();
			constexpr std::size_t max_instances =
				std::numeric_limits<uint32_t>::max() / sizeof(sprite_instance);
			if (expected_draw_count > max_instances)
			{
				throw std::length_error(
					"batcher2d recording capacity exceeds SDL's 32-bit buffer size limit");
			}

			recording_context context;
			context.m_instances.reserve(expected_draw_count);
			context.m_batches.reserve(std::min<std::size_t>(expected_draw_count, 256));
			context.m_owner = this;
			context.m_frame_generation = m_frame_generation;
			context.m_frame_state = m_recording_state;
			register_recording_context(context);
			return context;
		}

		void merge_recording_context(recording_context& context)
		{
			ensure_frame_active();
			ensure_recording_context(context, true);
			if (!context.m_sealed)
			{
				throw std::logic_error(
					"batcher2d recording context must be finished before it can be merged");
			}

			constexpr std::size_t max_instances =
				std::numeric_limits<uint32_t>::max() / sizeof(sprite_instance);
			if (context.m_instances.size() > max_instances - m_instances.size())
			{
				throw std::length_error(
					"batcher2d frame exceeds SDL's 32-bit buffer size limit");
			}
			if (context.m_batches.size() >
				std::numeric_limits<uint32_t>::max() - m_batches.size())
			{
				throw std::length_error(
					"batcher2d frame exceeds SDL's 32-bit batch-count limit");
			}

			const auto first_instance = static_cast<uint32_t>(m_instances.size());
			m_instances.reserve(m_instances.size() + context.m_instances.size());
			m_batches.reserve(m_batches.size() + context.m_batches.size());
			m_instances.insert(
				m_instances.end(),
				std::make_move_iterator(context.m_instances.begin()),
				std::make_move_iterator(context.m_instances.end()));
			for (auto batch : context.m_batches)
			{
				batch.first_instance += first_instance;
				m_batches.push_back(std::move(batch));
			}

			context.m_instances.clear();
			context.m_batches.clear();
			context.m_owner = nullptr;
			context.m_frame_generation = 0;
			context.release_registration();
		}

		void cancel_frame() noexcept
		{
			m_frame_active = false;
			m_recording_state.reset();
			m_instances.clear();
			m_batches.clear();
			m_last_stats = {};
		}

		// Consecutive commands with identical page and render state are batched without reordering.
		// Texture pages must remain alive until the command buffer is submitted.
		void draw_quad(
			const texture_region_view& source,
			const quad_transform& transform,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f),
			const quad_draw_state& state = {}
		)
		{
			ensure_frame_active();
			draw_quad_into(m_instances, m_batches, source, transform, tint, state);
		}

		void draw_quad(
			recording_context& context,
			const texture_region_view& source,
			const quad_transform& transform,
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f),
			const quad_draw_state& state = {}
		) const
		{
			ensure_recording_context(context, false);
			draw_quad_into(
				context.m_instances,
				context.m_batches,
				source,
				transform,
				tint,
				state);
		}

		void draw_quad(
			const texture& source,
			const rectangle& destination,
			const glm::vec4& uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f),
			const quad_draw_state& state = {}
		)
		{
			quad_transform transform{};
			transform.destination = destination;
			draw_quad(texture_region_view{ &source, uv }, transform, tint, state);
		}

		void draw_quad(
			const ::mngl::texture& source,
			const rectangle& destination,
			const glm::vec4& uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
			const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f),
			const quad_draw_state& state = {}
		)
		{
			draw_quad(source.raw_gpu_texture(), destination, uv, tint, state);
		}

		// Records upload and render commands but leaves command-buffer submission to the caller.
		void flush(command_buffer& cmd, texture& target)
		{
			const auto projection = coordinate_projection(target);
			flush(cmd, target, projection);
		}

		void flush(command_buffer& cmd, texture& target, const glm::mat4& view_projection)
		{
			ensure_frame_active();
			if (m_recording_state &&
				m_recording_state->outstanding_contexts.load(std::memory_order_acquire) != 0)
			{
				throw std::logic_error(
					"Merge or destroy batcher2d recording contexts before flushing");
			}
			m_frame_active = false;

			m_last_stats.sprites = static_cast<uint32_t>(m_instances.size());
			m_last_stats.batches = static_cast<uint32_t>(m_batches.size());
			m_last_stats.draw_calls = m_last_stats.batches;
			m_last_stats.upload_bytes = static_cast<uint32_t>(m_instances.size() * sizeof(sprite_instance));

			if (!cmd.valid())
			{
				throw std::invalid_argument("batcher2d flush requires a valid command buffer");
			}
			const auto* command_device = cmd.device_raw();
			if (command_device && command_device != m_device.raw())
			{
				throw std::invalid_argument("batcher2d and command buffer must use the same GPU device");
			}

			if (!target.valid() || target.width() == 0 || target.height() == 0)
			{
				m_last_stats.draw_calls = 0;
				m_last_stats.upload_bytes = 0;
				return;
			}
			if (target.device_raw() != m_device.raw())
			{
				throw std::invalid_argument("batcher2d target texture belongs to a different GPU device");
			}
			if (!target.has_usage(texture::usage_flags::color_target))
			{
				throw std::invalid_argument("batcher2d target texture must have color_target usage");
			}
			if (target.format() != m_target_format)
			{
				throw std::invalid_argument("batcher2d target texture format does not match its pipeline");
			}
			if (target.width() > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
				target.height() > static_cast<uint32_t>(std::numeric_limits<int>::max()))
			{
				throw std::length_error("batcher2d target exceeds SDL's scissor rectangle range");
			}
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					if (!std::isfinite(view_projection[column][row]))
					{
						throw std::invalid_argument("batcher2d view-projection matrix must be finite");
					}
				}
			}

			if (!m_instances.empty())
			{
				m_instance_buffer.upload(
					m_device.raw(),
					cmd,
					std::span<const sprite_instance>(m_instances),
					true);
			}

			render_pass pass(cmd, target, m_clear_color);
			const SDL_GPUViewport viewport{
				.x = 0.0f,
				.y = 0.0f,
				.w = static_cast<float>(target.width()),
				.h = static_cast<float>(target.height()),
				.min_depth = 0.0f,
				.max_depth = 1.0f
			};
			SDL_SetGPUViewport(pass.raw(), &viewport);

			if (!m_instances.empty())
			{
				SDL_PushGPUVertexUniformData(
					cmd.raw(),
					0,
					&view_projection,
					static_cast<uint32_t>(sizeof(view_projection)));

				pass.bind_vertex_buffer(0, m_quad_vertices.raw());
				pass.bind_index_buffer(m_quad_indices.raw(), SDL_GPU_INDEXELEMENTSIZE_16BIT);

				SDL_GPUGraphicsPipeline* bound_pipeline = nullptr;
				std::optional<filter_mode> bound_filter;
				std::optional<quad_effect> bound_effect;
				std::optional<SDL_Rect> bound_scissor;
				SDL_GPUTexture* bound_texture = nullptr;
				const SDL_Rect full_scissor{
					0,
					0,
					static_cast<int>(target.width()),
					static_cast<int>(target.height())
				};

				for (const auto& batch : m_batches)
				{
					if (batch.material.valid())
					{
						const auto& instance = resolve_material_instance(batch.material);
						const auto& material = resolve_material(instance.material);
						auto* pipeline = material.pipeline.handle();
						if (bound_pipeline != pipeline)
						{
							pass.bind_pipeline(material.pipeline);
							bound_pipeline = pipeline;
						}

						const auto& bindings = instance.bindings;
						if (!bindings.samplers.empty())
						{
							pass.bind_fragment_samplers(0, bindings.samplers);
						}
						if (!bindings.storage_textures.empty())
						{
							SDL_BindGPUFragmentStorageTextures(
								pass.raw(),
								0,
								bindings.storage_textures.data(),
								static_cast<uint32_t>(bindings.storage_textures.size()));
						}
						if (!bindings.storage_buffers.empty())
						{
							SDL_BindGPUFragmentStorageBuffers(
								pass.raw(),
								0,
								bindings.storage_buffers.data(),
								static_cast<uint32_t>(bindings.storage_buffers.size()));
						}
						for (uint32_t slot = 0; slot < bindings.uniform_blocks.size(); ++slot)
						{
							const auto& block = bindings.uniform_blocks[slot];
							SDL_PushGPUFragmentUniformData(
								cmd.raw(),
								slot,
								block.data(),
								static_cast<uint32_t>(block.size()));
						}

						bound_texture = nullptr;
						bound_filter.reset();
						bound_effect.reset();
					}
					else
					{
						auto* pipeline = m_pipelines[blend_index(batch.blend)].handle();
						if (bound_pipeline != pipeline)
						{
							pass.bind_pipeline(m_pipelines[blend_index(batch.blend)]);
							bound_pipeline = pipeline;
						}
						if (bound_texture != batch.texture || !bound_filter || *bound_filter != batch.filter)
						{
							pass.bind_fragment_sampler(
								0,
								batch.texture,
								m_samplers[filter_index(batch.filter)].raw());
							bound_texture = batch.texture;
							bound_filter = batch.filter;
						}
						if (!bound_effect || *bound_effect != batch.effect)
						{
							const std::array<uint32_t, 4> material_uniform{
								static_cast<uint32_t>(effect_index(batch.effect)),
								0,
								0,
								0
							};
							SDL_PushGPUFragmentUniformData(
								cmd.raw(),
								0,
								material_uniform.data(),
								static_cast<uint32_t>(sizeof(material_uniform)));
							bound_effect = batch.effect;
						}
					}

					const SDL_Rect scissor = batch.scissor.value_or(full_scissor);
					if (!bound_scissor || !equal_scissor(*bound_scissor, scissor))
					{
						SDL_SetGPUScissor(pass.raw(), &scissor);
						bound_scissor = scissor;
					}

					const uint32_t instance_offset =
						batch.first_instance * static_cast<uint32_t>(sizeof(sprite_instance));
					pass.bind_vertex_buffer(1, m_instance_buffer.raw(), instance_offset);
					pass.draw_indexed_primitives(6, batch.instance_count);
				}
			}
			pass.end_pass();
		}

		[[nodiscard]] const frame_stats& last_frame_stats() const noexcept
		{
			return m_last_stats;
		}

		[[nodiscard]] texture_format target_format() const noexcept
		{
			return m_target_format;
		}

	private:
		friend class ::mngl::two_d::renderer;

		[[nodiscard]] material_handle create_material_for_active_frame(
			const graphics_shader_asset& fragment_shader,
			sprite_blend_mode blend
		)
		{
			if (!m_frame_active)
			{
				throw std::logic_error("A frame-scoped material requires an active batcher2d frame");
			}
			return create_material_impl(fragment_shader, blend);
		}

		[[nodiscard]] material_instance_handle create_material_instance_for_active_frame(
			material_handle material,
			fragment_resource_bindings bindings
		)
		{
			if (!m_frame_active)
			{
				throw std::logic_error("A frame-scoped material instance requires an active batcher2d frame");
			}
			return create_material_instance_impl(material, std::move(bindings));
		}

		[[nodiscard]] material_handle create_material_impl(
			const graphics_shader_asset& fragment_shader,
			sprite_blend_mode blend
		)
		{
			// The frame only queues CPU-side draws; the render pass starts later in flush().
			if (fragment_shader.stage != graphics_shader_stage::fragment ||
				!fragment_shader.valid())
			{
				throw std::invalid_argument("Custom batcher2d material requires a valid fragment shader asset");
			}
			(void)blend_index(blend);

			graphics_shader shader(m_device.raw(), fragment_shader);
			auto pipeline = build_pipeline(shader, blend);
			custom_material value{
				std::move(shader),
				std::move(pipeline),
				fragment_shader.resources,
				blend
			};

			if (!m_free_material_slots.empty())
			{
				const uint32_t index = m_free_material_slots.back();
				m_free_material_slots.pop_back();
				auto& slot = m_custom_materials[index];
				slot.value.emplace(std::move(value));
				return material_handle{ index, slot.generation };
			}
			if (m_custom_materials.size() >= material_handle::invalid_value)
			{
				throw std::length_error("batcher2d exhausted its custom material handle space");
			}

			custom_material_slot slot{};
			slot.value.emplace(std::move(value));
			m_custom_materials.push_back(std::move(slot));
			const auto index = static_cast<uint32_t>(m_custom_materials.size() - 1);
			return material_handle{ index, m_custom_materials[index].generation };
		}

		[[nodiscard]] material_instance_handle create_material_instance_impl(
			material_handle material,
			fragment_resource_bindings bindings
		)
		{
			validate_material_bindings(material, bindings);
			custom_material_instance value{
				material,
				std::move(bindings)
			};

			if (!m_free_material_instance_slots.empty())
			{
				const uint32_t index = m_free_material_instance_slots.back();
				m_free_material_instance_slots.pop_back();
				auto& slot = m_custom_material_instances[index];
				slot.value.emplace(std::move(value));
				return material_instance_handle{ index, slot.generation };
			}
			if (m_custom_material_instances.size() >= material_instance_handle::invalid_value)
			{
				throw std::length_error("batcher2d exhausted its material instance handle space");
			}

			custom_material_instance_slot slot{};
			slot.value.emplace(std::move(value));
			m_custom_material_instances.push_back(std::move(slot));
			const auto index = static_cast<uint32_t>(m_custom_material_instances.size() - 1);
			return material_instance_handle{ index, m_custom_material_instances[index].generation };
		}

		static std::size_t blend_index(sprite_blend_mode blend)
		{
			switch (blend)
			{
			case sprite_blend_mode::alpha: return 0;
			case sprite_blend_mode::premultiplied_alpha: return 1;
			case sprite_blend_mode::opaque: return 2;
			case sprite_blend_mode::additive: return 3;
			default:
				throw std::invalid_argument("Unsupported batcher2d blend mode");
			}
		}

		static std::size_t filter_index(filter_mode filter)
		{
			switch (filter)
			{
			case filter_mode::linear: return 0;
			case filter_mode::nearest: return 1;
			default:
				throw std::invalid_argument("Unsupported batcher2d filter mode");
			}
		}

		static std::size_t effect_index(quad_effect effect)
		{
			switch (effect)
			{
			case quad_effect::textured: return 0;
			case quad_effect::circle: return 1;
			case quad_effect::ellipse: return 2;
			case quad_effect::rounded_rectangle: return 3;
			default:
				throw std::invalid_argument("Unsupported batcher2d quad effect");
			}
		}

		static SDL_GPUColorTargetBlendState blend_state_for(sprite_blend_mode blend)
		{
			SDL_GPUColorTargetBlendState state{};
			if (blend == sprite_blend_mode::opaque)
			{
				return state;
			}

			state.enable_blend = true;
			state.color_blend_op = SDL_GPU_BLENDOP_ADD;
			state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
			switch (blend)
			{
			case sprite_blend_mode::alpha:
				state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
				state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case sprite_blend_mode::premultiplied_alpha:
				state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case sprite_blend_mode::additive:
				state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				break;
			default:
				throw std::invalid_argument("Unsupported batcher2d blend mode");
			}
			return state;
		}

		graphics_pipeline build_pipeline(
			const graphics_shader& fragment_shader,
			sprite_blend_mode blend
		) const
		{
			return graphics_pipeline_builder{}
				.set_vertex_shader(m_vertex_shader)
				.set_fragment_shader(fragment_shader)
				.set_vertex_layout(m_vertex_layout)
				.set_primitive_type(primitive_type::triangle_list)
				.set_cull_mode(cull_mode::none)
				.add_color_target(
					static_cast<SDL_GPUTextureFormat>(m_target_format),
					blend_state_for(blend))
				.build(m_device.raw());
		}

		void ensure_resource_changes_allowed() const
		{
			if (m_frame_active)
			{
				throw std::logic_error("Material resources cannot change while a batcher2d frame is active");
			}
		}

		static uint32_t next_generation(uint32_t generation) noexcept
		{
			++generation;
			return generation == 0 ? 1 : generation;
		}

		const custom_material& resolve_material(material_handle handle) const
		{
			if (!handle.valid() || handle.value >= m_custom_materials.size())
			{
				throw std::out_of_range("Invalid batcher2d material handle");
			}
			const auto& slot = m_custom_materials[handle.value];
			if (slot.generation != handle.generation || !slot.value)
			{
				throw std::out_of_range("Stale batcher2d material handle");
			}
			return *slot.value;
		}

		const custom_material_instance& resolve_material_instance(material_instance_handle handle) const
		{
			if (!handle.valid() || handle.value >= m_custom_material_instances.size())
			{
				throw std::out_of_range("Invalid batcher2d material instance handle");
			}
			const auto& slot = m_custom_material_instances[handle.value];
			if (slot.generation != handle.generation || !slot.value)
			{
				throw std::out_of_range("Stale batcher2d material instance handle");
			}
			return *slot.value;
		}

		custom_material_instance& resolve_material_instance(material_instance_handle handle)
		{
			if (!handle.valid() || handle.value >= m_custom_material_instances.size())
			{
				throw std::out_of_range("Invalid batcher2d material instance handle");
			}
			auto& slot = m_custom_material_instances[handle.value];
			if (slot.generation != handle.generation || !slot.value)
			{
				throw std::out_of_range("Stale batcher2d material instance handle");
			}
			return *slot.value;
		}

		void validate_material_bindings(
			material_handle material,
			const fragment_resource_bindings& bindings
		) const
		{
			const auto& resources = resolve_material(material).resources;
			if (bindings.m_device && bindings.m_device != m_device.raw())
			{
				throw std::invalid_argument("Fragment material resources belong to a different GPU device");
			}
			if (bindings.samplers.size() != resources.samplers ||
				bindings.storage_textures.size() != resources.storage_textures ||
				bindings.storage_buffers.size() != resources.storage_buffers ||
				bindings.uniform_blocks.size() != resources.uniform_buffers)
			{
				throw std::invalid_argument("Fragment material bindings do not match the shader resource layout");
			}

			for (const auto& binding : bindings.samplers)
			{
				if (!binding.texture || !binding.sampler)
				{
					throw std::invalid_argument("Fragment material has a null sampled texture or sampler");
				}
			}
			for (auto* texture_handle : bindings.storage_textures)
			{
				if (!texture_handle)
				{
					throw std::invalid_argument("Fragment material has a null storage texture");
				}
			}
			for (auto* buffer_handle : bindings.storage_buffers)
			{
				if (!buffer_handle)
				{
					throw std::invalid_argument("Fragment material has a null storage buffer");
				}
			}
			for (const auto& uniform : bindings.uniform_blocks)
			{
				if (uniform.empty() || uniform.size() % 16 != 0 ||
					uniform.size() > std::numeric_limits<uint32_t>::max())
				{
					throw std::invalid_argument("Fragment uniform blocks must be non-empty std140-sized payloads");
				}
			}
		}

		static bool equal_scissor(const SDL_Rect& left, const SDL_Rect& right) noexcept
		{
			return left.x == right.x && left.y == right.y &&
				left.w == right.w && left.h == right.h;
		}

		static void register_recording_context(recording_context& context)
		{
			if (!context.m_frame_state)
			{
				throw std::logic_error(
					"batcher2d recording context requires an active frame");
			}

			auto current = context.m_frame_state->outstanding_contexts.load(
				std::memory_order_relaxed);
			for (;;)
			{
				if (current == std::numeric_limits<uint32_t>::max())
				{
					throw std::length_error(
						"batcher2d frame exhausted its recording-context count");
				}
				if (context.m_frame_state->outstanding_contexts.compare_exchange_weak(
					current,
					current + 1,
					std::memory_order_acq_rel,
					std::memory_order_relaxed))
				{
					context.m_registered = true;
					return;
				}
			}
		}

		void ensure_recording_context(
			const recording_context& context,
			bool allow_finished
		) const
		{
			ensure_frame_active();
			if (context.m_owner != this ||
				context.m_frame_generation != m_frame_generation ||
				context.m_frame_state != m_recording_state ||
				!context.m_registered)
			{
				throw std::invalid_argument(
					"batcher2d recording context belongs to a different or expired frame");
			}
			if (!allow_finished && context.m_sealed)
			{
				throw std::logic_error("batcher2d recording context has already been finished");
			}
		}

		void seal_recording_context(recording_context& context) const
		{
			ensure_recording_context(context, false);
			context.m_sealed = true;
		}

		void draw_quad_into(
			std::vector<sprite_instance>& instances,
			std::vector<draw_batch>& batches,
			const texture_region_view& source,
			const quad_transform& transform,
			const color& tint,
			const quad_draw_state& state
		) const
		{
			const auto& destination = transform.destination;
			if (!std::isfinite(destination.pos.x) || !std::isfinite(destination.pos.y) ||
				!std::isfinite(destination.size.x) || !std::isfinite(destination.size.y) ||
				destination.size.x < 0.0f || destination.size.y < 0.0f ||
				!std::isfinite(transform.pivot.x) || !std::isfinite(transform.pivot.y) ||
				!std::isfinite(transform.rotation_radians))
			{
				throw std::invalid_argument(
					"batcher2d transform values must be finite with non-negative dimensions");
			}
			const glm::mat4 model_matrix = transform.model_matrix();
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					if (!std::isfinite(model_matrix[column][row]))
					{
						throw std::invalid_argument("batcher2d model matrix must be finite");
					}
				}
			}
			if (destination.size.x == 0.0f || destination.size.y == 0.0f)
			{
				return;
			}
			if (!std::isfinite(source.uv_bounds.x) || !std::isfinite(source.uv_bounds.y) ||
				!std::isfinite(source.uv_bounds.z) || !std::isfinite(source.uv_bounds.w))
			{
				throw std::invalid_argument("batcher2d UV coordinates must be finite");
			}
			const custom_material_instance* custom_instance = nullptr;
			if (state.material.valid())
			{
				custom_instance = &resolve_material_instance(state.material);
			}
			else if (!source.page || !source.page->valid() ||
				!source.page->has_usage(texture::usage_flags::sampler))
			{
				throw std::invalid_argument(
					"batcher2d source texture must be valid and sampler-enabled");
			}
			if (state.scissor && (state.scissor->w < 0 || state.scissor->h < 0))
			{
				throw std::invalid_argument("batcher2d scissor dimensions must be non-negative");
			}
			if (state.scissor && (state.scissor->w == 0 || state.scissor->h == 0))
			{
				return;
			}
			if (!std::isfinite(state.effect_parameters.x) ||
				!std::isfinite(state.effect_parameters.y))
			{
				throw std::invalid_argument("batcher2d effect parameters must be finite");
			}

			const sprite_blend_mode blend = custom_instance
				? resolve_material(custom_instance->material).blend
				: state.blend;
			if (!custom_instance)
			{
				(void)blend_index(state.blend);
				(void)filter_index(state.filter);
				(void)effect_index(state.effect);
			}

			constexpr std::size_t max_instances =
				std::numeric_limits<uint32_t>::max() / sizeof(sprite_instance);
			if (instances.size() >= max_instances)
			{
				throw std::length_error("batcher2d frame exceeds SDL's 32-bit buffer size limit");
			}

			const auto rgba = tint.rgba_glsl();
			sprite_instance instance{
				.transform_x = {
					model_matrix[0][0],
					model_matrix[0][1]
				},
				.transform_y = {
					model_matrix[1][0],
					model_matrix[1][1]
				},
				.transform_translation = {
					model_matrix[3][0],
					model_matrix[3][1]
				},
				.effect_parameters = { state.effect_parameters.x, state.effect_parameters.y },
				.uv = {
					source.uv_bounds.x,
					source.uv_bounds.w,
					source.uv_bounds.z,
					source.uv_bounds.y
				},
				.tint = {
					pack_channel(rgba.r),
					pack_channel(rgba.g),
					pack_channel(rgba.b),
					pack_channel(rgba.a)
				}
			};

			auto* raw_texture = source.page ? source.page->raw() : nullptr;
			if (!batches.empty() && same_batch_key(batches.back(), raw_texture, state))
			{
				instances.push_back(instance);
				++batches.back().instance_count;
				return;
			}

			batches.push_back(draw_batch{
				.texture = raw_texture,
				.filter = state.filter,
				.blend = blend,
				.effect = state.effect,
				.scissor = state.scissor,
				.material = state.material,
				.first_instance = static_cast<uint32_t>(instances.size()),
				.instance_count = 1
			});
			try
			{
				instances.push_back(instance);
			}
			catch (...)
			{
				batches.pop_back();
				throw;
			}
		}

		static bool same_batch_key(
			const draw_batch& batch,
			SDL_GPUTexture* texture_handle,
			const quad_draw_state& state
		) noexcept
		{
			if (batch.material.valid() || state.material.valid())
			{
				return batch.material == state.material &&
					batch.scissor.has_value() == state.scissor.has_value() &&
					(!batch.scissor || equal_scissor(*batch.scissor, *state.scissor));
			}

			return batch.texture == texture_handle &&
				batch.filter == state.filter &&
				batch.blend == state.blend &&
				batch.effect == state.effect &&
				batch.scissor.has_value() == state.scissor.has_value() &&
				(!batch.scissor || equal_scissor(*batch.scissor, *state.scissor));
		}

		static glm::mat4 coordinate_projection(const texture& target)
		{
			glm::mat4 projection(1.0f);
			if (target.width() == 0 || target.height() == 0)
			{
				return projection;
			}

			const float short_side = static_cast<float>(
				std::min(target.width(), target.height()));
			const float coordinate_width = static_cast<float>(target.width()) / short_side;
			const float coordinate_height = static_cast<float>(target.height()) / short_side;
			projection[0][0] = 2.0f / coordinate_width;
			projection[1][1] = 2.0f / coordinate_height;
			projection[3][0] = -1.0f;
			projection[3][1] = -1.0f;
			return projection;
		}

		static uint32_t instance_buffer_size(std::size_t sprite_capacity)
		{
			constexpr std::size_t max_capacity =
				std::numeric_limits<uint32_t>::max() / sizeof(sprite_instance);
			if (sprite_capacity > max_capacity)
			{
				throw std::length_error("batcher2d initial capacity exceeds SDL's 32-bit buffer size limit");
			}
			return static_cast<uint32_t>(sprite_capacity * sizeof(sprite_instance));
		}

		static uint8_t pack_channel(float value)
		{
			if (!std::isfinite(value))
			{
				throw std::invalid_argument("batcher2d tint channels must be finite");
			}
			return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
		}

		void ensure_frame_active() const
		{
			if (!m_frame_active)
			{
				throw std::logic_error("batcher2d operation requires an active frame");
			}
		}

		const device& m_device;
		texture_format m_target_format = texture_format::invalid;
		vertex_layout m_vertex_layout;
		graphics_shader m_vertex_shader;
		graphics_shader m_fragment_shader;
		std::array<graphics_pipeline, 4> m_pipelines;
		std::array<sampler, 2> m_samplers;
		std::vector<custom_material_slot> m_custom_materials;
		std::vector<custom_material_instance_slot> m_custom_material_instances;
		std::vector<uint32_t> m_free_material_slots;
		std::vector<uint32_t> m_free_material_instance_slots;
		buffer m_quad_vertices;
		buffer m_quad_indices;
		buffer m_instance_buffer;
		std::vector<sprite_instance> m_instances;
		std::vector<draw_batch> m_batches;
		color m_clear_color = color(0.0f, 0.0f, 0.0f, 1.0f);
		frame_stats m_last_stats{};
		std::shared_ptr<recording_frame_state> m_recording_state;
		uint64_t m_frame_generation = 0;
		bool m_frame_active = false;
	};
}
