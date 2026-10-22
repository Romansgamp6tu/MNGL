#pragma once

#include <mods.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace mngl::mods::tree
{
	using tree_id = std::string_view;
	inline constexpr std::string_view algorithm_class_id =
		"mngl.tree.algorithm";

	namespace ids
	{
		inline constexpr tree_id bst = "mngl.tree.bst";
	}

	inline constexpr std::size_t no_tree_node =
		std::numeric_limits<std::size_t>::max();

	struct big_integer_text
	{
		std::string decimal;
	};

	struct key
	{
		using big_integer_text = mngl::mods::tree::big_integer_text;
		using value_type = std::variant<
			std::string,
			double,
			std::int64_t,
			big_integer_text>;

		value_type value;
		std::string detail;
		bool unquoted_serialized_value = false;
	};

	struct node
	{
		std::vector<key> keys;
		std::vector<std::size_t> children;
		std::array<std::size_t, 2> binary_children{
			no_tree_node,
			no_tree_node
		};
		bool leaf = false;
		bool emphasized = false;
		std::size_t next_leaf = no_tree_node;
	};

	struct tree
	{
		std::vector<node> nodes;
		std::size_t root = 0;
		std::size_t fanout = 0;
		bool multiway = false;
	};

	enum class key_kind : std::uint8_t
	{
		text,
		floating_point,
		signed_integer,
		big_integer
	};

	using key_kind_mask = std::uint8_t;

	[[nodiscard]] constexpr key_kind_mask key_kind_bit(key_kind value) noexcept
	{
		return static_cast<key_kind_mask>(1u << static_cast<unsigned>(value));
	}

	[[nodiscard]] constexpr bool supports(
		key_kind_mask kinds,
		key_kind value) noexcept
	{
		return (kinds & key_kind_bit(value)) != 0;
	}

	enum class operation : std::uint16_t
	{
		find = 1u << 0,
		insert = 1u << 1,
		random_insert = 1u << 2,
		erase = 1u << 3,
		merge = 1u << 4,
		split = 1u << 5
	};

	using operation_mask = std::uint16_t;

	[[nodiscard]] constexpr operation_mask operator|(operation left, operation right) noexcept
	{
		return static_cast<operation_mask>(left) |
			static_cast<operation_mask>(right);
	}

	[[nodiscard]] constexpr operation_mask operator|(
		operation_mask left,
		operation right) noexcept
	{
		return left | static_cast<operation_mask>(right);
	}

	[[nodiscard]] constexpr bool supports(
		operation_mask operations,
		operation value) noexcept
	{
		return (operations & static_cast<operation_mask>(value)) != 0;
	}

	struct animation_capabilities
	{
		operation_mask layout_transition_operations = 0;
		operation_mask step_event_operations = 0;
	};

	enum class node_variant_mode : std::uint8_t
	{
		uniform,
		emphasis
	};

	struct visualization_hints
	{
		node_variant_mode node_variants = node_variant_mode::uniform;
		std::array<std::string_view, 2> node_variant_labels{};
		std::string_view key_detail_prefix;
		bool show_key_details = true;
	};

	struct algorithm_info
	{
		tree_id id;
		std::string_view display_name;
		std::string_view file_stem;
		std::string_view serialization_prefix;
		std::uint8_t supported_key_kinds = 0;
		operation_mask supported_operations = 0;
		animation_capabilities animations;
		visualization_hints visualization;
		int legacy_tree_catalog_index = -1;
		std::array<std::string_view, 2> legacy_serialization_prefixes{};
	};

	inline constexpr operation_mask standard_search_tree_operations =
		static_cast<operation_mask>(operation::find) |
		operation::insert |
		operation::random_insert |
		operation::erase |
		operation::merge |
		operation::split;
	inline constexpr operation_mask standard_search_tree_layout_transitions =
		static_cast<operation_mask>(operation::insert) |
		operation::erase |
		operation::merge |
		operation::split;
	inline constexpr algorithm_info builtin_bst_info{
		ids::bst,
		"BST",
		"bst",
		"bst{",
		key_kind_bit(key_kind::signed_integer) |
			key_kind_bit(key_kind::floating_point),
		standard_search_tree_operations,
		{ standard_search_tree_layout_transitions, 0 },
		{},
		0
	};

	[[nodiscard]] inline key_kind default_key_kind_for(
		const algorithm_info& info)
	{
		for (const auto kind : {
			key_kind::signed_integer,
			key_kind::floating_point,
			key_kind::text,
			key_kind::big_integer })
		{
			if (supports(info.supported_key_kinds, kind))
			{
				return kind;
			}
		}
		throw std::invalid_argument(
			"Tree algorithm does not support any key kind");
	}

	struct visual_event
	{
		enum class kind : std::uint8_t
		{
			insert_node,
			rotate_node_up
		};

		kind action = kind::insert_node;
		std::size_t node_index = no_tree_node;
		std::size_t parent_index = no_tree_node;
		std::size_t grandparent_index = no_tree_node;
		std::size_t side = 0;
		node inserted_node;
};

	struct operation_result
	{
		bool inserted = false;
		bool changed = false;
		bool animation_events_complete = true;
		std::vector<visual_event> events;
	};

	struct bulk_insert_result
	{
		std::uint64_t inserted_count = 0;
		std::optional<std::string> serialization;
	};

	struct erase_result
	{
		bool erased = false;
		bool changed = false;
	};

	struct find_result
	{
		bool found = false;
		bool changed = false;
		std::size_t visited_nodes = 0;
		std::vector<std::size_t> path;
	};

	struct document
	{
		std::string algorithm_id;
		tree value;
		std::optional<key_kind> keys;
	};

	class algorithm : public mngl::mods::modable
	{
	public:
		~algorithm() override = default;

		[[nodiscard]] virtual const algorithm_info& info() const noexcept = 0;

		[[nodiscard]] tree_id id() const noexcept
		{
			return info().id;
		}

		[[nodiscard]] std::string_view name() const noexcept
		{
			return info().display_name;
		}

		[[nodiscard]] bool supports_animation() const noexcept
		{
			return supports(
				info().animations.step_event_operations,
				operation::insert);
		}

		[[nodiscard]] virtual tree create_empty(key_kind keys) const = 0;
		[[nodiscard]] virtual document deserialize(
			std::string_view source) const = 0;
		virtual void validate(const tree& value, key_kind keys) const = 0;
		[[nodiscard]] virtual find_result find(
			tree& value,
			const key& search_key,
			key_kind keys) const
		{
			(void)value;
			(void)search_key;
			(void)keys;
			throw std::logic_error("Search is not supported by this algorithm");
		}
		[[nodiscard]] virtual operation_result insert(
			tree& value,
			key insert_key,
			key_kind keys,
			bool collect_animation_events) const = 0;
		[[nodiscard]] virtual bulk_insert_result insert_many(
			tree& value,
			std::uint64_t requested_count,
			key_kind keys,
			std::mt19937_64& random_engine) const
		{
			(void)value;
			(void)requested_count;
			(void)keys;
			(void)random_engine;
			throw std::logic_error(
				"Bulk insertion is not supported by this algorithm");
		}
		[[nodiscard]] virtual erase_result erase(
			tree& value,
			const key& erase_key,
			key_kind keys) const
		{
			(void)value;
			(void)erase_key;
			(void)keys;
			throw std::logic_error("Erase is not supported by this algorithm");
		}
		[[nodiscard]] virtual bool merge(
			tree& left,
			const tree& right,
			key_kind keys) const
		{
			(void)left;
			(void)right;
			(void)keys;
			throw std::logic_error("Merge is not supported by this algorithm");
		}
		[[nodiscard]] virtual std::pair<tree, tree> split(
			const tree& value,
			const key& pivot,
			key_kind keys) const
		{
			(void)value;
			(void)pivot;
			(void)keys;
			throw std::logic_error("Split is not supported by this algorithm");
		}
		[[nodiscard]] virtual std::string serialize(const tree& value) const = 0;
	};
}

namespace tree_practicum
{
	namespace mods = ::mngl::mods::tree;
}
