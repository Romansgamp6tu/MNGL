#include <MNGL/line.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	void test_long_shallow_crossing()
	{
		const glm::vec2 first_start{0.0f, 0.0f};
		const glm::vec2 first_end{100'000'000.0f, 0.0f};
		const glm::vec2 second_start{0.0f, 1.0f};
		const glm::vec2 second_end{100'000'000.0f, -1.0f};

		require(mngl::lines_intersect(first_start, first_end, second_start, second_end),
		        "long segments crossing at a shallow angle were reported parallel");

		const auto intersection = mngl::line{first_start, first_end}.intersection_point(
		    mngl::line{second_start, second_end});
		require(intersection.has_value(), "the shallow crossing point was not returned");
		require(std::abs(intersection->x - 50'000'000.0f) <= 0.5f && std::abs(intersection->y) <= 0.001f,
		        "the shallow crossing point was inaccurate");
	}

	void test_parallel_and_collinear_segments()
	{
		require(!mngl::lines_intersect({0.0f, 0.0f}, {100'000'000.0f, 0.0f},
		                              {0.0f, 1.0f}, {100'000'000.0f, 1.0f}),
		        "separate parallel segments were reported intersecting");
		require(!mngl::lines_intersect({0.0f, 0.0f}, {10.0f, 0.0f}, {11.0f, 0.0f}, {20.0f, 0.0f}),
		        "disjoint collinear segments were reported intersecting");

		const auto overlap = mngl::line{{0.0f, 0.0f}, {10.0f, 0.0f}}.intersection_point(
		    mngl::line{{5.0f, 0.0f}, {15.0f, 0.0f}});
		require(overlap.has_value() && std::abs(overlap->x - 5.0f) <= 0.001f &&
		            std::abs(overlap->y) <= 0.001f,
		        "overlapping collinear segments did not return their first shared point");
	}
} // namespace

int main()
{
	try
	{
		test_long_shallow_crossing();
		test_parallel_and_collinear_segments();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
