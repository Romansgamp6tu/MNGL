#pragma once

#include "tree_algorithm_api.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace mngl::mods::tree
{
	inline constexpr std::string_view invariant_inspector_class_id =
		"mngl.tree.invariant-inspector";

	struct invariant_check
	{
		std::string name;
		bool passed = false;
		std::string detail;
	};

	struct invariant_report
	{
		std::vector<invariant_check> checks;
	};

	class invariant_inspector : public mngl::mods::modable
	{
	public:
		~invariant_inspector() override = default;

		[[nodiscard]] virtual std::string_view algorithm_id() const noexcept = 0;

		[[nodiscard]] virtual invariant_report inspect(
			const tree& value,
			key_kind keys) const = 0;

		[[nodiscard]] virtual std::string describe_node(
			const tree& value,
			key_kind keys,
			std::size_t node_index) const = 0;
	};
}
