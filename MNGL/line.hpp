#pragma once
#include "utilities.hpp"

namespace mngl
{
	bool linesIntersect(glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec2 p4, float epsilon = SDL_FLT_EPSILON)
	{
		float denom = (p4.y - p3.y) * (p2.x - p1.x) - (p4.x - p3.x) * (p2.y - p1.y);
		if (abs(denom) < epsilon)
		{
			return false;
		}
		float ua = ((p4.x - p3.x) * (p1.y - p3.y) - (p4.y - p3.y) * (p1.x - p3.x)) / denom;
		float ub = ((p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x)) / denom;
		return ua >= 0.0f && ua <= 1.0f && ub >= 0.0f && ub <= 1.0f;
	}

	struct line
	{
		glm::vec2 start;
		glm::vec2 end;
		line() = default;
		line(glm::vec2 s, glm::vec2 e) : start(s), end(e) {}
		bool intersects(const line& other) const
		{
			return linesIntersect(start, end, other.start, other.end);
		}
		friend bool intersects(const line& a, const line& b)
		{
			return a.intersects(b);
		}
		float length() const
		{
			return glm::length(end - start);
		}

		glm::vec2 direction() const
		{
			return glm::normalize(end - start);
		}

		std::optional<glm::vec2> intersectionPoint(const line& other, float epsilon = SDL_FLT_EPSILON) const
		{
			float denom = (other.end.y - other.start.y) * (end.x - start.x) - (other.end.x - other.start.x) * (end.y - start.y);
			if (abs(denom) < epsilon)
			{
				return std::nullopt;
			}
			float ua = ((other.end.x - other.start.x) * (start.y - other.start.y) - (other.end.y - other.start.y) * (start.x - other.start.x)) / denom;
			float ub = ((end.x - start.x) * (other.start.y - start.y) - (end.y - start.y) * (other.start.x - start.x)) / denom;
			if (ua >= 0.0f && ua <= 1.0f && ub >= 0.0f && ub <= 1.0f)
			{
				return start + ua * (end - start);
			}
			else
			{
				return std::nullopt;
			}
		}
	};
}