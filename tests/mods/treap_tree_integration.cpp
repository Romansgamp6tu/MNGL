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
				"Treap visual model node must contain one key");
			const auto* key_value =
				std::get_if<std::int64_t>(&node.keys.front().value);
			require(key_value != nullptr,
				"Treap visual model must contain int64_t keys");
			result.push_back(*key_value);
		}
		std::sort(result.begin(), result.end());
		return result;
	}

	void check_model_for_display(const tree& value)
	{
		require(!value.b_plus, "Treap must use the binary tree model");
		if (value.nodes.empty())
		{
			require(value.root == no_tree_node,
				"Empty Treap must use no_tree_node as its root");
			return;
		}
		require(value.root < value.nodes.size(),
			"Treap root must index a node for the canvas renderer");
		for (const auto& node : value.nodes)
		{
			require(node.keys.size() == 1,
				"Canvas nodes must contain exactly one key");
			require(node.children.empty(),
				"Treap children must use binary child indices");
			for (const auto child : node.binary_children)
			{
				require(child == no_tree_node || child < value.nodes.size(),
					"Canvas child index must point to an existing node");
			}
			require(!node.keys.front().detail.empty(),
				"Treap priority metadata must be available to the canvas");
		}
	}

	void insert(algorithm& treap, tree& value, std::int64_t key_value)
	{
		const auto result = treap.insert(
			value,
			key{ key_value, {}, false },
			key_kind::signed_integer,
			false);
		require(result.inserted && result.changed,
			"Treap insertion should add a new key");
	}

	void verify_package(const std::filesystem::path& package)
	{
		registry mod_registry;
		mod_registry.apply_declarations();
		auto& classes = mod_registry.define_class<algorithm>(
			std::string(algorithm_class_id));
		require(classes.implementations().empty(),
			"Treap must not appear before its mod is loaded");

		mod_loader loader(mod_registry);
		const auto loaded = loader.load_package(package);
		require(static_cast<bool>(loaded),
			"Treap package failed to load: " + loaded.message);

		const auto implementations = classes.implementations();
		const auto registered = std::find_if(
			implementations.begin(),
			implementations.end(),
			[](const auto& item)
			{
				return item.id == ids::treap;
			});
		require(registered != implementations.end() && !registered->builtin &&
			registered->owner_mod_id == "mngl.tree-practicum.treap",
			"Loaded Treap must appear in the factory registry as a mod implementation");

		auto implementation = classes.create(ids::treap);
		require(implementation != nullptr,
			"Treap factory did not create an algorithm instance");
		require(implementation->info().id == ids::treap &&
			implementation->info().display_name == "Treap",
			"Treap factory returned incorrect public metadata");

		auto value = implementation->create_empty(key_kind::signed_integer);
		require(value.nodes.empty() && value.root == no_tree_node,
			"New Treap file must start as an empty tree model");
		implementation->validate(value, key_kind::signed_integer);
		insert(*implementation, value, 40);
		insert(*implementation, value, 20);
		insert(*implementation, value, 60);
		insert(*implementation, value, 10);
		insert(*implementation, value, 30);
		insert(*implementation, value, 50);
		insert(*implementation, value, 70);
		check_model_for_display(value);
		implementation->validate(value, key_kind::signed_integer);

		const auto duplicate = implementation->insert(
			value,
			key{ std::int64_t{ 30 }, {}, false },
			key_kind::signed_integer,
			false);
		require(!duplicate.inserted && !duplicate.changed,
			"Treap must reject duplicate keys");
		const auto found = implementation->find(
			value,
			key{ std::int64_t{ 50 }, {}, false },
			key_kind::signed_integer);
		require(found.found && !found.path.empty() &&
			found.visited_nodes == found.path.size(),
			"Treap search should return a canvas highlight path");

		const auto first_serialization = implementation->serialize(value);
		require(first_serialization.starts_with("treap{"),
			"Treap must keep the existing .tree serialization format");
		auto reopened = implementation->deserialize(first_serialization);
		require(reopened.algorithm_id == ids::treap &&
			reopened.keys == key_kind::signed_integer,
			"Treap serialization should reopen as the same tree type and key kind");
		implementation->validate(reopened.value, key_kind::signed_integer);
		require(implementation->serialize(reopened.value) == first_serialization,
			"Treap serialization round trip should preserve priorities and shape");
		check_model_for_display(reopened.value);

		std::mt19937_64 random_engine{ 0x4d4e474cull };
		const auto bulk = implementation->insert_many(
			value,
			20,
			key_kind::signed_integer,
			random_engine);
		require(bulk.inserted_count == 20 && bulk.serialization.has_value(),
			"Treap bulk insertion should add the requested number of keys");
		implementation->validate(value, key_kind::signed_integer);
		check_model_for_display(value);

		const auto erased = implementation->erase(
			value,
			key{ std::int64_t{ 40 }, {}, false },
			key_kind::signed_integer);
		require(erased.erased && erased.changed,
			"Treap erase should remove an existing key");
		implementation->validate(value, key_kind::signed_integer);

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
				"Treap split left side must contain keys below the pivot");
		}
		for (const auto key_value : sorted_keys(right))
		{
			require(key_value >= 50,
				"Treap split right side must contain keys at or above the pivot");
		}
		require(implementation->merge(left, right, key_kind::signed_integer),
			"Treap merge should rejoin a non-empty split");
		implementation->validate(left, key_kind::signed_integer);
		check_model_for_display(left);
		require(sorted_keys(left) == keys_before_split,
			"Treap split followed by merge should preserve the key set");

		const auto save_path = std::filesystem::temp_directory_path() /
			("mngl-treap-roundtrip-" + std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()) +
				".tree");
		const auto saved_serialization = implementation->serialize(left);
		tree_practicum::write_tree_file(save_path, saved_serialization);
		const auto loaded_contents = tree_practicum::read_tree_file(save_path);
		const auto saved_document = implementation->deserialize(loaded_contents);
		std::error_code ignored;
		std::filesystem::remove(save_path, ignored);
		require(saved_document.algorithm_id == ids::treap &&
			implementation->serialize(saved_document.value) == saved_serialization,
			"Saved .tree file should reopen with the same Treap data");

		const auto unload = loader.unload("mngl.tree-practicum.treap");
		require(unload.state == mod_unload_state::deferred &&
			unload.live_objects > 0,
			"Treap library unload must wait while its algorithm object lives");
		implementation.reset();
		require(loader.collect_deferred() == 1,
			"Treap library should unload after its algorithm instance is destroyed");
	}
}

int main(int argc, char** argv)
{
	try
	{
		if (argc != 2)
		{
			throw std::invalid_argument(
				"usage: mngl_treap_tree_mod_integration <mod-package>");
		}
		verify_package(argv[1]);
		std::cout << "Treap mod load, operations, model, save, and unload passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
