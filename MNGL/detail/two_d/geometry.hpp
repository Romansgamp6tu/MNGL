#pragma once

#include "types.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

namespace mngl::two_d::detail
{
	using triangle_vertices = std::vector<glm::vec2>;

	struct tessellation_scratch
	{
		triangle_vertices triangles;
		std::vector<glm::vec2> points;
		std::vector<glm::vec2> normals;
		std::vector<glm::vec2> sides;
	};

	[[nodiscard]] inline bool finite(glm::vec2 value) noexcept
	{
		return std::isfinite(value.x) && std::isfinite(value.y);
	}

	inline void append_triangle(triangle_vertices& output, glm::vec2 first, glm::vec2 second, glm::vec2 third)
	{
		output.push_back(first);
		output.push_back(second);
		output.push_back(third);
	}

	inline void append_quad(triangle_vertices& output, glm::vec2 first, glm::vec2 second, glm::vec2 third,
	                        glm::vec2 fourth)
	{
		append_triangle(output, first, second, third);
		append_triangle(output, third, second, fourth);
	}

	inline void validate_points(std::span<const glm::vec2> points, const char* message)
	{
		if (std::ranges::any_of(points, [](glm::vec2 point) { return !finite(point); })) [[unlikely]]
		{
			throw std::invalid_argument(message);
		}
	}

	inline void append_segment(triangle_vertices& output, glm::vec2 first, glm::vec2 second, float thickness)
	{
		const glm::vec2 delta = second - first;
		const float length = glm::length(delta);
		if (length == 0.0f)
		{
			return;
		}

		const glm::vec2 side{-delta.y / length * thickness * 0.5f, delta.x / length * thickness * 0.5f};
		append_quad(output, first + side, first - side, second + side, second - side);
	}

	inline void append_polyline(triangle_vertices& output, std::span<const glm::vec2> source, float thickness,
	                            bool closed, tessellation_scratch& scratch)
	{
		if (!std::isfinite(thickness) || thickness < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Geometry line width must be finite and non-negative");
		}
		if (thickness == 0.0f || source.size() < 2)
		{
			return;
		}

		auto& points = scratch.points;
		points.clear();
		points.reserve(source.size());
		for (const auto point : source)
		{
			if (points.empty() || points.back().x != point.x || points.back().y != point.y)
			{
				points.push_back(point);
			}
		}
		if (closed && points.size() > 1 && points.front().x == points.back().x &&
		    points.front().y == points.back().y)
		{
			points.pop_back();
		}
		if (points.size() < 2)
		{
			return;
		}

		const bool loop = closed && points.size() > 2;
		const std::size_t segment_count = loop ? points.size() : points.size() - 1;
		auto& normals = scratch.normals;
		normals.resize(segment_count);
		for (std::size_t segment = 0; segment < segment_count; ++segment)
		{
			const auto delta = points[(segment + 1) % points.size()] - points[segment];
			const float length = glm::length(delta);
			normals[segment] = {-delta.y / length, delta.x / length};
		}

		auto& sides = scratch.sides;
		sides.resize(points.size());
		const float half_width = thickness * 0.5f;
		for (std::size_t point = 0; point < points.size(); ++point)
		{
			const bool has_previous = loop || point > 0;
			const bool has_next = loop || point + 1 < points.size();
			if (!has_previous)
			{
				sides[point] = normals.front() * half_width;
				continue;
			}
			if (!has_next)
			{
				sides[point] = normals.back() * half_width;
				continue;
			}

			const auto previous_normal = normals[(point + segment_count - 1) % segment_count];
			const auto next_normal = normals[point % segment_count];
			const auto sum = previous_normal + next_normal;
			const float sum_length = glm::length(sum);
			if (sum_length < 1e-5f)
			{
				sides[point] = next_normal * half_width;
				continue;
			}

			const auto miter = sum / sum_length;
			const float denominator = glm::dot(miter, next_normal);
			if (std::abs(denominator) < 0.25f)
			{
				sides[point] = next_normal * half_width;
				continue;
			}
			const float miter_length =
			    std::clamp(half_width / denominator, -half_width * 4.0f, half_width * 4.0f);
			sides[point] = miter * miter_length;
		}

		for (std::size_t segment = 0; segment < segment_count; ++segment)
		{
			const auto next = (segment + 1) % points.size();
			append_quad(output, points[segment] + sides[segment], points[segment] - sides[segment],
			            points[next] + sides[next], points[next] - sides[next]);
		}
	}

	[[nodiscard]] inline std::span<const glm::vec2> tessellate(const vertex_array& value,
	                                                           tessellation_scratch& scratch)
	{
		if (value.primitive != primitive_type::triangles)
		{
			validate_points(value.vertices, "Geometry vertices must be finite");
		}
		auto& output = scratch.triangles;
		output.clear();
		const auto vertices = std::span<const glm::vec2>(value.vertices);

		switch (value.primitive)
		{
		case primitive_type::points:
			if (!std::isfinite(value.point_size) || value.point_size < 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("Geometry point size must be finite and non-negative");
			}
			if (value.point_size == 0.0f)
			{
				return output;
			}
			for (const auto point : vertices)
			{
				const glm::vec2 half_size(value.point_size * 0.5f);
				append_quad(output, point + glm::vec2(-half_size.x, -half_size.y),
				            point + glm::vec2(half_size.x, -half_size.y),
				            point + glm::vec2(-half_size.x, half_size.y),
				            point + glm::vec2(half_size.x, half_size.y));
			}
			break;

		case primitive_type::lines:
			if (!std::isfinite(value.line_width) || value.line_width < 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("Geometry line width must be finite and non-negative");
			}
			if (vertices.size() % 2 != 0) [[unlikely]]
			{
				throw std::invalid_argument("A line-list vertex array requires an even number of vertices");
			}
			if (value.line_width == 0.0f)
			{
				return output;
			}
			for (std::size_t vertex = 0; vertex < vertices.size(); vertex += 2)
			{
				append_segment(output, vertices[vertex], vertices[vertex + 1], value.line_width);
			}
			break;

		case primitive_type::line_strip:
			append_polyline(output, vertices, value.line_width, value.closed, scratch);
			break;

		case primitive_type::triangles:
			if (vertices.size() % 3 != 0) [[unlikely]]
			{
				throw std::invalid_argument(
				    "A triangle-list vertex array requires a multiple of three vertices");
			}
			// The batcher validates and transforms these directly into its staging buffer.
			return vertices;

		case primitive_type::triangle_strip:
			if (vertices.size() < 3)
			{
				return output;
			}
			output.reserve((vertices.size() - 2) * 3);
			for (std::size_t vertex = 2; vertex < vertices.size(); ++vertex)
			{
				if (vertex % 2 == 0)
				{
					append_triangle(output, vertices[vertex - 2], vertices[vertex - 1], vertices[vertex]);
				}
				else
				{
					append_triangle(output, vertices[vertex - 1], vertices[vertex - 2], vertices[vertex]);
				}
			}
			break;

		case primitive_type::triangle_fan:
			if (vertices.size() < 3)
			{
				return output;
			}
			output.reserve((vertices.size() - 2) * 3);
			for (std::size_t vertex = 2; vertex < vertices.size(); ++vertex)
			{
				append_triangle(output, vertices.front(), vertices[vertex - 1], vertices[vertex]);
			}
			break;

		default:
			throw std::invalid_argument("Unsupported 2D geometry primitive type");
		}
		return output;
	}

	inline void tessellate(const convex_polygon& value, tessellation_scratch& scratch)
	{
		auto& output = scratch.triangles;
		output.clear();
		const auto points = std::span<const glm::vec2>(value.points);
		validate_points(points, "Polygon points must be finite");
		if (!std::isfinite(value.outline_thickness) || value.outline_thickness < 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Polygon outline thickness must be finite and non-negative");
		}
		if (points.empty())
		{
			return;
		}
		if (points.size() < 3) [[unlikely]]
		{
			throw std::invalid_argument("A convex polygon requires at least three points");
		}

		int winding = 0;
		std::int64_t revolutions = 0;
		glm::dvec2 first_direction{};
		glm::dvec2 previous_direction{};
		bool has_direction = false;
		auto angular_half = [](glm::dvec2 direction) noexcept
		{
			return direction.y > 0.0 || (direction.y == 0.0 && direction.x >= 0.0) ? 0 : 1;
		};
		// Consistent turn signs plus crossings of +X detect multi-winding contours without atan2.
		auto validate_turn = [&](glm::dvec2 first, glm::dvec2 second)
		{
			const double cross = first.x * second.y - first.y * second.x;
			const double dot = first.x * second.x + first.y * second.y;
			const double relative_tolerance = std::numeric_limits<double>::epsilon() * 16.0;
			const double cross_tolerance_squared =
			    relative_tolerance * relative_tolerance *
			    (first.x * first.x + first.y * first.y) * (second.x * second.x + second.y * second.y);
			if (cross * cross <= cross_tolerance_squared)
			{
				if (dot < 0.0) [[unlikely]]
				{
					throw std::invalid_argument("Polygon points must describe a convex contour in order");
				}
			}
			else
			{
				const int current_winding = cross > 0.0 ? 1 : -1;
				if (winding != 0 && winding != current_winding) [[unlikely]]
				{
					throw std::invalid_argument("Polygon points must describe a convex contour in order");
				}
				winding = current_winding;
			}

			if (cross > 0.0 && angular_half(first) == 1 && angular_half(second) == 0)
			{
				++revolutions;
			}
			else if (cross < 0.0 && angular_half(first) == 0 && angular_half(second) == 1)
			{
				--revolutions;
			}
		};
		for (std::size_t point = 0; point < points.size(); ++point)
		{
			const auto start = points[point];
			const auto end = points[(point + 1) % points.size()];
			const glm::dvec2 direction{static_cast<double>(end.x) - start.x,
			                           static_cast<double>(end.y) - start.y};
			if (direction.x == 0.0 && direction.y == 0.0)
			{
				continue;
			}
			if (!has_direction)
			{
				first_direction = direction;
				previous_direction = direction;
				has_direction = true;
				continue;
			}
			validate_turn(previous_direction, direction);
			previous_direction = direction;
		}
		if (has_direction)
		{
			validate_turn(previous_direction, first_direction);
		}
		if (winding == 0) [[unlikely]]
		{
			throw std::invalid_argument("A convex polygon must enclose a non-zero area");
		}
		if (revolutions != winding) [[unlikely]]
		{
			throw std::invalid_argument("Polygon points must describe a convex contour in order");
		}

		output.reserve((points.size() - 2) * 3);
		for (std::size_t point = 2; point < points.size(); ++point)
		{
			append_triangle(output, points.front(), points[point - 1], points[point]);
		}
	}

	inline void tessellate_outline(const convex_polygon& value, tessellation_scratch& scratch)
	{
		scratch.triangles.clear();
		append_polyline(scratch.triangles, value.points, value.outline_thickness, true, scratch);
	}
} // namespace mngl::two_d::detail
