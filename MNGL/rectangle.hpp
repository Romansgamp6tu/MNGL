#pragma once

#include <cstddef>
#include "utilities.hpp"
#include "line.hpp"
#include <span>
namespace mngl
{
	struct rectangle
	{
		union
		{
			glm::vec2 pos;
			struct
			{
				float x, y;
			};
		};

		union
		{
			glm::vec2 size;
			struct
			{
				float w, h;
			};
		};

		rectangle() : pos(0.0f), size(0.0f) {}
		rectangle(glm::vec2 p, glm::vec2 s) : pos(p), size(s) {}
		rectangle(float x, float y, float w, float h) : pos(x, y), size(w, h) {}
		rectangle(const SDL_Rect& r) : pos(r.x, r.y), size(r.w, r.h) {}
		rectangle(const SDL_FRect& r) : pos(r.x, r.y), size(r.w, r.h) {}

		bool empty() const { return size.x < 0 || size.y < 0; }
		bool equals(const rectangle& other, double epsilon = SDL_FLT_EPSILON) const
		{
			return (fabs(pos.x - other.pos.x) <= epsilon && fabs(pos.y - other.pos.y) <= epsilon &&
			        fabs(size.x - other.size.x) <= epsilon && fabs(size.y - other.size.y) <= epsilon);
		}

		bool operator==(const rectangle& other) const { return equals(other); }

		bool operator!=(const rectangle& other) const { return !equals(other); }

		bool intersects(const rectangle& other) const
		{
			return !(pos.x + size.x <= other.pos.x || pos.y + size.y <= other.pos.y ||
			         other.pos.x + other.size.x <= pos.x || other.pos.y + other.size.y <= pos.y);
		}
		friend bool intersects(const rectangle& a, const rectangle& b) { return a.intersects(b); }

		rectangle intersection(const rectangle& other) const
		{
			if (!intersects(other))
			{
				return rectangle{{0, 0}, {-1, -1}};
			}
			else
			{
				glm::vec2 newPos{std::max(pos.x, other.pos.x), std::max(pos.y, other.pos.y)};
				glm::vec2 newSize{std::min(pos.x + size.x, other.pos.x + other.size.x) - newPos.x,
				                  std::min(pos.y + size.y, other.pos.y + other.size.y) - newPos.y};
				return rectangle{newPos, newSize};
			}
		}
		friend rectangle intersection(const rectangle& a, const rectangle& b) { return a.intersection(b); }

		bool intersects_line(glm::vec2 line_start, glm::vec2 line_end) const
		{
			return intersects_line(line{line_start, line_end});
		}

		bool intersects_line(const line& l) const
		{
			line top{pos, {pos.x + size.x, pos.y}};
			line bottom{{pos.x, pos.y + size.y}, {pos.x + size.x, pos.y + size.y}};
			line left{pos, {pos.x, pos.y + size.y}};
			line right{{pos.x + size.x, pos.y}, {pos.x + size.x, pos.y + size.y}};
			return l.intersects(top) || l.intersects(bottom) || l.intersects(left) || l.intersects(right) ||
			       contains(l.start) || contains(l.end);
		}

		friend bool intersects_line(const rectangle& other, glm::vec2 line_start, glm::vec2 line_end)
		{
			return other.intersects_line(line{line_start, line_end});
		}

		friend bool intersects_line(const rectangle& other, const line& l)
		{
			return other.intersects_line(l);
		}

		friend bool intersects_line(glm::vec2 line_start, glm::vec2 line_end, const rectangle& other)
		{
			return other.intersects_line(line{line_start, line_end});
		}

		friend bool intersects_line(const line& l, const rectangle& other)
		{
			return other.intersects_line(l);
		}

		rectangle union_with(const rectangle& other) const
		{

			glm::vec2 newPos{std::min(pos.x, other.pos.x), std::min(pos.y, other.pos.y)};
			glm::vec2 newSize{std::max(pos.x + size.x, other.pos.x + other.size.x) - newPos.x,
			                  std::max(pos.y + size.y, other.pos.y + other.size.y) - newPos.y};
			return rectangle{newPos, newSize};
		}

		rectangle operator*(const rectangle& other) const { return intersection(other); }

		rectangle& operator*=(const rectangle& other)
		{
			*this = intersection(other);
			return *this;
		}

		rectangle operator+(const rectangle& other) const { return union_with(other); }

		rectangle& operator+=(const rectangle& other)
		{
			*this = union_with(other);
			return *this;
		}

		friend rectangle enclosing_points(std::span<const glm::vec2> points, const rectangle& clip)
		{
			if (points.empty())
				return {{0, 0}, {-1, -1}};

			float minX = points[0].x, maxX = points[0].x;
			float minY = points[0].y, maxY = points[0].y;

			for (std::size_t i = 1; i < points.size(); ++i)
			{
				minX = std::min(minX, points[i].x);
				maxX = std::max(maxX, points[i].x);
				minY = std::min(minY, points[i].y);
				maxY = std::max(maxY, points[i].y);
			}

			return rectangle{{minX, minY}, {maxX - minX, maxY - minY}} * clip;
		}

		bool contains(glm::vec2 point) const
		{
			return (point.x >= pos.x && point.x <= pos.x + size.x && point.y >= pos.y &&
			        point.y <= pos.y + size.y);
		}

		operator SDL_FRect() const { return {pos.x, pos.y, size.x, size.y}; }

		operator SDL_FRect*() const { return (SDL_FRect*)(&pos); }

		operator SDL_FRect&() { return *(SDL_FRect*)(&pos); }

		operator SDL_Rect() const { return {(int)pos.x, (int)pos.y, (int)size.x, (int)size.y}; }

		glm::vec2 center() const { return pos + size * 0.5f; }

		float area() const { return size.x * size.y; }

		float perimeter() const { return 2 * (size.x + size.y); }

		rectangle inflate(float amount)
		{
			return rectangle{pos - glm::vec2(amount), size + glm::vec2(2 * amount)};
		}

		glm::vec2 clamp(glm::vec2 point) const { return glm::clamp(point, pos, pos + size); }

		friend glm::vec2 clamp(glm::vec2 point, const rectangle& rect) { return rect.clamp(point); }

		glm::vec2 local(glm::vec2 point) const { return point - pos; }

		glm::vec2 global(glm::vec2 point) const { return point + pos; }

		float intersection_area(const rectangle& other) const
		{
			rectangle inter = intersection(other);
			if (inter.empty())
				return 0.0f;
			return inter.area();
		}

		[[nodiscard]] float intersectionArea(const rectangle& other) const
		{
			return intersection_area(other);
		}

		friend float intersection_area(const rectangle& a, const rectangle& b)
		{
			return a.intersection_area(b);
		}
		[[nodiscard]] bool intersectsLine(glm::vec2 start, glm::vec2 end) const
		{
			return intersects_line(start, end);
		}
		[[nodiscard]] bool intersectsLine(const line& value) const { return intersects_line(value); }
		[[nodiscard]] rectangle unionWith(const rectangle& other) const { return union_with(other); }
		friend bool intersectsLine(const rectangle& value, glm::vec2 start, glm::vec2 end)
		{
			return value.intersects_line(start, end);
		}
		friend bool intersectsLine(const rectangle& value, const line& segment)
		{
			return value.intersects_line(segment);
		}
		friend bool intersectsLine(glm::vec2 start, glm::vec2 end, const rectangle& value)
		{
			return value.intersects_line(start, end);
		}
		friend bool intersectsLine(const line& segment, const rectangle& value)
		{
			return value.intersects_line(segment);
		}
		friend float intersectionArea(const rectangle& a, const rectangle& b)
		{
			return a.intersection_area(b);
		}
	};
	using rectangle_proxy = proxy<rectangle, SDL_Rect,
	                              [](const rectangle& r)
	                              {
		                              return SDL_Rect{static_cast<int>(r.pos.x), static_cast<int>(r.pos.y),
									                  static_cast<int>(r.size.x), static_cast<int>(r.size.y)};
	                              },
	                              [](rectangle& r, const SDL_Rect& s)
	                              {
		                              r.pos.x = s.x;
		                              r.pos.y = s.y;
		                              r.size.x = s.w;
		                              r.size.y = s.h;
	                              }>;
} // namespace mngl
// Compatibility names; new code uses snake_case.
namespace mngl
{
	using rectangleProxy = rectangle_proxy;
}
