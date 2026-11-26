#pragma once

#include "command_buffer.hpp"
#include "file_io.hpp"
#include "renderer2d.hpp"
#include "detail/two_d/svg_parser.hpp"
#include "hit_test.hpp"
#include "view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mngl::two_d
{
	class svg_sprite;

	struct svg_view_tessellation
	{
		::mngl::view view;
		glm::uvec2 target_size{};
		float curve_tolerance_pixels = 0.25f;
	};

	struct svg_load_options
	{
		// Maximum curve-to-polyline deviation in SVG units when view_tessellation is unset.
		float curve_tolerance = 0.25f;
		// Maximum generated curve segments for each <path> or curved primitive element.
		std::size_t max_curve_segments = 1'000'000;
		// Optional screen-space tolerance resolved once for this view and render target size.
		std::optional<svg_view_tessellation> view_tessellation;
		// Retains per-element triangles and IDs for frame hit testing and GPU sprites.
		bool hit_testable = false;
		// Allows relative raster images inside the directory passed to load_file().
		bool allow_external_images = false;
	};

	// CPU-parsed SVG geometry drawn through the regular 2D frame API.
	class svg_object : public ::mngl::drawable
	{
	public:
		svg_object() = default;
		explicit svg_object(std::string_view source, const svg_load_options& options = {})
		{
			load(source, options);
		}
		svg_object(const svg_object&) = delete;
		svg_object& operator=(const svg_object&) = delete;
		svg_object(svg_object&&) noexcept = default;
		svg_object& operator=(svg_object&&) noexcept = default;
		~svg_object() override = default;

		[[nodiscard]] static svg_object from_file(const std::filesystem::path& path,
		                                          const svg_load_options& options = {})
		{
			svg_object result;
			result.load_file(path, options);
			return result;
		}

		svg_object& load(std::string_view source, const svg_load_options& options = {})
		{
			return load_document(source, options, {});
		}

		// Rebuilds curve geometry for a view and target size; call again when either changes.
		svg_object& sync_view(const ::mngl::view& view, glm::uvec2 target_size,
		                      float curve_tolerance_pixels = 0.25f)
		{
			if (!m_loaded) [[unlikely]]
			{
				throw std::logic_error("Cannot synchronize an SVG view before loading a document");
			}

			auto options = m_load_options;
			options.view_tessellation = svg_view_tessellation{view, target_size, curve_tolerance_pixels};
			return load_document(m_source, options, m_image_directory, m_decoded_images,
			                     m_viewport_size);
		}

		void on_click(::mngl::hit_test_callback callback)
		{
			::mngl::detail::set_hit_test_callback(m_click_callback, std::move(callback));
		}

		void clear_on_click() noexcept { m_click_callback.reset(); }

		svg_object& load_file(const std::filesystem::path& path, const svg_load_options& options = {})
		{
			const auto bytes = ::mngl::read_binary_file(path);
			const std::string source(bytes.begin(), bytes.end());
			const auto directory =
			    options.allow_external_images
			        ? std::filesystem::canonical(std::filesystem::absolute(path).parent_path())
			        : std::filesystem::path{};
			return load_document(source, options, directory);
		}

		// Uploads the already-prepared triangles once; the command buffer must be submitted by the caller.
		svg_object& upload(const gpu::device& device, gpu::command_buffer& command_buffer)
		{
			if (!m_loaded) [[unlikely]]
			{
				throw std::logic_error("Cannot upload an SVG object before loading a document");
			}

			std::vector<gpu_geometry_part> uploaded;
			std::shared_ptr<detail::svg::uploaded_assets> assets;
			uploaded.reserve(m_geometry.size());
			std::unordered_map<const std::vector<glm::vec2>*, std::size_t> triangle_instances;
			for (const auto& part : m_geometry)
				++triangle_instances[part.triangles.get()];
			std::unordered_map<const std::vector<glm::vec2>*,
			                   std::shared_ptr<detail::resident_mesh_storage>> shared_meshes;
			constexpr std::size_t max_mesh_vertices =
			    std::numeric_limits<std::uint32_t>::max() / sizeof(detail::resident_mesh_vertex);
			std::size_t first_part = 0;
			while (first_part < m_geometry.size())
			{
				const auto& first = m_geometry[first_part];
				const bool reused_triangles = triangle_instances[first.triangles.get()] > 1;
				if (first.svg_gradient || first.image || first.effect || reused_triangles)
				{
					mesh resident;
					if (reused_triangles)
					{
						const auto cached = shared_meshes.find(first.triangles.get());
						if (cached != shared_meshes.end())
							resident.m_storage = cached->second;
						else
						{
							resident = mesh(device, command_buffer, std::span<const glm::vec2>(*first.triangles));
							shared_meshes.emplace(first.triangles.get(), resident.m_storage);
						}
					}
					else if (first.svg_gradient)
						resident = mesh(device, command_buffer, std::span<const glm::vec2>(*first.triangles));
					if (first.image || first.effect)
					{
						if (!assets)
							assets = std::make_shared<detail::svg::uploaded_assets>();
						if (first.image)
							upload_image(*first.image, *assets, device, command_buffer);
						else
							upload_effect(*first.effect, *assets, device, command_buffer);
					}
					resident.transform = m_transform * ::mngl::two_d::transform(first.svg_transform);
					resident.tint = first.svg_tint * m_tint;
					resident.blend = m_blend;
					uploaded.push_back({std::move(resident), first.svg_tint, first.svg_transform,
					                    first.svg_gradient, first.image, first.effect});
					++first_part;
					continue;
				}

				std::size_t vertex_count = 0;
				std::size_t end_part = first_part;
				while (end_part < m_geometry.size() && !m_geometry[end_part].svg_gradient &&
				       !m_geometry[end_part].image && !m_geometry[end_part].effect &&
				       triangle_instances[m_geometry[end_part].triangles.get()] <= 1)
				{
					const auto part_vertex_count = m_geometry[end_part].triangles->size();
					if (part_vertex_count > max_mesh_vertices - vertex_count)
					{
						break;
					}
					vertex_count += part_vertex_count;
					++end_part;
				}

				if (end_part == first_part)
				{
					throw std::length_error("An SVG paint group exceeds SDL's 32-bit GPU buffer limit");
				}

				std::vector<detail::resident_mesh_vertex> vertices;
				vertices.reserve(vertex_count);
				for (std::size_t part_index = first_part; part_index < end_part; ++part_index)
				{
					const auto& part = m_geometry[part_index];
					const auto rgba = part.svg_tint.rgba_glsl();
					const std::array<float, 4> tint{rgba.r, rgba.g, rgba.b, rgba.a};
					for (const auto vertex : *part.triangles)
					{
						const auto transformed = detail::svg::apply_transform(part.svg_transform, vertex);
						vertices.push_back({transformed.x, transformed.y, tint});
					}
				}

				mesh combined(device, command_buffer,
				              std::span<const detail::resident_mesh_vertex>(vertices));
				combined.transform = m_transform;
				combined.tint = m_tint;
				combined.blend = m_blend;
				uploaded.push_back({std::move(combined), ::mngl::color(1.0f), glm::mat4(1.0f), std::nullopt});
				first_part = end_part;
			}
			if (!m_gpu_state)
				m_gpu_state = std::make_shared<gpu_state>();
			auto state = m_gpu_state;
			state->transform = m_transform;
			state->tint = m_tint;
			state->blend = m_blend;
			auto staged = std::make_shared<gpu_staged_upload>();
			staged->geometry = std::move(uploaded);
			staged->assets = std::move(assets);
			std::weak_ptr<gpu_state> weak_state = state;
			if (state->generation == std::numeric_limits<std::size_t>::max()) [[unlikely]]
			{
				throw std::length_error("SVG GPU upload generation limit was exceeded");
			}
			const auto generation = state->generation + 1;
			const auto epoch = state->epoch;
			command_buffer.on_completion(
			    [weak_state, generation, epoch, staged](bool submitted)
			    {
				    const auto state = weak_state.lock();
				    if (!state || state->epoch != epoch)
					    return;
				    if (state->pending_uploads != 0)
					    --state->pending_uploads;
				    if (submitted && generation > state->committed_generation)
				    {
					    for (auto& part : staged->geometry)
					    {
						    part.gpu_geometry.transform =
						        state->transform * ::mngl::two_d::transform(part.svg_transform);
						    part.gpu_geometry.tint = part.svg_tint * state->tint;
						    part.gpu_geometry.blend = state->blend;
					    }
					    state->geometry = std::move(staged->geometry);
					    state->assets = std::move(staged->assets);
					    state->committed_generation = generation;
					    state->resident = true;
				    }
			    });
			state->generation = generation;
			++state->pending_uploads;
			return *this;
		}

		// Uploads and submits through a temporary command buffer for the common synchronous path.
		svg_object& upload(const gpu::device& device)
		{
			gpu::command_buffer command_buffer(device);
			upload(device, command_buffer);
			command_buffer.submit();
			return *this;
		}

		void release_gpu() noexcept
		{
			if (m_gpu_state)
			{
				++m_gpu_state->epoch;
				m_gpu_state->pending_uploads = 0;
				m_gpu_state->geometry.clear();
				m_gpu_state->assets.reset();
				m_gpu_state->resident = false;
			}
		}

		[[nodiscard]] bool loaded() const noexcept { return m_loaded; }
		[[nodiscard]] bool empty() const noexcept { return m_geometry.empty(); }
		[[nodiscard]] bool gpu_resident() const noexcept { return m_gpu_state && m_gpu_state->resident; }
		[[nodiscard]] std::size_t geometry_count() const noexcept { return m_geometry.size(); }
		[[nodiscard]] glm::vec2 size() const noexcept { return m_viewport_size; }
		[[nodiscard]] glm::vec4 view_box() const noexcept { return m_view_box; }

		[[nodiscard]] const ::mngl::two_d::transform& transform() const noexcept { return m_transform; }
		svg_object& transform(const ::mngl::two_d::transform& value)
		{
			m_transform = value;
			apply_transforms();
			return *this;
		}

		[[nodiscard]] const ::mngl::color& tint() const noexcept { return m_tint; }
		svg_object& tint(::mngl::color value)
		{
			m_tint = value;
			apply_render_state();
			return *this;
		}

		[[nodiscard]] gpu::sprite_blend_mode blend() const noexcept { return m_blend; }
		svg_object& blend(gpu::sprite_blend_mode value)
		{
			m_blend = value;
			if (m_gpu_state)
				m_gpu_state->blend = value;
			if (gpu_resident())
			{
				for (auto& part : m_gpu_state->geometry)
				{
					part.gpu_geometry.blend = value;
				}
			}
			return *this;
		}

	private:
		friend class svg_sprite;

		struct geometry_part
		{
			std::shared_ptr<const std::vector<glm::vec2>> triangles;
			glm::mat4 svg_transform{1.0f};
			::mngl::color svg_tint{1.0f};
			std::optional<detail::svg::gradient_paint> svg_gradient;
			std::optional<detail::svg::image_paint> image;
			std::shared_ptr<const detail::svg::effect_layer> effect;
		};

		struct gpu_geometry_part
		{
			mesh gpu_geometry;
			::mngl::color svg_tint{1.0f};
			glm::mat4 svg_transform{1.0f};
			std::optional<detail::svg::gradient_paint> svg_gradient;
			std::optional<detail::svg::image_paint> image;
			std::shared_ptr<const detail::svg::effect_layer> effect;
		};

		struct gpu_state
		{
			std::vector<gpu_geometry_part> geometry;
			std::shared_ptr<const detail::svg::uploaded_assets> assets;
			::mngl::two_d::transform transform{};
			::mngl::color tint{1.0f};
			gpu::sprite_blend_mode blend = gpu::sprite_blend_mode::alpha;
			std::size_t generation = 0;
			bool resident = false;
			std::size_t committed_generation = 0;
			std::size_t pending_uploads = 0;
			std::size_t epoch = 0;
		};

		using hit_region = std::shared_ptr<const ::mngl::detail::svg_hit_test_geometry>;

		struct hit_clip_context
		{
			glm::mat4 inverse_transform{1.0f};
			std::shared_ptr<const std::vector<glm::vec2>> triangles;
		};

		static std::vector<glm::vec2> clip_hit_triangles(std::span<const glm::vec2> subject,
		                                                const glm::mat4& subject_to_clip,
		                                                const glm::mat4& clip_to_subject,
		                                                std::span<const glm::vec2> clip)
		{
			struct clip_triangle
			{
				struct edge
				{
					glm::vec2 start;
					glm::vec2 delta;
					float orientation = 1.0f;
				};

				std::array<glm::vec2, 3> vertices{};
				std::array<edge, 3> edges{};
				glm::vec2 minimum{};
				glm::vec2 maximum{};
				double aabb_padding = 0.0;
				bool bounds_valid = false;
			};

			std::vector<clip_triangle> clip_data;
			clip_data.reserve(clip.size() / 3);
			for (std::size_t i = 0; i + 2 < clip.size(); i += 3)
			{
				clip_triangle triangle;
				std::copy_n(clip.begin() + i, 3, triangle.vertices.begin());
				triangle.minimum = glm::min(triangle.vertices[0],
				                            glm::min(triangle.vertices[1], triangle.vertices[2]));
				triangle.maximum = glm::max(triangle.vertices[0],
				                            glm::max(triangle.vertices[1], triangle.vertices[2]));
				triangle.bounds_valid = ::mngl::detail::finite(triangle.minimum) &&
				                        ::mngl::detail::finite(triangle.maximum);
				const float area = (triangle.vertices[1].x - triangle.vertices[0].x) *
				                       (triangle.vertices[2].y - triangle.vertices[0].y) -
				                   (triangle.vertices[1].y - triangle.vertices[0].y) *
				                       (triangle.vertices[2].x - triangle.vertices[0].x);
				float minimum_edge_length = std::numeric_limits<float>::infinity();
				for (std::size_t edge = 0; edge < 3; ++edge)
				{
					const auto start = triangle.vertices[edge];
					const auto delta = triangle.vertices[(edge + 1) % 3] - start;
					triangle.edges[edge] = {start, delta, area >= 0.0f ? 1.0f : -1.0f};
					minimum_edge_length = std::min(minimum_edge_length, glm::length(delta));
				}
				// The half-plane test accepts a cross-product tolerance of 1e-6. Convert
				// that to a conservative distance for the broad-phase bounds.
				if (minimum_edge_length > 0.0f && std::isfinite(minimum_edge_length))
					triangle.aabb_padding = 1.0e-6 / minimum_edge_length;
				else
					triangle.bounds_valid = false;
				clip_data.push_back(triangle);
			}

			std::vector<glm::vec2> result;
			result.reserve(subject.size());
			constexpr float side_tolerance = 1.0e-6f;
			constexpr std::size_t scratch_capacity = 16;
			std::array<glm::vec2, scratch_capacity> scratch_a{};
			std::array<glm::vec2, scratch_capacity> scratch_b{};
			std::vector<glm::vec2> fallback_a;
			std::vector<glm::vec2> fallback_b;

			const auto emit_polygon = [&](std::span<const glm::vec2> polygon)
			{
				for (std::size_t p = 1; p + 1 < polygon.size(); ++p)
				{
					result.push_back(detail::svg::apply_transform(clip_to_subject, polygon[0]));
					result.push_back(detail::svg::apply_transform(clip_to_subject, polygon[p]));
					result.push_back(detail::svg::apply_transform(clip_to_subject, polygon[p + 1]));
				}
			};
			const auto emit_fallback = [&](const std::array<glm::vec2, 3>& initial,
			                               const clip_triangle& clip_triangle)
			{
				if (fallback_a.capacity() < scratch_capacity)
					fallback_a.reserve(scratch_capacity);
				if (fallback_b.capacity() < scratch_capacity)
					fallback_b.reserve(scratch_capacity);
				fallback_a.assign(initial.begin(), initial.end());
				fallback_b.clear();
				for (const auto& edge : clip_triangle.edges)
				{
					fallback_b.clear();
					for (std::size_t p = 0; p < fallback_a.size(); ++p)
					{
						const auto from = fallback_a[p];
						const auto to = fallback_a[(p + 1) % fallback_a.size()];
						const auto side = [&](glm::vec2 point)
						{
							const auto relative = point - edge.start;
							return edge.orientation *
							       (edge.delta.x * relative.y - edge.delta.y * relative.x);
						};
						const float sf = side(from), st = side(to);
						const bool from_inside = sf >= -side_tolerance;
						const bool to_inside = st >= -side_tolerance;
						if (from_inside != to_inside)
							fallback_b.push_back(from + (to - from) * (sf / (sf - st)));
						if (to_inside)
							fallback_b.push_back(to);
					}
					fallback_a.swap(fallback_b);
					fallback_b.clear();
					if (fallback_a.empty())
						break;
				}
				emit_polygon(fallback_a);
			};

			for (std::size_t i = 0; i + 2 < subject.size(); i += 3)
			{
				std::array<glm::vec2, 3> original{};
				glm::vec2 minimum(std::numeric_limits<float>::infinity());
				glm::vec2 maximum(-std::numeric_limits<float>::infinity());
				bool bounds_valid = true;
				for (std::size_t j = 0; j < 3; ++j)
				{
					original[j] = detail::svg::apply_transform(subject_to_clip, subject[i + j]);
					minimum = glm::min(minimum, original[j]);
					maximum = glm::max(maximum, original[j]);
					bounds_valid = bounds_valid && ::mngl::detail::finite(original[j]);
				}
				for (const auto& clip_triangle : clip_data)
				{
					if (bounds_valid && clip_triangle.bounds_valid &&
					    (static_cast<double>(maximum.x) + clip_triangle.aabb_padding < clip_triangle.minimum.x ||
					     static_cast<double>(clip_triangle.maximum.x) + clip_triangle.aabb_padding < minimum.x ||
					     static_cast<double>(maximum.y) + clip_triangle.aabb_padding < clip_triangle.minimum.y ||
					     static_cast<double>(clip_triangle.maximum.y) + clip_triangle.aabb_padding < minimum.y))
						continue;

					auto* polygon = &scratch_a;
					auto* clipped = &scratch_b;
					std::copy(original.begin(), original.end(), polygon->begin());
					std::size_t polygon_size = original.size();
					bool overflow = false;
					for (const auto& edge : clip_triangle.edges)
					{
						std::size_t clipped_size = 0;
						for (std::size_t p = 0; p < polygon_size; ++p)
						{
							const auto from = (*polygon)[p];
							const auto to = (*polygon)[(p + 1) % polygon_size];
							const auto side = [&](glm::vec2 point)
							{
								const auto relative = point - edge.start;
								return edge.orientation *
								       (edge.delta.x * relative.y - edge.delta.y * relative.x);
							};
							const float sf = side(from), st = side(to);
							const bool from_inside = sf >= -side_tolerance;
							const bool to_inside = st >= -side_tolerance;
							const auto append = [&](glm::vec2 point)
							{
								if (clipped_size == clipped->size())
								{
									overflow = true;
									return;
								}
								(*clipped)[clipped_size++] = point;
							};
							if (from_inside != to_inside)
								append(from + (to - from) * (sf / (sf - st)));
							if (to_inside)
								append(to);
							if (overflow)
								break;
						}
						if (overflow)
							break;
						if (clipped_size == 0)
						{
							polygon_size = 0;
							break;
						}
						std::swap(polygon, clipped);
						polygon_size = clipped_size;
					}
					if (overflow)
					{
						emit_fallback(original, clip_triangle);
						continue;
					}
					if (polygon_size >= 3)
						emit_polygon(std::span<const glm::vec2>(polygon->data(), polygon_size));
				}
			}
			return result;
		}

		static bool has_hit_clip_path(const detail::svg::effect_layer& layer) noexcept
		{
			return layer.hit_clip_empty || !layer.hit_clip_triangles.empty();
		}

		static std::vector<glm::vec2> collect_clip_layer_geometry(const detail::svg::effect_layer& layer)
		{
			std::vector<glm::vec2> triangles;
			const auto append = [&](const auto& self, const std::vector<detail::svg::geometry_data>& parts,
			                        const glm::mat4& transform, std::vector<hit_clip_context> clip_contexts,
			                        std::vector<glm::vec2>& output) -> void
			{
				for (const auto& part : parts)
				{
					const auto placement = transform * part.transform;
					if (part.effect)
					{
						auto nested_contexts = clip_contexts;
						if (part.effect->hit_clip)
						{
							nested_contexts.push_back(
							    {glm::inverse(placement), std::make_shared<const std::vector<glm::vec2>>(
							                                 detail::svg::quad_triangles(*part.effect->hit_clip))});
						}
						if (has_hit_clip_path(*part.effect))
						{
							std::vector<glm::vec2> nested_clip;
							self(self, part.effect->mask_source, glm::mat4(1.0f), {}, nested_clip);
							if (nested_clip.empty())
								continue;
							nested_contexts.push_back(
							    {glm::inverse(placement),
							     std::make_shared<const std::vector<glm::vec2>>(std::move(nested_clip))});
						}
						self(self, part.effect->source, placement, std::move(nested_contexts), output);
						continue;
					}

					auto subject = part.shared_triangles
					                   ? part.shared_triangles
					                   : std::make_shared<const std::vector<glm::vec2>>(part.triangles);
					for (const auto& context : clip_contexts)
					{
						const auto to_clip = context.inverse_transform * placement;
						const auto to_subject = glm::inverse(to_clip);
						if (!::mngl::detail::finite(to_clip) || !::mngl::detail::finite(to_subject))
						{
							subject = std::make_shared<const std::vector<glm::vec2>>();
							break;
						}
						const auto clipped = clip_hit_triangles(*subject, to_clip, to_subject, *context.triangles);
						subject = std::make_shared<const std::vector<glm::vec2>>(clipped);
					}
					for (const auto point : *subject)
						output.push_back(detail::svg::apply_transform(placement, point));
				}
			};
			append(append, layer.mask_source, glm::mat4(1.0f), {}, triangles);
			return triangles;
		}

		struct gpu_staged_upload
		{
			std::vector<gpu_geometry_part> geometry;
			std::shared_ptr<detail::svg::uploaded_assets> assets;
		};

		static void add_hit_region(const std::shared_ptr<const std::vector<glm::vec2>>& triangles,
		                           const glm::mat4& transform, const ::mngl::svg_hit_test_element& element,
		                           std::vector<hit_region>& output)
		{
			detail::svg::bounds_accumulator bounds;
			for (const auto point : *triangles)
				bounds.include(point);
			const auto rectangle = bounds.bounds();
			if (detail::svg::has_area(rectangle))
			{
				output.push_back(std::make_shared<::mngl::detail::svg_hit_test_geometry>(
				    ::mngl::detail::svg_hit_test_geometry{triangles, rectangle, transform, element}));
			}
		}

		static void collect_layer_hits(const std::shared_ptr<const detail::svg::effect_layer>& layer,
		                               const glm::mat4& transform, std::vector<hit_region>& output,
		                               std::vector<hit_clip_context> clip_contexts = {})
		{
			if (layer->hit_clip)
			{
				clip_contexts.push_back(
				    {glm::inverse(transform),
				     std::make_shared<const std::vector<glm::vec2>>(detail::svg::quad_triangles(*layer->hit_clip))});
			}
			if (has_hit_clip_path(*layer))
			{
				auto clip = collect_clip_layer_geometry(*layer);
				if (clip.empty())
					return;
				clip_contexts.push_back(
				    {glm::inverse(transform), std::make_shared<const std::vector<glm::vec2>>(std::move(clip))});
			}
			for (const auto& part : layer->source)
			{
				if (!part.pointer_events)
					continue;
				const auto placement = transform * part.transform;
				if (part.effect)
					collect_layer_hits(part.effect, placement, output, clip_contexts);
				else
				{
					auto triangles = part.shared_triangles
					                     ? part.shared_triangles
					                     : std::make_shared<const std::vector<glm::vec2>>(part.triangles);
					for (const auto& context : clip_contexts)
					{
						const auto to_clip = context.inverse_transform * placement;
						const auto to_subject = glm::inverse(to_clip);
						if (!::mngl::detail::finite(to_clip) || !::mngl::detail::finite(to_subject))
						{
							triangles = std::make_shared<const std::vector<glm::vec2>>();
							break;
						}
						const auto clipped = clip_hit_triangles(*triangles, to_clip, to_subject, *context.triangles);
						triangles = std::make_shared<const std::vector<glm::vec2>>(clipped);
					}
					add_hit_region(triangles, placement, part.svg_element, output);
				}
			}
		}

		static void upload_image(const detail::svg::image_paint& image, detail::svg::uploaded_assets& assets,
		                         const gpu::device& device, gpu::command_buffer& command)
		{
			const auto& pixels = *image.pixels;
			if (!assets.images.contains(&pixels))
			{
				auto texture = std::make_shared<::mngl::texture>(device, pixels.size.x, pixels.size.y,
				                                                 gpu::texture_format::r8g8b8a8_unorm,
				                                                 gpu::texture::usage_flags::sampler);
				texture->update_pixels(command, std::span<const std::uint8_t>(pixels.rgba));
				assets.images.emplace(&pixels, std::move(texture));
			}
		}

		static void upload_effect(const detail::svg::effect_layer& effect,
		                          detail::svg::uploaded_assets& assets, const gpu::device& device,
		                          gpu::command_buffer& command)
		{
			const auto upload_parts = [&](const std::vector<detail::svg::geometry_data>& parts)
			{
				for (const auto& part : parts)
				{
					if (part.effect)
						upload_effect(*part.effect, assets, device, command);
					else if (part.image)
						upload_image(*part.image, assets, device, command);
					else if (!assets.meshes.contains(&part) && part.shared_triangles)
					{
						const auto* key = part.shared_triangles.get();
						const auto cached = assets.shared_meshes.find(key);
						if (cached != assets.shared_meshes.end())
							assets.meshes.emplace(&part, cached->second);
						else
						{
							mesh resident(device, command, part.triangle_view());
							auto storage = std::move(resident.m_storage);
							assets.shared_meshes.emplace(key, storage);
							assets.meshes.emplace(&part, std::move(storage));
						}
					}
					else if (!assets.meshes.contains(&part))
					{
						mesh resident(device, command, part.triangle_view());
						assets.meshes.emplace(&part, std::move(resident.m_storage));
					}
				}
			};
			upload_parts(effect.source);
			upload_parts(effect.mask_source);
		}

		svg_object& load_document(std::string_view source, const svg_load_options& options,
		                          std::filesystem::path image_directory,
		                          detail::svg::decoded_image_cache decoded_images = {},
		                          std::optional<glm::vec2> layout_viewport_size = std::nullopt)
		{
			std::string source_copy(source);
			detail::svg::document_data document;
			if (options.view_tessellation)
			{
				const auto& view_settings = *options.view_tessellation;
				const auto visible_area = view_settings.view.visible_area(view_settings.target_size);
				detail::svg::view_tessellation_context view_context;
				view_context.curve_tolerance_pixels = view_settings.curve_tolerance_pixels;
				view_context.viewport_size = layout_viewport_size.value_or(glm::vec2(view_settings.target_size));
				view_context.pixels_per_world_unit =
				    std::max(static_cast<double>(view_settings.target_size.x) / visible_area.size.x,
					         static_cast<double>(view_settings.target_size.y) / visible_area.size.y);
				view_context.object_transform = m_transform.matrix();
				document = detail::svg::parse(source_copy, options.curve_tolerance,
				                              options.max_curve_segments, &view_context, image_directory,
				                              &decoded_images);
			}
			else
			{
				document = detail::svg::parse(source_copy, options.curve_tolerance,
				                              options.max_curve_segments, nullptr, image_directory,
				                              &decoded_images);
			}
			std::vector<geometry_part> geometry;
			std::vector<hit_region> hit_regions;
			geometry.reserve(document.geometry.size());
			for (auto& shape : document.geometry)
			{
				auto triangles = shape.shared_triangles
				                     ? std::move(shape.shared_triangles)
				                     : std::make_shared<const std::vector<glm::vec2>>(std::move(shape.triangles));
				if (options.hit_testable)
				{
					if (shape.effect)
						collect_layer_hits(shape.effect, shape.transform, hit_regions);
					else if (shape.pointer_events)
						add_hit_region(triangles, shape.transform, shape.svg_element, hit_regions);
				}
				geometry.push_back({std::move(triangles), shape.transform, shape.tint,
				                    std::move(shape.gradient), std::move(shape.image),
				                    std::move(shape.effect)});
			}

			release_gpu();
			if (!m_gpu_state)
				m_gpu_state = std::make_shared<gpu_state>();
			m_view_box = document.view_box;
			m_viewport_size = document.viewport_size;
			m_geometry = std::move(geometry);
			m_hit_regions = std::move(hit_regions);
			m_source = std::move(source_copy);
			m_image_directory = std::move(image_directory);
			m_decoded_images = std::move(decoded_images);
			m_load_options = options;
			m_loaded = true;
			return *this;
		}

		void apply_transforms() noexcept
		{
			if (m_gpu_state)
				m_gpu_state->transform = m_transform;
			if (gpu_resident())
			{
				for (auto& part : m_gpu_state->geometry)
				{
					part.gpu_geometry.transform = m_transform * ::mngl::two_d::transform(part.svg_transform);
				}
			}
		}

		void apply_render_state()
		{
			if (m_gpu_state)
			{
				m_gpu_state->transform = m_transform;
				m_gpu_state->tint = m_tint;
				m_gpu_state->blend = m_blend;
			}
			if (gpu_resident())
			{
				for (auto& part : m_gpu_state->geometry)
				{
					part.gpu_geometry.transform = m_transform * ::mngl::two_d::transform(part.svg_transform);
					part.gpu_geometry.tint = part.svg_tint * m_tint;
					part.gpu_geometry.blend = m_blend;
				}
			}
		}

		void draw(frame_2d& frame) const override
		{
			for (const auto& region : m_hit_regions)
			{
				frame.record_svg_hit_geometry(
				    region, ::mngl::two_d::transform(m_transform.matrix() * region->svg_transform),
				    m_click_callback);
			}

			if (gpu_resident())
			{
				for (const auto& part : m_gpu_state->geometry)
				{
					if (part.effect)
					{
						frame.submit_svg_effect(part.effect, part.gpu_geometry.transform,
						                        part.gpu_geometry.tint, m_blend, m_gpu_state->assets);
					}
					else if (part.image)
					{
						frame.submit_svg_image(*part.image, part.gpu_geometry.transform,
						                       part.gpu_geometry.tint, m_blend, m_gpu_state->assets);
					}
					else if (part.svg_gradient)
					{
						frame.submit_svg_gradient_mesh(part.gpu_geometry, *part.svg_gradient);
					}
					else
					{
						frame.draw(part.gpu_geometry);
					}
				}
				return;
			}

			for (const auto& part : m_geometry)
			{
				const auto transform = m_transform * ::mngl::two_d::transform(part.svg_transform);
				if (part.effect)
				{
					frame.submit_svg_effect(part.effect, transform, part.svg_tint * m_tint, m_blend);
				}
				else if (part.image)
				{
					frame.submit_svg_image(*part.image, transform, part.svg_tint * m_tint, m_blend);
				}
				else if (part.svg_gradient)
				{
					frame.submit_svg_gradient(std::span<const glm::vec2>(*part.triangles), transform,
					                          part.svg_tint * m_tint, m_blend, *part.svg_gradient);
				}
				else
				{
					frame.submit_svg_geometry(std::span<const glm::vec2>(*part.triangles), transform,
					                          part.svg_tint * m_tint, m_blend);
				}
			}
		}

		void prepare(frame_2d& frame) const override
		{
			if (gpu_resident())
			{
				for (const auto& part : m_gpu_state->geometry)
				{
					if (part.svg_gradient)
					{
						frame.prepare_svg_gradient(m_blend, *part.svg_gradient);
					}
				}
				return;
			}
			for (const auto& part : m_geometry)
			{
				if (part.image)
				{
					frame.prepare_svg_image(part.image->pixels);
				}
				else if (part.svg_gradient)
				{
					frame.prepare_svg_gradient(m_blend, *part.svg_gradient);
				}
			}
		}

		std::vector<geometry_part> m_geometry;
		std::shared_ptr<gpu_state> m_gpu_state = std::make_shared<gpu_state>();
		std::vector<hit_region> m_hit_regions;
		std::string m_source;
		std::filesystem::path m_image_directory;
		detail::svg::decoded_image_cache m_decoded_images;
		svg_load_options m_load_options;
		std::shared_ptr<const ::mngl::hit_test_callback> m_click_callback;
		glm::vec4 m_view_box{};
		glm::vec2 m_viewport_size{};
		::mngl::two_d::transform m_transform{};
		::mngl::color m_tint{1.0f};
		gpu::sprite_blend_mode m_blend = gpu::sprite_blend_mode::alpha;
		bool m_loaded = false;
	};
} // namespace mngl::two_d
