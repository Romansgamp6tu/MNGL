#define APP_NAME "MNGL frame clock regression test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#include <MNGL/input.hpp>
#include <MNGL/runtime.hpp>

#include <chrono>
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

	void test_first_frame_has_zero_elapsed_time()
	{
		mngl::input::set_max_fps(0);
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");
		require(mngl::input::delta_time() == 0.0, "The first frame should report zero elapsed time");
	}

	void test_long_frame_time_is_not_clamped()
	{
		const auto before = std::chrono::steady_clock::now();
		SDL_Delay(350);
		const auto after = std::chrono::steady_clock::now();
		require(mngl::input::next_frame(), "Input loop unexpectedly requested quit");

		const double wall_seconds = std::chrono::duration<double>(after - before).count();
		const double frame_seconds = mngl::input::delta_time();
		require(frame_seconds > 0.25, "A long frame interval was clamped instead of exposed");
		require(std::abs(frame_seconds - wall_seconds) < 0.1,
		        "delta_time() did not match the measured wall-clock interval");
	}
} // namespace

int main()
{
	try
	{
		mngl::runtime::ensure();
		test_first_frame_has_zero_elapsed_time();
		test_long_frame_time_is_not_clamped();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
