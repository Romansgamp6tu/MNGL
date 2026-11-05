#pragma once

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <stdexcept>

namespace mngl::two_d
{
	class transform
	{
	public:
		transform() noexcept = default;

		explicit transform(const glm::mat4& value) { matrix(value); }

		[[nodiscard]] const glm::mat4& matrix() const noexcept { return m_matrix; }

		transform& matrix(const glm::mat4& value)
		{
			validate(value);
			m_matrix = value;
			return *this;
		}

		transform& translate(glm::vec2 offset)
		{
			return matrix(glm::translate(m_matrix, glm::vec3(offset, 0.0f)));
		}

		transform& rotate(float radians, glm::vec2 origin = glm::vec2(0.0f))
		{
			glm::mat4 value = glm::translate(m_matrix, glm::vec3(origin, 0.0f));
			value = glm::rotate(value, radians, glm::vec3(0.0f, 0.0f, 1.0f));
			value = glm::translate(value, glm::vec3(-origin, 0.0f));
			return matrix(value);
		}

		transform& scale(glm::vec2 factors, glm::vec2 origin = glm::vec2(0.0f))
		{
			glm::mat4 value = glm::translate(m_matrix, glm::vec3(origin, 0.0f));
			value = glm::scale(value, glm::vec3(factors, 1.0f));
			value = glm::translate(value, glm::vec3(-origin, 0.0f));
			return matrix(value);
		}

		transform& scale(float factor, glm::vec2 origin = glm::vec2(0.0f))
		{
			return scale(glm::vec2(factor), origin);
		}

		transform& reset() noexcept
		{
			m_matrix = glm::mat4(1.0f);
			return *this;
		}

		transform& operator*=(const transform& other) { return matrix(m_matrix * other.m_matrix); }

		friend transform operator*(transform left, const transform& right)
		{
			left *= right;
			return left;
		}

	private:
		static void validate(const glm::mat4& value)
		{
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					if (!std::isfinite(value[column][row]))
					{
						throw std::invalid_argument("2D transform matrix must be finite");
					}
				}
			}

			if (!nearly_equal(value[0][2], 0.0f) || !nearly_equal(value[1][2], 0.0f) ||
			    !nearly_equal(value[2][0], 0.0f) || !nearly_equal(value[2][1], 0.0f) ||
			    !nearly_equal(value[2][2], 1.0f) || !nearly_equal(value[2][3], 0.0f) ||
			    !nearly_equal(value[3][2], 0.0f) || !nearly_equal(value[0][3], 0.0f) ||
			    !nearly_equal(value[1][3], 0.0f) || !nearly_equal(value[3][3], 1.0f))
			{
				throw std::invalid_argument(
				    "2D transform matrix must be an affine transform in the XY plane");
			}
		}

		static bool nearly_equal(float first, float second) noexcept
		{
			return std::abs(first - second) <= 1e-6f;
		}

		glm::mat4 m_matrix{1.0f};
	};
} // namespace mngl::two_d
