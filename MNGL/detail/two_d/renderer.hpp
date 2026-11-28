#pragma once

#include "frame.hpp"
#include "geometry.hpp"
#include "svg_effect_renderer.hpp"
#include "svg_parser.hpp"
#include "../container_growth.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <map>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace mngl::two_d
{
	class renderer
	{
		friend class frame_2d;
		friend class render_context;
		friend class ::mngl::detail::texture_render_session_impl;

	public:
		// Retains the device and borrows the texture store and optional swapchain window.
		renderer(const gpu::device& device, window& target_window, texture_store& textures)
		    : renderer(device, textures, required_swapchain_format(device, target_window))
		{
			m_window = &target_window;
			m_custom_materials_allowed = true;
		}

		renderer(const gpu::device& device, texture_store& textures, gpu::texture_format target_format)
		    : renderer(device, textures, target_format,
		               std::make_shared<detail::svg::temporary_target_budget>())
		{
		}

		renderer(const renderer&) = delete;
		renderer& operator=(const renderer&) = delete;
		renderer(renderer&&) = delete;
		renderer& operator=(renderer&&) = delete;

		~renderer()
		{
			if (m_frame_active.load(std::memory_order_acquire))
			{
				m_batcher.cancel_frame();
			}
			m_recording_state.reset();
			m_frame_control->owner = nullptr;
		}

		[[nodiscard]] frame_2d begin_frame(const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f))
		{
			if (!m_window) [[unlikely]]
			{
				throw std::logic_error("This 2D renderer requires a render target texture");
			}
			return begin_frame_impl(nullptr, clear_color, false);
		}

		[[nodiscard]] frame_2d begin_frame(::mngl::texture& target,
		                                   const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f),
		                                   bool preserve_target_contents = false)
		{
			if (!target.valid() || !target.can_render_to()) [[unlikely]]
			{
				throw std::invalid_argument("2D render target must be a valid color-target texture");
			}
			if (target.raw_gpu_texture().device_raw() != m_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("2D render target belongs to a different GPU device");
			}
			if (target.format() != target_format()) [[unlikely]]
			{
				throw std::invalid_argument("2D render target format does not match its renderer");
			}
			return begin_frame_impl(&target, clear_color, preserve_target_contents);
		}

		[[nodiscard]] gpu::texture_format target_format() const noexcept { return m_batcher.target_format(); }

		void enable_custom_materials()
		{
			std::lock_guard lock(m_frame_start_mutex);
			if (!available_for_frame()) [[unlikely]]
			{
				throw std::logic_error(
				    "Custom materials cannot be enabled while a 2D frame is active or being submitted");
			}
			m_custom_materials_allowed = true;
		}

		void enable_gpu_culling(bool enabled = true)
		{
			std::lock_guard lock(m_frame_start_mutex);
			if (!available_for_frame()) [[unlikely]]
			{
				throw std::logic_error(
				    "GPU culling cannot be changed while a 2D frame is active or being submitted");
			}
			m_batcher.enable_gpu_culling(enabled);
		}

		void disable_gpu_culling() { enable_gpu_culling(false); }

		void use_global_gpu_culling()
		{
			std::lock_guard lock(m_frame_start_mutex);
			if (!available_for_frame()) [[unlikely]]
			{
				throw std::logic_error(
				    "GPU culling cannot be changed while a 2D frame is active or being submitted");
			}
			m_batcher.use_global_gpu_culling();
		}

		[[nodiscard]] bool gpu_culling_enabled() const noexcept { return m_batcher.gpu_culling_enabled(); }

		void enable_profiling(bool enabled = true)
		{
			std::lock_guard lock(m_frame_start_mutex);
			if (!available_for_frame()) [[unlikely]]
			{
				throw std::logic_error(
				    "2D profiling cannot be changed while a frame is active or being submitted");
			}
			m_batcher.enable_profiling(enabled);
		}

		[[nodiscard]] bool profiling_enabled() const noexcept { return m_batcher.profiling_enabled(); }

	private:
		renderer(const gpu::device& device, texture_store& textures, gpu::texture_format target_format,
		         std::shared_ptr<detail::svg::temporary_target_budget> effect_target_budget)
		    : m_device(device), m_textures(textures), m_batcher(device, target_format),
		      m_frame_control(std::make_shared<detail::renderer_frame_control>()),
		      m_svg_effect_target_budget(std::move(effect_target_budget)),
		      m_svg_effect_output_targets(m_svg_effect_target_budget),
		      m_svg_effect_capture_targets(m_svg_effect_target_budget)
		{
			if (m_textures.device_raw() != m_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("2D renderer and texture store must use the same GPU device");
			}
			m_frame_control->owner = this;
		}

		[[nodiscard]] bool available_for_frame() const noexcept
		{
			return !m_frame_active.load(std::memory_order_acquire) &&
			       !m_command_submission_pending.load(std::memory_order_acquire);
		}

		void begin_command_submission()
		{
			if (!m_frame_active.load(std::memory_order_acquire)) [[unlikely]]
			{
				throw std::logic_error("Cannot submit a command buffer for an inactive 2D frame");
			}

			bool expected = false;
			if (!m_command_submission_pending.compare_exchange_strong(
			        expected, true, std::memory_order_acq_rel, std::memory_order_relaxed)) [[unlikely]]
			{
				throw std::logic_error("A command buffer is already being submitted for this 2D renderer");
			}
		}

		void end_command_submission() noexcept
		{
			m_command_submission_pending.store(false, std::memory_order_release);
		}

		[[nodiscard]] frame_2d begin_frame_impl(::mngl::texture* target, const color& clear_color,
		                                        bool preserve_target_contents)
		{
			std::lock_guard lock(m_frame_start_mutex);
			if (!available_for_frame()) [[unlikely]]
			{
				throw std::logic_error("A 2D renderer frame is already active or being submitted");
			}
			auto recording_state = std::make_shared<detail::renderer_frame_state>();

			for (const auto instance : m_automatic_instance_handles)
			{
				m_batcher.destroy_material_instance(instance);
			}
			m_automatic_instances.clear();
			m_automatic_svg_gradient_instances.clear();
			m_automatic_svg_gradient_fast_cache.clear();
			std::erase_if(m_svg_gradient_stop_buffers,
			              [](const auto& entry) { return entry.first.expired(); });
			std::erase_if(m_svg_image_textures, [](const auto& entry) { return entry.first.expired(); });
			m_automatic_instance_handles.clear();

			try
			{
				m_batcher.begin_frame(clear_color, preserve_target_contents);
				m_active_target = target;
				m_active_target_size =
				    target ? target->size() : (m_window ? m_window->pixel_size() : glm::uvec2{});
				m_active_scissor.reset();
				m_hit_test_items.clear();
				m_used_fonts.clear();
				if (++m_next_frame_token == 0)
				{
					++m_next_frame_token;
					for (auto& cached : m_automatic_materials)
					{
						cached.second.validated_frame_token = 0;
					}
				}
				m_active_frame_token = m_next_frame_token;
				m_recording_state = std::move(recording_state);
				frame_2d frame(m_frame_control, m_active_frame_token, m_recording_state);
				if (target)
				{
					frame.m_view = target->get_view();
				}
				m_frame_active.store(true, std::memory_order_release);
				return frame;
			}
			catch (...)
			{
				m_batcher.cancel_frame();
				m_active_target = nullptr;
				m_active_target_size = {};
				m_active_scissor.reset();
				m_active_frame_token = 0;
				m_recording_state.reset();
				m_hit_test_items.clear();
				m_used_fonts.clear();
				m_frame_active.store(false, std::memory_order_release);
				throw;
			}
		}

		[[nodiscard]] recording_context create_recording_context(const frame_2d& frame,
		                                                         std::size_t expected_draw_count)
		{
			ensure_frame(frame.m_token);
			if (frame.m_recording_context || !frame.m_recording_state ||
			    frame.m_recording_state != m_recording_state) [[unlikely]]
			{
				throw std::logic_error(
				    "Recording contexts can only be created from the active parent frame_2d");
			}

			auto context = recording_context(m_frame_control, m_recording_state, frame.m_token,
			                                 m_batcher.create_recording_context(expected_draw_count),
			                                 frame.m_view_projection, frame.m_view, m_active_scissor);
			context.register_with_frame();
			return context;
		}

		void merge_recording_context(const frame_2d& frame, recording_context& context)
		{
			ensure_frame(frame.m_token);
			if (frame.m_recording_context) [[unlikely]]
			{
				throw std::logic_error("A worker recording context cannot merge other recording contexts");
			}
			if (!context.valid() || !context.m_finished || context.m_control != m_frame_control ||
			    context.m_token != frame.m_token || context.m_recording_state != frame.m_recording_state ||
			    context.m_recording_state != m_recording_state) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Only a finished recording context from this frame can be merged");
			}
			if (context.m_order_independent && m_active_target && m_active_target->m_hit_testing_enabled)
			    [[unlikely]]
			{
				throw std::logic_error(
				    "Order-independent draw groups cannot be used while hit testing is enabled");
			}

			const std::size_t hit_item_offset = m_hit_test_items.size();
			if (context.m_hit_test_items.size() > m_hit_test_items.max_size() - hit_item_offset) [[unlikely]]
			{
				throw std::length_error("2D frame exhausted its hit-test item capacity");
			}
			if (context.m_used_fonts.size() > m_used_fonts.max_size() - m_used_fonts.size()) [[unlikely]]
			{
				throw std::length_error("2D frame exhausted its font reference capacity");
			}

			::mngl::detail::reserve_vector_growth(m_hit_test_items,
			                                      hit_item_offset + context.m_hit_test_items.size());
			::mngl::detail::reserve_vector_growth(m_used_fonts,
			                                      m_used_fonts.size() + context.m_used_fonts.size());
			if (context.m_svg_images.size() > m_svg_frame_images.max_size() - m_svg_frame_images.size() ||
			    context.m_svg_effects.size() > m_svg_effect_jobs.max_size() - m_svg_effect_jobs.size())
			{
				throw std::length_error("2D frame exhausted its SVG resource capacity");
			}
			::mngl::detail::reserve_vector_growth(m_svg_frame_images,
			                                      m_svg_frame_images.size() + context.m_svg_images.size());
			::mngl::detail::reserve_vector_growth(m_svg_effect_jobs,
			                                      m_svg_effect_jobs.size() + context.m_svg_effects.size());
			if (context.m_order_independent)
			{
				m_batcher.merge_order_independent_recording_context(context.m_batcher_context);
			}
			else
			{
				m_batcher.merge_recording_context(context.m_batcher_context);
			}

			for (auto& item : context.m_hit_test_items)
			{
				item.draw_index += hit_item_offset;
				m_hit_test_items.push_back(std::move(item));
			}
			for (auto* typeface : context.m_used_fonts)
			{
				if (std::find(m_used_fonts.begin(), m_used_fonts.end(), typeface) == m_used_fonts.end())
				{
					m_used_fonts.push_back(typeface);
				}
			}
			std::ranges::move(context.m_svg_images, std::back_inserter(m_svg_frame_images));
			std::ranges::move(context.m_svg_effects, std::back_inserter(m_svg_effect_jobs));
			context.detach_after_merge();
		}

		void ensure_preparation_allowed(std::uint64_t token) const
		{
			ensure_frame(token);
			if (m_recording_state && m_recording_state->outstanding_recording_contexts.load(
			                             std::memory_order_acquire) != 0) [[unlikely]]
			{
				throw std::logic_error("Prepare frame resources before creating worker recording contexts");
			}
		}

		void prepare_shader_sprite(std::uint64_t token, const ::mngl::sprite& value,
		                           const gpu::graphics_shader_asset& shader)
		{
			ensure_preparation_allowed(token);
			const ::mngl::texture* source = value.m_borrowed_source;
			if (!source && value.source.page.valid())
			{
				source = &m_textures.resolve(value.source.page);
			}
			(void)automatic_material_instance(token, shader, source, nullptr);
		}

		void prepare_font(std::uint64_t token, font& typeface, std::string_view text)
		{
			ensure_preparation_allowed(token);
			remember_font(nullptr, typeface);
			typeface.prepare_glyphs(text);
		}

		struct frame_font_uploads
		{
			std::vector<font*> fonts;
			std::vector<std::unique_lock<std::mutex>> locks;
		};

		[[nodiscard]] frame_font_uploads lock_frame_fonts(const frame_2d& frame)
		{
			ensure_frame(frame.m_token);
			if (frame.m_recording_context) [[unlikely]]
			{
				throw std::logic_error("Only a parent frame_2d can submit its font atlas updates");
			}

			std::sort(m_used_fonts.begin(), m_used_fonts.end(), std::less<font*>{});
			m_used_fonts.erase(std::unique(m_used_fonts.begin(), m_used_fonts.end()), m_used_fonts.end());

			frame_font_uploads uploads;
			uploads.fonts = m_used_fonts;
			uploads.locks.reserve(m_used_fonts.size());
			for (font* typeface : m_used_fonts)
			{
				if (!typeface || !typeface->m_implementation) [[unlikely]]
				{
					throw std::logic_error("2D frame contains an invalid font reference");
				}
				uploads.locks.emplace_back(typeface->m_implementation->upload_mutex);
			}
			return uploads;
		}

		void mark_frame_fonts_dirty(const frame_font_uploads& uploads) noexcept
		{
			for (font* typeface : uploads.fonts)
			{
				if (typeface)
				{
					typeface->mark_uploads_dirty();
				}
			}
		}

	public:
		[[nodiscard]] gpu::material_handle create_material(
		    const gpu::graphics_shader_asset& fragment_shader,
		    gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha)
		{
			if (!m_custom_materials_allowed) [[unlikely]]
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			return m_batcher.create_material(fragment_shader, blend);
		}

		[[nodiscard]] gpu::material_instance_handle create_material_instance(
		    gpu::material_handle material, gpu::fragment_resource_bindings bindings)
		{
			if (!m_custom_materials_allowed) [[unlikely]]
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			return m_batcher.create_material_instance(material, std::move(bindings));
		}

		[[nodiscard]] gpu::material_instance_handle create_material_instance_from_shader(
		    gpu::material_handle material, const gpu::graphics_shader_asset& fragment_shader,
		    gpu::fragment_resource_bindings bindings = {})
		{
			ensure_material_resources_change_allowed();
			bindings.add_uniform_blocks(fragment_shader);
			const gpu::sampler* automatic_sampler =
			    fragment_shader.needs_default_sampler() ? &default_sampler() : nullptr;
			bindings.add_shader_resources(fragment_shader, automatic_sampler);
			return create_material_instance(material, std::move(bindings));
		}

		void update_material_instance_from_shader(gpu::material_instance_handle instance,
		                                          const gpu::graphics_shader_asset& fragment_shader,
		                                          gpu::fragment_resource_bindings bindings = {})
		{
			ensure_material_resources_change_allowed();
			bindings.add_uniform_blocks(fragment_shader);
			const gpu::sampler* automatic_sampler =
			    fragment_shader.needs_default_sampler() ? &default_sampler() : nullptr;
			bindings.add_shader_resources(fragment_shader, automatic_sampler);
			update_material_instance(instance, std::move(bindings));
		}

		void update_material_instance(gpu::material_instance_handle instance,
		                              gpu::fragment_resource_bindings bindings)
		{
			if (!m_custom_materials_allowed) [[unlikely]]
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			m_batcher.update_material_instance(instance, std::move(bindings));
		}

		void destroy_material_instance(gpu::material_instance_handle instance)
		{
			if (!m_custom_materials_allowed) [[unlikely]]
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			m_batcher.destroy_material_instance(instance);
		}

		void destroy_material(gpu::material_handle material)
		{
			if (!m_custom_materials_allowed) [[unlikely]]
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			m_batcher.destroy_material(material);
		}

		[[nodiscard]] const gpu::batcher2d::frame_stats& last_frame_stats() const noexcept
		{
			return m_batcher.last_frame_stats();
		}

	private:
		friend class frame_2d;
		friend class recording_context;

		struct svg_gradient_uniforms
		{
			glm::vec4 coordinate_row_x{};
			glm::vec4 coordinate_row_y{};
			glm::vec4 parameters0{};
			glm::vec4 parameters1{};
			glm::uvec4 control{};
		};
		static_assert(sizeof(svg_gradient_uniforms) == 80);
		static_assert(std::is_trivially_copyable_v<svg_gradient_uniforms>);

		struct svg_gradient_gpu_stop
		{
			glm::vec4 color{};
			glm::vec4 parameters{};
		};
		static_assert(sizeof(svg_gradient_gpu_stop) == 32);
		static_assert(std::is_trivially_copyable_v<svg_gradient_gpu_stop>);

		struct svg_gradient_fast_key
		{
			const detail::svg::gradient_definition* definition = nullptr;
			svg_gradient_uniforms uniforms{};
			gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha;

			[[nodiscard]] bool operator==(const svg_gradient_fast_key& other) const noexcept
			{
				return definition == other.definition && blend == other.blend &&
				       std::memcmp(&uniforms, &other.uniforms, sizeof(uniforms)) == 0;
			}
		};

		struct svg_gradient_fast_key_hash
		{
			[[nodiscard]] std::size_t operator()(const svg_gradient_fast_key& key) const noexcept
			{
				std::size_t hash = std::hash<const void*>{}(key.definition);
				hash ^= std::hash<unsigned int>{}(static_cast<unsigned int>(key.blend)) +
				        0x9e3779b9u + (hash << 6) + (hash >> 2);
				const auto* bytes = reinterpret_cast<const std::byte*>(&key.uniforms);
				for (std::size_t offset = 0; offset < sizeof(key.uniforms); offset += sizeof(std::uint64_t))
				{
					std::uint64_t word = 0;
					std::memcpy(&word, bytes + offset, sizeof(word));
					hash ^= std::hash<std::uint64_t>{}(word) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
				}
				return hash;
			}
		};

		static gpu::texture_format required_swapchain_format(const gpu::device& device,
		                                                     const window& target_window)
		{
			if (!device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot create a 2D renderer with a null GPU device");
			}
			if (!target_window.raw()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot create a 2D renderer with a null window");
			}
			const auto format = device.swapchain_texture_format(target_window);
			if (format == gpu::texture_format::invalid) [[unlikely]]
			{
				throw std::runtime_error("The renderer window must be claimed by its GPU device");
			}
			return format;
		}

		[[nodiscard]] bool owns_frame(std::uint64_t token) const noexcept
		{
			return m_frame_active.load(std::memory_order_acquire) && token != 0 &&
			       token == m_active_frame_token;
		}

		[[nodiscard]] gpu::sampler& default_sampler()
		{
			if (!m_default_sampler)
			{
				m_default_sampler.emplace(gpu::sampler_builder::create_linear_clamp(m_device));
			}
			return *m_default_sampler;
		}

		void ensure_material_resources_change_allowed() const
		{
			if (!available_for_frame()) [[unlikely]]
			{
				throw std::logic_error(
				    "Material resources cannot change while a 2D frame is active or being submitted");
			}
		}

		struct automatic_instance_key
		{
			const gpu::graphics_shader_asset* shader = nullptr;
			const ::mngl::texture* source = nullptr;
			bool operator==(const automatic_instance_key&) const noexcept = default;
		};

		struct automatic_instance_key_hash
		{
			std::size_t operator()(const automatic_instance_key& key) const noexcept
			{
				const auto shader_hash = std::hash<const gpu::graphics_shader_asset*>{}(key.shader);
				const auto source_hash = std::hash<const ::mngl::texture*>{}(key.source);
				return shader_hash ^ (source_hash + 0x9e3779b9u + (shader_hash << 6) + (shader_hash >> 2));
			}
		};

		struct automatic_shader_material
		{
			gpu::material_handle material{};
			gpu::graphics_shader_stage stage = gpu::graphics_shader_stage::fragment;
			gpu::shader_format format = gpu::shader_format::invalid;
			std::string entrypoint;
			gpu::shader_resource_layout resources{};
			std::vector<std::uint8_t> bytecode;
			// The shader snapshot is checked once for each active frame token.
			std::uint64_t validated_frame_token = 0;
		};

		[[nodiscard]] static bool same_shader_code(const automatic_shader_material& cached,
		                                           const gpu::graphics_shader_asset& shader)
		{
			return cached.stage == shader.stage && cached.format == shader.code.format &&
			       cached.entrypoint == shader.entrypoint && cached.resources == shader.resources &&
			       cached.bytecode == shader.code.bytes;
		}

		[[nodiscard]] static std::size_t svg_gradient_blend_index(gpu::sprite_blend_mode blend)
		{
			switch (blend)
			{
			case gpu::sprite_blend_mode::alpha:
				return 0;
			case gpu::sprite_blend_mode::premultiplied_alpha:
				return 1;
			case gpu::sprite_blend_mode::opaque:
				return 2;
			case gpu::sprite_blend_mode::additive:
				return 3;
			default:
				throw std::invalid_argument("Unsupported SVG gradient blend mode");
			}
		}

		[[nodiscard]] static svg_gradient_uniforms make_svg_gradient_uniforms(
		    const detail::svg::gradient_paint& gradient)
		{
			if (!gradient.definition || gradient.definition->stops.empty() ||
			    gradient.definition->stops.size() > std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
			{
				throw std::invalid_argument("SVG gradient has no valid stop data");
			}
			const auto& transform = gradient.coordinate_transform;
			return {
			    .coordinate_row_x = {transform[0][0], transform[1][0], transform[3][0], 0.0f},
			    .coordinate_row_y = {transform[0][1], transform[1][1], transform[3][1], 0.0f},
			    .parameters0 = gradient.parameters0,
			    .parameters1 = gradient.parameters1,
			    .control = {static_cast<std::uint32_t>(gradient.definition->kind),
				            static_cast<std::uint32_t>(gradient.definition->spread),
				            static_cast<std::uint32_t>(gradient.definition->stops.size()),
				            gradient.definition->linear_rgb ? 1u : 0u},
			};
		}

		[[nodiscard]] gpu::graphics_shader_asset& svg_gradient_shader()
		{
			if (!m_svg_gradient_shader)
			{
				gpu::shader_compile_options options{};
				options.stage = gpu::graphics_shader_stage::fragment;
				options.format = detail::svg::select_svg_shader_format(m_device);
				options.resources.storage_buffers = 1;
				options.resources.uniform_buffers = 1;
				m_svg_gradient_shader.emplace(detail::svg::load_svg_shader(
				    paths::shader_directory("svg_gradient.frag.hlsl") / "svg_gradient.frag.hlsl", options));
			}
			return *m_svg_gradient_shader;
		}

		[[nodiscard]] gpu::material_handle svg_gradient_material(gpu::sprite_blend_mode blend,
		                                                         recording_context* context)
		{
			auto& material = m_svg_gradient_materials[svg_gradient_blend_index(blend)];
			if (material.valid())
			{
				return material;
			}
			if (context) [[unlikely]]
			{
				throw std::logic_error(
				    "Call frame_2d::prepare(svg_object) before recording SVG gradients in parallel");
			}
			material = m_batcher.create_material_for_active_frame(svg_gradient_shader(), blend);
			return material;
		}

		[[nodiscard]] gpu::buffer& svg_gradient_stop_buffer(
		    const std::shared_ptr<const detail::svg::gradient_definition>& definition,
		    recording_context* context)
		{
			if (const auto found = m_svg_gradient_stop_buffers.find(definition);
			    found != m_svg_gradient_stop_buffers.end())
			{
				return found->second;
			}
			if (context) [[unlikely]]
			{
				throw std::logic_error(
				    "Call frame_2d::prepare(svg_object) before recording SVG gradients in parallel");
			}

			std::vector<svg_gradient_gpu_stop> stops;
			stops.reserve(definition->stops.size());
			for (const auto& stop : definition->stops)
			{
				stops.push_back(
				    {.color = stop.value.rgba_glsl(), .parameters = {stop.offset, 0.0f, 0.0f, 0.0f}});
			}
			gpu::command_buffer command_buffer(m_device);
			auto buffer = gpu::buffer::create_with_data(m_device, command_buffer,
			                                            std::span<const svg_gradient_gpu_stop>(stops),
			                                            gpu::buffer_usage::graphics_storage_read);
			command_buffer.submit();
			auto inserted = m_svg_gradient_stop_buffers.emplace(
			    std::weak_ptr<const detail::svg::gradient_definition>(definition), std::move(buffer));
			return inserted.first->second;
		}

		[[nodiscard]] std::vector<std::uint8_t> svg_gradient_instance_key(
		    const svg_gradient_uniforms& uniforms, std::span<const svg_gradient_gpu_stop> stops,
		    gpu::sprite_blend_mode blend) const
		{
			if (stops.size() > (std::numeric_limits<std::size_t>::max() - sizeof(uniforms) - 1) /
			                       sizeof(svg_gradient_gpu_stop)) [[unlikely]]
			{
				throw std::length_error("SVG gradient material key exceeds the available address space");
			}
			std::vector<std::uint8_t> key(1 + sizeof(uniforms) + stops.size_bytes());
			key[0] = static_cast<std::uint8_t>(blend);
			std::memcpy(key.data() + 1, &uniforms, sizeof(uniforms));
			std::memcpy(key.data() + 1 + sizeof(uniforms), stops.data(), stops.size_bytes());
			return key;
		}

		[[nodiscard]] gpu::material_instance_handle automatic_svg_gradient_instance(
		    std::uint64_t token, const detail::svg::gradient_paint& gradient, gpu::sprite_blend_mode blend,
		    recording_context* context)
		{
			ensure_frame(token, context);
			const auto uniforms = make_svg_gradient_uniforms(gradient);
			const svg_gradient_fast_key fast_key{gradient.definition.get(), uniforms, blend};
			if (const auto found = m_automatic_svg_gradient_fast_cache.find(fast_key);
			    found != m_automatic_svg_gradient_fast_cache.end())
			{
				return found->second;
			}

			std::vector<svg_gradient_gpu_stop> stops;
			stops.reserve(gradient.definition->stops.size());
			for (const auto& stop : gradient.definition->stops)
			{
				stops.push_back(
				    {.color = stop.value.rgba_glsl(), .parameters = {stop.offset, 0.0f, 0.0f, 0.0f}});
			}
			auto key = svg_gradient_instance_key(uniforms, stops, blend);
			if (const auto found = m_automatic_svg_gradient_instances.find(key);
			    found != m_automatic_svg_gradient_instances.end())
			{
				m_automatic_svg_gradient_fast_cache.emplace(fast_key, found->second);
				return found->second;
			}
			if (context) [[unlikely]]
			{
				throw std::logic_error(
				    "Call frame_2d::prepare(svg_object) before recording SVG gradients in parallel");
			}

			auto& stop_buffer = svg_gradient_stop_buffer(gradient.definition, nullptr);
			auto material = svg_gradient_material(blend, nullptr);
			gpu::fragment_resource_bindings bindings;
			bindings.add_storage_buffer(stop_buffer);
			bindings.add_uniform_block(uniforms);
			::mngl::detail::reserve_vector_growth(m_automatic_instance_handles,
			                                      m_automatic_instance_handles.size() + 1);
			auto instance =
			    m_batcher.create_material_instance_for_active_frame(material, std::move(bindings));
			auto canonical = m_automatic_svg_gradient_instances.end();
			bool canonical_inserted = false;
			bool fast_inserted = false;
			try
			{
				auto insertion = m_automatic_svg_gradient_instances.emplace(std::move(key), instance);
				canonical = insertion.first;
				canonical_inserted = insertion.second;
				fast_inserted = m_automatic_svg_gradient_fast_cache.emplace(fast_key, instance).second;
				m_automatic_instance_handles.push_back(instance);
			}
			catch (...)
			{
				if (fast_inserted)
					m_automatic_svg_gradient_fast_cache.erase(fast_key);
				if (canonical_inserted)
					m_automatic_svg_gradient_instances.erase(canonical);
				m_batcher.destroy_material_instance(instance);
				throw;
			}
			return instance;
		}

		void prepare_svg_gradient(std::uint64_t token, const detail::svg::gradient_paint& gradient,
		                          gpu::sprite_blend_mode blend)
		{
			ensure_preparation_allowed(token);
			(void)automatic_svg_gradient_instance(token, gradient, blend, nullptr);
		}

		[[nodiscard]] gpu::material_instance_handle automatic_material_instance(
		    std::uint64_t token, const gpu::graphics_shader_asset& shader, const ::mngl::texture* source,
		    recording_context* context)
		{
			ensure_frame(token, context);
			if (!m_custom_materials_allowed) [[unlikely]]
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}

			auto material = m_automatic_materials.find(&shader);
			if (material == m_automatic_materials.end())
			{
				if (context) [[unlikely]]
				{
					throw std::logic_error(
					    "Call frame_2d::prepare(sprite) before recording a shader sprite in parallel");
				}
				automatic_shader_material entry{.material = {},
				                                .stage = shader.stage,
				                                .format = shader.code.format,
				                                .entrypoint = shader.entrypoint,
				                                .resources = shader.resources,
				                                .bytecode = shader.code.bytes,
				                                .validated_frame_token = token};
				auto inserted = m_automatic_materials.emplace(&shader, std::move(entry)).first;
				try
				{
					inserted->second.material =
					    m_batcher.create_material_for_active_frame(shader, gpu::sprite_blend_mode::alpha);
					material = m_automatic_materials.find(&shader);
				}
				catch (...)
				{
					m_automatic_materials.erase(&shader);
					throw;
				}
			}
			else if (material->second.validated_frame_token != token)
			{
				if (context) [[unlikely]]
				{
					throw std::logic_error(
					    "Call frame_2d::prepare(sprite) before recording a shader sprite in parallel");
				}
				if (!same_shader_code(material->second, shader)) [[unlikely]]
				{
					throw std::logic_error(
					    "A shader asset used by a sprite changed after its automatic material was created");
				}
				material->second.validated_frame_token = token;
			}

			const automatic_instance_key key{&shader, source};
			if (const auto existing = std::as_const(m_automatic_instances).find(key);
			    existing != std::as_const(m_automatic_instances).end())
			{
				return existing->second;
			}
			if (context) [[unlikely]]
			{
				throw std::logic_error(
				    "Call frame_2d::prepare(sprite) before recording this shader sprite in parallel");
			}

			auto bound_shader = shader;
			if (source)
			{
				const auto& values = bound_shader.resource_values();
				for (const auto& resource : bound_shader.resource_layout())
				{
					if (resource.kind == gpu::shader_resource_kind::sampled_texture &&
					    !values.contains(resource.name))
					{
						bound_shader[resource.name] = *source;
						break;
					}
				}
			}

			gpu::fragment_resource_bindings bindings;
			bindings.add_uniform_blocks(bound_shader);
			const gpu::sampler* sampler = bound_shader.needs_default_sampler() ? &default_sampler() : nullptr;
			bindings.add_shader_resources(bound_shader, sampler);

			::mngl::detail::reserve_vector_growth(m_automatic_instance_handles,
			                                      m_automatic_instance_handles.size() + 1);
			::mngl::detail::reserve_unordered_map_growth(m_automatic_instances,
			                                             m_automatic_instances.size() + 1);
			const auto instance = m_batcher.create_material_instance_for_active_frame(
			    material->second.material, std::move(bindings));
			m_automatic_instance_handles.push_back(instance);
			m_automatic_instances.emplace(key, instance);
			return instance;
		}

		void submit_shader_sprite(std::uint64_t token, recording_context* context,
		                          const ::mngl::sprite& value,
		                          const gpu::graphics_shader_asset& fragment_shader)
		{
			const ::mngl::texture* source = value.m_borrowed_source;
			if (!source && value.source.page.valid())
			{
				source = &m_textures.resolve(value.source.page);
			}
			auto state = value.state;
			state.material = automatic_material_instance(token, fragment_shader, source, context);

			gpu::quad_transform transform{};
			transform.destination = value.destination;
			transform.pivot = value.pivot;
			transform.rotation_radians = value.rotation_radians;
			transform.object_transform = value.transform;

			if (value.m_borrowed_source)
			{
				submit_texture(context, *value.m_borrowed_source, transform, value.tint, state,
				               value.click_callback);
			}
			else
			{
				submit_quad(context, value.source, transform, value.tint, state,
				            ::mngl::detail::hit_test_shape::quad, 0.0f, value.click_callback);
			}
		}

		void submit_quad(recording_context* context, const texture_region& source,
		                 const gpu::quad_transform& transform, const color& tint,
		                 const gpu::quad_draw_state& state,
		                 ::mngl::detail::hit_test_shape shape = ::mngl::detail::hit_test_shape::quad,
		                 float corner_radius = 0.0f,
		                 const std::shared_ptr<const ::mngl::hit_test_callback>& click_callback = {})
		{
			const auto draw_state = apply_frame_scissor(context, state);
			if (draw_state.material.valid() && !m_custom_materials_allowed) [[unlikely]]
			{
				throw std::logic_error("Custom materials are unavailable for this render-target format");
			}
			const gpu::texture* page = nullptr;
			const ::mngl::texture* source_texture = nullptr;
			if (source.page.valid())
			{
				source_texture = &m_textures.resolve(source.page);
				page = &source_texture->raw_gpu_texture();
				if (m_active_target && page == &m_active_target->raw_gpu_texture()) [[unlikely]]
				{
					throw std::invalid_argument(
					    "A render target texture cannot also be sampled in the same frame");
				}
			}
			else if (!draw_state.material.valid()) [[unlikely]]
			{
				throw std::invalid_argument("A textured quad requires a valid texture region");
			}
			const gpu::texture_region_view source_view{page, source.uv_bounds};
			if (context) [[unlikely]]
			{
				m_batcher.draw_quad_unchecked(context->m_batcher_context, source_view, transform, tint,
				                              draw_state);
			}
			else
			{
				m_batcher.draw_quad_unchecked(source_view, transform, tint, draw_state);
			}
			record_hit_item(context, transform, source.uv_bounds, source_texture, draw_state, shape,
			                corner_radius, click_callback);
		}

		void submit_rectangle(recording_context* context, const rectangle& bounds, const color& tint,
		                      const ::mngl::two_d::transform& object_transform = {},
		                      gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha)
		{
			gpu::quad_transform transform{};
			transform.destination = bounds;
			transform.matrix(object_transform.matrix());
			gpu::quad_draw_state state{};
			state.blend = blend;
			submit_quad(context, m_textures.white_region(), transform, tint, state);
		}

		void submit_vertex_array(recording_context* context, const vertex_array& value)
		{
			auto& scratch = context ? context->m_geometry_scratch : m_geometry_scratch;
			const auto triangles = detail::tessellate(value, scratch);
			submit_geometry(context, triangles, value.transform, value.tint, value.blend);
		}

		void submit_prepared_vertex_array(recording_context* context, const prepared_vertex_array& value)
		{
			submit_geometry<true>(context, value.triangle_vertices(), value.transform, value.tint,
			                      value.blend);
		}

		void submit_svg_geometry(recording_context* context, std::span<const glm::vec2> vertices,
		                         const ::mngl::two_d::transform& transform, const color& tint,
		                         gpu::sprite_blend_mode blend)
		{
			submit_geometry<true>(context, vertices, transform, tint, blend);
		}

		void submit_svg_gradient(std::uint64_t token, recording_context* context,
		                         std::span<const glm::vec2> vertices,
		                         const ::mngl::two_d::transform& transform, const color& tint,
		                         gpu::sprite_blend_mode blend, const detail::svg::gradient_paint& gradient)
		{
			const auto instance = automatic_svg_gradient_instance(token, gradient, blend, context);
			const auto& white_page = m_textures.resolve(m_textures.white_region().page).raw_gpu_texture();
			const gpu::texture_region_view source{&white_page, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)};
			gpu::quad_draw_state state{};
			state.blend = blend;
			state.material = instance;
			state = apply_frame_scissor(context, state);
			if (context)
			{
				m_batcher.draw_geometry_unchecked<true>(context->m_batcher_context, vertices, source,
				                                        transform.matrix(), tint, state);
			}
			else
			{
				m_batcher.draw_geometry_unchecked<true>(vertices, source, transform.matrix(), tint, state);
			}
		}

		void submit_svg_gradient_mesh(std::uint64_t token, recording_context* context, const mesh& value,
		                              const detail::svg::gradient_paint& gradient)
		{
			if (!value.m_storage) [[unlikely]]
			{
				throw std::logic_error("Cannot draw a moved-from SVG gradient mesh");
			}
			const auto instance = automatic_svg_gradient_instance(token, gradient, value.blend, context);
			const auto& white_page = m_textures.resolve(m_textures.white_region().page).raw_gpu_texture();
			const gpu::texture_region_view source{&white_page, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)};
			gpu::quad_draw_state state{};
			state.blend = value.blend;
			state.material = instance;
			state = apply_frame_scissor(context, state);
			if (context)
			{
				m_batcher.draw_mesh_unchecked(context->m_batcher_context, value.m_storage, source,
				                              value.transform.matrix(), value.tint, state);
			}
			else
			{
				m_batcher.draw_mesh_unchecked(value.m_storage, source, value.transform.matrix(), value.tint,
				                              state);
			}
		}

		[[nodiscard]] std::shared_ptr<const ::mngl::texture> svg_image_texture(
		    const std::shared_ptr<const detail::svg::image_pixels>& image, recording_context* context)
		{
			if (const auto found = m_svg_image_textures.find(image); found != m_svg_image_textures.end())
			{
				return found->second;
			}
			if (context)
			{
				throw std::logic_error(
				    "Call frame_2d::prepare(svg_object) before recording SVG images in parallel");
			}
			gpu::command_buffer command(m_device);
			auto texture = std::make_shared<::mngl::texture>(m_device, image->size.x, image->size.y,
			                                                 gpu::texture_format::r8g8b8a8_unorm,
			                                                 gpu::texture::usage_flags::sampler);
			texture->update_pixels(command, std::span<const std::uint8_t>(image->rgba));
			command.submit();
			m_svg_image_textures.emplace(image, texture);
			return texture;
		}

		void draw_svg_quad(recording_context* context, const gpu::texture& texture, rectangle bounds,
		                   glm::vec4 uv, const transform& matrix, const color& tint,
		                   gpu::sprite_blend_mode blend, std::uint64_t deferred_token = 0)
		{
			const gpu::texture_region_view source{&texture, uv};
			gpu::quad_transform placement{};
			placement.destination = bounds;
			placement.matrix(matrix.matrix());
			gpu::quad_draw_state state{};
			state.blend = blend;
			state.filter = gpu::filter_mode::linear;
			state = apply_frame_scissor(context, state);
			if (context)
			{
				if (deferred_token != 0)
					m_batcher.draw_quad_deferred(context->m_batcher_context, source, placement, tint, state,
					                             deferred_token);
				else
					m_batcher.draw_quad(context->m_batcher_context, source, placement, tint, state);
			}
			else
			{
				if (deferred_token != 0)
					m_batcher.draw_quad_deferred(source, placement, tint, state, deferred_token);
				else
					m_batcher.draw_quad(source, placement, tint, state);
			}
		}

		void submit_svg_image(recording_context* context, const detail::svg::image_paint& image,
		                      const transform& matrix, const color& tint, gpu::sprite_blend_mode blend,
		                      const std::shared_ptr<const detail::svg::uploaded_assets>& assets)
		{
			std::shared_ptr<const ::mngl::texture> texture;
			if (assets)
			{
				if (const auto found = assets->images.find(image.pixels.get()); found != assets->images.end())
					texture = found->second;
			}
			if (!texture)
				texture = svg_image_texture(image.pixels, context);
			auto& images = context ? context->m_svg_images : m_svg_frame_images;
			images.push_back(texture);
			draw_svg_quad(context, texture->raw_gpu_texture(), image.bounds, image.uv, matrix, tint, blend);
		}

		void submit_svg_effect(const frame_2d& frame,
		                       const std::shared_ptr<const detail::svg::effect_layer>& layer,
		                       const transform& matrix, color tint, gpu::sprite_blend_mode blend,
		                       const std::shared_ptr<const detail::svg::uploaded_assets>& assets)
		{
			if (m_active_target_size.x == 0 || m_active_target_size.y == 0)
				return;
			const auto region = layer->region;
			if (region.size.x == 0.0f || region.size.y == 0.0f)
				return;
			// Resolve the true size and visibility against the final frame view at finish_frame.
			const auto& placeholder =
			    m_textures.resolve(m_textures.white_region().page).raw_gpu_texture();
			auto deferred_token = m_next_svg_effect_deferred_token.fetch_add(1, std::memory_order_relaxed);
			if (deferred_token == 0)
				deferred_token = m_next_svg_effect_deferred_token.fetch_add(1, std::memory_order_relaxed);
			const bool premultiplied = blend == gpu::sprite_blend_mode::alpha ||
			                           blend == gpu::sprite_blend_mode::premultiplied_alpha;
			if (premultiplied)
			{
				tint = color(tint.r * tint.a, tint.g * tint.a, tint.b * tint.a, tint.a);
				blend = gpu::sprite_blend_mode::premultiplied_alpha;
			}
			auto* context = frame.m_recording_context;
			auto& jobs = context ? context->m_svg_effects : m_svg_effect_jobs;
			gpu::quad_draw_state state{};
			state.blend = blend;
			state.filter = gpu::filter_mode::linear;
			state = apply_frame_scissor(context, state);
			if (state.scissor && (state.scissor->w == 0 || state.scissor->h == 0))
				return;
			if (jobs.size() == jobs.max_size()) [[unlikely]]
				throw std::length_error("2D frame exhausted its SVG effect capacity");
			::mngl::detail::reserve_vector_growth(jobs, jobs.size() + 1);
			draw_svg_quad(context, placeholder, region, {0, 0, 1, 1}, matrix, tint, blend,
			              deferred_token);
			jobs.push_back({.layer = layer,
			                .assets = assets,
			                .deferred_token = deferred_token,
			                .placement = matrix.matrix(),
			                .target_size = m_active_target_size,
			                .scissor = state.scissor,
			                .premultiplied = premultiplied});
		}

		void draw_svg_layer_contents(frame_2d& frame, const std::vector<detail::svg::geometry_data>& parts,
		                             const std::shared_ptr<const detail::svg::uploaded_assets>& assets)
		{
			constexpr auto blend = gpu::sprite_blend_mode::alpha;
			for (const auto& part : parts)
			{
				const transform placement(part.transform);
				if (part.effect)
				{
					frame.submit_svg_effect(part.effect, placement, part.tint, blend, assets);
					continue;
				}
				if (part.image)
				{
					frame.submit_svg_image(*part.image, placement, part.tint, blend, assets);
					continue;
				}
				if (assets)
				{
					if (const auto found = assets->meshes.find(&part); found != assets->meshes.end())
					{
						mesh resident;
						resident.m_storage = found->second;
						resident.transform = placement;
						resident.tint = part.tint;
						resident.blend = blend;
						if (part.gradient)
							frame.submit_svg_gradient_mesh(resident, *part.gradient);
						else
							frame.submit_mesh(resident);
						continue;
					}
				}
				if (part.gradient)
					frame.submit_svg_gradient(part.triangle_view(), placement, part.tint, blend, *part.gradient);
				else
					frame.submit_svg_geometry(part.triangle_view(), placement, part.tint, blend);
			}
		}

		void render_svg_effects(gpu::command_buffer& command, const frame_2d& frame)
		{
			std::erase_if(m_svg_effect_command_states,
			              [](const auto& entry) { return entry.second.expired(); });
			if (m_svg_effect_jobs.empty())
				return;
			std::shared_ptr<detail::svg::temporary_target_command_state> command_state;
			const auto get_command_state = [&]() -> const auto&
			{
				if (command_state)
					return command_state;
				auto& weak_state = m_svg_effect_command_states[command.raw()];
				command_state = weak_state.lock();
				if (!command_state)
				{
					command_state = std::make_shared<detail::svg::temporary_target_command_state>();
					weak_state = command_state;
					command.retain_resource(command_state);
				}
				return command_state;
			};
			std::vector<detail::svg::effect_job*> visible_jobs;
			visible_jobs.reserve(m_svg_effect_jobs.size());
			std::vector<gpu::batcher2d::deferred_texture_resolution> deferred_resolutions;
			deferred_resolutions.reserve(m_svg_effect_jobs.size());
			std::vector<std::shared_ptr<::mngl::texture>> reusable_captures;
			for (auto& job : m_svg_effect_jobs)
			{
				const auto& region = job.layer->region;
				const auto end = region.pos + region.size;
				const std::array<glm::vec2, 4> corners = {
				    detail::svg::apply_transform(job.placement, region.pos),
				    detail::svg::apply_transform(job.placement, {end.x, region.pos.y}),
				    detail::svg::apply_transform(job.placement, end),
				    detail::svg::apply_transform(job.placement, {region.pos.x, end.y})};
				const auto projection = projection_for_frame(frame, job.target_size);
				bool intersects_scissor = !job.scissor;
				if (job.scissor && job.scissor->w > 0 && job.scissor->h > 0)
				{
					double left = std::numeric_limits<double>::max();
					double top = std::numeric_limits<double>::max();
					double right = std::numeric_limits<double>::lowest();
					double bottom = std::numeric_limits<double>::lowest();
					bool finite_projection = true;
					for (const auto point : corners)
					{
						const glm::vec4 clip = projection * glm::vec4(point, 0.0f, 1.0f);
						if (!std::isfinite(clip.w) || clip.w <= 0.0f)
						{
							finite_projection = false;
							break;
						}
						const double pixel_x = (static_cast<double>(clip.x) / clip.w + 1.0) *
						                       (0.5 * job.target_size.x);
						const double pixel_y = (1.0 - static_cast<double>(clip.y) / clip.w) *
						                       (0.5 * job.target_size.y);
						left = std::min(left, pixel_x);
						top = std::min(top, pixel_y);
						right = std::max(right, pixel_x);
						bottom = std::max(bottom, pixel_y);
					}
					if (finite_projection)
					{
						const double scissor_left = job.scissor->x;
						const double scissor_top = job.scissor->y;
						const double scissor_right = scissor_left + job.scissor->w;
						const double scissor_bottom = scissor_top + job.scissor->h;
						intersects_scissor = right > scissor_left && left < scissor_right &&
						                     bottom > scissor_top && top < scissor_bottom;
					}
					else
					{
						intersects_scissor = true;
					}
				}
				const bool visible = !m_batcher.outside_target(projection, corners) && intersects_scissor;
				if (!visible)
				{
					deferred_resolutions.push_back(
					    {.token = job.deferred_token, .texture = nullptr, .visible = false});
					continue;
				}
				const glm::dmat4 local_to_clip = glm::dmat4(projection) * glm::dmat4(job.placement);
				const glm::dvec2 pixels(job.target_size);
				const auto dimension = [&](int axis)
				{
					const double extent =
					    glm::length(glm::dvec2(local_to_clip[axis]) * pixels) * (0.5 * region.size[axis]);
					if (!std::isfinite(extent))
						throw std::invalid_argument("SVG effect placement overflows the frame projection");
					return static_cast<unsigned>(std::clamp(std::ceil(extent), 1.0, 2048.0));
				};
				const glm::uvec2 size{dimension(0), dimension(1)};
				const auto& active_command_state = get_command_state();
				job.resolved_output = m_svg_effect_output_targets.acquire(
				    size, [&](glm::uvec2 target_size)
			    {
				    return std::make_shared<gpu::texture>(
				        m_device, target_size, detail::svg::effect_format,
				        gpu::texture::usage_flags::sampler | gpu::texture::usage_flags::color_target);
			    }, active_command_state);
				const auto output_handle = job.resolved_output->raw();
				if (!active_command_state->retained_targets.contains(output_handle))
				{
					command.retain_resource(job.resolved_output);
					active_command_state->retained_targets.insert(output_handle);
				}
				m_svg_effect_output_targets.mark_command_owned(job.resolved_output, active_command_state);
				deferred_resolutions.push_back(
				    {.token = job.deferred_token, .texture = job.resolved_output->raw(), .visible = true});
				visible_jobs.push_back(&job);
			}
			if (!m_batcher.resolve_deferred_textures(deferred_resolutions))
				throw std::logic_error("Could not resolve deferred SVG effect batches");
			if (visible_jobs.empty())
				return;
			const auto& active_command_state = get_command_state();
			if (!m_svg_effect_backend)
				m_svg_effect_backend =
				    std::make_shared<detail::svg::effect_renderer>(m_device, m_svg_effect_target_budget);
			if (!m_svg_offscreen)
			{
				m_svg_offscreen = std::unique_ptr<renderer>(
				    new renderer(m_device, m_textures, detail::svg::effect_format, m_svg_effect_target_budget));
				m_svg_offscreen->m_svg_effect_backend = m_svg_effect_backend;
				m_svg_offscreen->disable_gpu_culling();
			}
			if (!command_state->retained_effect_backend)
			{
				command.retain_resource(m_svg_effect_backend);
				command_state->retained_effect_backend = true;
			}
			for (const auto* job_pointer : visible_jobs)
			{
				const auto& job = *job_pointer;
				const auto size = job.resolved_output->get_size();
				const auto& layer = *job.layer;
				const auto capture = [&](const std::vector<detail::svg::geometry_data>& parts)
				{
					auto target = std::shared_ptr<::mngl::texture>{};
					const auto reusable = std::ranges::find_if(
					    reusable_captures, [&](const auto& candidate) { return candidate->size() == size; });
					if (reusable != reusable_captures.end())
					{
						target = std::move(*reusable);
						reusable_captures.erase(reusable);
					}
					else
					{
						target = m_svg_effect_capture_targets.acquire(
						    size, [&](glm::uvec2 target_size)
						    {
							    return std::make_shared<::mngl::texture>(
							        m_device, target_size.x, target_size.y, detail::svg::effect_format);
						    }, active_command_state);
					}
					const auto capture_handle = target->raw_gpu_texture().raw();
					if (!active_command_state->retained_targets.contains(capture_handle))
					{
						command.retain_resource(target);
						active_command_state->retained_targets.insert(capture_handle);
					}
					m_svg_effect_capture_targets.mark_command_owned(target, active_command_state);
					auto frame = m_svg_offscreen->begin_frame(*target, color(0, 0, 0, 0));
					glm::mat4 projection(1.0f);
					projection[0][0] = 2.0f / layer.region.size.x;
					projection[1][1] = 2.0f / layer.region.size.y;
					projection[3][0] = -1.0f - layer.region.pos.x * projection[0][0];
					projection[3][1] = -1.0f - layer.region.pos.y * projection[1][1];
					frame.set_view_projection(projection);
					m_svg_offscreen->draw_svg_layer_contents(frame, parts, job.assets);
					frame.finish(command);
					return target;
				};
				auto source = capture(layer.source);
				auto mask = layer.mask && !layer.mask_source.empty() ? capture(layer.mask_source) : nullptr;
				const auto retain_filter_target = [&](const std::shared_ptr<gpu::texture>& resource)
				{
					const auto raw = resource->raw();
					if (!command_state->retained_targets.contains(raw))
					{
						command.retain_resource(resource);
						command_state->retained_targets.insert(raw);
					}
				};
				m_svg_effect_backend->render(command, *job.resolved_output, source->raw_gpu_texture(),
				                             mask ? &mask->raw_gpu_texture() : nullptr, layer.region,
				                             layer.mask, layer.filter, job.premultiplied, active_command_state,
				                             retain_filter_target);
				reusable_captures.push_back(std::move(source));
				if (mask)
					reusable_captures.push_back(std::move(mask));
			}
		}

		void submit_mesh(recording_context* context, const mesh& value)
		{
			if (!value.m_storage) [[unlikely]]
			{
				throw std::logic_error("Cannot draw a moved-from 2D mesh");
			}
			const auto& white_page = m_textures.resolve(m_textures.white_region().page).raw_gpu_texture();
			const gpu::texture_region_view source{&white_page, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)};
			gpu::quad_draw_state state{};
			state.blend = value.blend;
			state = apply_frame_scissor(context, state);
			if (context)
			{
				m_batcher.draw_mesh_unchecked(context->m_batcher_context, value.m_storage, source,
				                              value.transform.matrix(), value.tint, state);
			}
			else
			{
				m_batcher.draw_mesh_unchecked(value.m_storage, source, value.transform.matrix(), value.tint,
				                              state);
			}
		}

		void submit_convex_polygon(recording_context* context, const convex_polygon& value)
		{
			auto& scratch = context ? context->m_geometry_scratch : m_geometry_scratch;
			detail::tessellate(value, scratch);
			submit_geometry<true>(context, scratch.triangles, value.transform, value.fill_color, value.blend);
			if (value.outline_thickness > 0.0f)
			{
				detail::tessellate_outline(value, scratch);
				submit_geometry(context, scratch.triangles, value.transform, value.outline_color,
				                value.blend);
			}
		}

		template <bool vertices_validated = false>
		void submit_geometry(recording_context* context, std::span<const glm::vec2> vertices,
		                     const ::mngl::two_d::transform& transform, const color& tint,
		                     gpu::sprite_blend_mode blend)
		{
			if (vertices.empty())
			{
				return;
			}

			const auto& white_page = m_textures.resolve(m_textures.white_region().page).raw_gpu_texture();
			const gpu::texture_region_view source{&white_page, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)};
			gpu::quad_draw_state state{};
			state.blend = blend;
			state = apply_frame_scissor(context, state);
			if (context) [[unlikely]]
			{
				m_batcher.draw_geometry_unchecked<vertices_validated>(
				    context->m_batcher_context, vertices, source, transform.matrix(), tint, state);
			}
			else
			{
				m_batcher.draw_geometry_unchecked<vertices_validated>(vertices, source, transform.matrix(),
				                                                      tint, state);
			}
		}

		void submit_texture(recording_context* context, const ::mngl::texture& source,
		                    const gpu::quad_transform& transform, const color& tint,
		                    const gpu::quad_draw_state& state,
		                    const std::shared_ptr<const ::mngl::hit_test_callback>& click_callback = {})
		{
			if (!source.valid()) [[unlikely]]
			{
				throw std::invalid_argument("A 2D sprite source must be a valid texture");
			}
			if (m_active_target == &source) [[unlikely]]
			{
				throw std::invalid_argument(
				    "A render target texture cannot also be sampled in the same frame");
			}
			if (source.rendering()) [[unlikely]]
			{
				throw std::logic_error("A texture being rendered to cannot be used as a 2D sprite source");
			}

			const auto& gpu_texture = source.raw_gpu_texture();
			if (gpu_texture.device_raw() != m_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("2D sprite source texture belongs to a different GPU device");
			}
			if (!gpu_texture.has_usage(gpu::texture::usage_flags::sampler)) [[unlikely]]
			{
				throw std::invalid_argument("2D sprite source texture must have sampler usage");
			}

			const gpu::texture_region_view source_view{&gpu_texture, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)};
			const auto draw_state = apply_frame_scissor(context, state);
			if (context)
			{
				m_batcher.draw_quad_unchecked(context->m_batcher_context, source_view, transform, tint,
				                              draw_state);
			}
			else
			{
				m_batcher.draw_quad_unchecked(source_view, transform, tint, draw_state);
			}
			record_hit_item(context, transform, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), &source, draw_state,
			                ::mngl::detail::hit_test_shape::quad, 0.0f, click_callback);
		}

		void submit_circle(recording_context* context, const circle& value)
		{
			if (!std::isfinite(value.center.x) || !std::isfinite(value.center.y) ||
			    !std::isfinite(value.radius) || value.radius < 0.0f) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Circle center and radius must be finite and radius non-negative");
			}
			if (value.radius == 0.0f)
			{
				return;
			}

			gpu::quad_transform transform{};
			transform.destination = rectangle{value.center.x - value.radius, value.center.y - value.radius,
			                                  value.radius * 2.0f, value.radius * 2.0f};
			transform.matrix(value.transform.matrix());
			auto state = value.state;
			state.effect = gpu::quad_effect::circle;
			submit_quad(context, m_textures.white_region(), transform, value.tint, state,
			            ::mngl::detail::hit_test_shape::circle, 0.0f, value.click_callback);
		}

		void submit_ellipse(recording_context* context, const ellipse& value)
		{
			if (!std::isfinite(value.center.x) || !std::isfinite(value.center.y) ||
			    !std::isfinite(value.radii.x) || !std::isfinite(value.radii.y) || value.radii.x < 0.0f ||
			    value.radii.y < 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("Ellipse center and radii must be finite and radii non-negative");
			}
			if (value.radii.x == 0.0f || value.radii.y == 0.0f)
			{
				return;
			}

			gpu::quad_transform transform{};
			transform.destination = rectangle{value.center.x - value.radii.x, value.center.y - value.radii.y,
			                                  value.radii.x * 2.0f, value.radii.y * 2.0f};
			transform.matrix(value.transform.matrix());
			auto state = value.state;
			state.effect = gpu::quad_effect::ellipse;
			submit_quad(context, m_textures.white_region(), transform, value.tint, state,
			            ::mngl::detail::hit_test_shape::ellipse, 0.0f, value.click_callback);
		}

		void submit_rounded_rectangle(recording_context* context, const rounded_rectangle& value)
		{
			if (!std::isfinite(value.radius) || value.radius < 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("Rounded rectangle radius must be finite and non-negative");
			}
			if (!std::isfinite(value.bounds.pos.x) || !std::isfinite(value.bounds.pos.y) ||
			    !std::isfinite(value.bounds.size.x) || !std::isfinite(value.bounds.size.y) ||
			    value.bounds.size.x < 0.0f || value.bounds.size.y < 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("Rounded rectangle bounds must be finite and non-negative");
			}
			if (value.bounds.size.x == 0.0f || value.bounds.size.y == 0.0f)
			{
				return;
			}

			gpu::quad_transform transform{};
			transform.destination = value.bounds;
			transform.matrix(value.transform.matrix());
			auto state = value.state;
			state.effect = gpu::quad_effect::rounded_rectangle;
			const float half_min_dimension = std::min(value.bounds.size.x, value.bounds.size.y) * 0.5f;
			state.effect_parameters.x = std::clamp(value.radius / half_min_dimension, 0.0f, 1.0f);
			submit_quad(context, m_textures.white_region(), transform, value.tint, state,
			            ::mngl::detail::hit_test_shape::rounded_rectangle, value.radius,
			            value.click_callback);
		}

		void record_svg_hit_geometry(
		    std::uint64_t token, recording_context* context,
		    const std::shared_ptr<const ::mngl::detail::svg_hit_test_geometry>& geometry,
		    const ::mngl::two_d::transform& transform,
		    const std::shared_ptr<const ::mngl::hit_test_callback>& click_callback)
		{
			ensure_frame(token, context);
			if (!m_active_target || !m_active_target->m_hit_testing_enabled || !geometry ||
			    !geometry->triangles || geometry->triangles->empty())
			{
				return;
			}

			auto& hit_test_items = context ? context->m_hit_test_items : m_hit_test_items;
			gpu::quad_draw_state state{};
			state = apply_frame_scissor(context, state);
			std::optional<glm::ivec4> scissor;
			if (state.scissor)
			{
				scissor = glm::ivec4{state.scissor->x, state.scissor->y, state.scissor->w, state.scissor->h};
			}

			const glm::mat4 local_to_world = transform.matrix();
			const auto world_bounds =
			    ::mngl::detail::transformed_bounds(local_to_world, geometry->bounds);
			const float determinant = glm::determinant(local_to_world);
			glm::mat4 world_to_local(1.0f);
			bool inverse_valid =
			    std::isfinite(determinant) && std::abs(determinant) > std::numeric_limits<float>::min();
			if (inverse_valid)
			{
				world_to_local = glm::inverse(local_to_world);
				inverse_valid = ::mngl::detail::finite(world_to_local);
			}

			hit_test_items.push_back(::mngl::detail::hit_test_item{.draw_index = hit_test_items.size(),
			                                                       .destination = geometry->bounds,
			                                                       .world_to_local = world_to_local,
			                                                       .inverse_valid = inverse_valid,
			                                                       .world_bounds = world_bounds.value_or(::mngl::rectangle{}),
			                                                       .world_bounds_valid = world_bounds.has_value(),
			                                                       .scissor = scissor,
			                                                       .click_callback = click_callback,
			                                                       .svg_geometry = geometry});
		}

		void record_hit_item(recording_context* context, const gpu::quad_transform& transform,
		                     const glm::vec4& uv_bounds, const ::mngl::texture* source,
		                     const gpu::quad_draw_state& state, ::mngl::detail::hit_test_shape shape,
		                     float corner_radius,
		                     const std::shared_ptr<const ::mngl::hit_test_callback>& click_callback)
		{
			if (!m_active_target || !m_active_target->m_hit_testing_enabled)
			{
				return;
			}
			auto& hit_test_items = context ? context->m_hit_test_items : m_hit_test_items;

			std::shared_ptr<const ::mngl::detail::hit_test_frame> child_frame;
			const auto source_hit_frame = source && source->m_hit_test_frame_state
			                                  ? (source->m_hit_test_frame_state->latest_recorded
			                                         ? source->m_hit_test_frame_state->latest_recorded
			                                         : source->m_hit_test_frame_state->completed)
			                                  : nullptr;
			if (source_hit_frame && source_hit_frame->depth < ::mngl::detail::hit_test_max_depth)
			{
				child_frame = source_hit_frame;
			}

			if (shape == ::mngl::detail::hit_test_shape::quad)
			{
				switch (state.effect)
				{
				case gpu::quad_effect::circle:
					shape = ::mngl::detail::hit_test_shape::circle;
					break;
				case gpu::quad_effect::ellipse:
					shape = ::mngl::detail::hit_test_shape::ellipse;
					break;
				case gpu::quad_effect::rounded_rectangle:
					shape = ::mngl::detail::hit_test_shape::rounded_rectangle;
					corner_radius = std::clamp(state.effect_parameters.x, 0.0f, 1.0f) *
					                std::min(transform.destination.size.x, transform.destination.size.y) *
					                0.5f;
					break;
				case gpu::quad_effect::textured:
					break;
				}
			}

			std::optional<glm::ivec4> scissor;
			if (state.scissor)
			{
				scissor = glm::ivec4{state.scissor->x, state.scissor->y, state.scissor->w, state.scissor->h};
			}
			const glm::mat4 local_to_world = transform.model_matrix();
			const auto world_bounds = ::mngl::detail::transformed_bounds(
			    local_to_world, ::mngl::rectangle{glm::vec2(0.0f), glm::vec2(1.0f)});
			const float determinant = glm::determinant(local_to_world);
			glm::mat4 world_to_local(1.0f);
			bool inverse_valid =
			    std::isfinite(determinant) && std::abs(determinant) > std::numeric_limits<float>::min();
			if (inverse_valid)
			{
				world_to_local = glm::inverse(local_to_world);
				inverse_valid = ::mngl::detail::finite(world_to_local);
			}
			hit_test_items.push_back(
			    ::mngl::detail::hit_test_item{.draw_index = hit_test_items.size(),
				                              .destination = transform.destination,
				                              .world_to_local = world_to_local,
				                              .inverse_valid = inverse_valid,
					                              .world_bounds = world_bounds.value_or(::mngl::rectangle{}),
					                              .world_bounds_valid = world_bounds.has_value(),
				                              .shape = shape,
				                              .corner_radius = corner_radius,
				                              .uv_bounds = uv_bounds,
				                              .source_identity = source ? source->m_hit_test_identity : 0,
				                              .source_size = source ? source->size() : glm::uvec2{},
				                              .scissor = scissor,
				                              .child_frame = std::move(child_frame),
				                              .click_callback = click_callback});
		}

		void remember_font(recording_context* context, font& typeface)
		{
			if (typeface.device_raw() != m_device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("Font and 2D renderer must use the same GPU device");
			}
			auto& used_fonts = context ? context->m_used_fonts : m_used_fonts;
			if (std::find(used_fonts.begin(), used_fonts.end(), &typeface) == used_fonts.end())
			{
				used_fonts.push_back(&typeface);
			}
		}

		static void validate_text_options(glm::vec2 top_left, const text_draw_options& options)
		{
			if (!::mngl::detail::finite(top_left) || !std::isfinite(options.height) || options.height <= 0.0f)
			    [[unlikely]]
			{
				throw std::invalid_argument(
				    "Text position must be finite and text height must be finite and positive");
			}
			if (!std::isfinite(options.wrap_width) || options.wrap_width < 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("Text wrap width must be finite and non-negative");
			}

			const glm::vec4 tint = options.tint.rgba_glsl();
			if (!std::isfinite(tint.x) || !std::isfinite(tint.y) || !std::isfinite(tint.z) ||
			    !std::isfinite(tint.w)) [[unlikely]]
			{
				throw std::invalid_argument("Text tint channels must be finite");
			}

			if (options.clip_bounds)
			{
				const auto& clip = *options.clip_bounds;
				if (!::mngl::detail::finite(clip.pos) || !::mngl::detail::finite(clip.size) ||
				    clip.size.x < 0.0f || clip.size.y < 0.0f || !std::isfinite(clip.pos.x + clip.size.x) ||
				    !std::isfinite(clip.pos.y + clip.size.y)) [[unlikely]]
				{
					throw std::invalid_argument(
					    "Text clip bounds must be finite with non-negative dimensions");
				}
			}

			if (options.scissor && (options.scissor->w < 0 || options.scissor->h < 0)) [[unlikely]]
			{
				throw std::invalid_argument("Text scissor dimensions must be non-negative");
			}
		}

		[[nodiscard]] std::vector<std::string_view> wrap_text_line(
		    recording_context* context, font& typeface, std::string_view text, float scale,
		    float wrap_width)
		{
			std::vector<std::string_view> lines;
			if (text.empty())
			{
				lines.emplace_back(text);
				return lines;
			}

			const auto space = context ? typeface.glyph_prepared(' ') : typeface.glyph(' ', false);
			const float tab_width = space.advance * scale * 4.0f;
			std::size_t line_begin = 0;
			std::size_t offset = 0;
			std::size_t break_line_end = std::string_view::npos;
			std::size_t break_next_start = std::string_view::npos;
			bool in_wrap_whitespace = false;
			float pen_x = 0.0f;
			std::uint32_t previous_codepoint = 0;

			while (offset < text.size())
			{
				const std::size_t codepoint_begin = offset;
				std::uint32_t codepoint = 0;
				(void)::mngl::two_d::detail::next_utf8_codepoint(text, offset, codepoint);

				if (codepoint == ' ' || codepoint == '\t')
				{
					if (!in_wrap_whitespace)
					{
						break_line_end = codepoint_begin;
					}
					break_next_start = offset;
					in_wrap_whitespace = true;
				}
				else
				{
					in_wrap_whitespace = false;
				}

				float next_pen_x = pen_x;
				std::uint32_t next_previous_codepoint = previous_codepoint;
				if (codepoint == '\t')
				{
					if (tab_width > 0.0f)
					{
						next_pen_x = (std::floor(next_pen_x / tab_width) + 1.0f) * tab_width;
					}
					next_previous_codepoint = 0;
				}
				else
				{
					const auto metrics =
					    context ? typeface.glyph_prepared(codepoint) : typeface.glyph(codepoint, false);
					if (previous_codepoint != 0)
					{
						const float kerning =
						    context ? typeface.kerning_prepared(previous_codepoint, metrics.codepoint)
							        : typeface.kerning(previous_codepoint, metrics.codepoint);
						next_pen_x += kerning * scale;
					}
					next_pen_x += metrics.advance * scale;
					next_previous_codepoint = metrics.codepoint;
				}
				if (!std::isfinite(next_pen_x)) [[unlikely]]
				{
					throw std::length_error("Text line exceeds the renderer's coordinate range");
				}

				if (next_pen_x > wrap_width && codepoint_begin > line_begin)
				{
					if (break_line_end != std::string_view::npos && break_line_end > line_begin &&
					    break_next_start > break_line_end)
					{
						const auto line = text.substr(line_begin, break_line_end - line_begin);
						if (line.find_first_not_of(" \t") != std::string_view::npos)
						{
							lines.emplace_back(line);
						}
						line_begin = break_next_start;
					}
					else
					{
						const auto line = text.substr(line_begin, codepoint_begin - line_begin);
						if (line.find_first_not_of(" \t") != std::string_view::npos)
						{
							lines.emplace_back(line);
						}
						line_begin = codepoint_begin;
					}

					offset = line_begin;
					pen_x = 0.0f;
					previous_codepoint = 0;
					break_line_end = std::string_view::npos;
					break_next_start = std::string_view::npos;
					in_wrap_whitespace = false;
					continue;
				}

				pen_x = next_pen_x;
				previous_codepoint = next_previous_codepoint;
			}

			if (line_begin < text.size() || lines.empty())
			{
				lines.emplace_back(text.substr(line_begin));
			}
			return lines;
		}

		[[nodiscard]] float submit_text_line(recording_context* context, font& typeface,
		                                     std::string_view text, glm::vec2 baseline, float scale,
		                                     const text_draw_options& options)
		{
			if (!::mngl::detail::finite(baseline)) [[unlikely]]
			{
				throw std::length_error("Text line exceeds the renderer's coordinate range");
			}

			float pen_x = baseline.x;
			std::uint32_t previous_codepoint = 0;
			const auto space = context ? typeface.glyph_prepared(' ') : typeface.glyph(' ', false);
			const float tab_width = space.advance * scale * 4.0f;
			gpu::quad_draw_state state{};
			state.scissor = options.scissor;
			state = apply_frame_scissor(context, state);
			const bool scissor_empty = state.scissor && (state.scissor->w == 0 || state.scissor->h == 0);

			for (std::size_t offset = 0; offset < text.size();)
			{
				std::uint32_t codepoint = 0;
				(void)::mngl::two_d::detail::next_utf8_codepoint(text, offset, codepoint);
				if (codepoint == '\t')
				{
					if (tab_width > 0.0f)
					{
						const float relative_x = pen_x - baseline.x;
						pen_x = baseline.x + (std::floor(relative_x / tab_width) + 1.0f) * tab_width;
					}
					if (!std::isfinite(pen_x)) [[unlikely]]
					{
						throw std::length_error("Text line exceeds the renderer's coordinate range");
					}
					previous_codepoint = 0;
					continue;
				}

				const auto metrics =
				    context ? typeface.glyph_prepared(codepoint) : typeface.glyph(codepoint, false);
				if (previous_codepoint != 0)
				{
					const float kerning =
					    context ? typeface.kerning_prepared(previous_codepoint, metrics.codepoint)
						        : typeface.kerning(previous_codepoint, metrics.codepoint);
					pen_x += kerning * scale;
				}
				if (!std::isfinite(pen_x)) [[unlikely]]
				{
					throw std::length_error("Text line exceeds the renderer's coordinate range");
				}

				if (metrics.width > 0 && metrics.height > 0)
				{
					const rectangle glyph_bounds{pen_x + static_cast<float>(metrics.x_offset) * scale,
					                             baseline.y - (static_cast<float>(metrics.y_offset) +
					                                           static_cast<float>(metrics.height)) *
					                                              scale,
					                             static_cast<float>(metrics.width) * scale,
					                             static_cast<float>(metrics.height) * scale};
					if (!::mngl::detail::finite(glyph_bounds.pos) ||
					    !::mngl::detail::finite(glyph_bounds.size) ||
					    !std::isfinite(glyph_bounds.pos.x + glyph_bounds.size.x) ||
					    !std::isfinite(glyph_bounds.pos.y + glyph_bounds.size.y)) [[unlikely]]
					{
						throw std::length_error("Text glyph exceeds the renderer's coordinate range");
					}
					const bool in_clip =
					    !options.clip_bounds || glyph_bounds.intersects(*options.clip_bounds);
					if (!scissor_empty && in_clip)
					{
						const auto glyph = context ? metrics : typeface.glyph(codepoint, true);
						if (!glyph.atlas_page) [[unlikely]]
						{
							throw std::logic_error("Rasterized text glyph has no atlas page");
						}

						gpu::quad_transform transform{};
						transform.destination = glyph_bounds;
						const gpu::texture_region_view source{&glyph.atlas_page->raw_gpu_texture(),
						                                      glyph.uv_bounds};
						if (context)
						{
							m_batcher.draw_quad_unchecked(context->m_batcher_context, source, transform,
							                              options.tint, state);
						}
						else
						{
							m_batcher.draw_quad_unchecked(source, transform, options.tint, state);
						}
					}
				}

				pen_x += metrics.advance * scale;
				if (!std::isfinite(pen_x)) [[unlikely]]
				{
					throw std::length_error("Text line exceeds the renderer's coordinate range");
				}
				previous_codepoint = metrics.codepoint;
			}

			const float width = pen_x - baseline.x;
			if (!std::isfinite(width)) [[unlikely]]
			{
				throw std::length_error("Text line exceeds the renderer's coordinate range");
			}
			return width;
		}

		void record_text_hit(recording_context* context, const rectangle& bounds,
		                     const text_draw_options& options)
		{
			if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f)
			{
				return;
			}

			gpu::quad_transform transform{};
			transform.destination = bounds;
			gpu::quad_draw_state state{};
			state.scissor = options.scissor;
			record_hit_item(context, transform, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), nullptr, state,
			                ::mngl::detail::hit_test_shape::quad, 0.0f, {});
		}

		void submit_text(recording_context* context, font& typeface, std::string_view text,
		                 glm::vec2 top_left, const text_draw_options& options)
		{
			validate_text_options(top_left, options);
			if (text.empty() ||
			    (options.clip_bounds &&
			     (options.clip_bounds->size.x == 0.0f || options.clip_bounds->size.y == 0.0f)) ||
			    (options.scissor && (options.scissor->w == 0 || options.scissor->h == 0)))
			{
				return;
			}

			remember_font(context, typeface);
			const float scale = options.height / typeface.raster_height_pixels();
			const float line_height = typeface.line_height() * scale;
			const float ascent = typeface.ascent() * scale;
			if (!std::isfinite(scale) || scale <= 0.0f || !std::isfinite(line_height) ||
			    line_height <= 0.0f || !std::isfinite(ascent)) [[unlikely]]
			{
				throw std::invalid_argument("Font metrics cannot be scaled to the requested text height");
			}

			std::size_t line_index = 0;
			float max_width = 0.0f;
			const auto submit_visual_line = [&](std::string_view line_text)
			{
				const float top = top_left.y - static_cast<float>(line_index) * line_height;
				if (!std::isfinite(top)) [[unlikely]]
				{
					throw std::length_error("Text block exceeds the renderer's coordinate range");
				}
				const bool line_visible =
				    !options.clip_bounds ||
				    (top > options.clip_bounds->pos.y &&
				     top - line_height < options.clip_bounds->pos.y + options.clip_bounds->size.y);
				if (line_visible)
				{
					const float width = submit_text_line(context, typeface, line_text,
					                                     {top_left.x, top - ascent}, scale, options);
					max_width = std::max(max_width, width);
				}
				++line_index;
			};

			std::size_t line_begin = 0;
			while (line_begin <= text.size())
			{
				const std::size_t line_end = text.find('\n', line_begin);
				std::string_view line_text = text.substr(
				    line_begin, line_end == std::string_view::npos ? line_end : line_end - line_begin);
				if (!line_text.empty() && line_text.back() == '\r')
				{
					line_text.remove_suffix(1);
				}

				if (options.wrap_width > 0.0f)
				{
					const auto wrapped_lines =
					    wrap_text_line(context, typeface, line_text, scale, options.wrap_width);
					for (const auto wrapped_line : wrapped_lines)
					{
						submit_visual_line(wrapped_line);
					}
				}
				else
				{
					submit_visual_line(line_text);
				}

				if (line_end == std::string_view::npos)
				{
					break;
				}
				line_begin = line_end + 1;
			}

			const float block_height = static_cast<float>(line_index) * line_height;
			rectangle bounds{top_left.x, top_left.y - block_height, max_width, block_height};
			if (!::mngl::detail::finite(bounds.pos) || !::mngl::detail::finite(bounds.size) ||
			    !std::isfinite(bounds.pos.x + bounds.size.x) || !std::isfinite(bounds.pos.y + bounds.size.y))
			    [[unlikely]]
			{
				throw std::length_error("Text block exceeds the renderer's coordinate range");
			}
			if (options.clip_bounds)
			{
				bounds = bounds.intersection(*options.clip_bounds);
			}
			record_text_hit(context, bounds, options);
		}

		void submit_text_document(recording_context* context, font& typeface, const text_document& document,
		                          glm::vec2 top_left, const text_draw_options& options)
		{
			validate_text_options(top_left, options);
			if (options.wrap_width > 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("Automatic wrapping is not supported for text_document");
			}
			if (!options.clip_bounds) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Drawing a text document requires clip_bounds for line virtualization");
			}
			const auto& viewport = *options.clip_bounds;
			if (viewport.size.x == 0.0f || viewport.size.y == 0.0f || document.line_count() == 0 ||
			    (options.scissor && (options.scissor->w == 0 || options.scissor->h == 0)))
			{
				return;
			}

			remember_font(context, typeface);
			const float scale = options.height / typeface.raster_height_pixels();
			const float line_height = typeface.line_height() * scale;
			const float ascent = typeface.ascent() * scale;
			if (!std::isfinite(scale) || scale <= 0.0f || !std::isfinite(line_height) ||
			    line_height <= 0.0f || !std::isfinite(ascent)) [[unlikely]]
			{
				throw std::invalid_argument("Font metrics cannot be scaled to the requested text height");
			}

			const double first_offset =
			    (static_cast<double>(top_left.y) - (static_cast<double>(viewport.pos.y) + viewport.size.y)) /
			    line_height;
			const double end_offset = (static_cast<double>(top_left.y) - viewport.pos.y) / line_height;
			const std::size_t first_line =
			    first_offset <= 0.0
			        ? 0
			        : static_cast<std::size_t>(
			              std::min(std::floor(first_offset), static_cast<double>(document.line_count())));
			const std::size_t end_line =
			    end_offset <= 0.0 ? 0
				                  : static_cast<std::size_t>(std::min(
			                            std::ceil(end_offset), static_cast<double>(document.line_count())));
			if (first_line >= end_line)
			{
				return;
			}

			float max_width = 0.0f;
			for (std::size_t index = first_line; index < end_line; ++index)
			{
				const float line_top = top_left.y - static_cast<float>(index) * line_height;
				if (!std::isfinite(line_top)) [[unlikely]]
				{
					throw std::length_error("Text document exceeds the renderer's coordinate range");
				}
				max_width =
				    std::max(max_width, submit_text_line(context, typeface, document.line(index),
					                                     {top_left.x, line_top - ascent}, scale, options));
			}

			const float block_height = static_cast<float>(end_line - first_line) * line_height;
			rectangle bounds{top_left.x, top_left.y - static_cast<float>(end_line) * line_height, max_width,
			                 block_height};
			if (!::mngl::detail::finite(bounds.pos) || !::mngl::detail::finite(bounds.size) ||
			    !std::isfinite(bounds.pos.x + bounds.size.x) || !std::isfinite(bounds.pos.y + bounds.size.y))
			    [[unlikely]]
			{
				throw std::length_error("Text document exceeds the renderer's coordinate range");
			}
			bounds = bounds.intersection(viewport);
			record_text_hit(context, bounds, options);
		}

		void finish_frame(frame_2d& frame, gpu::command_buffer* external_command_buffer = nullptr)
		{
			ensure_frame(frame.m_token);
			if (frame.m_recording_context) [[unlikely]]
			{
				throw std::logic_error("A worker recording context cannot finish its parent frame");
			}
			if (m_recording_state && m_recording_state->outstanding_recording_contexts.load(
			                             std::memory_order_acquire) != 0) [[unlikely]]
			{
				throw std::logic_error(
				    "Finish the worker recording contexts and merge them before finishing the frame");
			}
			bool font_uploads_started = false;
			try
			{
				std::shared_ptr<::mngl::detail::hit_test_frame> hit_frame;
				if (m_active_target && m_active_target->m_hit_testing_enabled)
				{
					hit_frame = std::make_shared<::mngl::detail::hit_test_frame>();
					hit_frame->target_size = m_active_target->size();
					hit_frame->view_projection = projection_for_frame(frame, hit_frame->target_size);
					hit_frame->items = std::move(m_hit_test_items);
					for (const auto& item : hit_frame->items)
					{
						if (item.child_frame)
						{
							hit_frame->depth = std::max(hit_frame->depth, item.child_frame->depth + 1);
						}
					}
				}

				auto flush = [&](gpu::command_buffer& cmd)
				{
					if (!cmd.valid() || cmd.device_raw() != m_device.raw()) [[unlikely]]
					{
						throw std::invalid_argument(
						    "2D frame command buffer must belong to the renderer's GPU device");
					}

					font_uploads_started = true;
					for (font* typeface : m_used_fonts)
					{
						if (!typeface) [[unlikely]]
						{
							throw std::logic_error("2D frame contains a null font reference");
						}
						typeface->upload_pending(cmd);
					}
					render_svg_effects(cmd, frame);
					for (const auto& image : m_svg_frame_images)
					{
						cmd.retain_resource(image);
					}

					auto flush_target = [&](gpu::texture& target)
					{
						const auto projection = projection_for_frame(frame, target.get_size());
						m_batcher.flush(cmd, target, projection);
					};

					if (m_active_target)
					{
						flush_target(m_active_target->raw_gpu_texture());
					}
					else
					{
						if (!m_window) [[unlikely]]
						{
							throw std::logic_error("This 2D renderer requires a render target texture");
						}
						auto target = cmd.acquire_swapchain_texture(*m_window);
						flush_target(target);
					}
				};

				if (external_command_buffer)
				{
					flush(*external_command_buffer);
					if (hit_frame)
					{
						auto frame_state = m_active_target->m_hit_test_frame_state;
						if (!frame_state) [[unlikely]]
						{
							throw std::logic_error("Hit testing was enabled without a snapshot state");
						}
					std::weak_ptr<::mngl::detail::hit_test_frame_state> weak_state = frame_state;
					external_command_buffer->on_completion(
					    [weak_state, hit_frame](bool submitted)
					    {
						    if (auto state = weak_state.lock())
						    {
							    if (submitted)
							    {
								    state->completed = hit_frame;
							    }
							    if (state->latest_recorded == hit_frame)
							    {
								    state->latest_recorded.reset();
							    }
						    }
				    });
						frame_state->latest_recorded = hit_frame;
					}
				}
				else
				{
					gpu::command_buffer cmd(m_device);
					flush(cmd);
					if (hit_frame)
					{
						auto frame_state = m_active_target->m_hit_test_frame_state;
						if (!frame_state) [[unlikely]]
						{
							throw std::logic_error("Hit testing was enabled without a snapshot state");
						}
					std::weak_ptr<::mngl::detail::hit_test_frame_state> weak_state = frame_state;
					cmd.on_completion(
					    [weak_state, hit_frame](bool submitted)
					    {
						    if (auto state = weak_state.lock())
						    {
							    if (submitted)
							    {
								    state->completed = hit_frame;
							    }
							    if (state->latest_recorded == hit_frame)
							    {
								    state->latest_recorded.reset();
							    }
						    }
				    });
						frame_state->latest_recorded = hit_frame;
					}
					cmd.submit();
				}
			}
			catch (...)
			{
				if (font_uploads_started)
				{
					for (font* typeface : m_used_fonts)
					{
						if (typeface)
						{
							typeface->mark_uploads_dirty();
						}
					}
				}
				m_batcher.cancel_frame();
				m_active_frame_token = 0;
				m_active_target = nullptr;
				m_active_target_size = {};
				m_active_scissor.reset();
				m_recording_state.reset();
				m_hit_test_items.clear();
				m_used_fonts.clear();
				m_svg_frame_images.clear();
				m_svg_effect_jobs.clear();
				m_frame_active.store(false, std::memory_order_release);
				frame.detach();
				throw;
			}

			m_active_frame_token = 0;
			m_active_target = nullptr;
			m_active_target_size = {};
			m_active_scissor.reset();
			m_recording_state.reset();
			m_hit_test_items.clear();
			m_used_fonts.clear();
			m_svg_frame_images.clear();
			m_svg_effect_jobs.clear();
			m_frame_active.store(false, std::memory_order_release);
			frame.detach();
		}

		void cancel_frame(std::uint64_t token) noexcept
		{
			if (owns_frame(token))
			{
				m_batcher.cancel_frame();
				m_active_frame_token = 0;
				m_active_target = nullptr;
				m_active_target_size = {};
				m_active_scissor.reset();
				m_recording_state.reset();
				m_hit_test_items.clear();
				m_used_fonts.clear();
				m_svg_frame_images.clear();
				m_svg_effect_jobs.clear();
				m_frame_active.store(false, std::memory_order_release);
			}
		}

		static glm::mat4 coordinate_projection(glm::uvec2 target_size)
		{
			glm::mat4 projection(1.0f);
			if (target_size.x == 0 || target_size.y == 0)
			{
				return projection;
			}

			const float short_side = static_cast<float>(std::min(target_size.x, target_size.y));
			const float coordinate_width = static_cast<float>(target_size.x) / short_side;
			const float coordinate_height = static_cast<float>(target_size.y) / short_side;
			projection[0][0] = 2.0f / coordinate_width;
			projection[1][1] = 2.0f / coordinate_height;
			projection[3][0] = -1.0f;
			projection[3][1] = -1.0f;
			return projection;
		}

		static glm::mat4 projection_for_frame(const frame_2d& frame, glm::uvec2 target_size)
		{
			if (frame.m_view_projection)
			{
				return *frame.m_view_projection;
			}
			if (frame.m_view)
			{
				return frame.m_view->projection(target_size);
			}
			return coordinate_projection(target_size);
		}

		[[nodiscard]] bool bounds_visible(const frame_2d& frame, const ::mngl::rectangle& bounds) const
		{
			if (!std::isfinite(bounds.pos.x) || !std::isfinite(bounds.pos.y) ||
			    !std::isfinite(bounds.size.x) || !std::isfinite(bounds.size.y) || bounds.size.x < 0.0f ||
			    bounds.size.y < 0.0f || !std::isfinite(bounds.pos.x + bounds.size.x) ||
			    !std::isfinite(bounds.pos.y + bounds.size.y)) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Visible 2D bounds must be finite and have non-negative dimensions");
			}
			if (bounds.size.x == 0.0f || bounds.size.y == 0.0f || m_active_target_size.x == 0 ||
			    m_active_target_size.y == 0)
			{
				return false;
			}

			const float right = bounds.pos.x + bounds.size.x;
			const float top = bounds.pos.y + bounds.size.y;
			const std::array<glm::vec2, 4> corners{{
			    bounds.pos,
			    {right, bounds.pos.y},
			    {right, top},
			    {bounds.pos.x, top},
			}};
			return !m_batcher.outside_target(projection_for_frame(frame, m_active_target_size), corners);
		}

		[[nodiscard]] std::optional<SDL_Rect> exchange_frame_scissor(const frame_2d& frame, SDL_Rect scissor)
		{
			ensure_frame(frame.m_token);
			if (frame.m_recording_context) [[unlikely]]
			{
				throw std::logic_error("Cached-layer scissor scopes require the parent frame");
			}
			if (scissor.w < 0 || scissor.h < 0) [[unlikely]]
			{
				throw std::invalid_argument("Frame scissor dimensions must be non-negative");
			}

			auto previous = m_active_scissor;
			m_active_scissor = previous ? intersect_scissors(*previous, scissor) : scissor;
			return previous;
		}

		void restore_frame_scissor(std::optional<SDL_Rect> scissor) noexcept { m_active_scissor = scissor; }

		[[nodiscard]] gpu::quad_draw_state apply_frame_scissor(const recording_context* context,
		                                                       gpu::quad_draw_state state) const noexcept
		{
			const auto& frame_scissor = context ? context->m_scissor : m_active_scissor;
			if (!frame_scissor)
			{
				return state;
			}
			if (!state.scissor)
			{
				state.scissor = frame_scissor;
			}
			else if (state.scissor->w >= 0 && state.scissor->h >= 0)
			{
				state.scissor = intersect_scissors(*state.scissor, *frame_scissor);
			}
			return state;
		}

		[[nodiscard]] static SDL_Rect intersect_scissors(const SDL_Rect& left, const SDL_Rect& right) noexcept
		{
			const auto left_x = static_cast<std::int64_t>(left.x);
			const auto left_y = static_cast<std::int64_t>(left.y);
			const auto right_x = static_cast<std::int64_t>(right.x);
			const auto right_y = static_cast<std::int64_t>(right.y);
			const auto x0 = std::max(left_x, right_x);
			const auto y0 = std::max(left_y, right_y);
			const auto x1 = std::min(left_x + left.w, right_x + right.w);
			const auto y1 = std::min(left_y + left.h, right_y + right.h);
			if (x1 <= x0 || y1 <= y0)
			{
				return {static_cast<int>(x0), static_cast<int>(y0), 0, 0};
			}
			return {static_cast<int>(x0), static_cast<int>(y0), static_cast<int>(x1 - x0),
			        static_cast<int>(y1 - y0)};
		}

		void ensure_frame(std::uint64_t token) const
		{
			if (!owns_frame(token)) [[unlikely]]
			{
				throw std::logic_error("frame_2d is no longer active");
			}
		}

		void ensure_frame(std::uint64_t token, recording_context* context) const
		{
			ensure_frame(token);
			if (!context) [[likely]]
			{
				if (m_recording_state && m_recording_state->outstanding_recording_contexts.load(
				                             std::memory_order_acquire) != 0) [[unlikely]]
				{
					throw std::logic_error(
					    "Finish and merge worker recording contexts before drawing on the parent frame");
				}
				return;
			}

			if (!context->valid() || context->m_finished || context->m_control != m_frame_control ||
			    context->m_token != token || context->m_recording_state != m_recording_state) [[unlikely]]
			{
				throw std::logic_error("Recording context is no longer active for this frame");
			}
		}

		gpu::device m_device;
		window* m_window = nullptr;
		texture_store& m_textures;
		std::optional<gpu::sampler> m_default_sampler;
		std::optional<gpu::graphics_shader_asset> m_svg_gradient_shader;
		std::array<gpu::material_handle, 4> m_svg_gradient_materials{};
		std::map<std::weak_ptr<const detail::svg::gradient_definition>, gpu::buffer, std::owner_less<>>
		    m_svg_gradient_stop_buffers;
		std::map<std::vector<std::uint8_t>, gpu::material_instance_handle> m_automatic_svg_gradient_instances;
		std::unordered_map<svg_gradient_fast_key, gpu::material_instance_handle, svg_gradient_fast_key_hash>
		    m_automatic_svg_gradient_fast_cache;
		std::map<std::weak_ptr<const detail::svg::image_pixels>, std::shared_ptr<const ::mngl::texture>,
		         std::owner_less<>>
		    m_svg_image_textures;
		std::vector<std::shared_ptr<const ::mngl::texture>> m_svg_frame_images;
		std::vector<detail::svg::effect_job> m_svg_effect_jobs;
		std::shared_ptr<detail::svg::effect_renderer> m_svg_effect_backend;
		std::unordered_map<SDL_GPUCommandBuffer*,
		                   std::weak_ptr<detail::svg::temporary_target_command_state>>
		    m_svg_effect_command_states;
		std::unique_ptr<renderer> m_svg_offscreen;
		gpu::batcher2d m_batcher;
		std::unordered_map<const gpu::graphics_shader_asset*, automatic_shader_material>
		    m_automatic_materials;
		std::unordered_map<automatic_instance_key, gpu::material_instance_handle, automatic_instance_key_hash>
		    m_automatic_instances;
		std::vector<gpu::material_instance_handle> m_automatic_instance_handles;
		std::shared_ptr<detail::renderer_frame_control> m_frame_control;
		std::shared_ptr<detail::renderer_frame_state> m_recording_state;
		::mngl::texture* m_active_target = nullptr;
		glm::uvec2 m_active_target_size{};
		std::optional<SDL_Rect> m_active_scissor;
		std::vector<::mngl::detail::hit_test_item> m_hit_test_items;
		std::vector<font*> m_used_fonts;
		detail::tessellation_scratch m_geometry_scratch;
		std::shared_ptr<detail::svg::temporary_target_budget> m_svg_effect_target_budget;
		detail::svg::temporary_target_pool<gpu::texture> m_svg_effect_output_targets;
		detail::svg::temporary_target_pool<::mngl::texture> m_svg_effect_capture_targets;
		bool m_custom_materials_allowed = false;
		std::mutex m_frame_start_mutex;
		std::atomic<std::uint64_t> m_next_svg_effect_deferred_token{1};
		std::uint64_t m_next_frame_token = 0;
		std::uint64_t m_active_frame_token = 0;
		std::atomic_bool m_frame_active{false};
		std::atomic_bool m_command_submission_pending{false};
	};

	class render_context
	{
	public:
		struct frame_start
		{
			renderer* owner;
			frame_2d frame;
		};

		[[nodiscard]] static render_context& global()
		{
			static render_context context(gpu::device::global());
			return context;
		}

		render_context(const render_context&) = delete;
		render_context& operator=(const render_context&) = delete;

		[[nodiscard]] const gpu::device& gpu_device() const noexcept { return m_device; }

		[[nodiscard]] texture_store& textures() noexcept { return m_textures; }

		[[nodiscard]] renderer& renderer_for(gpu::texture_format target_format,
		                                     bool allow_custom_materials = false)
		{
			std::lock_guard lock(m_renderer_mutex);
			return renderer_for_locked(target_format, allow_custom_materials);
		}

		[[nodiscard]] frame_start begin_frame(::mngl::texture& target,
		                                      const color& clear_color = color(0.0f, 0.0f, 0.0f, 1.0f),
		                                      bool allow_custom_materials = false,
		                                      bool preserve_target_contents = false)
		{
			std::lock_guard lock(m_renderer_mutex);
			auto& target_renderer = renderer_for_locked(target.format(), allow_custom_materials);
			auto frame = target_renderer.begin_frame(target, clear_color, preserve_target_contents);
			return {&target_renderer, std::move(frame)};
		}

	private:
		[[nodiscard]] renderer& renderer_for_locked(gpu::texture_format target_format,
		                                            bool allow_custom_materials)
		{
			if (target_format == gpu::texture_format::invalid) [[unlikely]]
			{
				throw std::invalid_argument("A render context requires a valid target format");
			}

			auto& target_renderers = m_renderers[target_format];
			for (auto& target_renderer : target_renderers)
			{
				if (!target_renderer->available_for_frame())
				{
					continue;
				}
				if (allow_custom_materials)
				{
					target_renderer->enable_custom_materials();
				}
				return *target_renderer;
			}

			auto target_renderer = std::make_unique<renderer>(m_device, m_textures, target_format);
			if (allow_custom_materials)
			{
				target_renderer->enable_custom_materials();
			}
			auto& result = *target_renderer;
			target_renderers.push_back(std::move(target_renderer));
			return result;
		}

		explicit render_context(const gpu::device& device) : m_device(device), m_textures(device) {}

		gpu::device m_device;
		texture_store m_textures;
		std::mutex m_renderer_mutex;
		std::unordered_map<gpu::texture_format, std::vector<std::unique_ptr<renderer>>> m_renderers;
	};

} // namespace mngl::two_d
