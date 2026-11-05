#pragma once

#include "../renderer2d.hpp"
#include "../transform.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <condition_variable>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace mngl::two_d
{
	inline constexpr std::size_t no_tree_draw_node = std::numeric_limits<std::size_t>::max();

	enum class tree_node_style : unsigned char
	{
		normal,
		red,
		black,
		b_plus_leaf,
		b_plus_internal
	};

	struct tree_draw_node
	{
		std::vector<std::string> keys;
		std::vector<std::string> key_metadata;
		std::vector<std::size_t> children;
		std::size_t next_leaf = no_tree_draw_node;
		tree_node_style style = tree_node_style::normal;
		std::string annotation;
		float width = 0.0f;
		std::vector<float> cell_widths;
		bool highlighted = false;
	};

	struct tree_draw_model
	{
		static constexpr std::size_t no_node = no_tree_draw_node;

		std::vector<tree_draw_node> nodes;
		std::size_t root = no_node;
	};

	struct tree_draw_edge
	{
		std::size_t parent = no_tree_draw_node;
		std::size_t child_slot = 0;
		std::size_t child = no_tree_draw_node;
	};

	struct tree_draw_visibility
	{
		std::vector<std::size_t> nodes;
		std::vector<tree_draw_edge> edges;
		std::vector<std::size_t> leaf_link_sources;
	};

	struct tree_draw_cull_result
	{
		std::uint64_t request_id = 0;
		std::shared_ptr<const void> source;
		tree_draw_visibility visibility;
		std::exception_ptr error;
	};

	struct tree_layout_node
	{
		// Keep absolute positions precise until the camera origin is subtracted.
		glm::dvec2 center{};
		glm::vec2 size{};
		std::vector<float> cell_widths;
	};

	struct tree_layout
	{
		glm::dvec2 size{};
		std::vector<tree_layout_node> nodes;
	};

	struct tree_draw_area
	{
		glm::dvec2 pos{};
		glm::dvec2 size{};

		tree_draw_area() = default;
		tree_draw_area(glm::dvec2 position, glm::dvec2 dimensions) : pos(position), size(dimensions) {}
		tree_draw_area(double x, double y, double width, double height) : pos(x, y), size(width, height) {}
		tree_draw_area(const ::mngl::rectangle& area) : pos(area.pos), size(area.size) {}
	};

	struct tree_draw_options
	{
		glm::vec2 node_size{0.30f, 0.08f};
		float horizontal_gap = 0.08f;
		float vertical_gap = 0.12f;
		float edge_width = 0.004f;
		float corner_radius = 0.016f;
		float border_width = 0.003f;
		float value_text_height = 0.032f;
		color edge_color{0.48f, 0.55f, 0.65f, 1.0f};
		color node_border{0.20f, 0.40f, 0.63f, 1.0f};
		color node_fill{0.10f, 0.17f, 0.27f, 1.0f};
		color value_color{0.94f, 0.96f, 0.99f, 1.0f};
		bool show_leaf_links = true;
		float metadata_text_height = 0.018f;
		color red_node_fill{0.67f, 0.14f, 0.18f, 1.0f};
		color black_node_fill{0.10f, 0.11f, 0.14f, 1.0f};
		color metadata_color{0.72f, 0.78f, 0.87f, 1.0f};
		color b_plus_leaf_fill{0.12f, 0.32f, 0.29f, 1.0f};
		color b_plus_internal_fill{0.15f, 0.22f, 0.36f, 1.0f};
		color highlight_border{1.0f, 0.87f, 0.50f, 1.0f};
		color leaf_link_color{0.29f, 0.82f, 0.75f, 1.0f};
		const tree_draw_visibility* visibility = nullptr;
		std::span<const std::size_t> highlighted_nodes{};
		bool show_node_text = true;
		// Model rendering: world-space camera origin and viewport. GPU data stays FP32.
		glm::dvec2 coordinate_origin{};
		std::optional<tree_draw_area> clip_bounds;
	};

	using binary_tree_draw_options = tree_draw_options;

	class tree_drawable
	{
	public:
		virtual ~tree_drawable() = default;

		virtual void draw(frame_2d& frame, font& value_font, const ::mngl::rectangle& area,
		                  const tree_draw_options& options = {}) const = 0;

		virtual void draw_parallel(frame_2d& frame, font& value_font, const ::mngl::rectangle& area,
		                           const tree_draw_options& options = {}) const
		{
			draw(frame, value_font, area, options);
		}
	};

	class binary_tree_drawable : public tree_drawable
	{
	};

	namespace detail
	{
		inline constexpr std::size_t no_tree_node = std::numeric_limits<std::size_t>::max();

		enum class tree_node_annotation : unsigned char
		{
			none,
			height,
			priority,
			integer_metadata,
			red,
			black,
			b_plus_leaf,
			b_plus_internal
		};

		enum class tree_node_fill_role : unsigned char
		{
			default_fill,
			red,
			black
		};

		struct tree_node_snapshot
		{
			std::size_t key_begin = 0;
			std::size_t key_count = 0;
			std::size_t child_begin = 0;
			std::size_t child_count = 0;
			std::size_t next_leaf = no_tree_node;
			std::uint64_t metadata_value = 0;
			tree_node_annotation annotation = tree_node_annotation::none;
			tree_node_fill_role fill_role = tree_node_fill_role::default_fill;
		};

		struct tree_snapshot
		{
			std::vector<std::int64_t> keys;
			std::vector<tree_node_snapshot> nodes;
			std::vector<std::size_t> children;
			std::size_t root = no_tree_node;
		};

		template <class Metadata>
		concept red_black_color_metadata = std::is_enum_v<Metadata> && requires {
			Metadata::red;
			Metadata::black;
		};

		template <class Tree>
		concept displayable_binary_search_tree = requires(const Tree& tree) {
			typename Tree::value_type;
			typename Tree::node_type;
			requires std::same_as<typename Tree::value_type, std::int64_t>;
			{ tree.root() } -> std::convertible_to<const typename Tree::node_type*>;
			{ tree.root()->value() } -> std::convertible_to<std::int64_t>;
			{ tree.root()->left() } -> std::convertible_to<const typename Tree::node_type*>;
			{ tree.root()->right() } -> std::convertible_to<const typename Tree::node_type*>;
		};

		template <displayable_binary_search_tree Tree>
		[[nodiscard]] tree_snapshot capture_binary_tree(const Tree& tree)
		{
			using node_type = typename Tree::node_type;

			tree_snapshot result;
			const node_type* root = tree.root();
			if (!root)
			{
				return result;
			}

			const auto append_node = [&result](const node_type* node)
			{
				tree_node_snapshot snapshot_node;
				snapshot_node.key_begin = result.keys.size();
				snapshot_node.key_count = 1;
				result.keys.push_back(static_cast<std::int64_t>(node->value()));
				if constexpr (requires { node->metadata(); })
				{
					using metadata_type = std::remove_cvref_t<decltype(node->metadata())>;
					const auto metadata = node->metadata();
					if constexpr (red_black_color_metadata<metadata_type>)
					{
						if (metadata == metadata_type::red)
						{
							snapshot_node.annotation = tree_node_annotation::red;
							snapshot_node.fill_role = tree_node_fill_role::red;
						}
						else if (metadata == metadata_type::black)
						{
							snapshot_node.annotation = tree_node_annotation::black;
							snapshot_node.fill_role = tree_node_fill_role::black;
						}
						else
						{
							throw std::invalid_argument("Red-black tree node has an unknown color");
						}
					}
					else if constexpr (std::integral<metadata_type>)
					{
						snapshot_node.metadata_value = static_cast<std::uint64_t>(metadata);
						if constexpr (requires { typename Tree::seed_type; })
						{
							snapshot_node.annotation = tree_node_annotation::priority;
						}
						else if constexpr (requires(const Tree& source_tree) { source_tree.height(); })
						{
							snapshot_node.annotation = tree_node_annotation::height;
						}
						else
						{
							snapshot_node.annotation = tree_node_annotation::integer_metadata;
						}
					}
					else if constexpr (std::is_enum_v<metadata_type>)
					{
						using underlying_type = std::underlying_type_t<metadata_type>;
						snapshot_node.metadata_value =
						    static_cast<std::uint64_t>(static_cast<underlying_type>(metadata));
						snapshot_node.annotation = tree_node_annotation::integer_metadata;
					}
				}
				result.nodes.push_back(snapshot_node);
				return result.nodes.size() - 1;
			};

			result.root = append_node(root);
			std::vector<std::pair<const node_type*, std::size_t>> pending{{root, result.root}};
			while (!pending.empty())
			{
				const auto [source, index] = pending.back();
				pending.pop_back();

				const node_type* left = source->left();
				const node_type* right = source->right();
				result.nodes[index].child_begin = result.children.size();
				if (left)
				{
					const std::size_t left_index = append_node(left);
					result.children.push_back(left_index);
					pending.emplace_back(left, left_index);
				}
				if (right)
				{
					const std::size_t right_index = append_node(right);
					result.children.push_back(right_index);
					pending.emplace_back(right, right_index);
				}
				result.nodes[index].child_count = result.children.size() - result.nodes[index].child_begin;
			}

			return result;
		}

		template <class Tree>
		concept displayable_b_plus_tree =
		    requires(const Tree& tree, const typename Tree::node_type* node, std::size_t child_index) {
			    typename Tree::value_type;
			    typename Tree::node_type;
			    requires std::same_as<typename Tree::value_type, std::int64_t>;
			    { tree.root() } -> std::convertible_to<const typename Tree::node_type*>;
			    { node->is_leaf() } -> std::convertible_to<bool>;
			    { node->keys().size() } -> std::convertible_to<std::size_t>;
			    { *node->keys().begin() } -> std::convertible_to<std::int64_t>;
			    { node->child_count() } -> std::convertible_to<std::size_t>;
			    { node->child(child_index) } -> std::convertible_to<const typename Tree::node_type*>;
			    { node->previous_leaf() } -> std::convertible_to<const typename Tree::node_type*>;
			    { node->next_leaf() } -> std::convertible_to<const typename Tree::node_type*>;
		    };

		template <displayable_b_plus_tree Tree>
		[[nodiscard]] tree_snapshot capture_b_plus_tree(const Tree& tree)
		{
			using node_type = typename Tree::node_type;

			tree_snapshot result;
			const node_type* root = tree.root();
			if (!root)
			{
				return result;
			}

			const auto append_node = [&result](const node_type* source)
			{
				tree_node_snapshot node;
				node.key_begin = result.keys.size();
				node.key_count = source->keys().size();
				node.annotation = source->is_leaf() ? tree_node_annotation::b_plus_leaf
				                                    : tree_node_annotation::b_plus_internal;
				for (const auto& key : source->keys())
				{
					result.keys.push_back(static_cast<std::int64_t>(key));
				}
				result.nodes.push_back(node);
				return result.nodes.size() - 1;
			};

			result.root = append_node(root);
			std::vector<std::pair<const node_type*, std::size_t>> pending{{root, result.root}};
			std::vector<std::pair<const node_type*, std::size_t>> leaves;
			while (!pending.empty())
			{
				const auto [source, index] = pending.back();
				pending.pop_back();
				const std::size_t key_count = source->keys().size();
				const std::size_t child_count = source->child_count();
				if (source->is_leaf())
				{
					if (child_count != 0 || key_count == 0)
					{
						throw std::invalid_argument("B+ tree leaf has an invalid key or child count");
					}
					leaves.emplace_back(source, index);
					continue;
				}
				if (key_count == 0 || child_count != key_count + 1)
				{
					throw std::invalid_argument("B+ tree internal node must have one more child than keys");
				}

				result.nodes[index].child_begin = result.children.size();
				result.nodes[index].child_count = child_count;
				for (std::size_t i = 0; i < child_count; ++i)
				{
					const node_type* child = source->child(i);
					if (!child)
					{
						throw std::invalid_argument("B+ tree internal node contains a null child");
					}

					const std::size_t child_index = append_node(child);
					result.children.push_back(child_index);
				}
				for (std::size_t i = child_count; i-- > 0;)
				{
					pending.emplace_back(source->child(i),
					                     result.children[result.nodes[index].child_begin + i]);
				}
			}

			for (std::size_t i = 0; i < leaves.size(); ++i)
			{
				const node_type* expected_previous = i == 0 ? nullptr : leaves[i - 1].first;
				const node_type* expected_next = i + 1 == leaves.size() ? nullptr : leaves[i + 1].first;
				if (leaves[i].first->previous_leaf() != expected_previous ||
				    leaves[i].first->next_leaf() != expected_next)
				{
					throw std::invalid_argument("B+ tree leaf chain does not match child order");
				}
				if (i + 1 < leaves.size())
				{
					result.nodes[leaves[i].second].next_leaf = leaves[i + 1].second;
				}
			}

			return result;
		}

		[[nodiscard]] inline float checked_tree_coordinate(double value)
		{
			if (!std::isfinite(value) || value > std::numeric_limits<float>::max() ||
			    value < -std::numeric_limits<float>::max())
			{
				throw std::length_error("Tree layout exceeds the renderer's coordinate range");
			}
			return static_cast<float>(value);
		}

		[[nodiscard]] inline double checked_tree_world_coordinate(double value)
		{
			if (!std::isfinite(value))
			{
				throw std::length_error("Tree layout exceeds the CPU coordinate range");
			}
			return value;
		}

		[[nodiscard]] inline bool finite_color(const color& value) noexcept
		{
			const glm::vec4 channels = value.rgba_glsl();
			return std::isfinite(channels.x) && std::isfinite(channels.y) && std::isfinite(channels.z) &&
			       std::isfinite(channels.w);
		}

		inline void validate_tree_draw_area(const tree_draw_area& area)
		{
			if (!std::isfinite(area.pos.x) || !std::isfinite(area.pos.y) || !std::isfinite(area.size.x) ||
			    !std::isfinite(area.size.y) || !std::isfinite(area.pos.x + area.size.x) ||
			    !std::isfinite(area.pos.y + area.size.y) || area.size.x <= 0.0f || area.size.y <= 0.0f)
			{
				throw std::invalid_argument("Tree drawing area must be finite and positive");
			}
		}

		inline void validate_tree_draw_options(const tree_draw_options& options)
		{
			if (!std::isfinite(options.coordinate_origin.x) || !std::isfinite(options.coordinate_origin.y))
			{
				throw std::invalid_argument("Tree coordinate origin must be finite");
			}
			if (options.clip_bounds)
			{
				validate_tree_draw_area(*options.clip_bounds);
			}
			if (!std::isfinite(options.node_size.x) || !std::isfinite(options.node_size.y) ||
			    options.node_size.x <= 0.0f || options.node_size.y <= 0.0f ||
			    !std::isfinite(options.horizontal_gap) || options.horizontal_gap < 0.0f ||
			    !std::isfinite(options.vertical_gap) || options.vertical_gap <= 0.0f ||
			    !std::isfinite(options.edge_width) || options.edge_width < 0.0f ||
			    !std::isfinite(options.corner_radius) || options.corner_radius < 0.0f ||
			    !std::isfinite(options.border_width) || options.border_width < 0.0f ||
			    options.border_width >= std::min(options.node_size.x, options.node_size.y) * 0.5f ||
			    !std::isfinite(options.value_text_height) || options.value_text_height <= 0.0f ||
			    !std::isfinite(options.metadata_text_height) || options.metadata_text_height <= 0.0f)
			{
				throw std::invalid_argument("Tree drawing options contain invalid dimensions");
			}
			if (!finite_color(options.edge_color) || !finite_color(options.node_border) ||
			    !finite_color(options.node_fill) || !finite_color(options.red_node_fill) ||
			    !finite_color(options.black_node_fill) || !finite_color(options.value_color) ||
			    !finite_color(options.metadata_color) || !finite_color(options.b_plus_leaf_fill) ||
			    !finite_color(options.b_plus_internal_fill) || !finite_color(options.highlight_border) ||
			    !finite_color(options.leaf_link_color))
			{
				throw std::invalid_argument("Tree drawing colors must be finite");
			}
		}

		inline void validate_tree_draw_options(const tree_draw_area& area, const tree_draw_options& options)
		{
			validate_tree_draw_area(area);
			validate_tree_draw_options(options);
		}

		[[nodiscard]] inline glm::dvec2 tree_layout_offset(const tree_layout& layout,
		                                                   const tree_draw_area& area)
		{
			return {area.pos.x + std::max(0.0, (area.size.x - layout.size.x) * 0.5),
			        area.pos.y + area.size.y - layout.size.y};
		}

		[[nodiscard]] inline glm::vec2 tree_local_position(glm::dvec2 world, glm::dvec2 origin)
		{
			const glm::dvec2 local = world - origin;
			return {checked_tree_coordinate(local.x), checked_tree_coordinate(local.y)};
		}

		// Liang-Barsky clipping happens in double before narrowing long edges to float.
		[[nodiscard]] inline bool clip_tree_segment(glm::dvec2& start, glm::dvec2& end,
		                                            const tree_draw_area& viewport,
		                                            double padding = 0.0) noexcept
		{
			const glm::dvec2 delta = end - start;
			const glm::dvec2 lower = viewport.pos - glm::dvec2(padding);
			const glm::dvec2 upper = viewport.pos + viewport.size + glm::dvec2(padding);
			double enter = 0.0;
			double leave = 1.0;
			for (int axis = 0; axis < 2; ++axis)
			{
				if (delta[axis] == 0.0)
				{
					if (start[axis] < lower[axis] || start[axis] > upper[axis])
					{
						return false;
					}
					continue;
				}
				double first = (lower[axis] - start[axis]) / delta[axis];
				double last = (upper[axis] - start[axis]) / delta[axis];
				if (first > last)
				{
					std::swap(first, last);
				}
				enter = std::max(enter, first);
				leave = std::min(leave, last);
				if (enter > leave)
				{
					return false;
				}
			}
			end = start + delta * leave;
			start += delta * enter;
			return true;
		}

		[[nodiscard]] inline std::string_view tree_node_annotation_text(
		    const tree_node_snapshot& node,
		    std::array<char, std::numeric_limits<std::uint64_t>::digits10 + 4>& buffer)
		{
			switch (node.annotation)
			{
			case tree_node_annotation::none:
				return {};
			case tree_node_annotation::red:
				return "red";
			case tree_node_annotation::black:
				return "black";
			case tree_node_annotation::b_plus_leaf:
				return "leaf";
			case tree_node_annotation::b_plus_internal:
				return "index";
			case tree_node_annotation::height:
				buffer[0] = 'h';
				break;
			case tree_node_annotation::priority:
				buffer[0] = 'p';
				break;
			case tree_node_annotation::integer_metadata:
				buffer[0] = 'm';
				break;
			}

			buffer[1] = '=';
			const auto [value_end, conversion_error] =
			    std::to_chars(buffer.data() + 2, buffer.data() + buffer.size(), node.metadata_value);
			if (conversion_error != std::errc{})
			{
				throw std::logic_error("Could not format tree node metadata");
			}
			return {buffer.data(), static_cast<std::size_t>(value_end - buffer.data())};
		}

		inline void draw_fitted_tree_text(frame_2d& frame, font& value_font, std::string_view text,
		                                  const ::mngl::rectangle& bounds, float center_y,
		                                  float maximum_height, float line_height_factor, const color& tint,
		                                  bool prepared_glyphs = false)
		{
			if (text.empty())
			{
				return;
			}
			float text_height = std::min(maximum_height, bounds.size.y / line_height_factor);
			float text_width = prepared_glyphs
			                       ? value_font.measure_text_line_width_prepared(text, text_height)
			                       : value_font.measure_text_line_width(text, text_height);
			if (text_width > bounds.size.x)
			{
				text_height *= bounds.size.x / text_width;
				text_width = prepared_glyphs ? value_font.measure_text_line_width_prepared(text, text_height)
				                             : value_font.measure_text_line_width(text, text_height);
			}

			const float rendered_line_height = line_height_factor * text_height;
			text_draw_options text_options;
			text_options.height = text_height;
			text_options.tint = tint;
			text_options.clip_bounds = bounds;
			frame.draw_text(
			    value_font, text,
			    {bounds.pos.x + (bounds.size.x - text_width) * 0.5f, center_y + rendered_line_height * 0.5f},
			    text_options);
		}

		inline void draw_tree_segment(frame_2d& frame, glm::vec2 start, glm::vec2 end, float width,
		                              const color& tint)
		{
			if (width <= 0.0f)
			{
				return;
			}

			const double delta_x = static_cast<double>(end.x) - start.x;
			const double delta_y = static_cast<double>(end.y) - start.y;
			const double length = std::hypot(delta_x, delta_y);
			if (length == 0.0)
			{
				return;
			}
			const float segment_length = checked_tree_coordinate(length);
			const float angle = static_cast<float>(std::atan2(delta_y, delta_x));
			::mngl::rectangle bounds{0.0f, -width * 0.5f, segment_length, width};
			transform edge_transform;
			edge_transform.translate(start).rotate(angle);
			frame.draw(rectangle_shape(bounds, tint, edge_transform));
		}

		inline void draw_tree_world_segment(frame_2d& frame, glm::dvec2 start, glm::dvec2 end, float width,
		                                    const color& tint, const tree_draw_options& options)
		{
			if (options.clip_bounds && !clip_tree_segment(start, end, *options.clip_bounds, width * 0.5))
			{
				return;
			}
			draw_tree_segment(frame, tree_local_position(start, options.coordinate_origin),
			                  tree_local_position(end, options.coordinate_origin), width, tint);
		}

		inline void draw_tree_snapshot(frame_2d& frame, font& value_font, const tree_snapshot& snapshot,
		                               const ::mngl::rectangle& area, const tree_draw_options& options)
		{
			validate_tree_draw_options(area, options);

			const std::size_t count = snapshot.nodes.size();
			if (count == 0)
			{
				if (snapshot.root != no_tree_node || !snapshot.keys.empty() || !snapshot.children.empty())
				{
					throw std::invalid_argument("Empty tree snapshot contains nodes or references");
				}
				return;
			}
			if (snapshot.root >= count)
			{
				throw std::invalid_argument("Tree snapshot root is outside the node array");
			}

			for (const auto& node : snapshot.nodes)
			{
				if (node.key_begin > snapshot.keys.size() ||
				    node.key_count > snapshot.keys.size() - node.key_begin ||
				    node.child_begin > snapshot.children.size() ||
				    node.child_count > snapshot.children.size() - node.child_begin)
				{
					throw std::invalid_argument("Tree snapshot contains an invalid node range");
				}
				if (node.next_leaf != no_tree_node && node.next_leaf >= count)
				{
					throw std::invalid_argument("Tree snapshot leaf link is outside the node array");
				}
			}

			std::vector<unsigned char> discovered(count, 0);
			std::vector<std::size_t> depths(count, 0);
			std::vector<std::size_t> pending;
			discovered[snapshot.root] = 1;
			pending.push_back(snapshot.root);
			std::size_t reachable_count = 1;
			while (!pending.empty())
			{
				const std::size_t current = pending.back();
				pending.pop_back();
				const auto& node = snapshot.nodes[current];
				for (std::size_t i = 0; i < node.child_count; ++i)
				{
					const std::size_t child = snapshot.children[node.child_begin + i];
					if (child >= count || child <= current)
					{
						throw std::invalid_argument("Tree snapshot child is invalid or not parent-first");
					}
					if (discovered[child] != 0)
					{
						throw std::invalid_argument("Tree snapshot contains a cycle or shared child");
					}
					if (depths[current] == std::numeric_limits<std::size_t>::max())
					{
						throw std::length_error("Tree depth exceeds the supported range");
					}

					discovered[child] = 1;
					depths[child] = depths[current] + 1;
					++reachable_count;
					pending.push_back(child);
				}
				if (node.next_leaf != no_tree_node &&
				    (node.child_count != 0 || snapshot.nodes[node.next_leaf].child_count != 0))
				{
					throw std::invalid_argument("Tree snapshot leaf links must connect leaf nodes");
				}
			}
			if (reachable_count != count)
			{
				throw std::invalid_argument("Tree snapshot contains unreachable nodes");
			}

			const double cell_width = options.node_size.x;
			const double node_height = options.node_size.y;
			const double horizontal_gap = options.horizontal_gap;
			const double vertical_step = node_height + static_cast<double>(options.vertical_gap);
			std::vector<double> node_widths(count);
			std::vector<double> subtree_widths(count);
			for (std::size_t index = 0; index < count; ++index)
			{
				const double key_slots =
				    static_cast<double>(std::max<std::size_t>(1, snapshot.nodes[index].key_count));
				node_widths[index] = key_slots * cell_width;
				if (!std::isfinite(node_widths[index]))
				{
					throw std::length_error("Tree node width exceeds the supported range");
				}
			}
			for (std::size_t index = count; index-- > 0;)
			{
				const auto& node = snapshot.nodes[index];
				double children_width = 0.0;
				for (std::size_t i = 0; i < node.child_count; ++i)
				{
					if (i != 0)
					{
						children_width += horizontal_gap;
					}
					const std::size_t child = snapshot.children[node.child_begin + i];
					children_width += subtree_widths[child];
				}

				subtree_widths[index] = std::max(node_widths[index], children_width);
				if (!std::isfinite(subtree_widths[index]))
				{
					throw std::length_error("Tree layout width exceeds the supported range");
				}
			}

			const double root_width = subtree_widths[snapshot.root];
			const double content_left = static_cast<double>(area.pos.x) +
			                            std::max(0.0, (static_cast<double>(area.size.x) - root_width) * 0.5);
			const double top = static_cast<double>(area.pos.y) + area.size.y;
			struct placement
			{
				std::size_t index;
				double left;
			};
			std::vector<glm::vec2> centers(count);
			std::vector<placement> placements{{snapshot.root, content_left}};
			while (!placements.empty())
			{
				const placement current = placements.back();
				placements.pop_back();
				const auto& node = snapshot.nodes[current.index];
				const double center_x = current.left + subtree_widths[current.index] * 0.5;
				const double center_y =
				    top - node_height * 0.5 - static_cast<double>(depths[current.index]) * vertical_step;
				centers[current.index] = {checked_tree_coordinate(center_x),
				                          checked_tree_coordinate(center_y)};

				double children_width = 0.0;
				for (std::size_t i = 0; i < node.child_count; ++i)
				{
					if (i != 0)
					{
						children_width += horizontal_gap;
					}
					const std::size_t child = snapshot.children[node.child_begin + i];
					children_width += subtree_widths[child];
				}
				double child_right = current.left + (subtree_widths[current.index] + children_width) * 0.5;
				for (std::size_t i = node.child_count; i-- > 0;)
				{
					const std::size_t child = snapshot.children[node.child_begin + i];
					child_right -= subtree_widths[child];
					placements.push_back({child, child_right});
					child_right -= horizontal_gap;
				}
			}

			if (options.edge_width > 0.0f)
			{
				for (std::size_t index = 0; index < count; ++index)
				{
					const auto& node = snapshot.nodes[index];
					const double node_left = static_cast<double>(centers[index].x) - node_widths[index] * 0.5;
					for (std::size_t i = 0; i < node.child_count; ++i)
					{
						const std::size_t child = snapshot.children[node.child_begin + i];
						const double anchor_x =
						    node.child_count > 1 ? node_left + node_widths[index] * i / (node.child_count - 1)
							                     : centers[index].x;
						const glm::vec2 start{checked_tree_coordinate(anchor_x),
						                      checked_tree_coordinate(static_cast<double>(centers[index].y) -
						                                              node_height * 0.5)};
						const glm::vec2 end{centers[child].x,
						                    checked_tree_coordinate(static_cast<double>(centers[child].y) +
						                                            node_height * 0.5)};
						draw_tree_segment(frame, start, end, options.edge_width, options.edge_color);
					}
				}

				if (options.show_leaf_links)
				{
					for (std::size_t index = 0; index < count; ++index)
					{
						const std::size_t next = snapshot.nodes[index].next_leaf;
						if (next != no_tree_node)
						{
							draw_tree_segment(frame, centers[index], centers[next], options.edge_width,
							                  options.edge_color);
						}
					}
				}

#if 0
				[[nodiscard]] inline bool has_tree_model_children(
					const tree_draw_node& node) noexcept
				{
					return std::any_of(
						node.children.begin(),
						node.children.end(),
						[](std::size_t child)
						{
							return child != tree_draw_model::no_node;
						});
				}

				[[nodiscard]] inline tree_layout build_tree_layout(
					const tree_draw_model& model,
					const tree_draw_options& options)
				{
					validate_tree_draw_options(options);
					tree_layout result;
					const std::size_t count = model.nodes.size();
					if (count == 0)
					{
						if (model.root != tree_draw_model::no_node)
						{
							throw std::invalid_argument(
								"An empty tree model must not specify a root");
						}
						return result;
					}
					if (model.root >= count)
					{
						throw std::invalid_argument(
							"Tree model root is outside the node array");
					}

					result.nodes.resize(count);
					std::vector<double> node_widths(count);
					std::vector<double> subtree_widths(count);
					std::vector<std::size_t> depths(count, 0);
					std::vector<unsigned char> state(count, 0);
					std::vector<std::size_t> postorder;
					postorder.reserve(count);

					for (std::size_t index = 0; index < count; ++index)
					{
						const auto& node = model.nodes[index];
						if (!std::isfinite(node.width) || node.width < 0.0f)
						{
							throw std::invalid_argument(
								"Tree model node width must be finite and non-negative");
						}
						if (!node.key_metadata.empty() &&
							node.key_metadata.size() != node.keys.size())
						{
							throw std::invalid_argument(
								"Tree model key metadata must match its key count");
						}
						if (node.cell_widths.size() != 0 &&
							node.cell_widths.size() != node.keys.size())
						{
							throw std::invalid_argument(
								"Tree model cell widths must match its key count");
						}
						switch (node.style)
						{
						case tree_node_style::normal:
						case tree_node_style::red:
						case tree_node_style::black:
						case tree_node_style::b_plus_leaf:
						case tree_node_style::b_plus_internal:
							break;
						default:
							throw std::invalid_argument(
								"Tree model contains an unknown node style");
						}
						if (node.next_leaf != tree_draw_model::no_node &&
							node.next_leaf >= count)
						{
							throw std::invalid_argument(
								"Tree model leaf link is outside the node array");
						}
						for (const std::size_t child : node.children)
						{
							if (child != tree_draw_model::no_node && child >= count)
							{
								throw std::invalid_argument(
									"Tree model child is outside the node array");
							}
						}

						double node_width = 0.0;
						if (!node.cell_widths.empty())
						{
							for (const float cell_width : node.cell_widths)
							{
								if (!std::isfinite(cell_width) || cell_width <= 0.0f)
								{
									throw std::invalid_argument(
										"Tree model cell widths must be finite and positive");
								}
								node_width += cell_width;
							}
							if (node.width > 0.0f &&
								std::abs(node_width - node.width) >
									std::max(0.001, node_width * 0.00001))
							{
								throw std::invalid_argument(
									"Tree model node width must equal the sum of its cell widths");
							}
						}
						else if (node.width > 0.0f)
						{
							node_width = node.width;
						}
						else
						{
							node_width = static_cast<double>(
								std::max<std::size_t>(1, node.keys.size())) *
								options.node_size.x;
						}
						if (!std::isfinite(node_width) ||
							node_width <= 2.0 * options.border_width)
						{
							throw std::invalid_argument(
								"Tree model node width is too small for its border");
						}
						node_widths[index] = node_width;

						auto& layout_node = result.nodes[index];
						layout_node.size = {
							checked_tree_coordinate(node_width),
							options.node_size.y
						};
						if (!node.cell_widths.empty())
						{
							layout_node.cell_widths = node.cell_widths;
						}
						else if (!node.keys.empty())
						{
							const float cell_width = checked_tree_coordinate(
								node_width / node.keys.size());
							layout_node.cell_widths.assign(
								node.keys.size(),
								cell_width);
							const double assigned_width =
								static_cast<double>(cell_width) *
								static_cast<double>(node.keys.size() - 1);
							layout_node.cell_widths.back() =
								checked_tree_coordinate(node_width - assigned_width);
						}
					}

					struct traversal
					{
						std::size_t index;
						bool expanded;
					};
					std::vector<traversal> pending;
					pending.reserve(count * 2);
					state[model.root] = 1;
					pending.push_back({ model.root, false });
					std::size_t maximum_depth = 0;
					while (!pending.empty())
					{
						const traversal current = pending.back();
						pending.pop_back();
						const auto& node = model.nodes[current.index];
						if (current.expanded)
						{
							state[current.index] = 2;
							postorder.push_back(current.index);
							continue;
						}

						pending.push_back({ current.index, true });
						for (std::size_t i = node.children.size(); i-- > 0;)
						{
							const std::size_t child = node.children[i];
							if (child == tree_draw_model::no_node)
							{
								continue;
							}
							if (state[child] != 0)
							{
								throw std::invalid_argument(
									"Tree model contains a cycle or shared child");
							}
							if (depths[current.index] ==
								std::numeric_limits<std::size_t>::max())
							{
								throw std::length_error(
									"Tree depth exceeds the supported range");
							}

							state[child] = 1;
							depths[child] = depths[current.index] + 1;
							maximum_depth = std::max(maximum_depth, depths[child]);
							pending.push_back({ child, false });
						}
					}
					if (std::any_of(
						state.begin(),
						state.end(),
						[](unsigned char value) { return value == 0; }))
					{
						throw std::invalid_argument(
							"Tree model contains unreachable nodes");
					}
					for (std::size_t index = 0; index < count; ++index)
					{
						const std::size_t next = model.nodes[index].next_leaf;
						if (next != tree_draw_model::no_node &&
							(has_tree_model_children(model.nodes[index]) ||
								has_tree_model_children(model.nodes[next])))
						{
							throw std::invalid_argument(
								"Tree model leaf links must connect leaf nodes");
						}
					}

					const double horizontal_gap = options.horizontal_gap;
					for (const std::size_t index : postorder)
					{
						const auto& node = model.nodes[index];
						double children_width = 0.0;
						for (std::size_t i = 0; i < node.children.size(); ++i)
						{
							if (i != 0)
							{
								children_width += horizontal_gap;
							}
							const std::size_t child = node.children[i];
							children_width += child == tree_draw_model::no_node
								? options.node_size.x
								: subtree_widths[child];
						}
						subtree_widths[index] =
							std::max(node_widths[index], children_width);
						if (!std::isfinite(subtree_widths[index]))
						{
							throw std::length_error(
								"Tree layout width exceeds the supported range");
						}
					}

					const double node_height = options.node_size.y;
					const double vertical_step =
						node_height + static_cast<double>(options.vertical_gap);
					const double height =
						node_height + static_cast<double>(maximum_depth) * vertical_step;
					const double width = subtree_widths[model.root];
					result.size = {
						checked_tree_coordinate(width),
						checked_tree_coordinate(height)
					};

					struct placement
					{
						std::size_t index;
						double left;
					};
					std::vector<placement> placements{
						{ model.root, 0.0 }
					};
					while (!placements.empty())
					{
						const placement current = placements.back();
						placements.pop_back();
						const auto& node = model.nodes[current.index];
						auto& layout_node = result.nodes[current.index];
						const double center_x =
							current.left + subtree_widths[current.index] * 0.5;
						const double center_y =
							height - node_height * 0.5 -
							static_cast<double>(depths[current.index]) * vertical_step;
						layout_node.center = {
							checked_tree_coordinate(center_x),
							checked_tree_coordinate(center_y)
						};

						double children_width = 0.0;
						for (std::size_t i = 0; i < node.children.size(); ++i)
						{
							if (i != 0)
							{
								children_width += horizontal_gap;
							}
							const std::size_t child = node.children[i];
							children_width += child == tree_draw_model::no_node
								? options.node_size.x
								: subtree_widths[child];
						}
						double child_right =
							current.left +
							(subtree_widths[current.index] + children_width) * 0.5;
						for (std::size_t i = node.children.size(); i-- > 0;)
						{
							const std::size_t child = node.children[i];
							const double child_width =
								child == tree_draw_model::no_node
								? options.node_size.x
								: subtree_widths[child];
							child_right -= child_width;
							if (child != tree_draw_model::no_node)
							{
								placements.push_back({ child, child_right });
							}
							child_right -= horizontal_gap;
						}
					}
					return result;
				}

				inline void draw_tree_arrow(
					frame_2d& frame,
					glm::vec2 start,
					glm::vec2 end,
					float width,
					float arrow_size,
					const color& tint)
				{
					const glm::vec2 delta = end - start;
					const float length = glm::length(delta);
					if (!std::isfinite(length) || length <= 0.0f)
					{
						return;
					}
					const glm::vec2 direction = delta / length;
					const glm::vec2 perpendicular{ -direction.y, direction.x };
					const float head_size = std::min(arrow_size, length * 0.45f);
					const glm::vec2 base = end - direction * head_size;
					draw_tree_segment(frame, start, end, width, tint);
					draw_tree_segment(
						frame,
						end,
						base + perpendicular * (head_size * 0.55f),
						width,
						tint);
					draw_tree_segment(
						frame,
						end,
						base - perpendicular * (head_size * 0.55f),
						width,
						tint);
				}

				inline void draw_tree_model_snapshot(
					frame_2d& frame,
					font& value_font,
					const tree_draw_model& model,
					const ::mngl::rectangle& area,
					const tree_draw_options& options)
				{
					validate_tree_draw_options(area, options);
					const tree_layout layout = build_tree_layout(model, options);
					if (model.nodes.empty())
					{
						return;
					}

					const glm::vec2 offset{
						checked_tree_coordinate(
							static_cast<double>(area.pos.x) +
							std::max(
								0.0,
								(static_cast<double>(area.size.x) -
									layout.size.x) * 0.5)),
						checked_tree_coordinate(
							static_cast<double>(area.pos.y) +
							area.size.y - layout.size.y)
					};
					std::vector<glm::vec2> centers(layout.nodes.size());
					for (std::size_t i = 0; i < centers.size(); ++i)
					{
						centers[i] = layout.nodes[i].center + offset;
					}

					if (options.edge_width > 0.0f)
					{
						for (std::size_t index = 0; index < model.nodes.size(); ++index)
						{
							const auto& node = model.nodes[index];
							if (node.children.empty())
							{
								continue;
							}
							const float node_left =
								centers[index].x - layout.nodes[index].size.x * 0.5f;
							for (std::size_t i = 0; i < node.children.size(); ++i)
							{
								const std::size_t child = node.children[i];
								if (child == tree_draw_model::no_node)
								{
									continue;
								}
								const float child_fraction =
									node.children.size() > 1
									? static_cast<float>(i) /
										static_cast<float>(node.children.size() - 1)
									: 0.5f;
								const glm::vec2 start{
									node_left +
										layout.nodes[index].size.x * child_fraction,
									centers[index].y - options.node_size.y * 0.5f
								};
								const glm::vec2 end{
									centers[child].x,
									centers[child].y + options.node_size.y * 0.5f
								};
								draw_tree_segment(
									frame,
									start,
									end,
									options.edge_width,
									options.edge_color);
							}
						}

						if (options.show_leaf_links)
						{
							for (std::size_t index = 0; index < model.nodes.size(); ++index)
							{
								const std::size_t next = model.nodes[index].next_leaf;
								if (next == tree_draw_model::no_node)
								{
									continue;
								}
								const float link_y =
									centers[index].y -
									options.node_size.y * 0.5f -
									std::min(
										options.node_size.y * 0.35f,
										std::max(
											options.node_size.y * 0.12f,
											options.horizontal_gap * 0.20f));
								glm::vec2 start{
									centers[index].x +
										layout.nodes[index].size.x * 0.5f,
									link_y
								};
								glm::vec2 end{
									centers[next].x -
										layout.nodes[next].size.x * 0.5f,
									link_y
								};
								if (end.x <= start.x)
								{
									start = {
										centers[index].x,
										centers[index].y -
											options.node_size.y * 0.5f
									};
									end = {
										centers[next].x,
										centers[next].y -
											options.node_size.y * 0.5f
									};
								}
								draw_tree_arrow(
									frame,
									start,
									end,
									options.edge_width,
									std::max(
										options.node_size.y * 0.08f,
										options.edge_width * 2.5f),
									options.leaf_link_color);
							}
						}
					}

					const float raster_height = value_font.raster_height_pixels();
					const float line_height = value_font.line_height();
					if (!std::isfinite(raster_height) ||
						raster_height <= 0.0f ||
						!std::isfinite(line_height) ||
						line_height <= 0.0f)
					{
						throw std::logic_error(
							"Tree drawing requires a valid font");
					}
					const float line_height_factor = line_height / raster_height;
					if (!std::isfinite(line_height_factor) ||
						line_height_factor <= 0.0f)
					{
						throw std::logic_error(
							"Tree drawing font metrics cannot be scaled");
					}

					for (std::size_t index = 0; index < model.nodes.size(); ++index)
					{
						const auto& node = model.nodes[index];
						const auto& layout_node = layout.nodes[index];
						const ::mngl::rectangle node_bounds{
							centers[index] - layout_node.size * 0.5f,
							layout_node.size
						};
						const float node_radius = std::min(
							options.corner_radius,
							std::min(
								layout_node.size.x,
								layout_node.size.y) * 0.5f);
						if (options.border_width > 0.0f)
						{
							rounded_rectangle border;
							border.bounds = node_bounds;
							border.radius = node_radius;
							border.tint = node.highlighted
								? options.highlight_border
								: options.node_border;
							frame.draw(border);
						}

						const glm::vec2 inset(options.border_width);
						const ::mngl::rectangle fill_bounds{
							node_bounds.pos + inset,
							node_bounds.size - inset * 2.0f
						};
						rounded_rectangle fill;
						fill.bounds = fill_bounds;
						fill.radius = std::max(
							0.0f,
							node_radius - options.border_width);
						switch (node.style)
						{
						case tree_node_style::red:
							fill.tint = options.red_node_fill;
							break;
						case tree_node_style::black:
							fill.tint = options.black_node_fill;
							break;
						case tree_node_style::b_plus_leaf:
							fill.tint = options.b_plus_leaf_fill;
							break;
						case tree_node_style::b_plus_internal:
							fill.tint = options.b_plus_internal_fill;
							break;
						default:
							fill.tint = options.node_fill;
							break;
						}
						frame.draw(fill);

						float divider_x = node_bounds.pos.x;
						for (std::size_t i = 0; i < layout_node.cell_widths.size(); ++i)
						{
							if (i != 0 && options.edge_width > 0.0f)
							{
								const float divider_width = std::min(
									options.edge_width,
									layout_node.size.y * 0.1f);
								const ::mngl::rectangle divider{
									divider_x - divider_width * 0.5f,
									fill_bounds.pos.y,
									divider_width,
									fill_bounds.size.y
								};
								frame.draw(rectangle_shape(
									divider,
									options.edge_color,
									transform{}));
							}
							divider_x += layout_node.cell_widths[i];
						}

						const float vertical_padding =
							std::min(fill_bounds.size.y * 0.08f, layout_node.size.y * 0.1f);
						const float content_bottom =
							fill_bounds.pos.y + vertical_padding;
						const float content_height =
							fill_bounds.size.y - vertical_padding * 2.0f;
						float cell_left = node_bounds.pos.x;
						for (std::size_t i = 0; i < node.keys.size(); ++i)
						{
							const float cell_width = layout_node.cell_widths[i];
							const float horizontal_padding =
								std::min(
									cell_width * 0.06f,
									layout_node.size.y * 0.12f);
							std::string annotation;
							if (i == 0)
							{
								annotation = node.annotation;
							}
							if (!node.key_metadata.empty() &&
								!node.key_metadata[i].empty())
							{
								if (!annotation.empty())
								{
									annotation += " ";
								}
								annotation += node.key_metadata[i];
							}
							const bool has_annotation = !annotation.empty();
							const float row_height =
								has_annotation
								? content_height * 0.5f
								: content_height;
							const ::mngl::rectangle cell_text_bounds{
								cell_left + horizontal_padding,
								content_bottom,
								cell_width - horizontal_padding * 2.0f,
								content_height
							};
							const ::mngl::rectangle value_bounds{
								cell_text_bounds.pos.x,
								has_annotation
									? content_bottom + row_height
									: content_bottom,
								cell_text_bounds.size.x,
								row_height
							};
							draw_fitted_tree_text(
								frame,
								value_font,
								node.keys[i],
								value_bounds,
								value_bounds.pos.y + value_bounds.size.y * 0.5f,
								options.value_text_height,
								line_height_factor,
								options.value_color);
							if (has_annotation)
							{
								const ::mngl::rectangle metadata_bounds{
									cell_text_bounds.pos.x,
									content_bottom,
									cell_text_bounds.size.x,
									row_height
								};
								draw_fitted_tree_text(
									frame,
									value_font,
									annotation,
									metadata_bounds,
									metadata_bounds.pos.y +
										metadata_bounds.size.y * 0.5f,
									options.metadata_text_height,
									line_height_factor,
									options.metadata_color);
							}
							cell_left += cell_width;
						}
					}
				}
#endif
			}

			const float raster_height = value_font.raster_height_pixels();
			const float line_height = value_font.line_height();
			if (!std::isfinite(raster_height) || raster_height <= 0.0f || !std::isfinite(line_height) ||
			    line_height <= 0.0f)
			{
				throw std::logic_error("Tree drawing requires a valid font");
			}
			const float line_height_factor = line_height / raster_height;
			if (!std::isfinite(line_height_factor) || line_height_factor <= 0.0f)
			{
				throw std::logic_error("Tree drawing font metrics cannot be scaled");
			}

			for (std::size_t index = 0; index < count; ++index)
			{
				const auto& node = snapshot.nodes[index];
				const float node_width = checked_tree_coordinate(node_widths[index]);
				const glm::vec2 node_size{node_width, options.node_size.y};
				const glm::vec2 center = centers[index];
				const ::mngl::rectangle node_bounds{center - node_size * 0.5f, node_size};
				const float node_radius =
				    std::min(options.corner_radius, std::min(node_size.x, node_size.y) * 0.5f);
				if (options.border_width > 0.0f)
				{
					rounded_rectangle border;
					border.bounds = node_bounds;
					border.radius = node_radius;
					border.tint = options.node_border;
					frame.draw(border);
				}

				const glm::vec2 inset(options.border_width);
				const ::mngl::rectangle fill_bounds{node_bounds.pos + inset, node_bounds.size - inset * 2.0f};
				rounded_rectangle fill;
				fill.bounds = fill_bounds;
				fill.radius = std::max(0.0f, node_radius - options.border_width);
				switch (node.fill_role)
				{
				case tree_node_fill_role::red:
					fill.tint = options.red_node_fill;
					break;
				case tree_node_fill_role::black:
					fill.tint = options.black_node_fill;
					break;
				default:
					fill.tint = options.node_fill;
					break;
				}
				frame.draw(fill);

				if (options.edge_width > 0.0f && node.key_count > 1)
				{
					const float divider_width = std::min(options.edge_width, options.node_size.x * 0.1f);
					for (std::size_t i = 1; i < node.key_count; ++i)
					{
						const float divider_x = node_bounds.pos.x +
						                        options.node_size.x * static_cast<float>(i) -
						                        divider_width * 0.5f;
						const ::mngl::rectangle divider{divider_x, fill_bounds.pos.y, divider_width,
						                                fill_bounds.size.y};
						frame.draw(rectangle_shape(divider, options.edge_color, transform{}));
					}
				}

				std::array<char, std::numeric_limits<std::uint64_t>::digits10 + 4> annotation_buffer{};
				const std::string_view annotation_text = tree_node_annotation_text(node, annotation_buffer);
				const bool has_annotation = !annotation_text.empty();
				const float horizontal_padding = std::min(options.node_size.x * 0.06f, 0.012f);
				const float vertical_padding = std::min(fill_bounds.size.y * 0.08f, 0.006f);
				const float content_bottom = fill_bounds.pos.y + vertical_padding;
				const float content_height = fill_bounds.size.y - vertical_padding * 2.0f;
				const float row_height = has_annotation ? content_height * 0.5f : content_height;

				for (std::size_t i = 0; i < node.key_count; ++i)
				{
					std::array<char, std::numeric_limits<std::int64_t>::digits10 + 3> value_buffer{};
					const std::int64_t value = snapshot.keys[node.key_begin + i];
					const auto [value_end, conversion_error] =
					    std::to_chars(value_buffer.data(), value_buffer.data() + value_buffer.size(), value);
					if (conversion_error != std::errc{})
					{
						throw std::logic_error("Could not format an std::int64_t tree key");
					}
					const std::string_view value_text(
					    value_buffer.data(), static_cast<std::size_t>(value_end - value_buffer.data()));

					const float cell_left = node_bounds.pos.x + options.node_size.x * static_cast<float>(i);
					const ::mngl::rectangle cell_text_bounds{cell_left + horizontal_padding, content_bottom,
					                                         options.node_size.x - horizontal_padding * 2.0f,
					                                         content_height};
					const ::mngl::rectangle value_bounds{
					    cell_text_bounds.pos.x, has_annotation ? content_bottom + row_height : content_bottom,
					    cell_text_bounds.size.x, row_height};
					const float value_center_y = value_bounds.pos.y + value_bounds.size.y * 0.5f;
					draw_fitted_tree_text(frame, value_font, value_text, value_bounds, value_center_y,
					                      options.value_text_height, line_height_factor, options.value_color);

					if (has_annotation && i == 0)
					{
						const ::mngl::rectangle metadata_bounds{cell_text_bounds.pos.x, content_bottom,
						                                        cell_text_bounds.size.x, row_height};
						const float metadata_center_y = metadata_bounds.pos.y + metadata_bounds.size.y * 0.5f;
						draw_fitted_tree_text(frame, value_font, annotation_text, metadata_bounds,
						                      metadata_center_y, options.metadata_text_height,
						                      line_height_factor, options.metadata_color);
					}
				}
			}
		}

		[[nodiscard]] inline bool has_tree_model_children(const tree_draw_node& node) noexcept
		{
			return std::any_of(node.children.begin(), node.children.end(),
			                   [](std::size_t child) { return child != tree_draw_model::no_node; });
		}

		[[nodiscard]] inline tree_layout build_tree_layout(const tree_draw_model& model,
		                                                   const tree_draw_options& options)
		{
			validate_tree_draw_options(options);
			tree_layout result;
			const std::size_t count = model.nodes.size();
			if (count == 0)
			{
				if (model.root != tree_draw_model::no_node)
				{
					throw std::invalid_argument("An empty tree model must not specify a root");
				}
				return result;
			}
			if (model.root >= count)
			{
				throw std::invalid_argument("Tree model root is outside the node array");
			}

			result.nodes.resize(count);
			std::vector<double> subtree_widths(count);
			std::vector<std::size_t> depths(count, 0);
			std::vector<unsigned char> state(count, 0);
			std::vector<std::size_t> postorder;
			postorder.reserve(count);
			for (std::size_t index = 0; index < count; ++index)
			{
				const auto& node = model.nodes[index];
				if (!std::isfinite(node.width) || node.width < 0.0f)
				{
					throw std::invalid_argument("Tree model node width must be finite and non-negative");
				}
				if (!node.key_metadata.empty() && node.key_metadata.size() != node.keys.size())
				{
					throw std::invalid_argument("Tree model key metadata must match its key count");
				}
				if (!node.cell_widths.empty() && node.cell_widths.size() != node.keys.size())
				{
					throw std::invalid_argument("Tree model cell widths must match its key count");
				}
				switch (node.style)
				{
				case tree_node_style::normal:
				case tree_node_style::red:
				case tree_node_style::black:
				case tree_node_style::b_plus_leaf:
				case tree_node_style::b_plus_internal:
					break;
				default:
					throw std::invalid_argument("Tree model contains an unknown node style");
				}
				if (node.next_leaf != tree_draw_model::no_node && node.next_leaf >= count)
				{
					throw std::invalid_argument("Tree model leaf link is outside the node array");
				}
				for (const std::size_t child : node.children)
				{
					if (child != tree_draw_model::no_node && child >= count)
					{
						throw std::invalid_argument("Tree model child is outside the node array");
					}
				}

				double node_width = 0.0;
				auto& layout_node = result.nodes[index];
				if (!node.cell_widths.empty())
				{
					for (const float cell_width : node.cell_widths)
					{
						if (!std::isfinite(cell_width) || cell_width <= 0.0f)
						{
							throw std::invalid_argument("Tree model cell widths must be finite and positive");
						}
						node_width += cell_width;
					}
					if (node.width > 0.0f &&
					    std::abs(node_width - node.width) > std::max(0.001, node_width * 0.00001))
					{
						throw std::invalid_argument("Tree model width must equal the sum of its cell widths");
					}
					layout_node.cell_widths = node.cell_widths;
				}
				else if (node.width > 0.0f)
				{
					node_width = node.width;
					if (!node.keys.empty())
					{
						const float cell_width = checked_tree_coordinate(node_width / node.keys.size());
						layout_node.cell_widths.assign(node.keys.size(), cell_width);
						const double assigned_width =
						    static_cast<double>(cell_width) * static_cast<double>(node.keys.size() - 1);
						layout_node.cell_widths.back() = checked_tree_coordinate(node_width - assigned_width);
					}
				}
				else
				{
					node_width =
					    static_cast<double>(std::max<std::size_t>(1, node.keys.size())) * options.node_size.x;
					if (!node.keys.empty())
					{
						layout_node.cell_widths.assign(node.keys.size(), options.node_size.x);
					}
				}
				if (!std::isfinite(node_width) || node_width <= 2.0 * options.border_width)
				{
					throw std::invalid_argument("Tree model node width is too small for its border");
				}
				layout_node.size = {checked_tree_coordinate(node_width), options.node_size.y};
			}

			struct traversal
			{
				std::size_t index;
				bool expanded;
			};
			std::vector<traversal> pending;
			state[model.root] = 1;
			pending.push_back({model.root, false});
			std::size_t maximum_depth = 0;
			while (!pending.empty())
			{
				const traversal current = pending.back();
				pending.pop_back();
				const auto& node = model.nodes[current.index];
				if (current.expanded)
				{
					state[current.index] = 2;
					postorder.push_back(current.index);
					continue;
				}

				pending.push_back({current.index, true});
				for (std::size_t i = node.children.size(); i-- > 0;)
				{
					const std::size_t child = node.children[i];
					if (child == tree_draw_model::no_node)
					{
						continue;
					}
					if (state[child] != 0)
					{
						throw std::invalid_argument("Tree model contains a cycle or shared child");
					}
					if (depths[current.index] == std::numeric_limits<std::size_t>::max())
					{
						throw std::length_error("Tree depth exceeds the supported range");
					}

					state[child] = 1;
					depths[child] = depths[current.index] + 1;
					maximum_depth = std::max(maximum_depth, depths[child]);
					pending.push_back({child, false});
				}
			}
			if (std::any_of(state.begin(), state.end(), [](unsigned char value) { return value == 0; }))
			{
				throw std::invalid_argument("Tree model contains unreachable nodes");
			}
			for (std::size_t index = 0; index < count; ++index)
			{
				const std::size_t next = model.nodes[index].next_leaf;
				if (next != tree_draw_model::no_node &&
				    (next == index || has_tree_model_children(model.nodes[index]) ||
				     has_tree_model_children(model.nodes[next])))
				{
					throw std::invalid_argument("Tree model leaf links must connect distinct leaf nodes");
				}
			}

			for (const std::size_t index : postorder)
			{
				const auto& node = model.nodes[index];
				double children_width = 0.0;
				for (std::size_t i = 0; i < node.children.size(); ++i)
				{
					if (i != 0)
					{
						children_width += options.horizontal_gap;
					}
					const std::size_t child = node.children[i];
					children_width +=
					    child == tree_draw_model::no_node ? options.node_size.x : subtree_widths[child];
				}
				subtree_widths[index] =
				    std::max(static_cast<double>(result.nodes[index].size.x), children_width);
				if (!std::isfinite(subtree_widths[index]))
				{
					throw std::length_error("Tree layout width exceeds the supported range");
				}
			}

			const double node_height = options.node_size.y;
			const double vertical_step = node_height + static_cast<double>(options.vertical_gap);
			const double height = node_height + static_cast<double>(maximum_depth) * vertical_step;
			result.size = {checked_tree_world_coordinate(subtree_widths[model.root]),
			               checked_tree_world_coordinate(height)};

			struct placement
			{
				std::size_t index;
				double left;
			};
			std::vector<placement> placements{{model.root, 0.0}};
			while (!placements.empty())
			{
				const placement current = placements.back();
				placements.pop_back();
				const auto& node = model.nodes[current.index];
				auto& layout_node = result.nodes[current.index];
				layout_node.center = {
				    checked_tree_world_coordinate(current.left + subtree_widths[current.index] * 0.5),
				    checked_tree_world_coordinate(height - node_height * 0.5 -
					                              static_cast<double>(depths[current.index]) *
					                                  vertical_step)};

				double children_width = 0.0;
				for (std::size_t i = 0; i < node.children.size(); ++i)
				{
					if (i != 0)
					{
						children_width += options.horizontal_gap;
					}
					const std::size_t child = node.children[i];
					children_width +=
					    child == tree_draw_model::no_node ? options.node_size.x : subtree_widths[child];
				}
				double child_right = current.left + (subtree_widths[current.index] + children_width) * 0.5;
				for (std::size_t i = node.children.size(); i-- > 0;)
				{
					const std::size_t child = node.children[i];
					const double child_width =
					    child == tree_draw_model::no_node ? options.node_size.x : subtree_widths[child];
					child_right -= child_width;
					if (child != tree_draw_model::no_node)
					{
						placements.push_back({child, child_right});
					}
					child_right -= options.horizontal_gap;
				}
			}
			return result;
		}

		inline void draw_tree_world_arrow(frame_2d& frame, glm::dvec2 start, glm::dvec2 end, float width,
		                                  float arrow_size, const color& tint,
		                                  const tree_draw_options& options)
		{
			const glm::dvec2 delta = end - start;
			const double length = std::hypot(delta.x, delta.y);
			if (!std::isfinite(length) || length <= 0.0f)
			{
				return;
			}
			const glm::dvec2 direction = delta / length;
			const glm::dvec2 perpendicular{-direction.y, direction.x};
			const double head_size = std::min(static_cast<double>(arrow_size), length * 0.45);
			const glm::dvec2 base = end - direction * head_size;
			draw_tree_world_segment(frame, start, end, width, tint, options);
			draw_tree_world_segment(frame, end, base + perpendicular * (head_size * 0.55), width, tint,
			                        options);
			draw_tree_world_segment(frame, end, base - perpendicular * (head_size * 0.55), width, tint,
			                        options);
		}

		inline void draw_tree_model_snapshot(frame_2d& frame, font& value_font, const tree_draw_model& model,
		                                     const tree_draw_area& area, const tree_draw_options& options,
		                                     const tree_layout* precomputed_layout = nullptr,
		                                     bool allow_parallel_recording = false)
		{
			validate_tree_draw_options(area, options);
			tree_layout generated_layout;
			if (precomputed_layout == nullptr)
			{
				generated_layout = build_tree_layout(model, options);
				precomputed_layout = &generated_layout;
			}
			const tree_layout& layout = *precomputed_layout;
			if (layout.nodes.size() != model.nodes.size())
			{
				throw std::logic_error("Tree layout does not match the drawing model");
			}
			if (model.nodes.empty())
			{
				return;
			}
			constexpr std::size_t parallel_recording_minimum_nodes = 512;
			constexpr std::size_t parallel_recording_initial_worker_count = 2;
			constexpr std::size_t parallel_recording_minimum_nodes_per_worker =
			    parallel_recording_minimum_nodes / parallel_recording_initial_worker_count;

			const glm::dvec2 offset = tree_layout_offset(layout, area);
			const auto center_at = [&layout, &offset](std::size_t index)
			{
				if (index >= layout.nodes.size())
				{
					throw std::logic_error("Tree visibility contains an invalid node index");
				}
				return layout.nodes[index].center + offset;
			};

			if (options.edge_width > 0.0f)
			{
				const auto draw_edge = [&](std::size_t parent, std::size_t child_slot, std::size_t child)
				{
					if (parent >= model.nodes.size() || child >= model.nodes.size() ||
					    child_slot >= model.nodes[parent].children.size() ||
					    model.nodes[parent].children[child_slot] != child)
					{
						throw std::logic_error("Tree visibility contains an invalid edge");
					}
					const auto& node = model.nodes[parent];
					const glm::dvec2 parent_center = center_at(parent);
					const glm::dvec2 child_center = center_at(child);
					const double node_left = parent_center.x - layout.nodes[parent].size.x * 0.5f;
					const float fraction =
					    node.children.size() > 1
					        ? static_cast<float>(child_slot) / static_cast<float>(node.children.size() - 1)
					        : 0.5f;
					draw_tree_world_segment(frame,
					                        {node_left + layout.nodes[parent].size.x * fraction,
					                         parent_center.y - options.node_size.y * 0.5f},
					                        {child_center.x, child_center.y + options.node_size.y * 0.5f},
					                        options.edge_width, options.edge_color, options);
				};

				if (options.visibility != nullptr)
				{
					for (const auto& edge : options.visibility->edges)
					{
						draw_edge(edge.parent, edge.child_slot, edge.child);
					}
				}
				else
				{
					for (std::size_t parent = 0; parent < model.nodes.size(); ++parent)
					{
						for (std::size_t child_slot = 0; child_slot < model.nodes[parent].children.size();
						     ++child_slot)
						{
							const std::size_t child = model.nodes[parent].children[child_slot];
							if (child != tree_draw_model::no_node)
							{
								draw_edge(parent, child_slot, child);
							}
						}
					}
				}

				if (options.show_leaf_links)
				{
					const auto draw_leaf_link = [&](std::size_t index)
					{
						if (index >= model.nodes.size())
						{
							throw std::logic_error("Tree visibility contains an invalid leaf link");
						}
						const std::size_t next = model.nodes[index].next_leaf;
						if (next == tree_draw_model::no_node)
						{
							throw std::logic_error("Tree visibility references an absent leaf link");
						}
						if (next >= model.nodes.size())
						{
							throw std::logic_error("Tree leaf link contains an invalid node index");
						}
						const glm::dvec2 source_center = center_at(index);
						const glm::dvec2 target_center = center_at(next);
						const double link_y =
						    source_center.y - options.node_size.y * 0.5f -
						    std::min(options.node_size.y * 0.35f,
							         std::max(options.node_size.y * 0.12f, options.horizontal_gap * 0.20f));
						glm::dvec2 start{source_center.x + layout.nodes[index].size.x * 0.5f, link_y};
						glm::dvec2 end{target_center.x - layout.nodes[next].size.x * 0.5f, link_y};
						if (end.x <= start.x)
						{
							start = {source_center.x, source_center.y - options.node_size.y * 0.5f};
							end = {target_center.x, target_center.y - options.node_size.y * 0.5f};
						}
						draw_tree_world_arrow(
						    frame, start, end, options.edge_width,
						    std::max(options.node_size.y * 0.08f, options.edge_width * 2.5f),
						    options.leaf_link_color, options);
					};

					if (options.visibility != nullptr)
					{
						for (const std::size_t index : options.visibility->leaf_link_sources)
						{
							draw_leaf_link(index);
						}
					}
					else
					{
						for (std::size_t index = 0; index < model.nodes.size(); ++index)
						{
							if (model.nodes[index].next_leaf != tree_draw_model::no_node)
							{
								draw_leaf_link(index);
							}
						}
					}
				}
			}

			float line_height_factor = 0.0f;
			if (options.show_node_text)
			{
				const float raster_height = value_font.raster_height_pixels();
				const float line_height = value_font.line_height();
				if (!std::isfinite(raster_height) || raster_height <= 0.0f || !std::isfinite(line_height) ||
				    line_height <= 0.0f)
				{
					throw std::logic_error("Tree drawing requires a valid font");
				}
				line_height_factor = line_height / raster_height;
				if (!std::isfinite(line_height_factor) || line_height_factor <= 0.0f)
				{
					throw std::logic_error("Tree drawing font metrics cannot be scaled");
				}
			}
			const auto annotation_for = [](const tree_draw_node& node, std::size_t key_index)
			{
				std::string annotation;
				if (key_index == 0)
				{
					annotation = node.annotation;
				}
				if (!node.key_metadata.empty() && !node.key_metadata[key_index].empty())
				{
					if (!annotation.empty())
					{
						annotation += " ";
					}
					annotation += node.key_metadata[key_index];
				}
				return annotation;
			};
			const auto draw_node = [&](frame_2d& draw_frame, std::size_t index, bool prepared_glyphs)
			{
				if (index >= model.nodes.size())
				{
					throw std::logic_error("Tree visibility contains an invalid node index");
				}
				const auto& node = model.nodes[index];
				const auto& layout_node = layout.nodes[index];
				const ::mngl::rectangle node_bounds{
				    tree_local_position(center_at(index), options.coordinate_origin) -
				        layout_node.size * 0.5f,
				    layout_node.size};
				const float node_radius =
				    std::min(options.corner_radius, std::min(layout_node.size.x, layout_node.size.y) * 0.5f);
				if (options.border_width > 0.0f)
				{
					rounded_rectangle border;
					border.bounds = node_bounds;
					border.radius = node_radius;
					const bool is_highlighted =
					    node.highlighted ||
					    std::find(options.highlighted_nodes.begin(), options.highlighted_nodes.end(),
						          index) != options.highlighted_nodes.end();
					border.tint = is_highlighted ? options.highlight_border : options.node_border;
					draw_frame.draw(border);
				}

				const glm::vec2 inset(options.border_width);
				const ::mngl::rectangle fill_bounds{node_bounds.pos + inset, node_bounds.size - inset * 2.0f};
				rounded_rectangle fill;
				fill.bounds = fill_bounds;
				fill.radius = std::max(0.0f, node_radius - options.border_width);
				switch (node.style)
				{
				case tree_node_style::red:
					fill.tint = options.red_node_fill;
					break;
				case tree_node_style::black:
					fill.tint = options.black_node_fill;
					break;
				case tree_node_style::b_plus_leaf:
					fill.tint = options.b_plus_leaf_fill;
					break;
				case tree_node_style::b_plus_internal:
					fill.tint = options.b_plus_internal_fill;
					break;
				default:
					fill.tint = options.node_fill;
					break;
				}
				draw_frame.draw(fill);

				float cell_left = node_bounds.pos.x;
				for (std::size_t i = 0; i < layout_node.cell_widths.size(); ++i)
				{
					if (i != 0 && options.edge_width > 0.0f)
					{
						const float divider_width = std::min(options.edge_width, layout_node.size.y * 0.1f);
						draw_frame.draw(rectangle_shape(::mngl::rectangle{cell_left - divider_width * 0.5f,
						                                                  fill_bounds.pos.y, divider_width,
						                                                  fill_bounds.size.y},
						                                options.edge_color, transform{}));
					}
					cell_left += layout_node.cell_widths[i];
				}

				if (!options.show_node_text)
				{
					return;
				}

				const float vertical_padding =
				    std::min(fill_bounds.size.y * 0.08f, layout_node.size.y * 0.1f);
				const float content_bottom = fill_bounds.pos.y + vertical_padding;
				const float content_height = fill_bounds.size.y - vertical_padding * 2.0f;
				cell_left = node_bounds.pos.x;
				for (std::size_t i = 0; i < node.keys.size(); ++i)
				{
					const float cell_width = layout_node.cell_widths[i];
					const float horizontal_padding = std::min(cell_width * 0.06f, layout_node.size.y * 0.12f);
					const std::string annotation = annotation_for(node, i);
					const bool has_annotation = !annotation.empty();
					const float row_height = has_annotation ? content_height * 0.5f : content_height;
					const ::mngl::rectangle cell_text_bounds{cell_left + horizontal_padding, content_bottom,
					                                         cell_width - horizontal_padding * 2.0f,
					                                         content_height};
					const ::mngl::rectangle value_bounds{
					    cell_text_bounds.pos.x, has_annotation ? content_bottom + row_height : content_bottom,
					    cell_text_bounds.size.x, row_height};
					draw_fitted_tree_text(draw_frame, value_font, node.keys[i], value_bounds,
					                      value_bounds.pos.y + value_bounds.size.y * 0.5f,
					                      options.value_text_height, line_height_factor, options.value_color,
					                      prepared_glyphs);
					if (has_annotation)
					{
						const ::mngl::rectangle metadata_bounds{cell_text_bounds.pos.x, content_bottom,
						                                        cell_text_bounds.size.x, row_height};
						draw_fitted_tree_text(draw_frame, value_font, annotation, metadata_bounds,
						                      metadata_bounds.pos.y + metadata_bounds.size.y * 0.5f,
						                      options.metadata_text_height, line_height_factor,
						                      options.metadata_color, prepared_glyphs);
					}
					cell_left += cell_width;
				}
			};
			const std::size_t visible_node_count =
			    options.visibility != nullptr ? options.visibility->nodes.size() : 0;
			const unsigned int hardware_concurrency =
			    allow_parallel_recording && visible_node_count >= parallel_recording_minimum_nodes
			        ? std::thread::hardware_concurrency()
			        : 0;
			if (hardware_concurrency >= parallel_recording_initial_worker_count)
			{
				const std::size_t maximum_worker_count = static_cast<std::size_t>(hardware_concurrency);
				std::size_t worker_count = parallel_recording_initial_worker_count;
				while (worker_count < maximum_worker_count)
				{
					const std::size_t next_worker_count =
					    worker_count > maximum_worker_count / 2 ? maximum_worker_count : worker_count * 2;
					if (visible_node_count / next_worker_count < parallel_recording_minimum_nodes_per_worker)
					{
						break;
					}
					worker_count = next_worker_count;
				}
				if (options.show_node_text)
				{
					for (const std::size_t index : options.visibility->nodes)
					{
						if (index >= model.nodes.size())
						{
							continue;
						}
						const auto& node = model.nodes[index];
						for (std::size_t key_index = 0; key_index < node.keys.size(); ++key_index)
						{
							if (!node.keys[key_index].empty())
							{
								frame.prepare(value_font, node.keys[key_index]);
							}
							const std::string annotation = annotation_for(node, key_index);
							if (!annotation.empty())
							{
								frame.prepare(value_font, annotation);
							}
						}
					}
				}

				const auto partition_range = [worker_count](std::size_t count, std::size_t worker_index)
				{
					const std::size_t base = count / worker_count;
					const std::size_t remainder = count % worker_count;
					const std::size_t begin = worker_index * base + std::min(worker_index, remainder);
					const std::size_t length = base + (worker_index < remainder ? 1 : 0);
					return std::pair{begin, begin + length};
				};
				struct worker_recording
				{
					recording_context nodes;
					std::size_t node_begin = 0;
					std::size_t node_end = 0;
				};
				std::vector<worker_recording> workers(worker_count);
				for (std::size_t worker_index = 0; worker_index < worker_count; ++worker_index)
				{
					auto& worker = workers[worker_index];
					const auto [node_begin, node_end] =
					    partition_range(options.visibility->nodes.size(), worker_index);
					worker.node_begin = node_begin;
					worker.node_end = node_end;
					if (node_begin != node_end)
					{
						worker.nodes = frame.create_recording_context(node_end - node_begin);
					}
				}

				std::vector<std::future<void>> jobs;
				jobs.reserve(worker_count);
				for (std::size_t worker_index = 0; worker_index < worker_count; ++worker_index)
				{
					jobs.push_back(std::async(
					    std::launch::async,
					    [&, worker_index]
					    {
						    auto& worker = workers[worker_index];
						    worker.nodes.draw(
						        [&](frame_2d& worker_frame)
						        {
							        for (std::size_t index = worker.node_begin; index < worker.node_end;
									     ++index)
							        {
								        draw_node(worker_frame, options.visibility->nodes[index], true);
							        }
						        });
						    worker.nodes.finish();
					    }));
				}
				for (auto& job : jobs)
				{
					job.get();
				}

				for (auto& worker : workers)
				{
					if (worker.nodes.valid())
					{
						frame.merge(std::move(worker.nodes));
					}
				}
				return;
			}
			if (options.visibility != nullptr)
			{
				for (const std::size_t index : options.visibility->nodes)
				{
					draw_node(frame, index, false);
				}
			}
			else
			{
				for (std::size_t index = 0; index < model.nodes.size(); ++index)
				{
					draw_node(frame, index, false);
				}
			}
		}
	} // namespace detail

	[[nodiscard]] inline tree_layout measure_tree_layout(const tree_draw_model& model,
	                                                     const tree_draw_options& options = {})
	{
		return detail::build_tree_layout(model, options);
	}

	// Double-precision placement for large worlds; set coordinate_origin to the
	// camera center and use a matching, centered frame_2d projection.
	inline void draw_tree_model_parallel(frame_2d& frame, font& value_font, const tree_draw_model& model,
	                                     const tree_layout& layout, const tree_draw_area& area,
	                                     const tree_draw_options& options = {})
	{
		detail::draw_tree_model_snapshot(frame, value_font, model, area, options, &layout, true);
	}

	class tree_draw_culler
	{
	public:
		tree_draw_culler() : m_worker([this] { worker_loop(); }) {}

		~tree_draw_culler()
		{
			{
				std::lock_guard lock(m_mutex);
				m_stopping = true;
			}
			m_work_available.notify_one();
			if (m_worker.joinable())
			{
				m_worker.join();
			}
		}

		tree_draw_culler(const tree_draw_culler&) = delete;
		tree_draw_culler& operator=(const tree_draw_culler&) = delete;

		[[nodiscard]] tree_draw_visibility cull(const tree_draw_model& model, const tree_layout& layout,
		                                        const tree_draw_area& area, const tree_draw_area& viewport,
		                                        const tree_draw_options& options)
		{
			std::lock_guard call_lock(m_call_mutex);
			detail::validate_tree_draw_options(area, options);
			detail::validate_tree_draw_area(viewport);
			if (layout.nodes.size() != model.nodes.size())
			{
				throw std::logic_error("Tree layout does not match the culling model");
			}
			if (model.nodes.empty())
			{
				return {};
			}

#if defined(_OPENMP)
			if (model.nodes.size() >= openmp_minimum_chunk_size * 2 && omp_get_max_threads() > 1)
			{
				{
					std::unique_lock lock(m_mutex);
					m_work_complete.wait(
					    lock, [this] { return m_stopping || (!m_worker_active && !m_pending.has_value()); });
					if (m_stopping)
					{
						throw std::logic_error("Tree culling worker is shutting down");
					}
				}

				tree_draw_visibility result;
				collect_visible_range(model, layout, area, viewport, options, 0, model.nodes.size(), result);
				return result;
			}
#endif

			const std::size_t middle = model.nodes.size() / 2;
			if (middle == 0)
			{
				tree_draw_visibility result;
				collect_visible_range(model, layout, area, viewport, options, 0, model.nodes.size(), result);
				return result;
			}

			tree_draw_visibility result;
			tree_draw_visibility worker_result;
			std::uint64_t request_id = 0;
			{
				std::unique_lock lock(m_mutex);
				m_work_complete.wait(lock, [this]
				                     { return m_stopping || (!m_worker_active && !m_pending.has_value()); });
				if (m_stopping)
				{
					throw std::logic_error("Tree culling worker is shutting down");
				}
				request_id = next_request_id_locked();
				m_pending = cull_request{request_id,    true,     {},      &model, &layout,
				                         area,          viewport, options, middle, model.nodes.size(),
				                         &worker_result};
			}
			m_work_available.notify_one();

			std::exception_ptr main_error;
			try
			{
				collect_visible_range(model, layout, area, viewport, options, 0, middle, result);
			}
			catch (...)
			{
				main_error = std::current_exception();
			}

			std::exception_ptr worker_error;
			{
				std::unique_lock lock(m_mutex);
				m_work_complete.wait(lock,
				                     [this, request_id] { return m_completed_request_id == request_id; });
				worker_error = m_worker_error;
				m_worker_error = {};
			}
			if (main_error)
			{
				std::rethrow_exception(main_error);
			}
			if (worker_error)
			{
				std::rethrow_exception(worker_error);
			}

			result.nodes.insert(result.nodes.end(), worker_result.nodes.begin(), worker_result.nodes.end());
			result.edges.insert(result.edges.end(), worker_result.edges.begin(), worker_result.edges.end());
			result.leaf_link_sources.insert(result.leaf_link_sources.end(),
			                                worker_result.leaf_link_sources.begin(),
			                                worker_result.leaf_link_sources.end());
			return result;
		}

		template <class Snapshot>
		[[nodiscard]] std::uint64_t request_async(const std::shared_ptr<const Snapshot>& source,
		                                          const tree_draw_model& model, const tree_layout& layout,
		                                          const tree_draw_area& area, const tree_draw_area& viewport,
		                                          const tree_draw_options& options)
		{
			if (!source)
			{
				throw std::invalid_argument("Asynchronous tree culling requires a retained snapshot");
			}
			detail::validate_tree_draw_options(area, options);
			detail::validate_tree_draw_area(viewport);
			if (layout.nodes.size() != model.nodes.size())
			{
				throw std::logic_error("Tree layout does not match the culling model");
			}

			std::lock_guard call_lock(m_call_mutex);
			std::uint64_t request_id = 0;
			{
				std::lock_guard lock(m_mutex);
				if (m_stopping)
				{
					throw std::logic_error("Tree culling worker is shutting down");
				}
				request_id = next_request_id_locked();
				m_pending = cull_request{request_id,
				                         false,
				                         std::shared_ptr<const void>(source),
				                         &model,
				                         &layout,
				                         area,
				                         viewport,
				                         options,
				                         0,
				                         model.nodes.size(),
				                         nullptr};
			}
			m_work_available.notify_one();
			return request_id;
		}

		[[nodiscard]] std::optional<tree_draw_cull_result> take_completed_result()
		{
			std::lock_guard lock(m_mutex);
			if (!m_async_result.has_value())
			{
				return std::nullopt;
			}
			auto result = std::move(m_async_result);
			m_async_result.reset();
			return result;
		}

	private:
		static constexpr std::size_t openmp_minimum_chunk_size = 16'384;

		struct cull_request
		{
			std::uint64_t id = 0;
			bool synchronous = false;
			std::shared_ptr<const void> source;
			const tree_draw_model* model = nullptr;
			const tree_layout* layout = nullptr;
			tree_draw_area area;
			tree_draw_area viewport;
			tree_draw_options options;
			std::size_t begin = 0;
			std::size_t end = 0;
			tree_draw_visibility* output = nullptr;
		};

		[[nodiscard]] static bool intersects_view(glm::dvec2 first, glm::dvec2 second,
		                                          const tree_draw_area& viewport, float padding) noexcept
		{
			const double left = viewport.pos.x;
			const double bottom = viewport.pos.y;
			const double right = left + viewport.size.x;
			const double top = bottom + viewport.size.y;
			return std::max(first.x, second.x) + padding >= left &&
			       std::min(first.x, second.x) - padding <= right &&
			       std::max(first.y, second.y) + padding >= bottom &&
			       std::min(first.y, second.y) - padding <= top;
		}

		static void collect_visible_range(const tree_draw_model& model, const tree_layout& layout,
		                                  const tree_draw_area& area, const tree_draw_area& viewport,
		                                  const tree_draw_options& options, std::size_t begin,
		                                  std::size_t end, tree_draw_visibility& result)
		{
			const glm::dvec2 offset = detail::tree_layout_offset(layout, area);
			const auto center_at = [&layout, &offset](std::size_t index)
			{
				if (index >= layout.nodes.size())
				{
					throw std::logic_error("Tree visibility contains an invalid node index");
				}
				return layout.nodes[index].center + offset;
			};

			const std::size_t count = model.nodes.size();
			const float node_padding = options.border_width * 0.5f;
			const float edge_padding = options.edge_width * 0.5f;
			const float leaf_link_y_offset =
			    std::min(options.node_size.y * 0.35f,
				         std::max(options.node_size.y * 0.12f, options.horizontal_gap * 0.20f));
			const float arrow_size = std::max(options.node_size.y * 0.08f, options.edge_width * 2.5f);

			const auto collect_node = [&](std::size_t index, tree_draw_visibility& output)
			{
				const auto& node = model.nodes[index];
				const glm::dvec2 center = center_at(index);
				const glm::dvec2 size = layout.nodes[index].size;
				if (intersects_view(center - size * 0.5, center + size * 0.5, viewport, node_padding))
				{
					output.nodes.push_back(index);
				}

				if (options.edge_width <= 0.0f)
				{
					return;
				}
				const double node_left = center.x - size.x * 0.5;
				for (std::size_t child_slot = 0; child_slot < node.children.size(); ++child_slot)
				{
					const std::size_t child = node.children[child_slot];
					if (child == tree_draw_model::no_node)
					{
						continue;
					}
					if (child >= count)
					{
						throw std::logic_error("Tree culling encountered an invalid child index");
					}
					const float fraction =
					    node.children.size() > 1
					        ? static_cast<float>(child_slot) / static_cast<float>(node.children.size() - 1)
					        : 0.5f;
					const glm::dvec2 start{node_left + size.x * fraction,
					                       center.y - options.node_size.y * 0.5};
					const glm::dvec2 child_center = center_at(child);
					const glm::dvec2 finish{child_center.x, child_center.y + options.node_size.y * 0.5};
					if (intersects_view(start, finish, viewport, edge_padding))
					{
						output.edges.push_back({index, child_slot, child});
					}
				}

				if (!options.show_leaf_links || node.next_leaf == tree_draw_model::no_node)
				{
					return;
				}
				const std::size_t next = node.next_leaf;
				if (next >= count)
				{
					throw std::logic_error("Tree culling encountered an invalid leaf link");
				}
				const glm::dvec2 next_center = center_at(next);
				const double link_y = center.y - options.node_size.y * 0.5 - leaf_link_y_offset;
				glm::dvec2 start{center.x + size.x * 0.5, link_y};
				glm::dvec2 finish{next_center.x - layout.nodes[next].size.x * 0.5, link_y};
				if (finish.x <= start.x)
				{
					start = {center.x, center.y - options.node_size.y * 0.5};
					finish = {next_center.x, next_center.y - options.node_size.y * 0.5};
				}
				if (intersects_view(start, finish, viewport, edge_padding + arrow_size))
				{
					output.leaf_link_sources.push_back(index);
				}
			};

#if defined(_OPENMP)
			const std::size_t range_size = end - begin;
			const int maximum_threads = omp_get_max_threads();
			if (range_size >= openmp_minimum_chunk_size * 2 && maximum_threads > 1 &&
			    end <= static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
			{
				const std::size_t requested_threads = 1 + (range_size - 1) / openmp_minimum_chunk_size;
				const int thread_count =
				    static_cast<int>(std::min(static_cast<std::size_t>(maximum_threads), requested_threads));
				std::vector<tree_draw_visibility> thread_results(static_cast<std::size_t>(thread_count));
				std::vector<std::exception_ptr> thread_errors(static_cast<std::size_t>(thread_count));

#pragma omp parallel num_threads(thread_count)
				{
					const int thread_index = omp_get_thread_num();
					auto& thread_error = thread_errors[static_cast<std::size_t>(thread_index)];
					auto& thread_result = thread_results[static_cast<std::size_t>(thread_index)];

#pragma omp for schedule(static)
					for (std::int64_t raw_index = static_cast<std::int64_t>(begin);
					     raw_index < static_cast<std::int64_t>(end); ++raw_index)
					{
						if (thread_error)
						{
							continue;
						}
						try
						{
							collect_node(static_cast<std::size_t>(raw_index), thread_result);
						}
						catch (...)
						{
							thread_error = std::current_exception();
						}
					}
				}

				for (const auto& thread_error : thread_errors)
				{
					if (thread_error)
					{
						std::rethrow_exception(thread_error);
					}
				}

				std::size_t total_nodes = result.nodes.size();
				std::size_t total_edges = result.edges.size();
				std::size_t total_leaf_links = result.leaf_link_sources.size();
				for (const auto& thread_result : thread_results)
				{
					if (thread_result.nodes.size() > result.nodes.max_size() - total_nodes ||
					    thread_result.edges.size() > result.edges.max_size() - total_edges ||
					    thread_result.leaf_link_sources.size() >
					        result.leaf_link_sources.max_size() - total_leaf_links)
					{
						throw std::length_error("Tree visibility exceeds the supported range");
					}
					total_nodes += thread_result.nodes.size();
					total_edges += thread_result.edges.size();
					total_leaf_links += thread_result.leaf_link_sources.size();
				}
				result.nodes.reserve(total_nodes);
				result.edges.reserve(total_edges);
				result.leaf_link_sources.reserve(total_leaf_links);
				for (auto& thread_result : thread_results)
				{
					result.nodes.insert(result.nodes.end(), thread_result.nodes.begin(),
					                    thread_result.nodes.end());
					result.edges.insert(result.edges.end(), thread_result.edges.begin(),
					                    thread_result.edges.end());
					result.leaf_link_sources.insert(result.leaf_link_sources.end(),
					                                thread_result.leaf_link_sources.begin(),
					                                thread_result.leaf_link_sources.end());
				}
				return;
			}
#endif

			for (std::size_t index = begin; index < end; ++index)
			{
				collect_node(index, result);
			}
		}

		void worker_loop() noexcept
		{
			for (;;)
			{
				cull_request request;
				{
					std::unique_lock lock(m_mutex);
					m_work_available.wait(lock, [this] { return m_stopping || m_pending.has_value(); });
					if (m_stopping)
					{
						return;
					}
					request = std::move(*m_pending);
					m_pending.reset();
					m_worker_active = true;
				}

				tree_draw_visibility async_result;
				tree_draw_visibility& output = request.synchronous ? *request.output : async_result;
				std::exception_ptr worker_error;
				try
				{
					collect_visible_range(*request.model, *request.layout, request.area, request.viewport,
					                      request.options, request.begin, request.end, output);
				}
				catch (...)
				{
					worker_error = std::current_exception();
				}
				{
					std::lock_guard lock(m_mutex);
					m_worker_active = false;
					if (request.synchronous)
					{
						m_worker_error = worker_error;
						m_completed_request_id = request.id;
					}
					else
					{
						tree_draw_cull_result completed;
						completed.request_id = request.id;
						completed.source = std::move(request.source);
						completed.visibility = std::move(async_result);
						completed.error = worker_error;
						m_async_result = std::move(completed);
					}
				}
				m_work_complete.notify_all();
			}
		}

		[[nodiscard]] std::uint64_t next_request_id_locked() noexcept
		{
			if (m_next_request_id == std::numeric_limits<std::uint64_t>::max())
			{
				m_next_request_id = 0;
			}
			return ++m_next_request_id;
		}

		std::mutex m_call_mutex;
		std::mutex m_mutex;
		std::condition_variable m_work_available;
		std::condition_variable m_work_complete;
		std::optional<cull_request> m_pending;
		std::optional<tree_draw_cull_result> m_async_result;
		std::uint64_t m_next_request_id = 0;
		std::uint64_t m_completed_request_id = 0;
		std::exception_ptr m_worker_error;
		bool m_worker_active = false;
		bool m_stopping = false;
		std::thread m_worker;
	};

	class tree_model_display_adapter final : public tree_drawable
	{
	public:
		// The model and optional layout are borrowed and must outlive the adapter.
		explicit tree_model_display_adapter(const tree_draw_model& model) noexcept
		    : m_model(&model), m_layout(nullptr)
		{
		}

		tree_model_display_adapter(const tree_draw_model& model, const tree_layout& layout) noexcept
		    : m_model(&model), m_layout(&layout)
		{
		}

		void draw(frame_2d& frame, font& value_font, const ::mngl::rectangle& area,
		          const tree_draw_options& options = {}) const override
		{
			detail::draw_tree_model_snapshot(frame, value_font, *m_model, area, options, m_layout);
		}

		void draw_parallel(frame_2d& frame, font& value_font, const ::mngl::rectangle& area,
		                   const tree_draw_options& options = {}) const override
		{
			detail::draw_tree_model_snapshot(frame, value_font, *m_model, area, options, m_layout, true);
		}

	private:
		const tree_draw_model* m_model;
		const tree_layout* m_layout;
	};

	[[nodiscard]] inline std::unique_ptr<tree_drawable> make_tree_drawable(const tree_draw_model& model)
	{
		return std::make_unique<tree_model_display_adapter>(model);
	}

	[[nodiscard]] inline std::unique_ptr<tree_drawable> make_tree_drawable(const tree_draw_model& model,
	                                                                       const tree_layout& layout)
	{
		return std::make_unique<tree_model_display_adapter>(model, layout);
	}

	template <detail::displayable_binary_search_tree Tree>
	class binary_tree_display_adapter : public binary_tree_drawable
	{
	public:
		// The tree is borrowed and must outlive this display adapter.
		explicit binary_tree_display_adapter(const Tree& tree) noexcept : m_tree(&tree) {}

		void draw(frame_2d& frame, font& value_font, const ::mngl::rectangle& area,
		          const binary_tree_draw_options& options = {}) const override
		{
			detail::draw_tree_snapshot(frame, value_font, detail::capture_binary_tree(*m_tree), area,
			                           options);
		}

	protected:
		[[nodiscard]] const Tree& source_tree() const noexcept { return *m_tree; }

	private:
		const Tree* m_tree;
	};

	template <detail::displayable_b_plus_tree Tree> class b_plus_tree_display_adapter : public tree_drawable
	{
	public:
		// The tree is borrowed and must outlive this display adapter.
		explicit b_plus_tree_display_adapter(const Tree& tree) noexcept : m_tree(&tree) {}

		void draw(frame_2d& frame, font& value_font, const ::mngl::rectangle& area,
		          const tree_draw_options& options = {}) const override
		{
			detail::draw_tree_snapshot(frame, value_font, detail::capture_b_plus_tree(*m_tree), area,
			                           options);
		}

	protected:
		[[nodiscard]] const Tree& source_tree() const noexcept { return *m_tree; }

	private:
		const Tree* m_tree;
	};

	template <detail::displayable_binary_search_tree Tree>
	[[nodiscard]] std::unique_ptr<binary_tree_drawable> make_binary_tree_drawable(const Tree& tree)
	{
		return std::make_unique<binary_tree_display_adapter<Tree>>(tree);
	}

	template <detail::displayable_b_plus_tree Tree>
	[[nodiscard]] std::unique_ptr<tree_drawable> make_b_plus_tree_drawable(const Tree& tree)
	{
		return std::make_unique<b_plus_tree_display_adapter<Tree>>(tree);
	}

	template <detail::displayable_binary_search_tree Tree>
	[[nodiscard]] std::unique_ptr<tree_drawable> make_tree_drawable(const Tree& tree)
	{
		return make_binary_tree_drawable(tree);
	}

	template <detail::displayable_b_plus_tree Tree>
	[[nodiscard]] std::unique_ptr<tree_drawable> make_tree_drawable(const Tree& tree)
	{
		return make_b_plus_tree_drawable(tree);
	}
} // namespace mngl::two_d
