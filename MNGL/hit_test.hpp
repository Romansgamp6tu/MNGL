#pragma once

#include "rectangle.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace mngl
{
	struct hit_test_result;
	struct hit_test_path_entry;
	using hit_test_callback = std::function<void(const hit_test_result&, const hit_test_path_entry&)>;

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

		enum class hit_test_shape : std::uint8_t
		{
			quad,
			circle,
			ellipse,
			rounded_rectangle
		};

		struct hit_test_frame;

		struct hit_test_item
		{
			std::size_t draw_index = 0;
			rectangle destination{};
			glm::mat4 world_to_local{1.0f};
			bool inverse_valid = false;
			hit_test_shape shape = hit_test_shape::quad;
			float corner_radius = 0.0f;
			glm::vec4 uv_bounds{0.0f, 0.0f, 1.0f, 1.0f};
			std::uint64_t source_identity = 0;
			glm::uvec2 source_size{};
			std::optional<glm::ivec4> scissor;
			std::shared_ptr<const hit_test_frame> child_frame;
			std::shared_ptr<const hit_test_callback> click_callback;
		};

		struct hit_test_frame
		{
			glm::uvec2 target_size{};
			glm::mat4 view_projection{1.0f};
			std::size_t depth = 1;
			std::vector<hit_test_item> items;
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

		inline std::optional<glm::vec2> map_pixel_to_world_unbounded(const hit_test_frame& frame,
		                                                             glm::vec2 pixel_position)
		{
			if (frame.target_size.x == 0 || frame.target_size.y == 0 || !finite(pixel_position))
			{
				return std::nullopt;
			}

			const glm::vec2 ndc = 2.0f * pixel_position / glm::vec2(frame.target_size) - glm::vec2(1.0f);
			const glm::vec4 world_homogeneous =
			    glm::inverse(frame.view_projection) * glm::vec4(ndc, 0.0f, 1.0f);
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

			return {item.draw_index, local_coords, item.source_identity, item.source_size,
			        source_pixel_coords};
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
		    std::vector<const hit_test_frame*>& ancestors)
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

			for (std::size_t index = frame.items.size(); index > 0; --index)
			{
				const auto& item = frame.items[index - 1];
				if (item.scissor)
				{
					const glm::vec2 scissor_position{
					    pixel_position.x, static_cast<float>(frame.target_size.y) - pixel_position.y};
					const glm::vec4 scissor_bounds(*item.scissor);
					if (scissor_position.x < scissor_bounds.x || scissor_position.y < scissor_bounds.y ||
					    scissor_position.x >= scissor_bounds.x + scissor_bounds.z ||
					    scissor_position.y >= scissor_bounds.y + scissor_bounds.w)
					{
						continue;
					}
				}

				const auto local = local_position(item, *world_position);
				if (!local)
				{
					continue;
				}

				if (item.child_frame)
				{
					const glm::vec2 child_pixel =
					    map_local_to_texture_pixel(item, *local, item.child_frame->target_size);
					if (auto child_result =
					        hit_test_frame_at_pixel(*item.child_frame, child_pixel, ancestors))
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
			}

			return std::nullopt;
		}

		inline std::optional<hit_test_result> hit_test_frame_at_pixel(const hit_test_frame& frame,
		                                                              glm::vec2 pixel_position)
		{
			std::vector<const hit_test_frame*> ancestors;
			ancestors.reserve(8);
			return hit_test_frame_at_pixel(frame, pixel_position, ancestors);
		}
	} // namespace detail
} // namespace mngl
