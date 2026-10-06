#pragma once

#include "rectangle.hpp"

#include <cmath>
#include <stdexcept>

namespace mngl
{
	class view
	{
	public:
		explicit view(const rectangle& area)
		{
			set_area(area);
		}

		[[nodiscard]] const rectangle& area() const noexcept
		{
			return m_area;
		}

		view& set_area(const rectangle& area)
		{
			if (!std::isfinite(area.pos.x) || !std::isfinite(area.pos.y) ||
				!std::isfinite(area.size.x) || !std::isfinite(area.size.y) ||
				!std::isfinite(area.pos.x + area.size.x) ||
				!std::isfinite(area.pos.y + area.size.y) ||
				area.size.x <= 0.0f || area.size.y <= 0.0f)
			{
				throw std::invalid_argument("A 2D view area must be finite and have positive dimensions");
			}
			m_area = area;
			return *this;
		}

		[[nodiscard]] rectangle visible_area(glm::uvec2 target_size) const
		{
			if (target_size.x == 0 || target_size.y == 0)
			{
				throw std::invalid_argument("A 2D view requires a target with non-zero dimensions");
			}

			const float target_aspect =
				static_cast<float>(target_size.x) / static_cast<float>(target_size.y);
			float visible_width = m_area.size.x;
			float visible_height = m_area.size.y;
			if (visible_width / visible_height < target_aspect)
			{
				visible_width = visible_height * target_aspect;
			}
			else
			{
				visible_height = visible_width / target_aspect;
			}

			const glm::vec2 center = m_area.pos + m_area.size * 0.5f;
			const rectangle result{
				center - glm::vec2(visible_width, visible_height) * 0.5f,
				glm::vec2(visible_width, visible_height)
			};
			if (!std::isfinite(result.pos.x) || !std::isfinite(result.pos.y) ||
				!std::isfinite(result.size.x) || !std::isfinite(result.size.y) ||
				!std::isfinite(result.pos.x + result.size.x) ||
				!std::isfinite(result.pos.y + result.size.y))
			{
				throw std::overflow_error("The target aspect ratio overflows the 2D view area");
			}
			return result;
		}

		[[nodiscard]] glm::mat4 projection(glm::uvec2 target_size) const
		{
			const rectangle visible = visible_area(target_size);
			glm::mat4 result(1.0f);
			result[0][0] = 2.0f / visible.size.x;
			result[1][1] = 2.0f / visible.size.y;
			result[3][0] = -1.0f - 2.0f * visible.pos.x / visible.size.x;
			result[3][1] = -1.0f - 2.0f * visible.pos.y / visible.size.y;

			if (!std::isfinite(result[0][0]) || !std::isfinite(result[1][1]) ||
				!std::isfinite(result[3][0]) || !std::isfinite(result[3][1]))
			{
				throw std::overflow_error("The 2D view projection exceeds the supported numeric range");
			}
			return result;
		}

	private:
		rectangle m_area{};
	};
}
