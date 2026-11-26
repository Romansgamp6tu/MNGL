#pragma once

#include "../../rectangle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace mngl::two_d::detail::svg
{
	enum class filter_op : std::uint32_t
	{
		copy,
		source_alpha,
		blur,
		offset,
		flood,
		color_matrix,
		composite,
		blend,
		shadow,
		mask,
		to_linear,
		to_srgb,
		output
	};

	struct filter_step
	{
		filter_op operation{};
		// Indices 0 and 1 denote SourceGraphic and SourceAlpha.
		std::size_t input = 0;
		std::size_t input2 = 0;
		std::uint32_t mode = 0;
		glm::vec4 parameters{0.0f};
		std::array<glm::vec4, 5> matrix{};
		std::optional<rectangle> region;
		bool linear_rgb = true;
	};

	struct filter_data
	{
		rectangle region;
		bool linear_rgb = true;
		std::vector<filter_step> steps;
	};

struct filter_dependency_plan
{
	std::vector<bool> live_outputs;
	std::vector<std::size_t> consumers;
};

[[nodiscard]] inline constexpr bool filter_uses_primary_input(filter_op operation) noexcept
{
	return operation != filter_op::flood;
}

[[nodiscard]] inline constexpr bool filter_uses_secondary_input(filter_op operation) noexcept
{
	return operation == filter_op::composite || operation == filter_op::blend;
}

[[nodiscard]] inline filter_dependency_plan analyze_filter_dependencies(const std::vector<filter_step>& steps)
{
	filter_dependency_plan plan{
	    .live_outputs = std::vector<bool>(steps.size() + 2, false),
	    .consumers = std::vector<std::size_t>(steps.size() + 2, 0)};
	plan.live_outputs.back() = true;
	++plan.consumers.back();
	for (std::size_t index = steps.size(); index > 0; --index)
	{
		const auto& step = steps[index - 1];
		const auto output_index = index + 1;
		if (!plan.live_outputs[output_index])
			continue;
		if (filter_uses_primary_input(step.operation))
		{
			if (step.input >= output_index) [[unlikely]]
				throw std::logic_error("SVG filter primary input is outside the primitive graph");
			plan.live_outputs[step.input] = true;
			++plan.consumers[step.input];
		}
		if (filter_uses_secondary_input(step.operation))
		{
			if (step.input2 >= output_index) [[unlikely]]
				throw std::logic_error("SVG filter secondary input is outside the primitive graph");
			plan.live_outputs[step.input2] = true;
			++plan.consumers[step.input2];
		}
	}
	return plan;
}

	enum class mask_channel : std::uint32_t
	{
		alpha,
		luminance,
		linear_luminance
	};

	struct mask_data
	{
		rectangle region;
		mask_channel channel = mask_channel::luminance;
	};
} // namespace mngl::two_d::detail::svg
