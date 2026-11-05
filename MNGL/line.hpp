#pragma once
#include "utilities.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

namespace mngl
{
	namespace line_detail
	{
		inline double cross(double ax, double ay, double bx, double by)
		{
			return ax * by - ay * bx;
		}

		inline bool point_on_segment(glm::vec2 point, glm::vec2 start, glm::vec2 end, double epsilon)
		{
			const double dx = static_cast<double>(end.x) - start.x;
			const double dy = static_cast<double>(end.y) - start.y;
			const double px = static_cast<double>(point.x) - start.x;
			const double py = static_cast<double>(point.y) - start.y;
			const double length_squared = dx * dx + dy * dy;
			const double length = std::sqrt(length_squared);

			if (length <= epsilon)
				return px * px + py * py <= epsilon * epsilon;

			if (std::abs(cross(dx, dy, px, py)) > epsilon * length)
				return false;

			const double projection = px * dx + py * dy;
			return projection >= -epsilon * length && projection <= length_squared + epsilon * length;
		}

		inline std::optional<glm::vec2> segment_intersection(glm::vec2 p1, glm::vec2 p2, glm::vec2 p3,
		                                                     glm::vec2 p4, float epsilon)
		{
			const double tolerance = std::max(0.0, static_cast<double>(epsilon));
			const double rx = static_cast<double>(p2.x) - p1.x;
			const double ry = static_cast<double>(p2.y) - p1.y;
			const double sx = static_cast<double>(p4.x) - p3.x;
			const double sy = static_cast<double>(p4.y) - p3.y;
			const double qx = static_cast<double>(p3.x) - p1.x;
			const double qy = static_cast<double>(p3.y) - p1.y;
			const double r_length_squared = rx * rx + ry * ry;
			const double s_length_squared = sx * sx + sy * sy;
			const double r_length = std::sqrt(r_length_squared);
			const double s_length = std::sqrt(s_length_squared);

			if (r_length <= tolerance)
			{
				if (s_length <= tolerance)
				{
					const double dx = static_cast<double>(p3.x) - p1.x;
					const double dy = static_cast<double>(p3.y) - p1.y;
					if (dx * dx + dy * dy <= tolerance * tolerance)
						return p1;
					return std::nullopt;
				}
				return point_on_segment(p1, p3, p4, tolerance) ? std::optional<glm::vec2>(p1) : std::nullopt;
			}

			if (s_length <= tolerance)
			{
				return point_on_segment(p3, p1, p2, tolerance) ? std::optional<glm::vec2>(p3) : std::nullopt;
			}

			const double denominator = cross(rx, ry, sx, sy);
			const double parallel_tolerance = tolerance * r_length * s_length;
			const double t_tolerance = tolerance / r_length;

			if (std::abs(denominator) <= parallel_tolerance)
			{
				if (std::abs(cross(qx, qy, rx, ry)) > tolerance * r_length)
					return std::nullopt;

				// For an overlap, return the first shared point along p1-p2.
				const double t0 = (qx * rx + qy * ry) / r_length_squared;
				const double p4x = static_cast<double>(p4.x) - p1.x;
				const double p4y = static_cast<double>(p4.y) - p1.y;
				const double t1 = (p4x * rx + p4y * ry) / r_length_squared;
				const double overlap_start = std::max(0.0, std::min(t0, t1));
				const double overlap_end = std::min(1.0, std::max(t0, t1));
				if (overlap_start > overlap_end + t_tolerance)
					return std::nullopt;

				const double t = std::clamp(overlap_start, 0.0, 1.0);
				return glm::vec2{static_cast<float>(static_cast<double>(p1.x) + t * rx),
				                 static_cast<float>(static_cast<double>(p1.y) + t * ry)};
			}

			const double t = cross(qx, qy, sx, sy) / denominator;
			const double u = cross(qx, qy, rx, ry) / denominator;
			if (t < -t_tolerance || t > 1.0 + t_tolerance || u < -tolerance / s_length ||
			    u > 1.0 + tolerance / s_length)
				return std::nullopt;

			const double clamped_t = std::clamp(t, 0.0, 1.0);
			return glm::vec2{static_cast<float>(static_cast<double>(p1.x) + clamped_t * rx),
			                 static_cast<float>(static_cast<double>(p1.y) + clamped_t * ry)};
		}
	} // namespace line_detail

	inline bool lines_intersect(glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec2 p4,
	                            float epsilon = SDL_FLT_EPSILON)
	{
		return line_detail::segment_intersection(p1, p2, p3, p4, epsilon).has_value();
	}

	struct line
	{
		glm::vec2 start;
		glm::vec2 end;
		line() = default;
		line(glm::vec2 s, glm::vec2 e) : start(s), end(e) {}
		bool intersects(const line& other) const
		{
			return lines_intersect(start, end, other.start, other.end);
		}
		friend bool intersects(const line& a, const line& b) { return a.intersects(b); }
		float length() const { return glm::length(end - start); }

		glm::vec2 direction() const { return glm::normalize(end - start); }

		std::optional<glm::vec2> intersection_point(const line& other, float epsilon = SDL_FLT_EPSILON) const
		{
			return line_detail::segment_intersection(start, end, other.start, other.end, epsilon);
		}
		[[nodiscard]] std::optional<glm::vec2> intersectionPoint(const line& other,
		                                                         float epsilon = SDL_FLT_EPSILON) const
		{
			return intersection_point(other, epsilon);
		}
	};
} // namespace mngl
namespace mngl
{
	[[nodiscard]] inline bool linesIntersect(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d,
	                                         float epsilon = SDL_FLT_EPSILON)
	{
		return lines_intersect(a, b, c, d, epsilon);
	}
} // namespace mngl
