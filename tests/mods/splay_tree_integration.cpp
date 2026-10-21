#include <mod_loader.hpp>
#include <tree_algorithm_api.hpp>
#include <tree_practicum_storage.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	using mngl::mods::mod_loader;
	using mngl::mods::mod_unload_state;
	using mngl::mods::registry;
	using namespace mngl::mods::tree;

	void require(bool condition, std::string_view message)
	{
		if (!condition)
		{
			throw std::runtime_error(std::string(message));
		}
	}

	[[nodiscard]] std::vector<std::int64_t> sorted_keys(const tree& value)
	{
		std::vector<std::int64_t> result;
		result.reserve(value.nodes.size());
		for (const auto& node : value.nodes)
		{
			require(node.keys.size() == 1,
				"Splay display nodes must contain one key");
			const auto* key_value =
				std::get_if<std::int64_t>(&node.keys.front().value);
			require(key_value != nullptr,
				"Integer Splay model must contain int64_t keys");
			result.push_back(*key_value);
		}
		std::sort(result.begin(), result.end());
		return result;
	}

	void check_model_for_display(const tree& value)
	{
		require(!value.b_plus, "Splay must use the binary tree model");
		if (value.nodes.empty())
		{
			require(value.root == no_tree_node,
				"Empty Splay tree must use no_tree_node as its root");
			return;
		}
		require(value.root < value.nodes.size(),
			"Splay root must index a node for the canvas renderer");
		for (const auto& node : value.nodes)
		{
			require(node.keys.size() == 1,
				"Canvas nodes must contain exactly one key");
			std::vector<std::size_t> expected_children;
			for (const auto child : node.binary_children)
			{
				require(child == no_tree_node || child < value.nodes.size(),
					"Canvas child index must point to an existing node");
				if (child != no_tree_node)
				{
					expected_children.push_back(child);
				}
			}
			require(node.children == expected_children,
				"Canvas child list must match binary child indices");
			require(node.leaf == expected_children.empty(),
				"Canvas leaf marker must match the node's children");
			require(node.next_leaf == no_tree_node && !node.red,
				"Splay nodes must not carry B+ or red-black metadata");
		}
	}

	void set_child(tree& value, std::size_t parent, std::size_t side, std::size_t child)
	{
		value.nodes[parent].binary_children[side] = child;
		auto& node = value.nodes[parent];
		node.children.clear();
		for (const auto index : node.binary_children)
		{
			if (index != no_tree_node)
			{
				node.children.push_back(index);
			}
		}
		node.leaf = node.children.empty();
	}

	void play_animation_events(tree& value, const std::vector<visual_event>& events)
	{
		for (const auto& event : events)
		{
			if (event.action == visual_event::kind::insert_node)
			{
				require(event.node_index == value.nodes.size(),
					"Animated inserted node must be appended in order");
				if (event.parent_index == no_tree_node)
				{
					require(value.nodes.empty() && event.node_index == 0,
						"Animated root insertion must start from an empty tree");
					value.nodes.push_back(event.inserted_node);
					value.root = 0;
					continue;
				}
				require(event.parent_index < value.nodes.size() && event.side < 2 &&
					value.nodes[event.parent_index].binary_children[event.side] ==
						no_tree_node,
					"Animated insertion parent and side must be valid");
				value.nodes.push_back(event.inserted_node);
				set_child(value, event.parent_index, event.side, event.node_index);
				continue;
			}

			require(event.node_index < value.nodes.size() &&
				event.parent_index < value.nodes.size(),
				"Animated rotation must reference existing nodes");
			std::size_t parent_side = 2;
			if (value.nodes[event.parent_index].binary_children[0] ==
				event.node_index)
			{
				parent_side = 0;
			}
			else if (value.nodes[event.parent_index].binary_children[1] ==
				event.node_index)
			{
				parent_side = 1;
			}
			require(parent_side < 2,
				"Animated rotation node must be a child of its parent");

			std::size_t grandparent_side = 2;
			if (event.grandparent_index == no_tree_node)
			{
				require(value.root == event.parent_index,
					"Animated root rotation must start at the root");
			}
			else
			{
				require(event.grandparent_index < value.nodes.size(),
					"Animated grandparent must exist");
				if (value.nodes[event.grandparent_index].binary_children[0] ==
					event.parent_index)
				{
					grandparent_side = 0;
				}
				else if (value.nodes[event.grandparent_index].binary_children[1] ==
					event.parent_index)
				{
					grandparent_side = 1;
				}
				require(grandparent_side < 2,
					"Animated grandparent must reference the parent");
			}

			const auto moved = value.nodes[event.node_index]
				.binary_children[1 - parent_side];
			set_child(value, event.parent_index, parent_side, moved);
			set_child(value, event.node_index, 1 - parent_side,
				event.parent_index);
			if (event.grandparent_index == no_tree_node)
			{
				value.root = event.node_index;
			}
			else
			{
				set_child(value, event.grandparent_index,
					grandparent_side, event.node_index);
			}
		}
	}

	void insert(algorithm& splay, tree& value, std::int64_t key_value)
	{
		const auto before = value;
		const auto result = splay.insert(
			value,
			key{ key_value, {}, false },
			key_kind::signed_integer,
			true);
		require(result.inserted && result.changed,
			"Splay insertion should add a new key");
		require(result.animation_events_complete && !result.events.empty(),
			"Splay insertion should provide complete step animation events");
		tree animated = before;
		play_animation_events(animated, result.events);
		require(splay.serialize(animated) == splay.serialize(value),
			"Splay animation events should reproduce the final tree model");
		splay.validate(value, key_kind::signed_integer);
		check_model_for_display(value);
	}

	void verify_package(const std::filesystem::path& package)
	{
		registry mod_registry;
		mod_registry.apply_declarations();
		auto& classes = mod_registry.define_class<algorithm>(
			std::string(algorithm_class_id));
		require(classes.implementations().empty(),
			"Splay must not appear before its mod is loaded");

		mod_loader loader(mod_registry);
		const auto loaded = loader.load_package(package);
		require(static_cast<bool>(loaded),
			"Splay package failed to load: " + loaded.message);

		const auto implementations = classes.implementations();
		const auto registered = std::find_if(
			implementations.begin(),
			implementations.end(),
			[](const auto& item)
			{
				return item.id == ids::splay;
			});
		require(registered != implementations.end() && !registered->builtin &&
			registered->owner_mod_id == "mngl.tree-practicum.splay",
			"Loaded Splay must appear as a mod-owned implementation");

		auto implementation = classes.create(ids::splay);
		require(implementation != nullptr,
			"Splay factory did not create an algorithm instance");
		require(implementation->info().id == ids::splay &&
			implementation->info().display_name == "Splay" &&
			supports(implementation->info().animations.step_event_operations,
				operation::insert),
			"Splay factory returned incorrect metadata or animation capabilities");

		auto value = implementation->create_empty(key_kind::signed_integer);
		require(value.nodes.empty() && value.root == no_tree_node,
			"New Splay file must start as an empty tree model");
		implementation->validate(value, key_kind::signed_integer);
		check_model_for_display(value);
		insert(*implementation, value, 40);
		insert(*implementation, value, 20);
		insert(*implementation, value, 60);
		insert(*implementation, value, 10);
		insert(*implementation, value, 30);
		insert(*implementation, value, 50);
		insert(*implementation, value, 70);

		const auto duplicate_before = value;
		const auto duplicate = implementation->insert(
			value,
			key{ std::int64_t{ 10 }, {}, false },
			key_kind::signed_integer,
			true);
		require(!duplicate.inserted && duplicate.changed &&
			duplicate.animation_events_complete && !duplicate.events.empty(),
			"Splay duplicate insertion should splay and report its rotations");
		tree duplicate_animation = duplicate_before;
		play_animation_events(duplicate_animation, duplicate.events);
		require(implementation->serialize(duplicate_animation) ==
			implementation->serialize(value),
			"Duplicate insertion animation should reproduce its Splay restructuring");
		check_model_for_display(value);

		const auto found = implementation->find(
			value,
			key{ std::int64_t{ 50 }, {}, false },
			key_kind::signed_integer);
		require(found.found && found.changed && !found.path.empty() &&
			found.visited_nodes > 0 &&
			std::get<std::int64_t>(value.nodes[value.root].keys.front().value) == 50,
			"Splay search should find and move the key to the root");
		implementation->validate(value, key_kind::signed_integer);
		check_model_for_display(value);

		const auto first_serialization = implementation->serialize(value);
		require(first_serialization.starts_with("splay{"),
			"Splay must keep the existing .tree serialization format");
		auto reopened = implementation->deserialize(first_serialization);
		require(reopened.algorithm_id == ids::splay &&
			reopened.keys == key_kind::signed_integer,
			"Splay serialization should reopen with the same tree ID and key kind");
		implementation->validate(reopened.value, key_kind::signed_integer);
		require(implementation->serialize(reopened.value) == first_serialization,
			"Splay serialization round trip should preserve shape");
		check_model_for_display(reopened.value);

		std::mt19937_64 random_engine{ 0x4d4e474cull };
		const auto bulk = implementation->insert_many(
			value,
			20,
			key_kind::signed_integer,
			random_engine);
		require(bulk.inserted_count == 20 && bulk.serialization.has_value(),
			"Splay mass insertion should add the requested number of keys");
		implementation->validate(value, key_kind::signed_integer);
		check_model_for_display(value);

		const auto erased = implementation->erase(
			value,
			key{ std::int64_t{ 40 }, {}, false },
			key_kind::signed_integer);
		require(erased.erased && erased.changed,
			"Splay erase should remove an existing key");
		implementation->validate(value, key_kind::signed_integer);
		check_model_for_display(value);

		const auto keys_before_split = sorted_keys(value);
		auto [left, right] = implementation->split(
			value,
			key{ std::int64_t{ 50 }, {}, false },
			key_kind::signed_integer);
		implementation->validate(left, key_kind::signed_integer);
		implementation->validate(right, key_kind::signed_integer);
		check_model_for_display(left);
		check_model_for_display(right);
		for (const auto key_value : sorted_keys(left))
		{
			require(key_value < 50,
				"Splay split left side must contain keys below the pivot");
		}
		for (const auto key_value : sorted_keys(right))
		{
			require(key_value >= 50,
				"Splay split right side must contain keys at or above the pivot");
		}
		require(implementation->merge(left, right, key_kind::signed_integer),
			"Splay merge should rejoin the split trees");
		implementation->validate(left, key_kind::signed_integer);
		check_model_for_display(left);
		require(sorted_keys(left) == keys_before_split,
			"Splay split followed by merge should preserve the key set");

		auto floating = implementation->create_empty(key_kind::floating_point);
		for (const double floating_key : { 1.25, -2.5, 4.0 })
		{
			const auto result = implementation->insert(
				floating,
				key{ floating_key, {}, false },
				key_kind::floating_point,
				false);
			require(result.inserted && result.changed,
				"Splay should accept finite double keys");
		}
		const auto floating_serialization = implementation->serialize(floating);
		const auto floating_document =
			implementation->deserialize(floating_serialization);
		require(floating_document.keys == key_kind::floating_point &&
			implementation->serialize(floating_document.value) ==
				floating_serialization,
			"Splay should round-trip finite double keys");

		const auto save_path = std::filesystem::temp_directory_path() /
			("mngl-splay-roundtrip-" + std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()) +
				".tree");
		const auto saved_serialization = implementation->serialize(left);
		tree_practicum::write_tree_file(save_path, saved_serialization);
		const auto loaded_contents = tree_practicum::read_tree_file(save_path);
		const auto saved_document = implementation->deserialize(loaded_contents);
		std::error_code ignored;
		std::filesystem::remove(save_path, ignored);
		require(saved_document.algorithm_id == ids::splay &&
			implementation->serialize(saved_document.value) == saved_serialization,
			"Saved .tree file should reopen with the same Splay data");

		const auto unload = loader.unload("mngl.tree-practicum.splay");
		require(unload.state == mod_unload_state::deferred &&
			unload.live_objects > 0,
			"Splay library unload must wait while its algorithm object lives");
		implementation.reset();
		require(loader.collect_deferred() == 1,
			"Splay library should unload after its algorithm instance is destroyed");
	}
}

int main(int argc, char** argv)
{
	try
	{
		if (argc != 2)
		{
			throw std::invalid_argument(
				"usage: mngl_splay_tree_mod_integration <mod-package>");
		}
		verify_package(argv[1]);
		std::cout << "Splay mod load, animation, operations, model, save, and unload passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
