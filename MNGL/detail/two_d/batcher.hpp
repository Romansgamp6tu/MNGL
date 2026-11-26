#pragma once

#include "batcher_types.hpp"
#include "../container_growth.hpp"
#include "../../compute_pass.hpp"
#include "../../compute_pipeline.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mngl::gpu
{
	class batcher2d
	{
	private:
		using profile_clock = std::chrono::steady_clock;

		struct recording_frame_state
		{
			std::atomic<std::uint32_t> outstanding_contexts{0};
		};

		struct quad_vertex
		{
			float x;
			float y;
			std::array<float, 4> tint{1.0f, 1.0f, 1.0f, 1.0f};
		};

		struct geometry_block
		{
			std::uint32_t first_vertex = 0;
			std::uint32_t vertex_count = 0;
			glm::vec2 bounds_min{};
			glm::vec2 bounds_max{};
		};

		struct geometry_culling_block
		{
			std::array<float, 2> bounds_min{};
			std::array<float, 2> bounds_max{};
			std::array<float, 2> transform_x{};
			std::array<float, 2> transform_y{};
			std::array<float, 2> transform_translation{};
			std::uint32_t first_vertex = 0;
			std::uint32_t vertex_count = 0;
		};

		struct default_gpu_resources
		{
			explicit default_gpu_resources(const device& device) : device_owner(device) {}

			device device_owner;
			vertex_layout vertex_layout;
			graphics_shader vertex_shader;
			graphics_shader fragment_shader;
			std::array<graphics_pipeline, 4> pipelines;
			std::array<sampler, 2> samplers;
			buffer quad_vertices;
			buffer quad_indices;
			bool supports_instance_culling = false;
		};

		struct geometry_culling_uniforms
		{
			glm::mat4 view_projection{};
			std::uint32_t block_count = 0;
			std::array<std::uint32_t, 3> padding{};
		};

		struct instance_culling_range
		{
			std::uint32_t first_instance = 0;
			std::uint32_t instance_count = 0;
			std::array<float, 2> bounds_min{};
			std::array<float, 2> bounds_max{};
			std::uint32_t bypass = 0;
		};

		struct instance_culling_uniforms
		{
			glm::mat4 view_projection{};
			std::uint32_t instance_count = 0;
			std::uint32_t range_count = 0;
			std::array<std::uint32_t, 2> padding{};
		};

		struct order_independent_batch_range
		{
			std::size_t first_batch = 0;
			std::size_t batch_count = 0;
		};

		static constexpr std::size_t geometry_block_vertex_capacity = 3 * 64;

		struct sprite_instance
		{
			std::array<float, 2> transform_x;
			std::array<float, 2> transform_y;
			std::array<float, 2> transform_translation;
			std::array<float, 2> effect_parameters;
			std::array<float, 4> uv;
			std::array<std::uint8_t, 4> tint;
		};

		struct draw_batch
		{
			SDL_GPUTexture* texture = nullptr;
			filter_mode filter = filter_mode::linear;
			sprite_blend_mode blend = sprite_blend_mode::alpha;
			quad_effect effect = quad_effect::textured;
			std::optional<SDL_Rect> scissor;
			material_instance_handle material{};
			// Nonzero tokens isolate a deferred draw and identify it for texture resolution.
			std::uint64_t deferred_token = 0;
			std::uint32_t first_instance = 0;
			std::uint32_t instance_count = 0;
			bool geometry = false;
			std::uint32_t first_vertex = 0;
			std::uint32_t vertex_count = 0;
			std::uint32_t first_geometry_block = 0;
			std::uint32_t geometry_block_count = 0;
			std::array<std::uint8_t, 4> geometry_tint{};
			std::shared_ptr<::mngl::two_d::detail::resident_mesh_storage> resident_mesh;
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
			    : m_owner(std::exchange(other.m_owner, nullptr)),
			      m_frame_generation(std::exchange(other.m_frame_generation, 0)),
			      m_sealed(std::exchange(other.m_sealed, false)),
			      m_frame_state(std::move(other.m_frame_state)),
			      m_registered(std::exchange(other.m_registered, false)),
			      m_instances(std::move(other.m_instances)),
			      m_geometry_vertices(std::move(other.m_geometry_vertices)),
			      m_geometry_blocks(std::move(other.m_geometry_blocks)), m_batches(std::move(other.m_batches))
			{
			}

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
					m_geometry_vertices = std::move(other.m_geometry_vertices);
					m_geometry_blocks = std::move(other.m_geometry_blocks);
					m_batches = std::move(other.m_batches);
				}
				return *this;
			}

			~recording_context() noexcept { release_registration(); }

			[[nodiscard]] bool valid() const noexcept { return m_owner != nullptr; }
			[[nodiscard]] bool finished() const noexcept { return m_sealed; }

			void finish()
			{
				if (!m_owner) [[unlikely]]
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
					m_frame_state->outstanding_contexts.fetch_sub(1, std::memory_order_acq_rel);
				}
				m_registered = false;
				m_frame_state.reset();
			}

			const batcher2d* m_owner = nullptr;
			std::uint64_t m_frame_generation = 0;
			bool m_sealed = false;
			std::shared_ptr<recording_frame_state> m_frame_state;
			bool m_registered = false;
			std::vector<sprite_instance> m_instances;
			std::vector<quad_vertex> m_geometry_vertices;
			std::vector<geometry_block> m_geometry_blocks;
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
			std::uint32_t generation = 1;
			std::optional<custom_material> value;
		};

		struct custom_material_instance_slot
		{
			std::uint32_t generation = 1;
			std::optional<custom_material_instance> value;
		};

		static_assert(std::is_trivially_copyable_v<sprite_instance>);
		static_assert(std::is_standard_layout_v<sprite_instance>);
		static_assert(sizeof(quad_vertex) == sizeof(::mngl::two_d::detail::resident_mesh_vertex));
		static_assert(sizeof(geometry_culling_block) == 48);
		static_assert(sizeof(geometry_culling_uniforms) == 80);
		static_assert(sizeof(instance_culling_range) == 28);
		static_assert(sizeof(instance_culling_uniforms) == 80);
		static_assert(sizeof(SDL_GPUIndirectDrawCommand) == 16);
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
			std::uint32_t sprites = 0;
			std::uint32_t batches = 0;
			std::uint32_t draw_calls = 0;
			std::uint32_t upload_bytes = 0;
			std::chrono::nanoseconds frame_recording_wall_time{};
			std::chrono::nanoseconds culling_cpu_time{};
			std::chrono::nanoseconds upload_cpu_time{};
			std::chrono::nanoseconds command_encoding_cpu_time{};
			std::chrono::nanoseconds flush_cpu_time{};
		};

		batcher2d(const device& dev, texture_format target_format, const batcher2d_shader_assets& shaders,
		          std::size_t initial_sprite_capacity = 1024)
		    : m_device(dev), m_target_format(target_format),
		      m_instance_buffer(dev, instance_buffer_size(initial_sprite_capacity),
			                    buffer_usage::vertex | buffer_usage::compute_storage_read |
			                        buffer_usage::compute_storage_write)
		{
			if (!m_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot create batcher2d with a null GPU device");
			}
			if (m_target_format == texture_format::invalid) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d requires a valid color target format");
			}
			if (shaders.vertex.stage != graphics_shader_stage::vertex ||
			    shaders.fragment.stage != graphics_shader_stage::fragment) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d requires vertex and fragment shader assets");
			}

			m_shared_default_resources = acquire_default_gpu_resources(m_device, m_target_format, shaders);
			m_vertex_instance_culling_supported = m_shared_default_resources->supports_instance_culling;
			m_vertex_layout = m_shared_default_resources->vertex_layout;

			m_instances.reserve(initial_sprite_capacity);
			m_batches.reserve(std::min<std::size_t>(initial_sprite_capacity, 256));
			m_instance_culling_ranges.reserve(std::min<std::size_t>(initial_sprite_capacity, 256));

		}

		explicit batcher2d(const device& dev, texture_format target_format,
		                   std::size_t initial_sprite_capacity = 1024)
		    : batcher2d(dev, target_format, batcher2d_shader_assets::load_default(dev), initial_sprite_capacity)
		{
		}

		batcher2d(const device& dev, texture_format target_format,
		          const std::filesystem::path& shader_directory, std::size_t initial_sprite_capacity = 1024)
		    : batcher2d(dev, target_format, batcher2d_shader_assets::load_dxil(shader_directory),
			            initial_sprite_capacity)
		{
		}

		batcher2d(const batcher2d&) = delete;
		batcher2d& operator=(const batcher2d&) = delete;
		batcher2d(batcher2d&&) = delete;
		batcher2d& operator=(batcher2d&&) = delete;

		[[nodiscard]] material_handle create_material(const graphics_shader_asset& fragment_shader,
		                                              sprite_blend_mode blend = sprite_blend_mode::alpha)
		{
			ensure_resource_changes_allowed();
			return create_material_impl(fragment_shader, blend);
		}

		[[nodiscard]] material_instance_handle create_material_instance(material_handle material,
		                                                                fragment_resource_bindings bindings)
		{
			ensure_resource_changes_allowed();
			return create_material_instance_impl(material, std::move(bindings));
		}

		void update_material_instance(material_instance_handle instance, fragment_resource_bindings bindings)
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
				if (instance.value && instance.value->material == material) [[unlikely]]
				{
					throw std::logic_error("Destroy material instances before destroying their material");
				}
			}
			m_free_material_slots.push_back(material.value);
			auto& slot = m_custom_materials[material.value];
			slot.value.reset();
			slot.generation = next_generation(slot.generation);
		}

		void begin_frame(const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f),
		                 bool preserve_target_contents = false)
		{
			if (m_frame_active) [[unlikely]]
			{
				throw std::logic_error("batcher2d frame is already active");
			}
			if (!m_gpu_culling_overridden)
			{
				apply_gpu_culling(::mngl::gpu::gpu_culling_enabled());
			}

			auto recording_state = std::make_shared<recording_frame_state>();
			m_instances.clear();
			m_geometry_vertices.clear();
			m_geometry_blocks.clear();
			m_geometry_culling_blocks.clear();
			m_instance_culling_ranges.clear();
			m_batches.clear();
			m_order_independent_batch_ranges.clear();
			m_batch_extension_blocked = false;
			m_clear_color = clear_color;
			m_preserve_target_contents = preserve_target_contents;
			m_last_stats = {};
			if (++m_frame_generation == 0)
			{
				++m_frame_generation;
			}
			m_recording_state = std::move(recording_state);
			m_frame_active = true;
			if (m_profiling_enabled)
			{
				m_frame_started_at = profile_clock::now();
			}
		}

		[[nodiscard]] recording_context create_recording_context(std::size_t expected_draw_count = 0) const
		{
			ensure_frame_active();
			constexpr std::size_t max_instances =
			    std::numeric_limits<std::uint32_t>::max() / sizeof(sprite_instance);
			if (expected_draw_count > max_instances) [[unlikely]]
			{
				throw std::length_error(
				    "batcher2d recording capacity exceeds SDL's 32-bit buffer size limit");
			}

			recording_context context;
			context.m_instances.reserve(expected_draw_count);
			context.m_geometry_blocks.reserve(expected_draw_count);
			context.m_batches.reserve(std::min<std::size_t>(expected_draw_count, 256));
			context.m_owner = this;
			context.m_frame_generation = m_frame_generation;
			context.m_frame_state = m_recording_state;
			register_recording_context(context);
			return context;
		}

		void merge_recording_context(recording_context& context)
		{
			merge_recording_context_impl(context, false);
		}

	private:
		void merge_recording_context_impl(recording_context& context, bool order_independent)
		{
			ensure_frame_active();
			ensure_recording_context(context, true);
			if (!context.m_sealed) [[unlikely]]
			{
				throw std::logic_error(
				    "batcher2d recording context must be finished before it can be merged");
			}

			constexpr std::size_t max_instances =
			    std::numeric_limits<std::uint32_t>::max() / sizeof(sprite_instance);
			if (context.m_instances.size() > max_instances - m_instances.size()) [[unlikely]]
			{
				throw std::length_error("batcher2d frame exceeds SDL's 32-bit buffer size limit");
			}
			constexpr std::size_t max_geometry_vertices =
			    std::numeric_limits<std::uint32_t>::max() / sizeof(quad_vertex);
			if (context.m_geometry_vertices.size() > max_geometry_vertices - m_geometry_vertices.size())
			    [[unlikely]]
			{
				throw std::length_error("batcher2d frame geometry exceeds SDL's 32-bit buffer size limit");
			}
			if (context.m_geometry_blocks.size() >
			    std::numeric_limits<std::uint32_t>::max() - m_geometry_blocks.size()) [[unlikely]]
			{
				throw std::length_error("batcher2d frame exceeds SDL's 32-bit geometry-block limit");
			}
			if (context.m_batches.size() > std::numeric_limits<std::uint32_t>::max() - m_batches.size())
			    [[unlikely]]
			{
				throw std::length_error("batcher2d frame exceeds SDL's 32-bit batch-count limit");
			}

			const auto first_instance = static_cast<std::uint32_t>(m_instances.size());
			const auto first_vertex = static_cast<std::uint32_t>(m_geometry_vertices.size());
			const auto first_geometry_block = static_cast<std::uint32_t>(m_geometry_blocks.size());
			const std::size_t first_batch = m_batches.size();
			const std::size_t merged_batch_count = context.m_batches.size();
			::mngl::detail::reserve_vector_growth(
			    m_instances, m_instances.size() + context.m_instances.size(), max_instances);
			::mngl::detail::reserve_vector_growth(
			    m_geometry_vertices, m_geometry_vertices.size() + context.m_geometry_vertices.size(),
			    max_geometry_vertices);
			::mngl::detail::reserve_vector_growth(m_geometry_blocks,
			                                      m_geometry_blocks.size() + context.m_geometry_blocks.size(),
			                                      std::numeric_limits<std::uint32_t>::max());
			::mngl::detail::reserve_vector_growth(m_batches, m_batches.size() + context.m_batches.size(),
			                                      std::numeric_limits<std::uint32_t>::max());
			if (order_independent && merged_batch_count != 0)
			{
				::mngl::detail::reserve_vector_growth(m_order_independent_batch_ranges,
				                                      m_order_independent_batch_ranges.size() + 1);
			}
			m_instances.insert(m_instances.end(), std::make_move_iterator(context.m_instances.begin()),
			                   std::make_move_iterator(context.m_instances.end()));
			m_geometry_vertices.insert(m_geometry_vertices.end(), context.m_geometry_vertices.begin(),
			                           context.m_geometry_vertices.end());
			for (auto block : context.m_geometry_blocks)
			{
				block.first_vertex += first_vertex;
				m_geometry_blocks.push_back(std::move(block));
			}
			for (auto batch : context.m_batches)
			{
				batch.first_instance += first_instance;
				if (batch.geometry)
				{
					batch.first_vertex += first_vertex;
					batch.first_geometry_block += first_geometry_block;
				}
				m_batches.push_back(std::move(batch));
			}
			if (order_independent && merged_batch_count != 0)
			{
				m_order_independent_batch_ranges.push_back({first_batch, merged_batch_count});
				m_batch_extension_blocked = true;
			}
			else if (merged_batch_count != 0)
			{
				m_batch_extension_blocked = false;
			}

			context.m_instances.clear();
			context.m_geometry_vertices.clear();
			context.m_geometry_blocks.clear();
			context.m_batches.clear();
			context.m_owner = nullptr;
			context.m_frame_generation = 0;
			context.release_registration();
		}

		void merge_order_independent_recording_context(recording_context& context)
		{
			merge_recording_context_impl(context, true);
		}

	public:
		void cancel_frame() noexcept
		{
			m_frame_active = false;
			m_recording_state.reset();
			m_instances.clear();
			m_geometry_vertices.clear();
			m_geometry_blocks.clear();
			m_geometry_culling_blocks.clear();
			m_instance_culling_ranges.clear();
			m_batches.clear();
			m_order_independent_batch_ranges.clear();
			m_batch_extension_blocked = false;
			m_preserve_target_contents = false;
			m_last_stats = {};
		}

		struct deferred_texture_resolution
		{
			std::uint64_t token = 0;
			SDL_GPUTexture* texture = nullptr;
			bool visible = false;
		};

		// Resolve all effect placeholders with one pass over the merged batch list.
		[[nodiscard]] bool resolve_deferred_textures(std::span<const deferred_texture_resolution> resolutions)
		{
			std::unordered_map<std::uint64_t, std::size_t> resolution_indices;
			resolution_indices.reserve(resolutions.size());
			std::vector<draw_batch*> matched_batches(resolutions.size(), nullptr);
			for (std::size_t index = 0; index < resolutions.size(); ++index)
			{
				const auto& resolution = resolutions[index];
				if (resolution.token == 0 || (resolution.visible && !resolution.texture) ||
				    !resolution_indices.emplace(resolution.token, index).second)
				{
					return false;
				}
			}

			for (auto& batch : m_batches)
			{
				if (batch.deferred_token == 0)
				{
					continue;
				}
				const auto resolution = resolution_indices.find(batch.deferred_token);
				if (resolution == resolution_indices.end())
				{
					return false;
				}
				auto*& matched = matched_batches[resolution->second];
				if (matched || batch.geometry || batch.instance_count != 1)
				{
					return false;
				}
				matched = &batch;
			}

			for (const auto* matched : matched_batches)
			{
				if (!matched)
				{
					return false;
				}
			}

			for (std::size_t index = 0; index < resolutions.size(); ++index)
			{
				const auto& resolution = resolutions[index];
				if (resolution.visible)
				{
					matched_batches[index]->texture = resolution.texture;
				}
				else
				{
					matched_batches[index]->instance_count = 0;
				}
			}
			return true;
		}

		// Shared visibility predicate for deferred renderer work and batch submission.
		template <std::size_t PointCount>
		[[nodiscard]] static bool is_outside_target(const glm::mat4& view_projection,
		                                           const std::array<glm::vec2, PointCount>& points) noexcept
		{
			return outside_target(view_projection, points);
		}

		// Consecutive commands with identical page and render state are batched without reordering.
		// Texture pages must remain alive until the command buffer is submitted.
		void draw_quad(const texture_region_view& source, const quad_transform& transform,
		               const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f), const quad_draw_state& state = {})
		{
			ensure_frame_active();
			draw_quad_unchecked(source, transform, tint, state);
		}

		void draw_quad(recording_context& context, const texture_region_view& source,
		               const quad_transform& transform, const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f),
		               const quad_draw_state& state = {}) const
		{
			ensure_recording_context(context, false);
			draw_quad_unchecked(context, source, transform, tint, state);
		}

		// Triangle vertices are transformed and staged in one reusable frame buffer.
		void draw_geometry(std::span<const glm::vec2> vertices, const texture_region_view& source,
		                   const glm::mat4& transform, const color& tint, const quad_draw_state& state = {})
		{
			ensure_frame_active();
			draw_geometry_unchecked(vertices, source, transform, tint, state);
		}

		void draw_geometry(recording_context& context, std::span<const glm::vec2> vertices,
		                   const texture_region_view& source, const glm::mat4& transform, const color& tint,
		                   const quad_draw_state& state = {}) const
		{
			ensure_recording_context(context, false);
			draw_geometry_unchecked(context, vertices, source, transform, tint, state);
		}

		void draw_quad(const texture& source, const rectangle& destination,
		               const glm::vec4& uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
		               const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f), const quad_draw_state& state = {})
		{
			quad_transform transform{};
			transform.destination = destination;
			draw_quad(texture_region_view{&source, uv}, transform, tint, state);
		}

		void draw_quad(const ::mngl::texture& source, const rectangle& destination,
		               const glm::vec4& uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
		               const color& tint = color(1.0f, 1.0f, 1.0f, 1.0f), const quad_draw_state& state = {})
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
			const bool profile = m_profiling_enabled;
			const auto flush_started_at = profile ? profile_clock::now() : profile_clock::time_point{};
			if (m_recording_state &&
			    m_recording_state->outstanding_contexts.load(std::memory_order_acquire) != 0) [[unlikely]]
			{
				throw std::logic_error("Merge or destroy batcher2d recording contexts before flushing");
			}
			m_frame_active = false;
			if (profile)
			{
				m_last_stats.frame_recording_wall_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
				    flush_started_at - m_frame_started_at);
			}

			m_last_stats.sprites = static_cast<std::uint32_t>(m_instances.size());
			m_last_stats.batches = static_cast<std::uint32_t>(m_batches.size());
			m_last_stats.draw_calls = m_last_stats.batches;

			if (!cmd.valid()) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d flush requires a valid command buffer");
			}
			const auto* command_device = cmd.device_raw();
			if (command_device && command_device != m_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d and command buffer must use the same GPU device");
			}
			cmd.retain_resource(m_shared_default_resources);

			if (!target.valid() || target.width() == 0 || target.height() == 0)
			{
				m_last_stats.draw_calls = 0;
				m_last_stats.upload_bytes = 0;
				if (profile)
				{
					m_last_stats.flush_cpu_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
					    profile_clock::now() - flush_started_at);
				}
				return;
			}
			if (target.device_raw() != m_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d target texture belongs to a different GPU device");
			}
			if (!target.has_usage(texture::usage_flags::color_target)) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d target texture must have color_target usage");
			}
			if (target.format() != m_target_format) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d target texture format does not match its pipeline");
			}
			if (target.width() > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
			    target.height() > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) [[unlikely]]
			{
				throw std::length_error("batcher2d target exceeds SDL's scissor rectangle range");
			}
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					if (!std::isfinite(view_projection[column][row])) [[unlikely]]
					{
						throw std::invalid_argument("batcher2d view-projection matrix must be finite");
					}
				}
			}

			const bool order_independent_sorting_enabled =
			    ::mngl::gpu::order_independent_batch_sorting_enabled();
			const bool cpu_culling_enabled = ::mngl::gpu::cpu_culling_enabled();
			const bool defer_batch_sorting_for_geometry_culling =
			    !m_gpu_culling_enabled && cpu_culling_enabled && !m_geometry_vertices.empty();
			if (order_independent_sorting_enabled && !defer_batch_sorting_for_geometry_culling)
			{
				sort_order_independent_batches();
			}
			const auto culling_started_at = profile ? profile_clock::now() : profile_clock::time_point{};
			std::uint32_t visible_batch_count =
			    cull_offscreen_primitives(view_projection, m_gpu_culling_enabled, cpu_culling_enabled);
			if (profile)
			{
				m_last_stats.culling_cpu_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
				    profile_clock::now() - culling_started_at);
			}
			if (order_independent_sorting_enabled && defer_batch_sorting_for_geometry_culling)
			{
				sort_order_independent_batches();
				visible_batch_count = static_cast<std::uint32_t>(std::count_if(
				    m_batches.begin(), m_batches.end(), [](const draw_batch& batch)
				    { return batch.instance_count != 0 && (!batch.geometry || batch.vertex_count != 0); }));
			}
			for (const auto& batch : m_batches)
			{
				if (batch.resident_mesh)
				{
					cmd.retain_resource(batch.resident_mesh);
				}
			}
			if (m_gpu_culling_enabled)
			{
				build_geometry_culling_blocks();
				build_instance_culling_ranges();
			}
			m_last_stats.sprites = static_cast<std::uint32_t>(m_instances.size());
			m_last_stats.batches = visible_batch_count;
			m_last_stats.draw_calls = m_last_stats.batches;
			const std::size_t instance_bytes = m_instances.size() * sizeof(sprite_instance);
			const std::size_t geometry_bytes = m_geometry_vertices.size() * sizeof(quad_vertex);
			const std::size_t culling_bytes =
			    m_geometry_culling_blocks.size() * sizeof(geometry_culling_block);
			const std::size_t instance_culling_bytes =
			    m_gpu_culling_enabled ? m_instance_culling_ranges.size() * sizeof(instance_culling_range) : 0;
			if (instance_bytes > std::numeric_limits<std::uint32_t>::max() ||
			    geometry_bytes > std::numeric_limits<std::uint32_t>::max() - instance_bytes ||
			    culling_bytes > std::numeric_limits<std::uint32_t>::max() - instance_bytes - geometry_bytes ||
			    instance_culling_bytes > std::numeric_limits<std::uint32_t>::max() - instance_bytes -
			                                 geometry_bytes - culling_bytes) [[unlikely]]
			{
				throw std::length_error("batcher2d frame upload exceeds SDL's 32-bit buffer size limit");
			}
			const std::size_t upload_bytes =
			    instance_bytes + geometry_bytes + culling_bytes + instance_culling_bytes;
			m_last_stats.upload_bytes = static_cast<std::uint32_t>(upload_bytes);
			ensure_instance_culling_capacity(static_cast<std::uint32_t>(instance_culling_bytes));

			const auto upload_started_at = profile ? profile_clock::now() : profile_clock::time_point{};
			std::uint32_t instance_upload_size = 0;
			if (!m_instances.empty())
			{
				instance_upload_size = m_instance_buffer.stage_upload(
				    m_device.raw(), std::as_bytes(std::span<const sprite_instance>(m_instances)));
			}
			std::uint32_t geometry_upload_size = 0;
			if (!m_geometry_vertices.empty())
			{
				geometry_upload_size = m_geometry_buffer.stage_upload(
				    m_device.raw(), std::as_bytes(std::span<const quad_vertex>(m_geometry_vertices)));
			}
			std::uint32_t culling_upload_size = 0;
			if (!m_geometry_culling_blocks.empty())
			{
				if (m_geometry_culling_blocks.size() >
				        std::numeric_limits<std::uint32_t>::max() / sizeof(SDL_GPUIndirectDrawCommand) ||
				    m_geometry_culling_blocks.size() > std::numeric_limits<std::uint32_t>::max() /
				                                           sizeof(geometry_culling_block)) [[unlikely]]
				{
					throw std::length_error(
					    "batcher2d GPU culling command data exceeds SDL's 32-bit buffer limit");
				}
				ensure_gpu_culling_capacity(static_cast<std::uint32_t>(m_geometry_culling_blocks.size() *
				                                                       sizeof(geometry_culling_block)),
				                            static_cast<std::uint32_t>(m_geometry_culling_blocks.size() *
				                                                       sizeof(SDL_GPUIndirectDrawCommand)));
				culling_upload_size = m_geometry_culling_input.stage_upload(
				    m_device.raw(),
				    std::as_bytes(std::span<const geometry_culling_block>(m_geometry_culling_blocks)));
			}
			std::uint32_t instance_culling_upload_size = 0;
			if (m_gpu_culling_enabled && !m_instance_culling_ranges.empty())
			{
				instance_culling_upload_size = m_instance_culling_input.stage_upload(
				    m_device.raw(),
				    std::as_bytes(std::span<const instance_culling_range>(m_instance_culling_ranges)));
			}
			if (instance_upload_size != 0 || geometry_upload_size != 0 || culling_upload_size != 0 ||
			    instance_culling_upload_size != 0)
			{
				detail::gpu_copy_pass copy_pass(cmd);
				if (instance_upload_size != 0)
				{
					m_instance_buffer.record_upload(copy_pass, instance_upload_size, true);
				}
				if (geometry_upload_size != 0)
				{
					m_geometry_buffer.record_upload(copy_pass, geometry_upload_size, true);
				}
				if (culling_upload_size != 0)
				{
					m_geometry_culling_input.record_upload(copy_pass, culling_upload_size, true);
				}
				if (instance_culling_upload_size != 0)
				{
					m_instance_culling_input.record_upload(copy_pass, instance_culling_upload_size, true);
				}
			}
			if (profile)
			{
				m_last_stats.upload_cpu_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
				    profile_clock::now() - upload_started_at);
			}

			const auto command_encoding_started_at =
			    profile ? profile_clock::now() : profile_clock::time_point{};
			if (m_gpu_culling_enabled && !m_instances.empty())
			{
				const std::array<compute_pass::storage_buffer_write_binding, 1> write_buffers{
				    compute_pass::storage_buffer_write_binding{m_instance_buffer, false}};
				compute_pass culling_pass(cmd, m_instance_culling_pipeline, {}, write_buffers);
				culling_pass.bind_storage_buffer(0, m_instance_culling_input);
				culling_pass.push_uniform(
				    0, instance_culling_uniforms{view_projection,
					                             static_cast<std::uint32_t>(m_instances.size()),
					                             static_cast<std::uint32_t>(m_instance_culling_ranges.size()),
					                             {}});
				culling_pass.dispatch_for({static_cast<std::uint32_t>(m_instances.size()), 1, 1});
			}
			if (!m_geometry_culling_blocks.empty())
			{
				const std::array<compute_pass::storage_buffer_write_binding, 1> write_buffers{
				    compute_pass::storage_buffer_write_binding{m_geometry_indirect_commands, true}};
				compute_pass culling_pass(cmd, m_geometry_culling_pipeline, {}, write_buffers);
				culling_pass.bind_storage_buffer(0, m_geometry_culling_input);
				culling_pass.push_uniform(
				    0, geometry_culling_uniforms{view_projection,
					                             static_cast<std::uint32_t>(m_geometry_culling_blocks.size()),
					                             {}});
				culling_pass.dispatch_for(
				    {static_cast<std::uint32_t>(m_geometry_culling_blocks.size()), 1, 1});
			}

			render_pass pass(cmd, target, m_clear_color,
			                 m_preserve_target_contents ? render_pass::loader::load
			                                            : render_pass::loader::clear);
			const SDL_GPUViewport viewport{.x = 0.0f,
			                               .y = 0.0f,
			                               .w = static_cast<float>(target.width()),
			                               .h = static_cast<float>(target.height()),
			                               .min_depth = 0.0f,
			                               .max_depth = 1.0f};
			SDL_SetGPUViewport(pass.raw(), &viewport);

			if (!m_instances.empty())
			{
				SDL_PushGPUVertexUniformData(cmd.raw(), 0, &view_projection,
				                             static_cast<std::uint32_t>(sizeof(view_projection)));
				SDL_GPUGraphicsPipeline* bound_pipeline = nullptr;
				std::optional<filter_mode> bound_filter;
				std::optional<quad_effect> bound_effect;
				std::optional<SDL_Rect> bound_scissor;
				SDL_GPUTexture* bound_texture = nullptr;
				SDL_GPUBuffer* bound_vertex_buffer = nullptr;
				SDL_GPUBuffer* bound_instance_buffer = nullptr;
				std::uint32_t bound_instance_offset = 0;
				SDL_GPUBuffer* bound_index_buffer = nullptr;
				bool has_bound_index_buffer = false;
				const SDL_Rect full_scissor{0, 0, static_cast<int>(target.width()),
				                            static_cast<int>(target.height())};

				for (const auto& batch : m_batches)
				{
					if (batch.instance_count == 0 || (batch.geometry && batch.vertex_count == 0))
					{
						continue;
					}

					if (batch.material.valid())
					{
						const auto& instance = resolve_material_instance(batch.material);
						const auto& material = resolve_material(instance.material);
						auto* pipeline = material.pipeline.raw();
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
							    pass.raw(), 0, bindings.storage_textures.data(),
							    static_cast<std::uint32_t>(bindings.storage_textures.size()));
						}
						if (!bindings.storage_buffers.empty())
						{
							SDL_BindGPUFragmentStorageBuffers(
							    pass.raw(), 0, bindings.storage_buffers.data(),
							    static_cast<std::uint32_t>(bindings.storage_buffers.size()));
						}
						for (std::uint32_t slot = 0; slot < bindings.uniform_blocks.size(); ++slot)
						{
							const auto& block = bindings.uniform_blocks[slot];
							SDL_PushGPUFragmentUniformData(cmd.raw(), slot, block.data(),
							                               static_cast<std::uint32_t>(block.size()));
						}

						bound_texture = nullptr;
						bound_filter.reset();
						bound_effect.reset();
					}
					else
					{
						auto* pipeline = m_shared_default_resources->pipelines[blend_index(batch.blend)].raw();
						if (bound_pipeline != pipeline)
						{
							pass.bind_pipeline(m_shared_default_resources->pipelines[blend_index(batch.blend)]);
							bound_pipeline = pipeline;
						}
						if (bound_texture != batch.texture || !bound_filter || *bound_filter != batch.filter)
						{
							pass.bind_fragment_sampler(0, batch.texture,
							                           m_shared_default_resources->samplers[filter_index(batch.filter)].raw());
							bound_texture = batch.texture;
							bound_filter = batch.filter;
						}
						if (!bound_effect || *bound_effect != batch.effect)
						{
							const std::array<std::uint32_t, 4> material_uniform{
							    static_cast<std::uint32_t>(effect_index(batch.effect)), 0, 0, 0};
							SDL_PushGPUFragmentUniformData(
							    cmd.raw(), 0, material_uniform.data(),
							    static_cast<std::uint32_t>(sizeof(material_uniform)));
							bound_effect = batch.effect;
						}
					}

					const SDL_Rect scissor = batch.scissor.value_or(full_scissor);
					if (!bound_scissor || !equal_scissor(*bound_scissor, scissor))
					{
						SDL_SetGPUScissor(pass.raw(), &scissor);
						bound_scissor = scissor;
					}

					const std::uint32_t instance_offset =
					    batch.first_instance * static_cast<std::uint32_t>(sizeof(sprite_instance));
					SDL_GPUBuffer* vertex_buffer = nullptr;
					if (batch.resident_mesh)
					{
						vertex_buffer = batch.resident_mesh->vertex_buffer.raw();
					}
					else if (batch.geometry)
					{
						vertex_buffer = m_geometry_buffer.raw();
					}
					else
					{
						vertex_buffer = m_shared_default_resources->quad_vertices.raw();
						if (!has_bound_index_buffer ||
						    bound_index_buffer != m_shared_default_resources->quad_indices.raw())
						{
							pass.bind_index_buffer(m_shared_default_resources->quad_indices.raw(),
							                       SDL_GPU_INDEXELEMENTSIZE_16BIT);
							bound_index_buffer = m_shared_default_resources->quad_indices.raw();
							has_bound_index_buffer = true;
						}
					}

					const bool instance_binding_changed = bound_instance_buffer != m_instance_buffer.raw() ||
					                                      bound_instance_offset != instance_offset;
					if (bound_vertex_buffer != vertex_buffer)
					{
						if (instance_binding_changed)
						{
							const std::array<SDL_GPUBufferBinding, 2> bindings{
							    SDL_GPUBufferBinding{.buffer = vertex_buffer, .offset = 0},
							    SDL_GPUBufferBinding{.buffer = m_instance_buffer.raw(),
								                     .offset = instance_offset}};
							pass.bind_vertex_buffers(0, bindings);
							bound_instance_buffer = m_instance_buffer.raw();
							bound_instance_offset = instance_offset;
						}
						else
						{
							pass.bind_vertex_buffer(0, vertex_buffer);
						}
						bound_vertex_buffer = vertex_buffer;
					}
					else if (instance_binding_changed)
					{
						pass.bind_vertex_buffer(1, m_instance_buffer.raw(), instance_offset);
						bound_instance_buffer = m_instance_buffer.raw();
						bound_instance_offset = instance_offset;
					}
					if (batch.resident_mesh)
					{
						pass.draw_primitives(batch.vertex_count, batch.instance_count);
					}
					else if (batch.geometry)
					{
						if (m_gpu_culling_enabled)
						{
							const auto command_offset = static_cast<std::uint32_t>(
							    batch.first_geometry_block * sizeof(SDL_GPUIndirectDrawCommand));
							SDL_DrawGPUPrimitivesIndirect(pass.raw(), m_geometry_indirect_commands.raw(),
							                              command_offset, batch.geometry_block_count);
						}
						else
						{
							pass.draw_primitives(batch.vertex_count, 1, batch.first_vertex);
						}
					}
					else
					{
						pass.draw_indexed_primitives(6, batch.instance_count);
					}
				}
			}
			pass.end_pass();
			if (profile)
			{
				const auto finished_at = profile_clock::now();
				m_last_stats.command_encoding_cpu_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
				    finished_at - command_encoding_started_at);
				m_last_stats.flush_cpu_time =
				    std::chrono::duration_cast<std::chrono::nanoseconds>(finished_at - flush_started_at);
			}
		}

		[[nodiscard]] const frame_stats& last_frame_stats() const noexcept { return m_last_stats; }

		[[nodiscard]] texture_format target_format() const noexcept { return m_target_format; }

		void enable_gpu_culling(bool enabled = true)
		{
			ensure_resource_changes_allowed();
			apply_gpu_culling(enabled);
			m_gpu_culling_overridden = true;
		}

		void disable_gpu_culling() { enable_gpu_culling(false); }

		void use_global_gpu_culling()
		{
			ensure_resource_changes_allowed();
			m_gpu_culling_overridden = false;
			apply_gpu_culling(::mngl::gpu::gpu_culling_enabled());
		}

		[[nodiscard]] bool gpu_culling_enabled() const noexcept { return m_gpu_culling_enabled; }

		void enable_profiling(bool enabled = true)
		{
			if (m_frame_active) [[unlikely]]
			{
				throw std::logic_error("Profiling cannot be changed while a batcher2d frame is active");
			}
			m_profiling_enabled = enabled;
		}

		[[nodiscard]] bool profiling_enabled() const noexcept { return m_profiling_enabled; }

	private:
		void apply_gpu_culling(bool enabled)
		{
			if (enabled && !m_vertex_instance_culling_supported) [[unlikely]]
			{
				throw std::logic_error("GPU culling requires a vertex shader that declares and consumes the "
				                       "instance-visibility mask");
			}
			if (enabled && (!m_geometry_culling_pipeline.valid() || !m_instance_culling_pipeline.valid()))
			{
				initialize_gpu_culling();
			}
			m_gpu_culling_enabled = enabled;
		}
		friend class ::mngl::two_d::renderer;

		// The renderer checks its public frame/context boundary before reaching these fast paths.
		void draw_quad_deferred(const texture_region_view& source, const quad_transform& transform,
		                        const color& tint, const quad_draw_state& state,
		                        std::uint64_t deferred_token)
		{
			ensure_frame_active();
			draw_quad_unchecked(source, transform, tint, state, deferred_token);
		}

		void draw_quad_deferred(recording_context& context, const texture_region_view& source,
		                        const quad_transform& transform, const color& tint,
		                        const quad_draw_state& state, std::uint64_t deferred_token) const
		{
			ensure_recording_context(context, false);
			draw_quad_unchecked(context, source, transform, tint, state, deferred_token);
		}

		void draw_quad_unchecked(const texture_region_view& source, const quad_transform& transform,
		                         const color& tint, const quad_draw_state& state,
		                         std::uint64_t deferred_token = 0)
		{
			const bool allow_extension = !m_batch_extension_blocked;
			const std::size_t batch_count = m_batches.size();
			draw_quad_into(m_instances, m_batches, source, transform, tint, state, allow_extension,
			               deferred_token);
			if (!allow_extension && m_batches.size() != batch_count)
			{
				m_batch_extension_blocked = false;
			}
		}

		void draw_quad_unchecked(recording_context& context, const texture_region_view& source,
		                         const quad_transform& transform, const color& tint,
		                         const quad_draw_state& state, std::uint64_t deferred_token = 0) const
		{
			draw_quad_into(context.m_instances, context.m_batches, source, transform, tint, state, true,
			               deferred_token);
		}

		template <bool vertices_validated = false>
		void draw_geometry_unchecked(std::span<const glm::vec2> vertices, const texture_region_view& source,
		                             const glm::mat4& transform, const color& tint,
		                             const quad_draw_state& state)
		{
			const bool allow_extension = !m_batch_extension_blocked;
			const std::size_t batch_count = m_batches.size();
			draw_geometry_into<vertices_validated>(m_instances, m_geometry_vertices, m_geometry_blocks,
			                                       m_batches, vertices, source, transform, tint, state,
			                                       allow_extension);
			if (!allow_extension && m_batches.size() != batch_count)
			{
				m_batch_extension_blocked = false;
			}
		}

		template <bool vertices_validated = false>
		void draw_geometry_unchecked(recording_context& context, std::span<const glm::vec2> vertices,
		                             const texture_region_view& source, const glm::mat4& transform,
		                             const color& tint, const quad_draw_state& state) const
		{
			draw_geometry_into<vertices_validated>(context.m_instances, context.m_geometry_vertices,
			                                       context.m_geometry_blocks, context.m_batches, vertices,
			                                       source, transform, tint, state);
		}

		void draw_mesh_unchecked(
		    const std::shared_ptr<::mngl::two_d::detail::resident_mesh_storage>& mesh_storage,
		    const texture_region_view& source, const glm::mat4& transform, const color& tint,
		    const quad_draw_state& state)
		{
			const bool allow_extension = !m_batch_extension_blocked;
			const std::size_t batch_count = m_batches.size();
			draw_mesh_into(m_instances, m_batches, mesh_storage, source, transform, tint, state,
			               allow_extension);
			if (!allow_extension && m_batches.size() != batch_count)
			{
				m_batch_extension_blocked = false;
			}
		}

		void draw_mesh_unchecked(
		    recording_context& context,
		    const std::shared_ptr<::mngl::two_d::detail::resident_mesh_storage>& mesh_storage,
		    const texture_region_view& source, const glm::mat4& transform, const color& tint,
		    const quad_draw_state& state) const
		{
			draw_mesh_into(context.m_instances, context.m_batches, mesh_storage, source, transform, tint,
			               state);
		}

		[[nodiscard]] material_handle create_material_for_active_frame(
		    const graphics_shader_asset& fragment_shader, sprite_blend_mode blend)
		{
			if (!m_frame_active) [[unlikely]]
			{
				throw std::logic_error("A frame-scoped material requires an active batcher2d frame");
			}
			return create_material_impl(fragment_shader, blend);
		}

		[[nodiscard]] material_instance_handle create_material_instance_for_active_frame(
		    material_handle material, fragment_resource_bindings bindings)
		{
			if (!m_frame_active) [[unlikely]]
			{
				throw std::logic_error("A frame-scoped material instance requires an active batcher2d frame");
			}
			return create_material_instance_impl(material, std::move(bindings));
		}

		[[nodiscard]] material_handle create_material_impl(const graphics_shader_asset& fragment_shader,
		                                                   sprite_blend_mode blend)
		{
			// The frame only queues CPU-side draws; the render pass starts later in flush().
			if (fragment_shader.stage != graphics_shader_stage::fragment || !fragment_shader.valid())
			    [[unlikely]]
			{
				throw std::invalid_argument(
				    "Custom batcher2d material requires a valid fragment shader asset");
			}
			(void)blend_index(blend);

			graphics_shader shader(m_device, fragment_shader);
			auto pipeline = build_pipeline(shader, blend);
			custom_material value{std::move(shader), std::move(pipeline), fragment_shader.resources, blend};

			if (!m_free_material_slots.empty())
			{
				const std::uint32_t index = m_free_material_slots.back();
				m_free_material_slots.pop_back();
				auto& slot = m_custom_materials[index];
				slot.value.emplace(std::move(value));
				return material_handle{index, slot.generation};
			}
			if (m_custom_materials.size() >= material_handle::invalid_value) [[unlikely]]
			{
				throw std::length_error("batcher2d exhausted its custom material handle space");
			}

			custom_material_slot slot{};
			slot.value.emplace(std::move(value));
			m_custom_materials.push_back(std::move(slot));
			const auto index = static_cast<std::uint32_t>(m_custom_materials.size() - 1);
			return material_handle{index, m_custom_materials[index].generation};
		}

		[[nodiscard]] material_instance_handle create_material_instance_impl(
		    material_handle material, fragment_resource_bindings bindings)
		{
			validate_material_bindings(material, bindings);
			custom_material_instance value{material, std::move(bindings)};

			if (!m_free_material_instance_slots.empty())
			{
				const std::uint32_t index = m_free_material_instance_slots.back();
				m_free_material_instance_slots.pop_back();
				auto& slot = m_custom_material_instances[index];
				slot.value.emplace(std::move(value));
				return material_instance_handle{index, slot.generation};
			}
			if (m_custom_material_instances.size() >= material_instance_handle::invalid_value) [[unlikely]]
			{
				throw std::length_error("batcher2d exhausted its material instance handle space");
			}

			custom_material_instance_slot slot{};
			slot.value.emplace(std::move(value));
			m_custom_material_instances.push_back(std::move(slot));
			const auto index = static_cast<std::uint32_t>(m_custom_material_instances.size() - 1);
			return material_instance_handle{index, m_custom_material_instances[index].generation};
		}

		static std::size_t blend_index(sprite_blend_mode blend)
		{
			switch (blend)
			{
			case sprite_blend_mode::alpha:
				return 0;
			case sprite_blend_mode::premultiplied_alpha:
				return 1;
			case sprite_blend_mode::opaque:
				return 2;
			case sprite_blend_mode::additive:
				return 3;
			default:
				throw std::invalid_argument("Unsupported batcher2d blend mode");
			}
		}

		static std::size_t filter_index(filter_mode filter)
		{
			switch (filter)
			{
			case filter_mode::linear:
				return 0;
			case filter_mode::nearest:
				return 1;
			default:
				throw std::invalid_argument("Unsupported batcher2d filter mode");
			}
		}

		static std::size_t effect_index(quad_effect effect)
		{
			switch (effect)
			{
			case quad_effect::textured:
				return 0;
			case quad_effect::circle:
				return 1;
			case quad_effect::ellipse:
				return 2;
			case quad_effect::rounded_rectangle:
				return 3;
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

		graphics_pipeline build_pipeline(const graphics_shader& fragment_shader,
		                                 sprite_blend_mode blend) const
		{
			return graphics_pipeline_builder{}
			    .set_debug_name(std::string("MNGL 2D batcher blend mode ") +
			                    std::to_string(static_cast<unsigned int>(blend)))
			    .set_vertex_shader(m_shared_default_resources->vertex_shader)
			    .set_fragment_shader(fragment_shader)
			    .set_vertex_layout(m_vertex_layout)
			    .set_primitive_type(primitive_type::triangle_list)
			    .set_cull_mode(cull_mode::none)
			    .add_color_target(static_cast<SDL_GPUTextureFormat>(m_target_format), blend_state_for(blend))
			    .build(m_device);
		}

		static std::shared_ptr<const default_gpu_resources> acquire_default_gpu_resources(
		    const device& device, texture_format target_format, const batcher2d_shader_assets& shaders)
		{
			if (!shaders.shareable_default)
			{
				return create_default_gpu_resources(device, target_format, shaders);
			}

			static std::mutex cache_mutex;
			static std::unordered_map<std::string, std::weak_ptr<const default_gpu_resources>> cache;
			const auto key = default_gpu_resources_key(device, target_format, shaders);
			std::lock_guard lock(cache_mutex);
			for (auto entry = cache.begin(); entry != cache.end();)
			{
				if (entry->second.expired())
				{
					entry = cache.erase(entry);
				}
				else
				{
					++entry;
				}
			}

			if (const auto found = cache.find(key); found != cache.end())
			{
				if (auto shared = found->second.lock())
				{
					return shared;
				}
			}

			auto created = create_default_gpu_resources(device, target_format, shaders);
			cache.insert_or_assign(key, created);
			return created;
		}

		static std::string default_gpu_resources_key(const device& device, texture_format target_format,
		                                             const batcher2d_shader_assets& shaders)
		{
			std::string key;
			auto append = [&](const void* data, std::size_t size)
			{
				key.append(static_cast<const char*>(data), size);
			};
			auto append_value = [&]<typename T>(const T& value)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				append(&value, sizeof(value));
			};
			auto append_string = [&](std::string_view value)
			{
				const auto size = value.size();
				append_value(size);
				append(value.data(), value.size());
			};
			auto append_shader = [&](const graphics_shader_asset& shader)
			{
				append_value(shader.stage);
				append_value(shader.code.format);
				append_string(shader.entrypoint);
				append_value(shader.resources.samplers);
				append_value(shader.resources.storage_textures);
				append_value(shader.resources.storage_buffers);
				append_value(shader.resources.uniform_buffers);
				append_value(shader.resources.read_write_storage_textures);
				append_value(shader.resources.read_write_storage_buffers);
				const auto size = shader.code.bytes.size();
				append_value(size);
				append(shader.code.bytes.data(), size);
			};

		const auto device_address = reinterpret_cast<std::uintptr_t>(device.raw());
		append_value(device_address);
		append_value(target_format);
		append_value(shaders.supports_instance_culling);
		append_shader(shaders.vertex);
		append_shader(shaders.fragment);
		return key;
		}

		static std::shared_ptr<const default_gpu_resources> create_default_gpu_resources(
		    const device& device, texture_format target_format, const batcher2d_shader_assets& shaders)
		{
			auto resources = std::make_shared<default_gpu_resources>(device);
			resources->supports_instance_culling = shaders.supports_instance_culling;
			resources->vertex_layout.add_buffer(0, sizeof(quad_vertex));
			resources->vertex_layout.add_attribute(0, 0, vertex_elemetn_fmt::float2, offsetof(quad_vertex, x));
			resources->vertex_layout.add_buffer(1, sizeof(sprite_instance), vertex_input_rate::instance);
			resources->vertex_layout.add_attribute(1, 1, vertex_elemetn_fmt::float2,
			                                      offsetof(sprite_instance, transform_x));
			resources->vertex_layout.add_attribute(2, 1, vertex_elemetn_fmt::float2,
			                                      offsetof(sprite_instance, transform_y));
			resources->vertex_layout.add_attribute(3, 1, vertex_elemetn_fmt::float2,
			                                      offsetof(sprite_instance, transform_translation));
			resources->vertex_layout.add_attribute(4, 1, vertex_elemetn_fmt::float2,
			                                      offsetof(sprite_instance, effect_parameters));
			resources->vertex_layout.add_attribute(5, 1, vertex_elemetn_fmt::float4,
			                                      offsetof(sprite_instance, uv));
			resources->vertex_layout.add_attribute(6, 1, vertex_elemetn_fmt::ubyte4_norm,
			                                      offsetof(sprite_instance, tint));
			resources->vertex_layout.add_attribute(7, 0, vertex_elemetn_fmt::float4,
			                                      offsetof(quad_vertex, tint));

			resources->vertex_shader = graphics_shader(device, shaders.vertex);
			resources->fragment_shader = graphics_shader(device, shaders.fragment);
			for (std::uint8_t index = 0;
			     index <= static_cast<std::uint8_t>(sprite_blend_mode::additive); ++index)
			{
				const auto blend = static_cast<sprite_blend_mode>(index);
				resources->pipelines[blend_index(blend)] =
				    graphics_pipeline_builder{}
				        .set_debug_name(std::string("MNGL 2D batcher blend mode ") +
				                        std::to_string(static_cast<unsigned int>(blend)))
				        .set_vertex_shader(resources->vertex_shader)
				        .set_fragment_shader(resources->fragment_shader)
				        .set_vertex_layout(resources->vertex_layout)
				        .set_primitive_type(primitive_type::triangle_list)
				        .set_cull_mode(cull_mode::none)
				        .add_color_target(static_cast<SDL_GPUTextureFormat>(target_format),
				                          blend_state_for(blend))
				        .build(device);
			}
			resources->samplers[filter_index(filter_mode::linear)] =
			    sampler_builder::create_linear_clamp(device);
			resources->samplers[filter_index(filter_mode::nearest)] =
			    sampler_builder::create_point_clamp(device);

			const std::array<quad_vertex, 4> vertices{
			    {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}}};
			const std::array<std::uint16_t, 6> indices{0, 1, 2, 2, 1, 3};
			command_buffer commands(device);
			resources->quad_vertices = buffer::create_with_data(
			    device, commands, std::span<const quad_vertex>(vertices), buffer_usage::vertex);
			resources->quad_indices = buffer::create_with_data(
			    device, commands, std::span<const std::uint16_t>(indices), buffer_usage::index);
			commands.submit();
			return resources;
		}

		void ensure_resource_changes_allowed() const
		{
			if (m_frame_active) [[unlikely]]
			{
				throw std::logic_error("Material resources cannot change while a batcher2d frame is active");
			}
		}

		static std::uint32_t next_generation(std::uint32_t generation) noexcept
		{
			++generation;
			return generation == 0 ? 1 : generation;
		}

		const custom_material& resolve_material(material_handle handle) const
		{
			if (!handle.valid() || handle.value >= m_custom_materials.size()) [[unlikely]]
			{
				throw std::out_of_range("Invalid batcher2d material handle");
			}
			const auto& slot = m_custom_materials[handle.value];
			if (slot.generation != handle.generation || !slot.value) [[unlikely]]
			{
				throw std::out_of_range("Stale batcher2d material handle");
			}
			return *slot.value;
		}

		const custom_material_instance& resolve_material_instance(material_instance_handle handle) const
		{
			if (!handle.valid() || handle.value >= m_custom_material_instances.size()) [[unlikely]]
			{
				throw std::out_of_range("Invalid batcher2d material instance handle");
			}
			const auto& slot = m_custom_material_instances[handle.value];
			if (slot.generation != handle.generation || !slot.value) [[unlikely]]
			{
				throw std::out_of_range("Stale batcher2d material instance handle");
			}
			return *slot.value;
		}

		custom_material_instance& resolve_material_instance(material_instance_handle handle)
		{
			if (!handle.valid() || handle.value >= m_custom_material_instances.size()) [[unlikely]]
			{
				throw std::out_of_range("Invalid batcher2d material instance handle");
			}
			auto& slot = m_custom_material_instances[handle.value];
			if (slot.generation != handle.generation || !slot.value) [[unlikely]]
			{
				throw std::out_of_range("Stale batcher2d material instance handle");
			}
			return *slot.value;
		}

		void validate_material_bindings(material_handle material,
		                                const fragment_resource_bindings& bindings) const
		{
			const auto& resources = resolve_material(material).resources;
			if (bindings.m_device && bindings.m_device != m_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("Fragment material resources belong to a different GPU device");
			}
			if (bindings.samplers.size() != resources.samplers ||
			    bindings.storage_textures.size() != resources.storage_textures ||
			    bindings.storage_buffers.size() != resources.storage_buffers ||
			    bindings.uniform_blocks.size() != resources.uniform_buffers) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Fragment material bindings do not match the shader resource layout");
			}

			for (const auto& binding : bindings.samplers)
			{
				if (!binding.texture || !binding.sampler) [[unlikely]]
				{
					throw std::invalid_argument("Fragment material has a null sampled texture or sampler");
				}
			}
			for (auto* texture_handle : bindings.storage_textures)
			{
				if (!texture_handle) [[unlikely]]
				{
					throw std::invalid_argument("Fragment material has a null storage texture");
				}
			}
			for (auto* buffer_handle : bindings.storage_buffers)
			{
				if (!buffer_handle) [[unlikely]]
				{
					throw std::invalid_argument("Fragment material has a null storage buffer");
				}
			}
			for (const auto& uniform : bindings.uniform_blocks)
			{
				if (uniform.empty() || uniform.size() % 16 != 0 ||
				    uniform.size() > std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
				{
					throw std::invalid_argument(
					    "Fragment uniform blocks must be non-empty std140-sized payloads");
				}
			}
		}

		static bool equal_scissor(const SDL_Rect& left, const SDL_Rect& right) noexcept
		{
			return left.x == right.x && left.y == right.y && left.w == right.w && left.h == right.h;
		}

		[[nodiscard]] bool batch_state_less(const draw_batch& left, const draw_batch& right)
		{
			if (left.material.valid() != right.material.valid())
			{
				return !left.material.valid();
			}

			if (left.material.valid())
			{
				const auto& left_instance = resolve_material_instance(left.material);
				const auto& right_instance = resolve_material_instance(right.material);
				if (left_instance.material.value != right_instance.material.value)
				{
					return left_instance.material.value < right_instance.material.value;
				}
				if (left_instance.material.generation != right_instance.material.generation)
				{
					return left_instance.material.generation < right_instance.material.generation;
				}
				if (left.material.value != right.material.value)
				{
					return left.material.value < right.material.value;
				}
				if (left.material.generation != right.material.generation)
				{
					return left.material.generation < right.material.generation;
				}
			}
			else
			{
				if (left.blend != right.blend)
				{
					return left.blend < right.blend;
				}
				if (left.texture != right.texture)
				{
					return std::less<SDL_GPUTexture*>{}(left.texture, right.texture);
				}
				if (left.filter != right.filter)
				{
					return left.filter < right.filter;
				}
				if (left.effect != right.effect)
				{
					return left.effect < right.effect;
				}
			}

			if (left.scissor.has_value() != right.scissor.has_value())
			{
				return !left.scissor.has_value();
			}
			if (left.scissor && right.scissor)
			{
				const auto left_rect =
				    std::array{left.scissor->x, left.scissor->y, left.scissor->w, left.scissor->h};
				const auto right_rect =
				    std::array{right.scissor->x, right.scissor->y, right.scissor->w, right.scissor->h};
				if (left_rect != right_rect)
				{
					return left_rect < right_rect;
				}
			}

			const std::uint8_t left_source = left.resident_mesh ? 2 : (left.geometry ? 1 : 0);
			const std::uint8_t right_source = right.resident_mesh ? 2 : (right.geometry ? 1 : 0);
			if (left_source != right_source)
			{
				return left_source < right_source;
			}
			if (left.resident_mesh && right.resident_mesh && left.resident_mesh != right.resident_mesh)
			{
				return std::less<const ::mngl::two_d::detail::resident_mesh_storage*>{}(
				    left.resident_mesh.get(), right.resident_mesh.get());
			}
			return false;
		}

		void sort_order_independent_batches()
		{
			for (std::size_t range_index = 0; range_index < m_order_independent_batch_ranges.size();
			     ++range_index)
			{
				auto& range = m_order_independent_batch_ranges[range_index];
				const auto old_batch_count = range.batch_count;
				if (old_batch_count < 2)
				{
					continue;
				}

				auto first = m_batches.begin() + static_cast<std::ptrdiff_t>(range.first_batch);
				auto last = first + static_cast<std::ptrdiff_t>(old_batch_count);
				std::size_t first_instance = std::numeric_limits<std::size_t>::max();
				for (auto batch = first; batch != last; ++batch)
				{
					first_instance =
					    std::min(first_instance, static_cast<std::size_t>(batch->first_instance));
				}

				std::sort(first, last, [this](const draw_batch& left, const draw_batch& right)
				          { return batch_state_less(left, right); });

				// Sorting batch metadata also changes the order of the instance ranges those
				// batches refer to, even when no adjacent batches can be merged.
				m_order_independent_instance_scratch.clear();
				std::size_t write_batch = range.first_batch;
				for (std::size_t read_batch = range.first_batch;
				     read_batch < range.first_batch + old_batch_count; ++read_batch)
				{
					auto batch = m_batches[read_batch];
					const std::size_t source_first = batch.first_instance;
					const std::size_t source_count = batch.instance_count;
					if (source_first > m_instances.size() || source_count > m_instances.size() - source_first)
					    [[unlikely]]
					{
						throw std::logic_error(
						    "batcher2d order-independent batch has an invalid instance range");
					}
					constexpr std::size_t max_instances =
					    std::numeric_limits<std::uint32_t>::max() / sizeof(sprite_instance);
					if (first_instance > max_instances ||
					    m_order_independent_instance_scratch.size() > max_instances - first_instance ||
					    source_count > max_instances - first_instance -
					                       m_order_independent_instance_scratch.size()) [[unlikely]]
					{
						throw std::length_error("batcher2d sorted instance range exceeds SDL's 32-bit limit");
					}

					const auto sorted_first_instance = static_cast<std::uint32_t>(
					    first_instance + m_order_independent_instance_scratch.size());
					m_order_independent_instance_scratch.insert(
					    m_order_independent_instance_scratch.end(),
					    m_instances.begin() + static_cast<std::ptrdiff_t>(source_first),
					    m_instances.begin() + static_cast<std::ptrdiff_t>(source_first + source_count));

					if (write_batch != range.first_batch &&
					    can_merge_order_independent_batches(m_batches[write_batch - 1], batch))
					{
						auto& merged = m_batches[write_batch - 1];
						if (batch.instance_count >
						    std::numeric_limits<std::uint32_t>::max() - merged.instance_count) [[unlikely]]
						{
							throw std::length_error(
							    "batcher2d merged instance count exceeds SDL's 32-bit limit");
						}
						merged.instance_count += batch.instance_count;
					}
					else
					{
						batch.first_instance = sorted_first_instance;
						m_batches[write_batch++] = std::move(batch);
					}
				}

				std::copy(m_order_independent_instance_scratch.begin(),
				          m_order_independent_instance_scratch.end(),
				          m_instances.begin() + static_cast<std::ptrdiff_t>(first_instance));
				const auto new_batch_count = write_batch - range.first_batch;
				const auto removed_batch_count = old_batch_count - new_batch_count;
				if (removed_batch_count != 0)
				{
					const auto old_batch_end = range.first_batch + old_batch_count;
					m_batches.erase(m_batches.begin() + static_cast<std::ptrdiff_t>(write_batch),
					                m_batches.begin() + static_cast<std::ptrdiff_t>(old_batch_end));
					for (std::size_t next_range = range_index + 1;
					     next_range < m_order_independent_batch_ranges.size(); ++next_range)
					{
						if (m_order_independent_batch_ranges[next_range].first_batch >= old_batch_end)
						{
							m_order_independent_batch_ranges[next_range].first_batch -= removed_batch_count;
						}
					}
				}
				range.batch_count = new_batch_count;
			}
		}

		[[nodiscard]] static bool can_merge_order_independent_batches(const draw_batch& left,
		                                                              const draw_batch& right) noexcept
		{
			if (left.deferred_token != right.deferred_token || left.geometry || right.geometry ||
			    left.resident_mesh != right.resident_mesh ||
			    left.material != right.material || left.scissor.has_value() != right.scissor.has_value() ||
			    (left.scissor && !equal_scissor(*left.scissor, *right.scissor)))
			{
				return false;
			}
			if (left.material.valid())
			{
				return true;
			}
			return left.texture == right.texture && left.filter == right.filter &&
			       left.blend == right.blend && left.effect == right.effect;
		}

		static void register_recording_context(recording_context& context)
		{
			if (!context.m_frame_state) [[unlikely]]
			{
				throw std::logic_error("batcher2d recording context requires an active frame");
			}

			auto current = context.m_frame_state->outstanding_contexts.load(std::memory_order_relaxed);
			for (;;)
			{
				if (current == std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
				{
					throw std::length_error("batcher2d frame exhausted its recording-context count");
				}
				if (context.m_frame_state->outstanding_contexts.compare_exchange_weak(
				        current, current + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
				{
					context.m_registered = true;
					return;
				}
			}
		}

		void ensure_recording_context(const recording_context& context, bool allow_finished) const
		{
			ensure_frame_active();
			if (context.m_owner != this || context.m_frame_generation != m_frame_generation ||
			    context.m_frame_state != m_recording_state || !context.m_registered) [[unlikely]]
			{
				throw std::invalid_argument(
				    "batcher2d recording context belongs to a different or expired frame");
			}
			if (!allow_finished && context.m_sealed) [[unlikely]]
			{
				throw std::logic_error("batcher2d recording context has already been finished");
			}
		}

		void seal_recording_context(recording_context& context) const
		{
			ensure_recording_context(context, false);
			context.m_sealed = true;
		}

		void draw_quad_into(std::vector<sprite_instance>& instances, std::vector<draw_batch>& batches,
		                    const texture_region_view& source, const quad_transform& transform,
		                    const color& tint, const quad_draw_state& state,
		                    bool allow_extension = true, std::uint64_t deferred_token = 0) const
		{
			const auto& destination = transform.destination;
			if (!std::isfinite(destination.pos.x) || !std::isfinite(destination.pos.y) ||
			    !std::isfinite(destination.size.x) || !std::isfinite(destination.size.y) ||
			    destination.size.x < 0.0f || destination.size.y < 0.0f || !std::isfinite(transform.pivot.x) ||
			    !std::isfinite(transform.pivot.y) || !std::isfinite(transform.rotation_radians)) [[unlikely]]
			{
				throw std::invalid_argument(
				    "batcher2d transform values must be finite with non-negative dimensions");
			}
			const auto components = transform.model_components();
			if (!std::isfinite(components.transform_x.x) || !std::isfinite(components.transform_x.y) ||
			    !std::isfinite(components.transform_y.x) || !std::isfinite(components.transform_y.y) ||
			    !std::isfinite(components.translation.x) || !std::isfinite(components.translation.y))
			    [[unlikely]]
			{
				throw std::invalid_argument("batcher2d model matrix must be finite");
			}
			if (destination.size.x == 0.0f || destination.size.y == 0.0f)
			{
				return;
			}
			if (!std::isfinite(source.uv_bounds.x) || !std::isfinite(source.uv_bounds.y) ||
			    !std::isfinite(source.uv_bounds.z) || !std::isfinite(source.uv_bounds.w)) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d UV coordinates must be finite");
			}
			const custom_material_instance* custom_instance = nullptr;
			if (state.material.valid())
			{
				custom_instance = &resolve_material_instance(state.material);
			}
			else if (!source.page || !source.page->valid() ||
			         !source.page->has_usage(texture::usage_flags::sampler)) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d source texture must be valid and sampler-enabled");
			}
			if (state.scissor && (state.scissor->w < 0 || state.scissor->h < 0)) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d scissor dimensions must be non-negative");
			}
			if (state.scissor && (state.scissor->w == 0 || state.scissor->h == 0))
			{
				return;
			}
			if (!std::isfinite(state.effect_parameters.x) || !std::isfinite(state.effect_parameters.y))
			    [[unlikely]]
			{
				throw std::invalid_argument("batcher2d effect parameters must be finite");
			}

			const sprite_blend_mode blend =
			    custom_instance ? resolve_material(custom_instance->material).blend : state.blend;
			if (!custom_instance)
			{
				(void)blend_index(state.blend);
				(void)filter_index(state.filter);
				(void)effect_index(state.effect);
			}

			constexpr std::size_t max_instances =
			    std::numeric_limits<std::uint32_t>::max() / sizeof(sprite_instance);
			if (instances.size() >= max_instances) [[unlikely]]
			{
				throw std::length_error("batcher2d frame exceeds SDL's 32-bit buffer size limit");
			}

			const auto rgba = tint.rgba_glsl();
			sprite_instance instance{
			    .transform_x = {components.transform_x.x, components.transform_x.y},
			    .transform_y = {components.transform_y.x, components.transform_y.y},
			    .transform_translation = {components.translation.x, components.translation.y},
			    .effect_parameters = {state.effect_parameters.x, state.effect_parameters.y},
			    .uv = {source.uv_bounds.x, source.uv_bounds.w, source.uv_bounds.z, source.uv_bounds.y},
			    .tint = {pack_channel(rgba.r), pack_channel(rgba.g), pack_channel(rgba.b),
				         pack_channel(rgba.a)}};

			auto* raw_texture = source.page ? source.page->raw() : nullptr;
			if (allow_extension && !batches.empty() && !batches.back().geometry &&
			    !batches.back().resident_mesh &&
			    same_batch_key(batches.back(), raw_texture, state, deferred_token))
			{
				instances.push_back(instance);
				++batches.back().instance_count;
				return;
			}

			batches.push_back(draw_batch{.texture = raw_texture,
			                             .filter = state.filter,
			                             .blend = blend,
			                             .effect = state.effect,
			                             .scissor = state.scissor,
			                             .material = state.material,
			                             .deferred_token = deferred_token,
			                             .first_instance = static_cast<std::uint32_t>(instances.size()),
			                             .instance_count = 1});
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

		void draw_mesh_into(std::vector<sprite_instance>& instances, std::vector<draw_batch>& batches,
		                    const std::shared_ptr<::mngl::two_d::detail::resident_mesh_storage>& mesh_storage,
		                    const texture_region_view& source, const glm::mat4& transform, const color& tint,
		                    const quad_draw_state& state, bool allow_extension = true) const
		{
			if (!mesh_storage || !mesh_storage->vertex_buffer.valid() || mesh_storage->vertex_count == 0)
			    [[unlikely]]
			{
				throw std::invalid_argument("batcher2d requires a valid persistent geometry mesh");
			}
			if (mesh_storage->device.raw() != m_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("2D mesh and renderer must use the same GPU device");
			}
			if (state.effect != quad_effect::textured) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d meshes require the textured effect");
			}
			if (!source.page || !source.page->valid() ||
			    !source.page->has_usage(texture::usage_flags::sampler)) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d mesh tint texture must be valid and sampler-enabled");
			}
			if (state.scissor && (state.scissor->w < 0 || state.scissor->h < 0)) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d scissor dimensions must be non-negative");
			}
			if (state.scissor && (state.scissor->w == 0 || state.scissor->h == 0))
			{
				return;
			}
			(void)blend_index(state.blend);
			(void)filter_index(state.filter);
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					if (!std::isfinite(transform[column][row])) [[unlikely]]
					{
						throw std::invalid_argument("batcher2d mesh transform must be finite");
					}
				}
			}
			if (instances.size() >= std::numeric_limits<std::uint32_t>::max() / sizeof(sprite_instance))
			    [[unlikely]]
			{
				throw std::length_error("batcher2d frame exceeds SDL's 32-bit buffer size limit");
			}

			const auto rgba = tint.rgba_glsl();
			const sprite_instance instance{.transform_x = {transform[0][0], transform[0][1]},
			                               .transform_y = {transform[1][0], transform[1][1]},
			                               .transform_translation = {transform[3][0], transform[3][1]},
			                               .effect_parameters = {0.0f, 0.0f},
			                               .uv = {0.0f, 1.0f, 1.0f, 0.0f},
			                               .tint = {pack_channel(rgba.r), pack_channel(rgba.g),
			                                        pack_channel(rgba.b), pack_channel(rgba.a)}};

			auto* white_page = source.page->raw();
			if (allow_extension && !batches.empty() && batches.back().resident_mesh == mesh_storage &&
			    same_batch_key(batches.back(), white_page, state))
			{
				if (batches.back().instance_count == std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
				{
					throw std::length_error("batcher2d mesh instance count exceeds SDL's 32-bit limit");
				}
				instances.push_back(instance);
				++batches.back().instance_count;
				return;
			}
			if (batches.size() >= std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
			{
				throw std::length_error("batcher2d frame exceeds SDL's 32-bit batch-count limit");
			}

			const auto first_instance = static_cast<std::uint32_t>(instances.size());
			const auto first_batch = batches.size();
			::mngl::detail::reserve_vector_growth(batches, batches.size() + 1,
			                                      std::numeric_limits<std::uint32_t>::max());
			::mngl::detail::reserve_vector_growth(instances, instances.size() + 1,
			                                      std::numeric_limits<std::uint32_t>::max() /
			                                          sizeof(sprite_instance));
			try
			{
				batches.push_back(draw_batch{.texture = white_page,
				                             .filter = state.filter,
				                             .blend = state.blend,
				                             .effect = quad_effect::textured,
				                             .scissor = state.scissor,
				                             .material = state.material,
				                             .first_instance = first_instance,
				                             .instance_count = 1,
				                             .vertex_count = mesh_storage->vertex_count,
				                             .resident_mesh = mesh_storage});
				instances.push_back(instance);
			}
			catch (...)
			{
				if (batches.size() != first_batch)
				{
					batches.pop_back();
				}
				throw;
			}
		}

		template <bool vertices_validated>
		void draw_geometry_into(std::vector<sprite_instance>& instances,
		                        std::vector<quad_vertex>& geometry_vertices,
		                        std::vector<geometry_block>& geometry_blocks,
		                        std::vector<draw_batch>& batches, std::span<const glm::vec2> vertices,
		                        const texture_region_view& source, const glm::mat4& transform,
		                        const color& tint, const quad_draw_state& state,
		                        bool allow_extension = true) const
		{
			if (vertices.empty())
			{
				return;
			}
			if (vertices.size() % 3 != 0) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d geometry requires a triangle-list vertex count");
			}
			if (state.effect != quad_effect::textured) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d geometry requires the textured effect");
			}
			if (!source.page || !source.page->valid() ||
			    !source.page->has_usage(texture::usage_flags::sampler)) [[unlikely]]
			{
				throw std::invalid_argument(
				    "batcher2d geometry tint texture must be valid and sampler-enabled");
			}
			if (state.scissor && (state.scissor->w < 0 || state.scissor->h < 0)) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d scissor dimensions must be non-negative");
			}
			if (state.scissor && (state.scissor->w == 0 || state.scissor->h == 0))
			{
				return;
			}
			(void)blend_index(state.blend);
			(void)filter_index(state.filter);

			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					if (!std::isfinite(transform[column][row])) [[unlikely]]
					{
						throw std::invalid_argument("batcher2d geometry transform must be finite");
					}
				}
			}

			constexpr std::size_t max_instances =
			    std::numeric_limits<std::uint32_t>::max() / sizeof(sprite_instance);
			constexpr std::size_t max_geometry_vertices =
			    std::numeric_limits<std::uint32_t>::max() / sizeof(quad_vertex);
			const std::size_t added_geometry_blocks =
			    (vertices.size() - 1) / geometry_block_vertex_capacity + 1;
			if (vertices.size() > max_geometry_vertices - geometry_vertices.size()) [[unlikely]]
			{
				throw std::length_error("batcher2d frame geometry exceeds SDL's 32-bit buffer size limit");
			}
			if (added_geometry_blocks > std::numeric_limits<std::uint32_t>::max() - geometry_blocks.size())
			    [[unlikely]]
			{
				throw std::length_error("batcher2d frame exceeds SDL's 32-bit geometry-block limit");
			}

			const auto rgba = tint.rgba_glsl();
			const std::array<std::uint8_t, 4> packed_tint{pack_channel(rgba.r), pack_channel(rgba.g),
			                                              pack_channel(rgba.b), pack_channel(rgba.a)};
			const glm::vec2 transform_x{transform[0][0], transform[0][1]};
			const glm::vec2 transform_y{transform[1][0], transform[1][1]};
			const glm::vec2 translation{transform[3][0], transform[3][1]};
			const sprite_instance instance{.transform_x = {transform_x.x, transform_x.y},
			                               .transform_y = {transform_y.x, transform_y.y},
			                               .transform_translation = {translation.x, translation.y},
			                               .effect_parameters = {0.0f, 0.0f},
			                               .uv = {0.0f, 1.0f, 1.0f, 0.0f},
			                               .tint = packed_tint};
			auto append_local_vertices = [&]
			{
				for (std::size_t block_offset = 0; block_offset < vertices.size();
				     block_offset += geometry_block_vertex_capacity)
				{
					const auto block_vertex_count =
					    std::min(geometry_block_vertex_capacity, vertices.size() - block_offset);
					geometry_block block;
					block.first_vertex = static_cast<std::uint32_t>(geometry_vertices.size());
					block.vertex_count = static_cast<std::uint32_t>(block_vertex_count);
					block.bounds_min = glm::vec2(std::numeric_limits<float>::max());
					block.bounds_max = glm::vec2(std::numeric_limits<float>::lowest());
					for (std::size_t vertex_index = block_offset;
					     vertex_index < block_offset + block_vertex_count; ++vertex_index)
					{
						const auto vertex = vertices[vertex_index];
						if constexpr (!vertices_validated)
						{
							if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)) [[unlikely]]
							{
								throw std::invalid_argument("batcher2d geometry vertices must be finite");
							}
						}
						geometry_vertices.push_back({vertex.x, vertex.y});
						block.bounds_min.x = std::min(block.bounds_min.x, vertex.x);
						block.bounds_min.y = std::min(block.bounds_min.y, vertex.y);
						block.bounds_max.x = std::max(block.bounds_max.x, vertex.x);
						block.bounds_max.y = std::max(block.bounds_max.y, vertex.y);
					}
					const std::array<glm::vec2, 4> corners{{
					    block.bounds_min,
					    {block.bounds_max.x, block.bounds_min.y},
					    block.bounds_max,
					    {block.bounds_min.x, block.bounds_max.y},
					}};
					for (const auto corner : corners)
					{
						const auto transformed = transform_geometry_point(instance, corner);
						if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y)) [[unlikely]]
						{
							throw std::invalid_argument(
							    "batcher2d transformed geometry vertices must be finite");
						}
					}
					geometry_blocks.push_back(block);
				}
			};
			auto* raw_texture = source.page->raw();
			const bool can_extend_batch =
			    allow_extension && !batches.empty() && batches.back().geometry &&
			    batches.back().geometry_tint == packed_tint &&
			    same_geometry_transform(instances[batches.back().first_instance], instance) &&
			    same_batch_key(batches.back(), raw_texture, state);
			if (can_extend_batch)
			{
				auto& batch = batches.back();
				if (vertices.size() > std::numeric_limits<std::uint32_t>::max() - batch.vertex_count)
				    [[unlikely]]
				{
					throw std::length_error(
					    "batcher2d geometry batch exceeds SDL's 32-bit vertex count limit");
				}
				const auto first_vertex = geometry_vertices.size();
				::mngl::detail::reserve_vector_growth(
				    geometry_vertices, geometry_vertices.size() + vertices.size(), max_geometry_vertices);
				::mngl::detail::reserve_vector_growth(geometry_blocks,
				                                      geometry_blocks.size() + added_geometry_blocks,
				                                      std::numeric_limits<std::uint32_t>::max());
				const auto first_geometry_block = geometry_blocks.size();
				try
				{
					append_local_vertices();
				}
				catch (...)
				{
					geometry_vertices.resize(first_vertex);
					geometry_blocks.resize(first_geometry_block);
					throw;
				}
				batch.vertex_count += static_cast<std::uint32_t>(vertices.size());
				batch.geometry_block_count += static_cast<std::uint32_t>(added_geometry_blocks);
				return;
			}

			if (instances.size() >= max_instances) [[unlikely]]
			{
				throw std::length_error("batcher2d frame exceeds SDL's 32-bit buffer size limit");
			}
			if (batches.size() >= std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
			{
				throw std::length_error("batcher2d frame exceeds SDL's 32-bit batch-count limit");
			}

			const auto first_instance = static_cast<std::uint32_t>(instances.size());
			const auto first_vertex = static_cast<std::uint32_t>(geometry_vertices.size());
			const auto first_geometry_block = static_cast<std::uint32_t>(geometry_blocks.size());
			::mngl::detail::reserve_vector_growth(batches, batches.size() + 1,
			                                      std::numeric_limits<std::uint32_t>::max());
			::mngl::detail::reserve_vector_growth(instances, instances.size() + 1, max_instances);
			::mngl::detail::reserve_vector_growth(
			    geometry_vertices, geometry_vertices.size() + vertices.size(), max_geometry_vertices);
			::mngl::detail::reserve_vector_growth(geometry_blocks,
			                                      geometry_blocks.size() + added_geometry_blocks,
			                                      std::numeric_limits<std::uint32_t>::max());
			const auto first_batch = batches.size();
			try
			{
				append_local_vertices();
				batches.push_back(
				    draw_batch{.texture = raw_texture,
					           .filter = state.filter,
					           .blend = state.blend,
					           .effect = quad_effect::textured,
					           .scissor = state.scissor,
					           .material = state.material,
					           .first_instance = first_instance,
					           .instance_count = 1,
					           .geometry = true,
					           .first_vertex = first_vertex,
					           .vertex_count = static_cast<std::uint32_t>(vertices.size()),
					           .first_geometry_block = first_geometry_block,
					           .geometry_block_count = static_cast<std::uint32_t>(added_geometry_blocks),
					           .geometry_tint = packed_tint});
				instances.push_back(instance);
			}
			catch (...)
			{
				geometry_vertices.resize(first_vertex);
				geometry_blocks.resize(first_geometry_block);
				if (batches.size() != first_batch)
				{
					batches.pop_back();
				}
				if (instances.size() != first_instance)
				{
					instances.pop_back();
				}
				throw;
			}
		}

		static bool same_batch_key(const draw_batch& batch, SDL_GPUTexture* texture_handle,
		                           const quad_draw_state& state, std::uint64_t deferred_token = 0) noexcept
		{
			if (batch.deferred_token != deferred_token)
			{
				return false;
			}
			if (batch.material.valid() || state.material.valid())
			{
				return batch.material == state.material &&
				       batch.scissor.has_value() == state.scissor.has_value() &&
				       (!batch.scissor || equal_scissor(*batch.scissor, *state.scissor));
			}

			return batch.texture == texture_handle && batch.filter == state.filter &&
			       batch.blend == state.blend && batch.effect == state.effect &&
			       batch.scissor.has_value() == state.scissor.has_value() &&
			       (!batch.scissor || equal_scissor(*batch.scissor, *state.scissor));
		}

		enum class clip_relation : std::uint8_t
		{
			outside,
			partial,
			inside
		};

		// Classify projected points conservatively against the target's 2D clip rectangle.
		template <std::size_t PointCount>
		[[nodiscard]] static clip_relation classify_clip_points(
		    const glm::mat4& view_projection, const std::array<glm::vec2, PointCount>& points) noexcept
		{
			bool outside_left = true;
			bool outside_right = true;
			bool outside_bottom = true;
			bool outside_top = true;
			bool fully_inside = true;
			for (const auto point : points)
			{
				const float clip_x =
				    view_projection[0][0] * point.x + view_projection[1][0] * point.y + view_projection[3][0];
				const float clip_y =
				    view_projection[0][1] * point.x + view_projection[1][1] * point.y + view_projection[3][1];
				const float clip_w =
				    view_projection[0][3] * point.x + view_projection[1][3] * point.y + view_projection[3][3];
				if (!std::isfinite(clip_x) || !std::isfinite(clip_y) || !std::isfinite(clip_w) ||
				    clip_w <= 0.0f)
				{
					return clip_relation::partial;
				}

				outside_left = outside_left && clip_x < -clip_w;
				outside_right = outside_right && clip_x > clip_w;
				outside_bottom = outside_bottom && clip_y < -clip_w;
				outside_top = outside_top && clip_y > clip_w;
				fully_inside = fully_inside && clip_x >= -clip_w && clip_x <= clip_w && clip_y >= -clip_w &&
				               clip_y <= clip_w;
			}
			if (outside_left || outside_right || outside_bottom || outside_top)
			{
				return clip_relation::outside;
			}
			return fully_inside ? clip_relation::inside : clip_relation::partial;
		}

		template <std::size_t PointCount>
		[[nodiscard]] static bool outside_target(const glm::mat4& view_projection,
		                                         const std::array<glm::vec2, PointCount>& points) noexcept
		{
			return classify_clip_points(view_projection, points) == clip_relation::outside;
		}

		[[nodiscard]] static glm::vec2 transform_geometry_point(const sprite_instance& instance,
		                                                        glm::vec2 point) noexcept
		{
			return {
			    instance.transform_x[0] * point.x + instance.transform_y[0] * point.y +
			        instance.transform_translation[0],
			    instance.transform_x[1] * point.x + instance.transform_y[1] * point.y +
			        instance.transform_translation[1],
			};
		}

		[[nodiscard]] static bool same_geometry_transform(const sprite_instance& left,
		                                                  const sprite_instance& right) noexcept
		{
			return left.transform_x == right.transform_x && left.transform_y == right.transform_y &&
			       left.transform_translation == right.transform_translation;
		}

		[[nodiscard]] static clip_relation classify_transformed_bounds(
		    const glm::mat4& view_projection, glm::vec2 minimum, glm::vec2 maximum,
		    const sprite_instance& instance) noexcept
		{
			const std::array<glm::vec2, 4> corners{{
			    transform_geometry_point(instance, minimum),
			    transform_geometry_point(instance, {maximum.x, minimum.y}),
			    transform_geometry_point(instance, maximum),
			    transform_geometry_point(instance, {minimum.x, maximum.y}),
			}};
			return classify_clip_points(view_projection, corners);
		}

		void initialize_gpu_culling()
		{
			shader_compile_options geometry_options{};
			geometry_options.stage = shader_stage::compute;
			geometry_options.format = static_cast<shader_format>(default_shader_format);
			geometry_options.resources.storage_buffers = 1;
			geometry_options.resources.read_write_storage_buffers = 1;
			geometry_options.resources.uniform_buffers = 1;
			const auto geometry_shader = shader_loader::compile_compute(
			    paths::shader_directory("batcher2d.cull.hlsl") / "batcher2d.cull.hlsl", geometry_options);
			auto geometry_pipeline = compute_pipeline_builder{}
			                             .set_debug_name("MNGL 2D geometry block culling")
			                             .set_compute_shader(geometry_shader)
			                             .build(m_device);

			auto instance_options = geometry_options;
			instance_options.resources.storage_buffers = 1;
			instance_options.resources.read_write_storage_buffers = 1;
			const auto instance_shader = shader_loader::compile_compute(
			    paths::shader_directory("batcher2d.cull_instances.hlsl") / "batcher2d.cull_instances.hlsl",
			    instance_options);
			auto instance_pipeline = compute_pipeline_builder{}
			                             .set_debug_name("MNGL 2D instance culling")
			                             .set_compute_shader(instance_shader)
			                             .build(m_device);

			m_geometry_culling_pipeline = std::move(geometry_pipeline);
			m_instance_culling_pipeline = std::move(instance_pipeline);
		}

		void ensure_gpu_culling_capacity(std::uint32_t metadata_bytes, std::uint32_t command_bytes)
		{
			const auto grow_capacity = [](std::uint32_t current, std::uint32_t required)
			{
				constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
				std::uint64_t capacity = std::max<std::uint64_t>(current, 4096);
				while (capacity < required && capacity < maximum)
				{
					capacity = std::min<std::uint64_t>(capacity * 2, maximum);
				}
				if (capacity < required) [[unlikely]]
				{
					throw std::length_error("batcher2d GPU culling buffer exceeds SDL's 32-bit size limit");
				}
				return static_cast<std::uint32_t>(capacity);
			};

			if (!m_geometry_culling_input.valid() || m_geometry_culling_input.size() < metadata_bytes)
			{
				m_geometry_culling_input =
				    buffer(m_device, grow_capacity(m_geometry_culling_input.size(), metadata_bytes),
					       buffer_usage::compute_storage_read);
			}
			if (!m_geometry_indirect_commands.valid() || m_geometry_indirect_commands.size() < command_bytes)
			{
				m_geometry_indirect_commands =
				    buffer(m_device, grow_capacity(m_geometry_indirect_commands.size(), command_bytes),
					       buffer_usage::compute_storage_write | buffer_usage::indirect);
			}
		}

		void ensure_instance_culling_capacity(std::uint32_t range_bytes)
		{
			const auto grow_capacity = [](std::uint32_t current, std::uint32_t required)
			{
				constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
				std::uint64_t capacity = std::max<std::uint64_t>(current, 4096);
				while (capacity < required && capacity < maximum)
				{
					capacity = std::min<std::uint64_t>(capacity * 2, maximum);
				}
				if (capacity < required) [[unlikely]]
				{
					throw std::length_error(
					    "batcher2d instance-culling buffer exceeds SDL's 32-bit size limit");
				}
				return static_cast<std::uint32_t>(capacity);
			};

			if (range_bytes != 0 &&
			    (!m_instance_culling_input.valid() || m_instance_culling_input.size() < range_bytes))
			{
				m_instance_culling_input =
				    buffer(m_device, grow_capacity(m_instance_culling_input.size(), range_bytes),
					       buffer_usage::compute_storage_read);
			}
		}

		void build_geometry_culling_blocks()
		{
			m_geometry_culling_blocks.clear();
			if (m_geometry_blocks.size() >
			    std::numeric_limits<std::uint32_t>::max() / sizeof(geometry_culling_block)) [[unlikely]]
			{
				throw std::length_error("batcher2d GPU culling metadata exceeds SDL's 32-bit buffer limit");
			}
			m_geometry_culling_blocks.reserve(m_geometry_blocks.size());
			for (auto& batch : m_batches)
			{
				if (!batch.geometry || batch.geometry_block_count == 0)
				{
					continue;
				}
				if (batch.instance_count == 0 || batch.first_instance >= m_instances.size()) [[unlikely]]
				{
					throw std::logic_error(
					    "batcher2d geometry batch has no transform instance for GPU culling");
				}
				const auto& transform = m_instances[batch.first_instance];
				const std::size_t source_first_block = batch.first_geometry_block;
				const std::size_t block_end = source_first_block + batch.geometry_block_count;
				batch.first_geometry_block = static_cast<std::uint32_t>(m_geometry_culling_blocks.size());
				for (std::size_t block_index = source_first_block; block_index < block_end; ++block_index)
				{
					const auto& block = m_geometry_blocks[block_index];
					m_geometry_culling_blocks.push_back({
					    .bounds_min = {block.bounds_min.x, block.bounds_min.y},
					    .bounds_max = {block.bounds_max.x, block.bounds_max.y},
					    .transform_x = transform.transform_x,
					    .transform_y = transform.transform_y,
					    .transform_translation = transform.transform_translation,
					    .first_vertex = block.first_vertex,
					    .vertex_count = block.vertex_count,
					});
				}
			}
		}

		void build_instance_culling_ranges()
		{
			m_instance_culling_ranges.clear();
			if (m_batches.size() > std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
			{
				throw std::length_error("batcher2d instance-culling range count exceeds SDL's 32-bit limit");
			}
			m_instance_culling_ranges.reserve(m_batches.size());
			for (const auto& batch : m_batches)
			{
				if (batch.instance_count == 0)
				{
					continue;
				}
				if (batch.first_instance > m_instances.size() ||
				    batch.instance_count > m_instances.size() - batch.first_instance) [[unlikely]]
				{
					throw std::logic_error("batcher2d instance-culling range is outside the instance buffer");
				}

				instance_culling_range range{.first_instance = batch.first_instance,
				                             .instance_count = batch.instance_count};
				if (batch.resident_mesh)
				{
					std::lock_guard lock(batch.resident_mesh->mutex);
					range.bounds_min = {batch.resident_mesh->bounds_min.x, batch.resident_mesh->bounds_min.y};
					range.bounds_max = {batch.resident_mesh->bounds_max.x, batch.resident_mesh->bounds_max.y};
				}
				else if (batch.geometry)
				{
					range.bypass = 1;
				}
				else
				{
					range.bounds_min = {0.0f, 0.0f};
					range.bounds_max = {1.0f, 1.0f};
				}
				m_instance_culling_ranges.push_back(range);
			}

			std::sort(m_instance_culling_ranges.begin(), m_instance_culling_ranges.end(),
			          [](const instance_culling_range& left, const instance_culling_range& right)
			          { return left.first_instance < right.first_instance; });
		}

		[[nodiscard]] std::uint32_t cull_offscreen_primitives(const glm::mat4& view_projection,
		                                                      bool gpu_culling_enabled,
		                                                      bool cpu_culling_enabled)
		{
			if (gpu_culling_enabled || !cpu_culling_enabled)
			{
				return static_cast<std::uint32_t>(std::count_if(
				    m_batches.begin(), m_batches.end(), [](const draw_batch& batch)
				    { return batch.instance_count != 0 && (!batch.geometry || batch.vertex_count != 0); }));
			}

			std::size_t instance_output = 0;
			std::size_t vertex_output = 0;
			std::uint32_t visible_batches = 0;
			for (auto& batch : m_batches)
			{
				if (batch.resident_mesh)
				{
					glm::vec2 mesh_bounds_min;
					glm::vec2 mesh_bounds_max;
					{
						std::lock_guard lock(batch.resident_mesh->mutex);
						mesh_bounds_min = batch.resident_mesh->bounds_min;
						mesh_bounds_max = batch.resident_mesh->bounds_max;
					}
					const std::size_t first_instance = batch.first_instance;
					const std::size_t instance_end = first_instance + batch.instance_count;
					batch.first_instance = static_cast<std::uint32_t>(instance_output);
					batch.instance_count = 0;
					for (std::size_t instance_index = first_instance; instance_index < instance_end;
					     ++instance_index)
					{
						const auto& instance = m_instances[instance_index];
						if (classify_transformed_bounds(view_projection, mesh_bounds_min, mesh_bounds_max,
						                                instance) == clip_relation::outside)
						{
							continue;
						}
						if (instance_output != instance_index)
						{
							m_instances[instance_output] = instance;
						}
						++instance_output;
						++batch.instance_count;
					}
					if (batch.instance_count != 0)
					{
						++visible_batches;
					}
					continue;
				}

				if (batch.geometry)
				{
					const std::size_t first_instance = batch.first_instance;
					const std::size_t instance_count = batch.instance_count;
					const auto& geometry_transform = m_instances[first_instance];
					const std::size_t output_first_vertex = vertex_output;
					const std::size_t first_block = batch.first_geometry_block;
					const std::size_t block_end = first_block + batch.geometry_block_count;
					for (std::size_t block_index = first_block; block_index < block_end; ++block_index)
					{
						const auto& block = m_geometry_blocks[block_index];
						const std::size_t block_first_vertex = block.first_vertex;
						const std::size_t block_vertex_end = block_first_vertex + block.vertex_count;
						const auto relation = classify_transformed_bounds(
						    view_projection, block.bounds_min, block.bounds_max, geometry_transform);
						if (relation == clip_relation::outside)
						{
							continue;
						}

						if (relation == clip_relation::inside)
						{
							if (vertex_output == block_first_vertex)
							{
								vertex_output += block.vertex_count;
							}
							else
							{
								for (std::size_t vertex = block_first_vertex; vertex < block_vertex_end;
								     ++vertex)
								{
									m_geometry_vertices[vertex_output] = m_geometry_vertices[vertex];
									++vertex_output;
								}
							}
							continue;
						}

						for (std::size_t vertex = block_first_vertex; vertex + 2 < block_vertex_end;
						     vertex += 3)
						{
							const std::array<glm::vec2, 3> triangle{{
							    {m_geometry_vertices[vertex].x, m_geometry_vertices[vertex].y},
							    {m_geometry_vertices[vertex + 1].x, m_geometry_vertices[vertex + 1].y},
							    {m_geometry_vertices[vertex + 2].x, m_geometry_vertices[vertex + 2].y},
							}};
							const std::array<glm::vec2, 3> transformed_triangle{{
							    transform_geometry_point(geometry_transform, triangle[0]),
							    transform_geometry_point(geometry_transform, triangle[1]),
							    transform_geometry_point(geometry_transform, triangle[2]),
							}};
							if (outside_target(view_projection, transformed_triangle))
							{
								continue;
							}

							for (std::size_t corner = 0; corner < triangle.size(); ++corner)
							{
								if (vertex_output != vertex + corner)
								{
									m_geometry_vertices[vertex_output] = m_geometry_vertices[vertex + corner];
								}
								++vertex_output;
							}
						}
					}

					batch.first_vertex = static_cast<std::uint32_t>(output_first_vertex);
					batch.vertex_count = static_cast<std::uint32_t>(vertex_output - output_first_vertex);
					batch.first_instance = static_cast<std::uint32_t>(instance_output);
					batch.instance_count = 0;
					if (batch.vertex_count != 0)
					{
						++visible_batches;
						for (std::size_t instance = 0; instance < instance_count; ++instance)
						{
							const std::size_t source_instance = first_instance + instance;
							if (instance_output != source_instance)
							{
								m_instances[instance_output] = m_instances[source_instance];
							}
							++instance_output;
						}
						batch.instance_count = static_cast<std::uint32_t>(instance_count);
					}
					continue;
				}

				const std::size_t first_instance = batch.first_instance;
				const std::size_t instance_end = first_instance + batch.instance_count;
				batch.first_instance = static_cast<std::uint32_t>(instance_output);
				batch.instance_count = 0;
				for (std::size_t instance_index = first_instance; instance_index < instance_end;
				     ++instance_index)
				{
					const auto& instance = m_instances[instance_index];
					const glm::vec2 origin{instance.transform_translation[0],
					                       instance.transform_translation[1]};
					const glm::vec2 transform_x{instance.transform_x[0], instance.transform_x[1]};
					const glm::vec2 transform_y{instance.transform_y[0], instance.transform_y[1]};
					const std::array<glm::vec2, 4> quad{origin, origin + transform_x, origin + transform_y,
					                                    origin + transform_x + transform_y};
					if (outside_target(view_projection, quad))
					{
						continue;
					}

					if (instance_output != instance_index)
					{
						m_instances[instance_output] = instance;
					}
					++instance_output;
					++batch.instance_count;
				}
				if (batch.instance_count != 0)
				{
					++visible_batches;
				}
			}

			m_instances.resize(instance_output);
			m_geometry_vertices.resize(vertex_output);
			return visible_batches;
		}

		static glm::mat4 coordinate_projection(const texture& target)
		{
			glm::mat4 projection(1.0f);
			if (target.width() == 0 || target.height() == 0)
			{
				return projection;
			}

			const float short_side = static_cast<float>(std::min(target.width(), target.height()));
			const float coordinate_width = static_cast<float>(target.width()) / short_side;
			const float coordinate_height = static_cast<float>(target.height()) / short_side;
			projection[0][0] = 2.0f / coordinate_width;
			projection[1][1] = 2.0f / coordinate_height;
			projection[3][0] = -1.0f;
			projection[3][1] = -1.0f;
			return projection;
		}

		static std::uint32_t instance_buffer_size(std::size_t sprite_capacity)
		{
			constexpr std::size_t max_capacity =
			    std::numeric_limits<std::uint32_t>::max() / sizeof(sprite_instance);
			if (sprite_capacity > max_capacity) [[unlikely]]
			{
				throw std::length_error("batcher2d initial capacity exceeds SDL's 32-bit buffer size limit");
			}
			return static_cast<std::uint32_t>(sprite_capacity * sizeof(sprite_instance));
		}

		static std::uint8_t pack_channel(float value)
		{
			if (!std::isfinite(value)) [[unlikely]]
			{
				throw std::invalid_argument("batcher2d tint channels must be finite");
			}
			return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
		}

		void ensure_frame_active() const
		{
			if (!m_frame_active) [[unlikely]]
			{
				throw std::logic_error("batcher2d operation requires an active frame");
			}
		}

		device m_device;
		texture_format m_target_format = texture_format::invalid;
		vertex_layout m_vertex_layout;
		std::shared_ptr<const default_gpu_resources> m_shared_default_resources;
		std::vector<custom_material_slot> m_custom_materials;
		std::vector<custom_material_instance_slot> m_custom_material_instances;
		std::vector<std::uint32_t> m_free_material_slots;
		std::vector<std::uint32_t> m_free_material_instance_slots;
		buffer m_instance_buffer;
		buffer m_geometry_buffer;
		buffer m_geometry_culling_input;
		buffer m_geometry_indirect_commands;
		buffer m_instance_culling_input;
		compute_pipeline m_geometry_culling_pipeline;
		compute_pipeline m_instance_culling_pipeline;
		std::vector<sprite_instance> m_instances;
		std::vector<quad_vertex> m_geometry_vertices;
		std::vector<geometry_block> m_geometry_blocks;
		std::vector<geometry_culling_block> m_geometry_culling_blocks;
		std::vector<instance_culling_range> m_instance_culling_ranges;
		std::vector<draw_batch> m_batches;
		std::vector<order_independent_batch_range> m_order_independent_batch_ranges;
		std::vector<sprite_instance> m_order_independent_instance_scratch;
		color m_clear_color = color(0.0f, 0.0f, 0.0f, 1.0f);
		bool m_preserve_target_contents = false;
		frame_stats m_last_stats{};
		profile_clock::time_point m_frame_started_at{};
		std::shared_ptr<recording_frame_state> m_recording_state;
		std::uint64_t m_frame_generation = 0;
		bool m_frame_active = false;
		bool m_gpu_culling_enabled = false;
		bool m_gpu_culling_overridden = false;
		bool m_vertex_instance_culling_supported = false;
		bool m_profiling_enabled = false;
		bool m_batch_extension_blocked = false;
	};
} // namespace mngl::gpu
