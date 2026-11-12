#include <MNGL/color.hpp>

#include <cmath>
#include <stdexcept>

namespace
{
	constexpr float epsilon = 0.00001f;

	constexpr bool near(float left, float right)
	{
		return left > right ? left - right < epsilon : right - left < epsilon;
	}

	constexpr bool rgb_is(const mngl::color& value, float red, float green, float blue)
	{
		const auto rgba = value.rgba_glsl();
		return near(rgba.x, red) && near(rgba.y, green) && near(rgba.z, blue);
	}

	constexpr mngl::color red_hsv = mngl::color::from_hsv(glm::vec3{0.f, 1.f, 1.f});
	static_assert(rgb_is(red_hsv, 1.f, 0.f, 0.f));
	static_assert(rgb_is(mngl::color::from_hsl(glm::vec3{1.f / 3.f, 1.f, 0.5f}), 0.f, 1.f, 0.f));
	static_assert(rgb_is(mngl::color::from_hsv(glm::uvec3{120, 255, 255}), 0.f, 1.f, 0.f));
	static_assert(rgb_is(mngl::color::from_hsl(glm::uvec3{120, 255, 128}), 1.f / 255.f, 1.f, 1.f / 255.f));
	constexpr mngl::color blue_rgb{0.f, 0.f, 1.f};
	constexpr mngl::color green_rgb{0.f, 1.f, 0.f};
	static_assert(blue_rgb.to_hsv().x > 0.66f);
	static_assert(green_rgb.to_hsl().y == 1.f);
	static_assert(blue_rgb.to_hsv_255() == glm::uvec3{240, 255, 255});
	static_assert(green_rgb.to_hsl_255() == glm::uvec3{120, 255, 128});
	static_assert(near(mngl::color::linearize(0.5f), 0.21404114f));
	static_assert(near(mngl::color::gamma(0.5f), 0.73535698f));
	static_assert(near(mngl::color{1.f}.luminance(), 1.f));
	static_assert(near(mngl::color::contrast(mngl::color{1.f}, mngl::color{0.f}), 21.f));

	constexpr mngl::color red_half{1.f, 0.f, 0.f, 0.5f};
	constexpr mngl::color transparent{0.f, 0.f, 0.f, 0.f};
	constexpr mngl::color red_over_transparent = mngl::color::blends::normal(red_half, transparent);
	static_assert(near(red_over_transparent.rgba_glsl().x, 1.f) &&
	              near(red_over_transparent.rgba_glsl().w, 0.5f));
	static_assert(mngl::color::blends::normal(red_over_transparent, transparent) == red_over_transparent);

	constexpr mngl::color red_over_blue = mngl::color::blends::normal(
	    red_half, mngl::color{0.f, 0.f, 1.f, 0.5f});
	static_assert(near(red_over_blue.rgba_glsl().x, 2.f / 3.f) &&
	              near(red_over_blue.rgba_glsl().z, 1.f / 3.f));
	static_assert(near(red_over_blue.rgba_glsl().w, 0.75f));

	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}
} // namespace

int main()
{
	const mngl::color blue{0.f, 0.f, 1.f, 0.25f};
	const auto over_transparent = mngl::color::blends::normal(red_half, transparent);
	require(over_transparent == red_half, "Transparent background changed a straight-alpha color");
	const auto transparent_over_blue = mngl::color::blends::normal(transparent, blue);
	require(transparent_over_blue == blue, "Transparent foreground changed the background");

	const auto result = mngl::color::blends::normal(red_half, blue);
	const auto result_rgba = result.rgba_glsl();
	require(std::abs(result_rgba.x - 0.8f) < epsilon && std::abs(result_rgba.z - 0.2f) < epsilon &&
	            std::abs(result_rgba.w - 0.625f) < epsilon,
	        "Straight-alpha source-over result was incorrect");
	const auto layered = mngl::color::blends::normal(result, transparent);
	const auto layered_rgba = layered.rgba_glsl();
	require(std::abs(layered_rgba.x - result_rgba.x) < epsilon &&
	            std::abs(layered_rgba.z - result_rgba.z) < epsilon &&
	            std::abs(layered_rgba.w - result_rgba.w) < epsilon,
	        "Normal blending was not composable across layers");
	return 0;
}
