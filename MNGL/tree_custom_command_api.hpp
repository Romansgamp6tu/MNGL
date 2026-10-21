#pragma once

#include "tree_algorithm_api.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mngl::mods::tree
{
	inline constexpr std::string_view custom_command_class_id =
		"mngl.tree.custom-command";

	enum class command_parameter_type : std::uint8_t
	{
		text,
		bit_string,
		signed_integer,
		floating_point,
		boolean
	};

	using command_argument = std::variant<
		std::string,
		std::int64_t,
		double,
		bool>;

	struct command_result
	{
		std::string message;
		std::vector<std::size_t> highlighted_nodes;
		bool changed = false;
	};

	class custom_command_handler : public mngl::mods::modable
	{
	public:
		~custom_command_handler() override = default;

		[[nodiscard]] virtual bool supports_command(
			std::string_view command_id) const noexcept = 0;

		[[nodiscard]] virtual command_result execute(
			std::string_view command_id,
			tree& value,
			key_kind keys,
			std::span<const command_argument> arguments) const = 0;
	};
}
