#pragma once

#include "rectangle.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mngl
{
	struct hit_test_result;
	struct hit_test_path_entry;
	using hit_test_callback = std::function<void(const hit_test_result&, const hit_test_path_entry&)>;

	struct svg_hit_test_element
	{
		// SVG id attribute, or an empty string when the element has no id.
		std::string id;
		// Local SVG tag name, such as "path" or "rect".
		std::string name;
		// Zero-based order of drawable elements after expanding <use> references.
		std::size_t index = 0;
	};

	struct hit_test_path_entry
	{
		// Zero-based index among the recorded draw calls for this render target.
		std::size_t draw_index = 0;
		// Coordinates in the drawable's local space, with the origin at its bottom-left.
		glm::vec2 local_coords{};
		// Opaque texture-object identity; zero means this drawable has no texture source.
		std::uint64_t source_identity = 0;
		glm::uvec2 source_size{};
		// Coordinates in the sampled texture, with the origin at its bottom-left.
		glm::vec2 source_pixel_coords{};
		// Present for SVG geometry hits; local_coords then refers to this element's path space.
		std::optional<svg_hit_test_element> svg_element;
	};

	struct hit_test_result
	{
		// Ordered from the window drawable to the deepest drawable in a sampled target.
		std::vector<hit_test_path_entry> path;
		std::shared_ptr<const hit_test_callback> click_callback;
		std::size_t callback_path_index = 0;

		[[nodiscard]] bool invoke_click_callback() const
		{
			if (!click_callback || callback_path_index >= path.size())
			{
				return false;
			}
			(*click_callback)(*this, path[callback_path_index]);
			return true;
		}
	};

	namespace detail
	{
		inline void set_hit_test_callback(std::shared_ptr<const hit_test_callback>& destination,
		                                  hit_test_callback callback)
		{
			if (!callback)
			{
				destination.reset();
				return;
			}
			destination = std::make_shared<const hit_test_callback>(std::move(callback));
		}

		inline constexpr std::size_t hit_test_max_depth = 128;
		inline constexpr std::size_t hit_test_spatial_index_item_threshold = 128;
		inline constexpr std::size_t hit_test_spatial_index_leaf_size = 8;
		inline constexpr std::size_t hit_test_spatial_index_query_threshold_for_items(std::size_t item_count) noexcept
		{
			// Mixed hit/miss runs crossed over near 113/112 calls in Release and 200/247 in Debug.
		#if defined(_DEBUG) || !defined(NDEBUG)
			return item_count >= 4096 ? 256 : 224;
		#else
			return item_count >= 4096 ? 128 : 160;
		#endif
		}

		enum class hit_test_shape : std::uint8_t
		{
			quad,
			circle,
			ellipse,
			rounded_rectangle
		};

		struct svg_hit_test_geometry
		{
			std::shared_ptr<const std::vector<glm::vec2>> triangles;
			rectangle bounds{};
			glm::mat4 svg_transform{1.0f};
			svg_hit_test_element element;
		};

		struct hit_test_frame;

		struct hit_test_item
		{
			std::size_t draw_index = 0;
			rectangle destination{};
			glm::mat4 world_to_local{1.0f};
			bool inverse_valid = false;
			rectangle world_bounds{};
			bool world_bounds_valid = false;
			hit_test_shape shape = hit_test_shape::quad;
			float corner_radius = 0.0f;
			glm::vec4 uv_bounds{0.0f, 0.0f, 1.0f, 1.0f};
			std::uint64_t source_identity = 0;
			glm::uvec2 source_size{};
			std::optional<glm::ivec4> scissor;
			std::shared_ptr<const hit_test_frame> child_frame;
			std::shared_ptr<const hit_test_callback> click_callback;
			std::shared_ptr<const svg_hit_test_geometry> svg_geometry;
		};

		struct hit_test_frame
		{
			glm::uvec2 target_size{};
			glm::mat4 view_projection{1.0f};
			mutable std::once_flag inverse_once;
			mutable glm::mat4 inverse_view_projection{1.0f};
			mutable bool inverse_view_projection_valid = false;
			std::size_t depth = 1;
			std::vector<hit_test_item> items;
			mutable std::atomic_size_t spatial_index_query_count{0};
			mutable std::once_flag spatial_index_once;
			mutable bool spatial_index_usable = false;
			mutable std::size_t spatial_index_build_count = 0;
			mutable std::size_t spatial_index_root = std::numeric_limits<std::size_t>::max();
			mutable std::vector<std::size_t> spatial_index_items;
			mutable std::vector<std::size_t> spatial_index_unbounded_items;
			mutable std::vector<struct hit_test_spatial_node> spatial_index_nodes;
		};

		struct hit_test_spatial_bounds
		{
			glm::vec2 minimum{};
			glm::vec2 maximum{};
		};

		struct hit_test_spatial_node
		{
			hit_test_spatial_bounds bounds{};
			std::size_t begin = 0;
			std::size_t count = 0;
			std::size_t left = std::numeric_limits<std::size_t>::max();
			std::size_t right = std::numeric_limits<std::size_t>::max();
		};

		struct hit_test_frame_state
		{
			std::shared_ptr<const hit_test_frame> completed;
			// A frame recorded into an external command buffer is available only as a
			// nested source for subsequent work recorded before that command is submitted.
			std::shared_ptr<const hit_test_frame> latest_recorded;
		};

		inline bool finite(glm::vec2 value) noexcept
		{
			return std::isfinite(value.x) && std::isfinite(value.y);
		}

		inline bool finite(const glm::mat4& value) noexcept
		{
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					if (!std::isfinite(value[column][row]))
					{
						return false;
					}
				}
			}
			return true;
		}

		enum class hit_test_query_mode : std::uint8_t
		{
			adaptive,
			linear,
			indexed,
			scan_without_bounds
		};

		struct hit_test_candidate_list
		{
			static constexpr std::size_t inline_capacity = 16;
			std::array<std::size_t, inline_capacity> inline_items{};
			std::size_t inline_count = 0;
			bool overflow = false;

			bool push_back(std::size_t value)
			{
				if (inline_count >= inline_capacity)
				{
					overflow = true;
					return false;
				}
				inline_items[inline_count++] = value;
				return true;
			}

			void sort_descending()
			{
				std::sort(inline_items.begin(), inline_items.begin() + static_cast<std::ptrdiff_t>(inline_count),
				          std::greater<>{});
			}
		};

		inline bool spatial_bounds_for_item(const hit_test_item& item, hit_test_spatial_bounds& bounds) noexcept
		{
			if (!item.world_bounds_valid || !finite(item.world_bounds.pos) || !finite(item.world_bounds.size) ||
			    item.world_bounds.size.x < 0.0f || item.world_bounds.size.y < 0.0f)
			{
				return false;
			}

			bounds.minimum = item.world_bounds.pos;
			bounds.maximum = item.world_bounds.pos + item.world_bounds.size;
			return finite(bounds.maximum);
		}

		inline bool spatial_bounds_contain(const hit_test_spatial_bounds& bounds, glm::vec2 point) noexcept
		{
			return point.x >= bounds.minimum.x && point.y >= bounds.minimum.y &&
			       point.x <= bounds.maximum.x && point.y <= bounds.maximum.y;
		}

		inline void build_hit_test_spatial_index(const hit_test_frame& frame, bool force = false)
		{
			++frame.spatial_index_build_count;
			if (!force && frame.items.size() < hit_test_spatial_index_item_threshold)
			{
				return;
			}

			frame.spatial_index_usable = false;
			frame.spatial_index_root = std::numeric_limits<std::size_t>::max();
			frame.spatial_index_items.clear();
			frame.spatial_index_unbounded_items.clear();
			frame.spatial_index_nodes.clear();
			for (std::size_t index = 0; index < frame.items.size(); ++index)
			{
				hit_test_spatial_bounds bounds;
				if (spatial_bounds_for_item(frame.items[index], bounds))
				{
					frame.spatial_index_items.push_back(index);
				}
				else
				{
					// Items without a safe AABB must retain the old exact-query path.
					frame.spatial_index_unbounded_items.push_back(index);
				}
			}

			const auto minimum_indexed_items =
			    force ? std::size_t{1} : hit_test_spatial_index_item_threshold / 2;
			if (frame.spatial_index_items.size() < minimum_indexed_items)
			{
				return;
			}

			const auto invalid_index = std::numeric_limits<std::size_t>::max();
			const auto build_node = [&](auto&& self, std::size_t begin, std::size_t end) -> std::size_t
			{
				const std::size_t node_index = frame.spatial_index_nodes.size();
				frame.spatial_index_nodes.emplace_back();
				auto& node = frame.spatial_index_nodes[node_index];
				node.bounds.minimum = glm::vec2(std::numeric_limits<float>::max());
				node.bounds.maximum = glm::vec2(std::numeric_limits<float>::lowest());
				for (std::size_t cursor = begin; cursor < end; ++cursor)
				{
					hit_test_spatial_bounds item_bounds;
					(void)spatial_bounds_for_item(frame.items[frame.spatial_index_items[cursor]], item_bounds);
					node.bounds.minimum = glm::min(node.bounds.minimum, item_bounds.minimum);
					node.bounds.maximum = glm::max(node.bounds.maximum, item_bounds.maximum);
				}

				const auto item_count = end - begin;
				if (item_count <= hit_test_spatial_index_leaf_size)
				{
					node.begin = begin;
					node.count = item_count;
					return node_index;
				}

				const double extent_x = static_cast<double>(node.bounds.maximum.x) - node.bounds.minimum.x;
				const double extent_y = static_cast<double>(node.bounds.maximum.y) - node.bounds.minimum.y;
				const int axis = extent_x >= extent_y ? 0 : 1;
				const std::size_t middle = begin + item_count / 2;
				std::nth_element(frame.spatial_index_items.begin() + static_cast<std::ptrdiff_t>(begin),
				                 frame.spatial_index_items.begin() + static_cast<std::ptrdiff_t>(middle),
				                 frame.spatial_index_items.begin() + static_cast<std::ptrdiff_t>(end),
				                 [&](std::size_t left_index, std::size_t right_index)
				                 {
					                 hit_test_spatial_bounds left_bounds;
					                 hit_test_spatial_bounds right_bounds;
					                 (void)spatial_bounds_for_item(frame.items[left_index], left_bounds);
					                 (void)spatial_bounds_for_item(frame.items[right_index], right_bounds);
					                 const double left_center =
					                     (static_cast<double>(left_bounds.minimum[axis]) + left_bounds.maximum[axis]) * 0.5;
					                 const double right_center =
					                     (static_cast<double>(right_bounds.minimum[axis]) + right_bounds.maximum[axis]) * 0.5;
					                 return left_center < right_center;
				                 });

				const std::size_t left = self(self, begin, middle);
				const std::size_t right = self(self, middle, end);
				// Recursive calls may reallocate the node vector, so reacquire by index.
				frame.spatial_index_nodes[node_index].left = left;
				frame.spatial_index_nodes[node_index].right = right;
				frame.spatial_index_nodes[node_index].count = 0;
				frame.spatial_index_nodes[node_index].begin = invalid_index;
				return node_index;
			};

			frame.spatial_index_nodes.reserve(frame.spatial_index_items.size());
			frame.spatial_index_root = build_node(build_node, 0, frame.spatial_index_items.size());
			frame.spatial_index_usable = true;
		}

		inline bool collect_hit_test_candidates(const hit_test_frame& frame, std::size_t node_index,
		                                        glm::vec2 point, hit_test_candidate_list& candidates)
		{
			const auto& node = frame.spatial_index_nodes[node_index];
			if (!spatial_bounds_contain(node.bounds, point))
			{
				return true;
			}
			if (node.count != 0)
			{
				for (std::size_t index = node.begin; index < node.begin + node.count; ++index)
				{
					if (!candidates.push_back(frame.spatial_index_items[index]))
						return false;
				}
				return true;
			}

			return collect_hit_test_candidates(frame, node.left, point, candidates) &&
			       collect_hit_test_candidates(frame, node.right, point, candidates);
		}

		inline bool hit_test_spatial_index_ready(const hit_test_frame& frame, hit_test_query_mode mode)
		{
			if (mode == hit_test_query_mode::linear || mode == hit_test_query_mode::scan_without_bounds)
			{
				return false;
			}

			if (mode == hit_test_query_mode::adaptive)
			{
				if (frame.items.size() < hit_test_spatial_index_item_threshold)
				{
					return false;
				}
				const auto query_count = frame.spatial_index_query_count.fetch_add(1, std::memory_order_relaxed) + 1;
				if (query_count < hit_test_spatial_index_query_threshold_for_items(frame.items.size()))
				{
					return false;
				}
			}

			std::call_once(frame.spatial_index_once,
			               [&frame, mode]
			               { build_hit_test_spatial_index(frame, mode == hit_test_query_mode::indexed); });
			return frame.spatial_index_usable;
		}

		inline hit_test_candidate_list hit_test_candidates_at(const hit_test_frame& frame, glm::vec2 world_position)
		{
			hit_test_candidate_list candidates;
			bool complete = true;
			if (frame.spatial_index_root != std::numeric_limits<std::size_t>::max())
			{
				complete = collect_hit_test_candidates(frame, frame.spatial_index_root, world_position, candidates);
			}
			for (std::size_t index = 0; complete && index < frame.spatial_index_unbounded_items.size(); ++index)
			{
				complete = candidates.push_back(frame.spatial_index_unbounded_items[index]);
			}
			if (complete)
			{
				candidates.sort_descending();
			}
			else
			{
				candidates.overflow = true;
			}
			return candidates;
		}

		inline std::optional<rectangle> transformed_bounds(const glm::mat4& transform,
		                                                   const rectangle& local_bounds)
		{
			if (!finite(transform) || !finite(local_bounds.pos) || !finite(local_bounds.size) ||
			    local_bounds.size.x < 0.0f || local_bounds.size.y < 0.0f)
			{
				return std::nullopt;
			}

			const std::array<glm::vec2, 4> corners{
			    local_bounds.pos,
			    local_bounds.pos + glm::vec2(local_bounds.size.x, 0.0f),
			    local_bounds.pos + local_bounds.size,
			    local_bounds.pos + glm::vec2(0.0f, local_bounds.size.y)};
			glm::vec2 minimum(std::numeric_limits<float>::max());
			glm::vec2 maximum(std::numeric_limits<float>::lowest());
			float first_w = 0.0f;
			for (std::size_t index = 0; index < corners.size(); ++index)
			{
				const glm::vec4 point = transform * glm::vec4(corners[index], 0.0f, 1.0f);
				if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.w) ||
				    std::abs(point.w) <= std::numeric_limits<float>::epsilon())
				{
					return std::nullopt;
				}
				if (index == 0)
					first_w = point.w;
				else if (std::signbit(first_w) != std::signbit(point.w))
					return std::nullopt;

				const glm::vec2 cartesian = glm::vec2(point) / point.w;
				if (!finite(cartesian))
					return std::nullopt;
				minimum = glm::min(minimum, cartesian);
				maximum = glm::max(maximum, cartesian);
			}
			return rectangle{minimum, maximum - minimum};
		}

		inline std::optional<glm::vec2> map_pixel_to_world_unbounded(const hit_test_frame& frame,
		                                                             glm::vec2 pixel_position)
		{
			if (frame.target_size.x == 0 || frame.target_size.y == 0 || !finite(pixel_position))
			{
				return std::nullopt;
			}

			const glm::vec2 ndc = 2.0f * pixel_position / glm::vec2(frame.target_size) - glm::vec2(1.0f);
			std::call_once(frame.inverse_once,
			               [&frame]
			               {
				               if (!finite(frame.view_projection))
				               {
					               return;
				               }
				               const auto inverse = glm::inverse(frame.view_projection);
				               if (finite(inverse))
				               {
					               frame.inverse_view_projection = inverse;
					               frame.inverse_view_projection_valid = true;
				               }
			               });
			if (!frame.inverse_view_projection_valid)
			{
				return std::nullopt;
			}
			const glm::vec4 world_homogeneous =
			    frame.inverse_view_projection * glm::vec4(ndc, 0.0f, 1.0f);
			if (!std::isfinite(world_homogeneous.x) || !std::isfinite(world_homogeneous.y) ||
			    !std::isfinite(world_homogeneous.w) ||
			    std::abs(world_homogeneous.w) <= std::numeric_limits<float>::epsilon())
			{
				return std::nullopt;
			}

			const glm::vec2 world = glm::vec2(world_homogeneous) / world_homogeneous.w;
			return finite(world) ? std::optional<glm::vec2>(world) : std::nullopt;
		}

		inline std::optional<glm::vec2> map_pixel_to_world(const hit_test_frame& frame,
		                                                   glm::vec2 pixel_position)
		{
			if (frame.target_size.x == 0 || frame.target_size.y == 0 || !finite(pixel_position) ||
			    pixel_position.x < 0.0f || pixel_position.y < 0.0f ||
			    pixel_position.x > static_cast<float>(frame.target_size.x) ||
			    pixel_position.y > static_cast<float>(frame.target_size.y))
			{
				return std::nullopt;
			}
			return map_pixel_to_world_unbounded(frame, pixel_position);
		}

		inline std::optional<glm::vec2> local_position_unbounded(const hit_test_item& item,
		                                                         glm::vec2 world_position)
		{
			const auto& destination = item.destination;
			if (!finite(world_position) || !item.inverse_valid || !finite(destination.pos) ||
			    !finite(destination.size) || destination.size.x <= 0.0f || destination.size.y <= 0.0f)
			{
				return std::nullopt;
			}

			const glm::vec4 local_homogeneous = item.world_to_local * glm::vec4(world_position, 0.0f, 1.0f);
			if (!std::isfinite(local_homogeneous.x) || !std::isfinite(local_homogeneous.y) ||
			    !std::isfinite(local_homogeneous.w) ||
			    std::abs(local_homogeneous.w) <= std::numeric_limits<float>::epsilon())
			{
				return std::nullopt;
			}

			if (item.svg_geometry)
			{
				if (!item.svg_geometry->triangles)
				{
					return std::nullopt;
				}
				const glm::vec2 local{local_homogeneous.x / local_homogeneous.w,
				                      local_homogeneous.y / local_homogeneous.w};
				const auto& bounds = item.svg_geometry->bounds;
				if (!finite(local) || local.x < bounds.pos.x || local.y < bounds.pos.y ||
				    local.x > bounds.pos.x + bounds.size.x || local.y > bounds.pos.y + bounds.size.y)
				{
					return std::nullopt;
				}

				const auto& triangles = *item.svg_geometry->triangles;
				const auto cross = [](glm::vec2 first, glm::vec2 second, glm::vec2 point)
				{
					return (static_cast<double>(second.x) - first.x) *
					           (static_cast<double>(point.y) - first.y) -
					       (static_cast<double>(second.y) - first.y) *
					           (static_cast<double>(point.x) - first.x);
				};
				for (std::size_t index = 0; index + 2 < triangles.size(); index += 3)
				{
					const auto a = triangles[index];
					const auto b = triangles[index + 1];
					const auto c = triangles[index + 2];
					if (cross(a, b, c) == 0.0)
					{
						continue;
					}
					const double ab = cross(a, b, local);
					const double bc = cross(b, c, local);
					const double ca = cross(c, a, local);
					if ((ab >= 0.0 && bc >= 0.0 && ca >= 0.0) || (ab <= 0.0 && bc <= 0.0 && ca <= 0.0))
					{
						return local;
					}
				}
				return std::nullopt;
			}

			const glm::vec2 local_unit = glm::vec2(local_homogeneous) / local_homogeneous.w;
			const glm::vec2 result = local_unit * destination.size;
			if (!finite(result))
			{
				return std::nullopt;
			}

			return result;
		}

		inline std::optional<glm::vec2> local_position(const hit_test_item& item, glm::vec2 world_position)
		{
			const auto result = local_position_unbounded(item, world_position);
			if (item.svg_geometry)
			{
				return result;
			}
			const auto& destination = item.destination;
			if (!result || result->x < 0.0f || result->y < 0.0f || result->x > destination.size.x ||
			    result->y > destination.size.y)
			{
				return std::nullopt;
			}

			const glm::vec2 centered = *result / destination.size * 2.0f - glm::vec2(1.0f);
			switch (item.shape)
			{
			case hit_test_shape::circle:
			case hit_test_shape::ellipse:
				if (glm::dot(centered, centered) > 1.0f)
				{
					return std::nullopt;
				}
				break;
			case hit_test_shape::rounded_rectangle:
			{
				const glm::vec2 half_size = destination.size * 0.5f;
				const float radius = std::clamp(item.corner_radius, 0.0f, std::min(half_size.x, half_size.y));
				if (radius > 0.0f)
				{
					const glm::vec2 from_center = glm::abs(*result - half_size);
					const glm::vec2 box_distance = from_center - (half_size - glm::vec2(radius));
					const float distance = glm::length(glm::max(box_distance, glm::vec2(0.0f))) +
					                       std::min(std::max(box_distance.x, box_distance.y), 0.0f) - radius;
					if (distance > 0.0f)
					{
						return std::nullopt;
					}
				}
				break;
			}
			case hit_test_shape::quad:
				break;
			}

			return result;
		}

		inline glm::vec2 map_local_to_texture_pixel(const hit_test_item& item, glm::vec2 local_coords,
		                                            glm::uvec2 texture_size)
		{
			if (texture_size.x == 0 || texture_size.y == 0 || !finite(local_coords) ||
			    !finite(item.destination.size) || item.destination.size.x <= 0.0f ||
			    item.destination.size.y <= 0.0f)
			{
				return {};
			}

			const glm::vec2 local_uv = local_coords / item.destination.size;
			const float u = glm::mix(item.uv_bounds.x, item.uv_bounds.z, local_uv.x);
			const float v = glm::mix(item.uv_bounds.w, item.uv_bounds.y, local_uv.y);
			return {u * static_cast<float>(texture_size.x), (1.0f - v) * static_cast<float>(texture_size.y)};
		}

		inline hit_test_path_entry make_hit_test_path_entry(const hit_test_item& item, glm::vec2 local_coords)
		{
			const glm::vec2 source_pixel_coords =
			    item.source_identity != 0 ? map_local_to_texture_pixel(item, local_coords, item.source_size)
				                          : glm::vec2{};

			hit_test_path_entry result{item.draw_index, local_coords, item.source_identity, item.source_size,
			                           source_pixel_coords};
			if (item.svg_geometry)
			{
				result.svg_element = item.svg_geometry->element;
			}
			return result;
		}

		inline std::optional<hit_test_path_entry> map_hit_test_path_to_source_unbounded(
		    const hit_test_frame& root_frame, const std::vector<hit_test_path_entry>& path,
		    glm::vec2 root_pixel_position, std::uint64_t source_identity)
		{
			if (path.empty() || source_identity == 0)
			{
				return std::nullopt;
			}

			const hit_test_frame* current_frame = &root_frame;
			glm::vec2 current_pixel_position = root_pixel_position;
			for (const auto& path_entry : path)
			{
				if (path_entry.draw_index >= current_frame->items.size())
				{
					return std::nullopt;
				}

				const auto& item = current_frame->items[path_entry.draw_index];
				if (item.source_identity != path_entry.source_identity)
				{
					return std::nullopt;
				}
				const auto world_position =
				    map_pixel_to_world_unbounded(*current_frame, current_pixel_position);
				if (!world_position)
				{
					return std::nullopt;
				}
				const auto local = local_position_unbounded(item, *world_position);
				if (!local)
				{
					return std::nullopt;
				}
				if (item.source_identity == source_identity)
				{
					return make_hit_test_path_entry(item, *local);
				}
				if (!item.child_frame)
				{
					return std::nullopt;
				}

				current_pixel_position =
				    map_local_to_texture_pixel(item, *local, item.child_frame->target_size);
				current_frame = item.child_frame.get();
			}

			return std::nullopt;
		}

		inline std::optional<hit_test_result> hit_test_frame_at_pixel(
		    const hit_test_frame& frame, glm::vec2 pixel_position,
		    std::vector<const hit_test_frame*>& ancestors, hit_test_query_mode mode)
		{
			if (ancestors.size() >= hit_test_max_depth ||
			    std::find(ancestors.begin(), ancestors.end(), &frame) != ancestors.end())
			{
				return std::nullopt;
			}

			const auto world_position = map_pixel_to_world(frame, pixel_position);
			if (!world_position)
			{
				return std::nullopt;
			}

			ancestors.push_back(&frame);
			struct pop_ancestor
			{
				std::vector<const hit_test_frame*>& values;
				~pop_ancestor() { values.pop_back(); }
			} pop{ancestors};

			const auto consider_item = [&](std::size_t item_index) -> std::optional<hit_test_result>
			{
				const auto& item = frame.items[item_index];
				if (item.scissor)
				{
					const glm::vec2 scissor_position{
					    pixel_position.x, static_cast<float>(frame.target_size.y) - pixel_position.y};
					const glm::vec4 scissor_bounds(*item.scissor);
					if (scissor_position.x < scissor_bounds.x || scissor_position.y < scissor_bounds.y ||
					    scissor_position.x >= scissor_bounds.x + scissor_bounds.z ||
					    scissor_position.y >= scissor_bounds.y + scissor_bounds.w)
					{
						return std::nullopt;
					}
				}

				if (mode != hit_test_query_mode::scan_without_bounds && item.world_bounds_valid &&
				    (world_position->x < item.world_bounds.pos.x || world_position->y < item.world_bounds.pos.y ||
				     world_position->x > item.world_bounds.pos.x + item.world_bounds.size.x ||
				     world_position->y > item.world_bounds.pos.y + item.world_bounds.size.y))
				{
					return std::nullopt;
				}

				const auto local = local_position(item, *world_position);
				if (!local)
				{
					return std::nullopt;
				}

				if (item.child_frame)
				{
					const glm::vec2 child_pixel =
					    map_local_to_texture_pixel(item, *local, item.child_frame->target_size);
					if (auto child_result =
					        hit_test_frame_at_pixel(*item.child_frame, child_pixel, ancestors, mode))
					{
						child_result->path.insert(child_result->path.begin(),
						                          make_hit_test_path_entry(item, *local));
						if (child_result->click_callback)
						{
							++child_result->callback_path_index;
						}
						else if (item.click_callback)
						{
							child_result->click_callback = item.click_callback;
							child_result->callback_path_index = 0;
						}
						return child_result;
					}
				}

				return hit_test_result{.path = {make_hit_test_path_entry(item, *local)},
				                       .click_callback = item.click_callback,
				                       .callback_path_index = 0};
			};

			bool use_index = hit_test_spatial_index_ready(frame, mode);
			std::size_t reverse_scan_end = frame.items.size();
			if (use_index)
			{
				if (reverse_scan_end != 0)
				{
					const auto topmost_index = reverse_scan_end - 1;
					if (auto result = consider_item(topmost_index))
						return result;
					reverse_scan_end = topmost_index;
				}

				auto candidates = hit_test_candidates_at(frame, *world_position);
				if (candidates.overflow)
				{
					// Dense overlaps are cheaper to resolve from the top of the draw list.
					use_index = false;
				}
				else
				{
					for (std::size_t index = 0; index < candidates.inline_count; ++index)
					{
						if (candidates.inline_items[index] == frame.items.size() - 1)
							continue;
						if (auto result = consider_item(candidates.inline_items[index]))
							return result;
					}
				}
			}
			if (!use_index)
			{
				for (std::size_t index = reverse_scan_end; index > 0; --index)
				{
					if (auto result = consider_item(index - 1))
						return result;
				}
			}

			return std::nullopt;
		}

		inline std::optional<hit_test_result> hit_test_frame_at_pixel(const hit_test_frame& frame,
		                                                              glm::vec2 pixel_position,
		                                                              hit_test_query_mode mode)
		{
			std::vector<const hit_test_frame*> ancestors;
			ancestors.reserve(8);
			return hit_test_frame_at_pixel(frame, pixel_position, ancestors, mode);
		}

		inline std::optional<hit_test_result> hit_test_frame_at_pixel(const hit_test_frame& frame,
		                                                              glm::vec2 pixel_position)
		{
			return hit_test_frame_at_pixel(frame, pixel_position, hit_test_query_mode::adaptive);
		}
	} // namespace detail
} // namespace mngl
