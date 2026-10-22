#pragma once

#include "console.hpp"
#include "mod_command_catalog.hpp"
#include "mod_loader.hpp"
#include "tree_algorithm_api.hpp"
#include "tree_custom_command_api.hpp"
#include "tree_invariant_api.hpp"
#include "tree_practicum_storage.hpp"

#include <SDL3/SDL_filesystem.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace tree_practicum::console_cli
{
	[[nodiscard]] inline std::filesystem::path path_from_utf8(
		std::string_view value)
	{
		std::u8string encoded;
		encoded.reserve(value.size());
		for (const unsigned char byte : value)
		{
			encoded.push_back(static_cast<char8_t>(byte));
		}
		return std::filesystem::path(std::move(encoded));
	}

	struct parsed_bst_file
	{
		mngl::mods::tree::tree value;
		mngl::mods::tree::key_kind keys =
			mngl::mods::tree::key_kind::signed_integer;
	};

	class bst_file_parser
	{
		using tree = mngl::mods::tree::tree;
		using key_kind = mngl::mods::tree::key_kind;
		using key = mngl::mods::tree::key;
		using node = mngl::mods::tree::node;

		static constexpr std::size_t maximum_node_count = 1'000'000;
		static constexpr std::size_t maximum_depth = 1'024;

	public:
		explicit bst_file_parser(std::string_view source)
			: m_source(source)
		{
		}

		[[nodiscard]] parsed_bst_file parse()
		{
			skip_whitespace();
			if (!consume("bst{"))
			{
				fail("file does not contain a BST serialization");
			}
			m_value.root = parse_node(0);
			expect('}');
			skip_whitespace();
			if (m_position != m_source.size())
			{
				fail("unexpected characters after the tree");
			}
			assign_key_values();
			validate_ordering();
			return { std::move(m_value), m_keys };
		}

	private:
		[[noreturn]] void fail(std::string_view message) const
		{
			throw std::invalid_argument(
				"Invalid BST serialization at byte " +
				std::to_string(m_position) + ": " + std::string(message));
		}

		void skip_whitespace() noexcept
		{
			while (m_position < m_source.size())
			{
				const char value = m_source[m_position];
				if (value != ' ' && value != '\t' &&
					value != '\r' && value != '\n')
				{
					break;
				}
				++m_position;
			}
		}

		[[nodiscard]] bool consume(std::string_view value) noexcept
		{
			if (!m_source.substr(m_position).starts_with(value))
			{
				return false;
			}
			m_position += value.size();
			return true;
		}

		void expect(char value)
		{
			skip_whitespace();
			if (m_position >= m_source.size() ||
				m_source[m_position] != value)
			{
				fail(std::string("expected '") + value + "'");
			}
			++m_position;
		}

		[[nodiscard]] std::size_t parse_node(std::size_t depth)
		{
			skip_whitespace();
			if (consume("#"))
			{
				return mngl::mods::tree::no_tree_node;
			}
			if (depth >= maximum_depth)
			{
				fail("maximum tree depth exceeded");
			}
			expect('(');

			const std::size_t node_index = m_value.nodes.size();
			if (node_index >= maximum_node_count)
			{
				fail("maximum node count exceeded");
			}
			m_value.nodes.emplace_back();
			m_raw_keys.emplace_back(read_key());

			const std::size_t left = parse_node(depth + 1);
			expect(',');
			const std::size_t right = parse_node(depth + 1);
			expect(')');
			m_value.nodes[node_index].binary_children = { left, right };
			if (left != mngl::mods::tree::no_tree_node)
			{
				m_value.nodes[node_index].children.push_back(left);
			}
			if (right != mngl::mods::tree::no_tree_node)
			{
				m_value.nodes[node_index].children.push_back(right);
			}
			m_value.nodes[node_index].leaf = m_value.nodes[node_index].children.empty();
			return node_index;
		}

		[[nodiscard]] std::string read_key()
		{
			skip_whitespace();
			const std::size_t start = m_position;
			while (m_position < m_source.size() &&
				!m_source.substr(m_position).starts_with(",,"))
			{
				if (m_source[m_position] == '|' ||
					m_source[m_position] == '"')
				{
					fail("BST keys must be unquoted numeric values");
				}
				++m_position;
			}
			if (m_position == m_source.size())
			{
				fail("expected key separator ',,'");
			}
			std::string_view token = m_source.substr(start, m_position - start);
			m_position += 2;
			while (!token.empty() &&
				(token.back() == ' ' || token.back() == '\t' ||
					token.back() == '\r' || token.back() == '\n'))
			{
				token.remove_suffix(1);
			}
			if (token.empty())
			{
				fail("BST key cannot be empty");
			}
			return std::string(token);
		}

		[[nodiscard]] static bool is_integer_token(std::string_view token) noexcept
		{
			if (token.empty()) return false;
			if (token.front() == '+' || token.front() == '-')
			{
				token.remove_prefix(1);
			}
			return !token.empty() && std::all_of(
				token.begin(), token.end(), [](char value)
				{
					return value >= '0' && value <= '9';
				});
		}

		[[nodiscard]] static bool parse_integer(
			std::string_view token,
			std::int64_t& value) noexcept
		{
			if (token.starts_with('+')) token.remove_prefix(1);
			const auto result = std::from_chars(
				token.data(), token.data() + token.size(), value);
			return result.ec == std::errc{} &&
				result.ptr == token.data() + token.size();
		}

		[[nodiscard]] static bool parse_finite_double(
			std::string_view token,
			double& value) noexcept
		{
			if (token.starts_with('+')) token.remove_prefix(1);
			const auto result = std::from_chars(
				token.data(),
				token.data() + token.size(),
				value,
				std::chars_format::general);
			return result.ec == std::errc{} &&
				result.ptr == token.data() + token.size() &&
				std::isfinite(value);
		}

		void assign_key_values()
		{
			bool has_floating_point = false;
			bool has_out_of_range_integer = false;
			for (const auto& token : m_raw_keys)
			{
				if (is_integer_token(token))
				{
					std::int64_t integer = 0;
					if (!parse_integer(token, integer))
					{
						has_out_of_range_integer = true;
					}
				}
				else
				{
					double floating_point = 0.0;
					if (!parse_finite_double(token, floating_point))
					{
						fail("BST key must be a finite integer or number");
					}
					has_floating_point = true;
				}
			}

			if (has_floating_point && has_out_of_range_integer)
			{
				fail("cannot mix floating point keys and out-of-range integers");
			}
			if (has_out_of_range_integer)
			{
				fail("BST key is outside the supported int64 range");
			}
			m_keys = has_floating_point
				? key_kind::floating_point
				: key_kind::signed_integer;

			for (std::size_t index = 0; index < m_raw_keys.size(); ++index)
			{
				key parsed_key;
				if (m_keys == key_kind::floating_point)
				{
					double value = 0.0;
					if (!parse_finite_double(m_raw_keys[index], value))
					{
						fail("BST key must be a finite number");
					}
					parsed_key.value = value;
				}
				else
				{
					std::int64_t value = 0;
					if (!parse_integer(m_raw_keys[index], value))
					{
						fail("BST key must be an int64 value");
					}
					parsed_key.value = value;
				}
				m_value.nodes[index].keys.push_back(std::move(parsed_key));
			}
		}

		[[nodiscard]] int compare_keys(const key& left, const key& right) const
		{
			if (m_keys == key_kind::signed_integer)
			{
				const auto left_value = std::get<std::int64_t>(left.value);
				const auto right_value = std::get<std::int64_t>(right.value);
				return left_value < right_value ? -1 : left_value > right_value ? 1 : 0;
			}
			const auto left_value = std::get<double>(left.value);
			const auto right_value = std::get<double>(right.value);
			return left_value < right_value ? -1 : left_value > right_value ? 1 : 0;
		}

		void validate_ordering() const
		{
			if (m_value.nodes.empty()) return;
			struct frame
			{
				std::size_t index;
				std::size_t lower = mngl::mods::tree::no_tree_node;
				std::size_t upper = mngl::mods::tree::no_tree_node;
			};
			std::vector<frame> pending{ { m_value.root } };
			while (!pending.empty())
			{
				const frame current = pending.back();
				pending.pop_back();
				const auto& current_key = m_value.nodes[current.index].keys.front();
				if (current.lower != mngl::mods::tree::no_tree_node &&
					compare_keys(
						current_key,
						m_value.nodes[current.lower].keys.front()) <= 0)
				{
					fail("BST ordering invariant is violated");
				}
				if (current.upper != mngl::mods::tree::no_tree_node &&
					compare_keys(
						current_key,
						m_value.nodes[current.upper].keys.front()) >= 0)
				{
					fail("BST ordering invariant is violated");
				}
				const auto [left, right] =
					m_value.nodes[current.index].binary_children;
				if (right != mngl::mods::tree::no_tree_node)
				{
					pending.push_back({ right, current.index, current.upper });
				}
				if (left != mngl::mods::tree::no_tree_node)
				{
					pending.push_back({ left, current.lower, current.index });
				}
			}
		}

		std::string_view m_source;
		std::size_t m_position = 0;
		std::vector<std::string> m_raw_keys;
		tree m_value;
		key_kind m_keys = key_kind::signed_integer;
	};

	class application
	{
		struct current_tree_state
		{
			std::string tree_id;
			std::string algorithm_id;
			mngl::mods::tree::key_kind keys =
				mngl::mods::tree::key_kind::signed_integer;
			std::string file_path;
			std::string serialization;
		};

		struct loaded_tree_state
		{
			mngl::mods::tree::tree value;
			mngl::mods::tree::key_kind keys =
				mngl::mods::tree::key_kind::signed_integer;
			std::unique_ptr<mngl::mods::tree::algorithm> algorithm;
		};

		class bst_invariant_inspector final :
			public mngl::mods::tree::invariant_inspector
		{
		public:
			[[nodiscard]] std::string_view algorithm_id() const noexcept override
			{
				return mngl::mods::tree::ids::bst;
			}

			[[nodiscard]] mngl::mods::tree::invariant_report inspect(
				const mngl::mods::tree::tree& value,
				mngl::mods::tree::key_kind) const override
			{
				mngl::mods::tree::invariant_report report;
				try
				{
					std::vector<unsigned char> visited(value.nodes.size(), 0);
					std::vector<std::size_t> pending;
					if (value.nodes.empty())
					{
						if (value.root != mngl::mods::tree::no_tree_node)
							throw std::invalid_argument("empty tree has a root");
					}
					else
					{
						if (value.root >= value.nodes.size())
							throw std::invalid_argument("root index is out of range");
						pending.push_back(value.root);
					}
					while (!pending.empty())
					{
						const auto index = pending.back();
						pending.pop_back();
						if (index >= value.nodes.size() || visited[index] != 0)
							throw std::invalid_argument("invalid or repeated child link");
						visited[index] = 1;
						const auto& node = value.nodes[index];
						if (node.keys.size() != 1)
							throw std::invalid_argument("a BST node must contain one key");
						for (const auto child : node.binary_children)
						{
							if (child != mngl::mods::tree::no_tree_node)
								pending.push_back(child);
						}
					}
					if (std::count(visited.begin(), visited.end(), 1) !=
						static_cast<std::ptrdiff_t>(value.nodes.size()))
					{
						throw std::invalid_argument("unreachable nodes are present");
					}
					(void)application::serialize_bst(value);
					report.checks.push_back({
						"Порядок BST", true,
						"Для каждого узла ключи слева меньше, ключи справа больше." });
					report.checks.push_back({
						"Связность структуры", true,
						"Нет циклов и недостижимых узлов; у каждого узла один ключ." });
				}
				catch (const std::exception& error)
				{
					report.checks.push_back({
						"Структура BST", false,
						"Нарушение: " + std::string(error.what()) });
				}
				return report;
			}

			[[nodiscard]] std::string describe_node(
				const mngl::mods::tree::tree& value,
				mngl::mods::tree::key_kind,
				std::size_t node_index) const override
			{
				if (node_index >= value.nodes.size())
					throw std::out_of_range("Node index is outside the tree");
				const auto& node = value.nodes[node_index];
				if (node.keys.size() != 1)
					throw std::invalid_argument("A BST node must contain one key");
				return "node=" + std::to_string(node_index) +
					" key=" + format_key(node.keys.front()) +
					" left=" + format_child(node.binary_children[0]) +
					" right=" + format_child(node.binary_children[1]);
			}

		private:
			[[nodiscard]] static std::string format_key(
				const mngl::mods::tree::key& value)
			{
				if (const auto* integer = std::get_if<std::int64_t>(&value.value))
					return std::to_string(*integer);
				if (const auto* number = std::get_if<double>(&value.value))
					return std::to_string(*number);
				return value.detail;
			}

			[[nodiscard]] static std::string format_child(std::size_t child)
			{
				return child == mngl::mods::tree::no_tree_node
					? "null"
					: std::to_string(child);
			}
		};

	public:
		application()
			: m_mod_loader(m_mod_registry)
		{
			initialize_tree_mods();
			register_commands();
		}

		application(const application&) = delete;
		application& operator=(const application&) = delete;

		int run_process(int argc, char** argv) const
		{
			return m_dispatcher.run_process(argc, argv);
		}

	private:
		static std::string_view key_kind_name(
			mngl::mods::tree::key_kind kind) noexcept
		{
			using key_kind = mngl::mods::tree::key_kind;
			switch (kind)
			{
			case key_kind::text: return "text";
			case key_kind::floating_point: return "number";
			case key_kind::signed_integer: return "integer";
			case key_kind::big_integer: return "big-integer";
			}
			return "unknown";
		}

		static std::string supported_key_kinds(
			const mngl::mods::tree::algorithm_info& info)
		{
			using key_kind = mngl::mods::tree::key_kind;
			std::string result;
			for (const auto kind : {
				key_kind::signed_integer,
				key_kind::floating_point,
				key_kind::text,
				key_kind::big_integer })
			{
				if (!mngl::mods::tree::supports(
					info.supported_key_kinds,
					kind))
				{
					continue;
				}
				if (!result.empty()) result += ", ";
				result += key_kind_name(kind);
			}
			return result;
		}

		static bool valid_algorithm_info(
			std::string_view implementation_id,
			const mngl::mods::tree::algorithm_info& info) noexcept
		{
			if (info.id != implementation_id ||
				info.display_name.empty() ||
				info.file_stem.empty() ||
				info.serialization_prefix.empty() ||
				info.supported_key_kinds == 0)
			{
				return false;
			}
			return std::all_of(
				info.file_stem.begin(),
				info.file_stem.end(),
				[](char character)
				{
					return (character >= 'a' && character <= 'z') ||
						(character >= 'A' && character <= 'Z') ||
						(character >= '0' && character <= '9') ||
						character == '_' || character == '-';
				});
		}

		void initialize_tree_mods()
		{
			m_mod_registry.apply_declarations();
			(void)m_mod_registry.define_class<mngl::mods::tree::algorithm>(
				std::string(mngl::mods::tree::algorithm_class_id));
			(void)m_mod_registry.define_class<
				mngl::mods::tree::custom_command_handler>(
					std::string(mngl::mods::tree::custom_command_class_id));
			auto& invariant_inspectors = m_mod_registry.define_class<
				mngl::mods::tree::invariant_inspector>(
					std::string(mngl::mods::tree::invariant_inspector_class_id));
			invariant_inspectors.add_builtin(
				std::string(mngl::mods::tree::ids::bst),
				[] { return std::make_unique<bst_invariant_inspector>(); });

			std::error_code path_error;
			std::filesystem::path executable_directory;
			if (const char* executable_base = SDL_GetBasePath())
			{
				executable_directory = path_from_utf8(executable_base);
			}
			else
			{
				executable_directory = std::filesystem::current_path(path_error);
				if (path_error)
				{
					executable_directory = ".";
					m_mod_load_errors.push_back(
						"Could not determine the executable directory: " +
						path_error.message());
				}
			}
			m_executable_directory = executable_directory;

			const auto mods_directory = executable_directory / "mods";
			path_error.clear();
			if (!std::filesystem::is_directory(mods_directory, path_error))
			{
				if (path_error)
				{
					m_mod_load_errors.push_back(
						"Could not inspect the mods directory: " +
						path_error.message());
				}
				refresh_invariant_inspectors();
				load_custom_command_catalogs(mods_directory);
				return;
			}

			try
			{
				for (const auto& result : m_mod_loader.load_all(mods_directory))
				{
					if (!result)
					{
						m_mod_load_errors.push_back(
							(result.mod_id.empty() ? "<unknown>" : result.mod_id) +
							": " + result.message);
					}
				}
			}
			catch (const std::exception& error)
			{
				m_mod_load_errors.push_back(error.what());
			}
			refresh_invariant_inspectors();
			load_custom_command_catalogs(mods_directory);
		}

		void register_commands()
		{
			m_commands.add_command(
				"tree.types",
				"List the built-in tree and loaded tree algorithms")
				.handler([this]
				{
					return list_tree_types();
				});

			m_commands.add_command(
				"tree.list",
				"List trees currently open in this command session")
				.handler([this]
				{
					return list_open_trees();
				});

			m_commands.add_command(
				"tree.files",
				"List .tree files in a directory")
				.argument<std::string>("directory", "Directory to scan")
				.handler([this](const std::string& directory)
				{
					return list_tree_files(directory);
				});

			m_commands.add_command(
				"tree.create",
				"Create an empty tree file using a built-in or loaded algorithm")
				.argument<std::string>("algorithm-id", "ID from tree.types")
				.argument<std::string>("file", "Path for the new .tree file")
				.flag("force", "Replace the destination if it already exists")
				.handler([this](
					const std::string& algorithm_id,
					const std::string& file_path,
					bool force)
				{
					return create_tree(algorithm_id, file_path, force);
				});

			m_commands.add_command(
				"tree.status",
				"Show the selected tree metadata in this command session")
				.handler([this]
				{
					return describe_selected_tree(false);
				});

			m_commands.add_command(
				"tree.get.selected",
				"Show the selected tree and its serialization in this command session")
				.handler([this]
				{
					return describe_selected_tree(true);
				});

			m_commands.add_command(
				"tree.get.file",
				"Read, validate, select, and show a tree file")
				.argument<std::string>("file_name", "Path to the .tree file")
				.handler([this](const std::string& file_name)
				{
					return get_tree_file(file_name);
				});

			m_commands.add_command(
				"tree.open",
				"Open, validate, select, and show a .tree file")
				.argument<std::string>("file_name", "Path to the .tree file")
				.handler([this](const std::string& file_name)
				{
					return get_tree_file(file_name);
				});

			m_commands.add_command(
				"tree.select",
				"Select an open tree by session ID or file path")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the .tree file")
				.handler([this](const std::string& tree_id_or_file)
				{
					return select_tree(tree_id_or_file);
				});

			m_commands.add_command(
				"tree.close",
				"Close an open tree in this command session")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the .tree file")
				.handler([this](const std::string& tree_id_or_file)
				{
					return close_tree(tree_id_or_file);
				});

			m_commands.add_command(
				"tree.save-as",
				"Save a copy of an open tree to a new .tree file")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the source .tree file")
				.argument<std::string>("destination", "New .tree file path")
				.handler([this](const std::string& tree_id_or_file, const std::string& destination)
				{
					return save_tree_as(tree_id_or_file, destination);
				});

			m_commands.add_command(
				"tree.rename",
				"Rename an open .tree file without changing its session ID")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the .tree file")
				.argument<std::string>("destination", "New .tree file path")
				.handler([this](const std::string& tree_id_or_file, const std::string& destination)
				{
					return rename_tree_file(tree_id_or_file, destination);
				});

			m_commands.add_command(
				"tree.delete",
				"Delete a valid .tree file and close it in this session")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the .tree file")
				.handler([this](const std::string& tree_id_or_file)
				{
					return delete_tree_file(tree_id_or_file);
				});

			m_commands.add_command(
				"tree.invariants",
				"Inspect invariants for a tree using its built-in or mod inspector")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the .tree file")
				.handler([this](const std::string& tree_id_or_file)
				{
					return inspect_tree_invariants(tree_id_or_file);
				});

			m_commands.add_command(
				"tree.node",
				"Show algorithm-specific details for a node index")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the .tree file")
				.argument<std::int64_t>("node_index", "Zero-based node index")
				.handler([this](const std::string& tree_id_or_file, std::int64_t node_index)
				{
					return describe_tree_node(tree_id_or_file, node_index);
				});

			m_commands.add_command(
				"tree.commands",
				"List custom commands provided by the selected tree algorithm")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the .tree file")
				.handler([this](const std::string& tree_id_or_file)
				{
					return list_custom_tree_commands(tree_id_or_file);
				});

			m_commands.add_command(
				"tree.custom",
				"Run a mod command with arguments encoded as a JSON array")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the .tree file")
				.argument<std::string>("command_id", "Command name from tree.commands")
				.argument<std::string>("arguments_json", "JSON array matching the command parameter types")
				.handler([this](const std::string& tree_id_or_file, const std::string& command_id, const std::string& arguments_json)
				{
					return run_custom_tree_command(tree_id_or_file, command_id, arguments_json);
				});

			m_commands.add_command(
				"tree.find",
				"Find a key in an open tree by session ID or file path")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the .tree file")
				.argument<std::string>("key", "Key to find")
				.handler([this](const std::string& tree_id_or_file, const std::string& key)
				{
					return find_key(tree_id_or_file, key);
				});

			m_commands.add_command(
				"tree.insert",
				"Insert a key into an open tree by session ID or file path")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the .tree file")
				.argument<std::string>("key", "Key to insert")
				.handler([this](const std::string& tree_id_or_file, const std::string& key)
				{
					return insert_key(tree_id_or_file, key);
				});

			m_commands.add_command(
				"tree.erase",
				"Erase a key from an open tree by session ID or file path")
				.argument<std::string>("tree_id_or_file", "Session ID or path to the .tree file")
				.argument<std::string>("key", "Key to erase")
				.handler([this](const std::string& tree_id_or_file, const std::string& key)
				{
					return erase_key(tree_id_or_file, key);
				});

			m_commands.add_command(
				"tree.insert-random",
				"Insert a requested number of random unique keys")
				.argument<std::string>("tree_id_or_file", "Session ID or tree file path")
				.argument<std::int64_t>("count", "Number of unique keys to insert")
				.handler([this](
					const std::string& tree_id_or_file,
					std::int64_t count)
				{
					return insert_random(tree_id_or_file, count, std::nullopt);
				});

			m_commands.add_command(
				"tree.insert-random.seeded",
				"Insert random unique keys using a reproducible seed")
				.argument<std::string>("tree_id_or_file", "Session ID or tree file path")
				.argument<std::int64_t>("count", "Number of unique keys to insert")
				.argument<std::int64_t>("seed", "Random generator seed")
				.handler([this](
					const std::string& tree_id_or_file,
					std::int64_t count,
					std::int64_t seed)
				{
					return insert_random(tree_id_or_file, count, seed);
				});

			m_commands.add_command(
				"tree.merge",
				"Merge a source tree into the destination tree")
				.argument<std::string>("destination", "Destination session ID or tree file path")
				.argument<std::string>("source", "Source session ID or tree file path")
				.handler([this](
					const std::string& destination,
					const std::string& source)
				{
					return merge_trees(destination, source);
				});

			m_commands.add_command(
				"tree.split",
				"Split a tree into values below and at-or-above a pivot")
				.argument<std::string>("tree_id_or_file", "Session ID or source tree file path")
				.argument<std::string>("pivot", "Split boundary key")
				.argument<std::string>("left_file", "Output file for keys below the pivot")
				.argument<std::string>("right_file", "Output file for keys at or above the pivot")
				.handler([this](
					const std::string& tree_id_or_file,
					const std::string& pivot,
					const std::string& left_file,
					const std::string& right_file)
				{
					return split_tree(
						tree_id_or_file, pivot, left_file, right_file);
				});
		}

		[[nodiscard]] std::string describe_selected_tree(
			bool include_serialization) const
		{
			if (!m_selected_tree_id)
			{
				return "No tree is selected in this command session.\n";
			}
			const auto selected = m_open_trees.find(*m_selected_tree_id);
			if (selected == m_open_trees.end())
			{
				return "No tree is selected in this command session.\n";
			}
			const auto& current_tree = selected->second;
			std::string output =
				"tree_id=" + current_tree.tree_id +
				"\nalgorithm_id=" + current_tree.algorithm_id +
				"\nkey_kind=" +
				std::string(key_kind_name(current_tree.keys)) +
				"\nfile=" + current_tree.file_path + "\n";
			if (include_serialization)
			{
				output += "serialization=" + current_tree.serialization + "\n";
			}
			return output;
		}

		[[nodiscard]] static std::string path_to_utf8(
			const std::filesystem::path& path)
		{
			const auto encoded = path.u8string();
			return std::string(encoded.begin(), encoded.end());
		}

		[[nodiscard]] mngl::console::command_result list_open_trees() const
		{
			if (m_open_trees.empty())
			{
				return { "No trees are open in this command session.\n", 0, {} };
			}
			std::vector<const current_tree_state*> trees;
			trees.reserve(m_open_trees.size());
			for (const auto& [_, state] : m_open_trees)
				trees.push_back(&state);
			std::sort(trees.begin(), trees.end(), [](const auto* left, const auto* right)
			{
				return left->tree_id < right->tree_id;
			});

			std::string output;
			for (const auto* state : trees)
			{
				output += "tree_id=" + state->tree_id +
					"\nalgorithm_id=" + state->algorithm_id +
					"\nkey_kind=" + std::string(key_kind_name(state->keys)) +
					"\nfile=" + state->file_path +
					"\nselected=" +
					((m_selected_tree_id && *m_selected_tree_id == state->tree_id)
						? "true\n\n" : "false\n\n");
			}
			return { std::move(output), 0, {} };
		}

		[[nodiscard]] mngl::console::command_result list_tree_files(
			std::string_view directory_text) const
		{
			const auto directory = path_from_utf8(directory_text);
			std::error_code error;
			std::filesystem::directory_iterator iterator(directory, error);
			if (error)
				throw std::runtime_error("Cannot list directory: " + error.message());
			std::vector<std::filesystem::path> files;
			for (const auto& entry : iterator)
			{
				std::error_code entry_error;
				if (!entry.is_regular_file(entry_error) || entry_error ||
					entry.path().extension() != ".tree")
					continue;
				files.push_back(entry.path());
			}
			std::sort(files.begin(), files.end());
			std::string output = "directory=" + std::string(directory_text) + "\n";
			for (const auto& file : files)
				output += "file=" + path_to_utf8(file) + "\n";
			output += "count=" + std::to_string(files.size()) + "\n";
			return { std::move(output), 0, {} };
		}

		[[nodiscard]] current_tree_state* find_open_tree(
			std::string_view tree_id_or_file)
		{
			if (const auto found = m_open_trees.find(
				std::string(tree_id_or_file)); found != m_open_trees.end())
				return &found->second;
			for (auto& [_, state] : m_open_trees)
			{
				if (same_file_path(state.file_path, tree_id_or_file))
					return &state;
			}
			return nullptr;
		}

		[[nodiscard]] mngl::console::command_result select_tree(
			std::string_view tree_id_or_file)
		{
			(void)require_tree(tree_id_or_file);
			return { describe_selected_tree(false), 0, {} };
		}

		void select_default_open_tree()
		{
			if (m_open_trees.empty())
			{
				m_selected_tree_id.reset();
				return;
			}
			const auto first = std::min_element(
				m_open_trees.begin(),
				m_open_trees.end(),
				[](const auto& left, const auto& right)
				{
					return left.first < right.first;
				});
			m_selected_tree_id = first->first;
		}

		[[nodiscard]] mngl::console::command_result close_tree(
			std::string_view tree_id_or_file)
		{
			auto* state = find_open_tree(tree_id_or_file);
			if (!state)
				throw std::invalid_argument("Tree is not open in this command session");
			const std::string tree_id = state->tree_id;
			m_open_trees.erase(tree_id);
			if (m_selected_tree_id && *m_selected_tree_id == tree_id)
				select_default_open_tree();
			return {
				"closed_tree_id=" + tree_id +
				"\nselected_tree_id=" +
				(m_selected_tree_id ? *m_selected_tree_id : "") + "\n",
				0,
				{}
			};
		}

		[[nodiscard]] mngl::console::command_result save_tree_as(
			std::string_view tree_id_or_file,
			std::string_view destination_text)
		{
			const auto& state = require_tree(tree_id_or_file);
			const std::string source_id = state.tree_id;
			const std::string algorithm_id = state.algorithm_id;
			const auto keys = state.keys;
			const std::string serialization = state.serialization;
			if (same_file_path(state.file_path, destination_text))
				throw std::invalid_argument("Save-as destination must differ from the source file");
			for (const auto& [_, open_tree] : m_open_trees)
			{
				if (same_file_path(open_tree.file_path, destination_text))
					throw std::invalid_argument(
						"Save-as destination is already open as another tree");
			}
			const auto destination = path_from_utf8(destination_text);
			std::error_code error;
			if (std::filesystem::exists(destination, error))
				throw std::invalid_argument("Save-as destination already exists");
			if (error)
				throw std::runtime_error("Cannot inspect save-as destination: " + error.message());
			tree_practicum::write_tree_file(destination, serialization);
			const std::string new_id = register_tree_state({
				{}, algorithm_id, keys, std::string(destination_text), serialization });
			return {
				"source_tree_id=" + source_id +
				"\ntree_id=" + new_id +
				"\nfile=" + std::string(destination_text) + "\nsaved=true\n",
				0,
				{}
			};
		}

		[[nodiscard]] mngl::console::command_result rename_tree_file(
			std::string_view tree_id_or_file,
			std::string_view destination_text)
		{
			auto& state = require_tree(tree_id_or_file);
			const std::string tree_id = state.tree_id;
			if (same_file_path(state.file_path, destination_text))
				throw std::invalid_argument("Rename destination must differ from the source file");
			for (const auto& [other_id, other] : m_open_trees)
			{
				if (other_id != tree_id && same_file_path(other.file_path, destination_text))
					throw std::invalid_argument("Rename destination is already open as another tree");
			}
			const auto destination = path_from_utf8(destination_text);
			std::error_code error;
			if (std::filesystem::exists(destination, error))
				throw std::invalid_argument("Rename destination already exists");
			if (error)
				throw std::runtime_error("Cannot inspect rename destination: " + error.message());
			const auto source = path_from_utf8(state.file_path);
			std::filesystem::rename(source, destination, error);
			if (error)
				throw std::runtime_error("Cannot rename tree file: " + error.message());
			m_open_trees.at(tree_id).file_path = std::string(destination_text);
			return {
				"tree_id=" + tree_id + "\nfile=" +
				std::string(destination_text) + "\nrenamed=true\n",
				0,
				{}
			};
		}

		[[nodiscard]] mngl::console::command_result delete_tree_file(
			std::string_view tree_id_or_file)
		{
			auto& state = require_tree(tree_id_or_file);
			const std::string tree_id = state.tree_id;
			const std::string file = state.file_path;
			const auto path = path_from_utf8(file);
			std::error_code error;
			const bool removed = std::filesystem::remove(path, error);
			if (error)
				throw std::runtime_error("Cannot delete tree file: " + error.message());
			if (!removed)
				throw std::runtime_error("Tree file was not removed");
			m_open_trees.erase(tree_id);
			if (m_selected_tree_id && *m_selected_tree_id == tree_id)
				select_default_open_tree();
			return {
				"deleted_tree_id=" + tree_id + "\nfile=" + file + "\ndeleted=true\n",
				0,
				{}
			};
		}

		void refresh_invariant_inspectors()
		{
			m_invariant_inspectors.clear();
			auto& registry = m_mod_registry.classes<
				mngl::mods::tree::invariant_inspector>(
					mngl::mods::tree::invariant_inspector_class_id);
			for (const auto& implementation : registry.implementations())
			{
				auto inspector = registry.create(implementation.id);
				if (!inspector || inspector->algorithm_id() != implementation.id)
				{
					m_mod_load_errors.push_back(
						"Invalid invariant inspector registration for '" +
						implementation.id + "'");
					continue;
				}
				m_invariant_inspectors.emplace(
					implementation.id, std::move(inspector));
			}
		}

		[[nodiscard]] std::string custom_command_identity(
			std::string_view algorithm_id,
			std::string_view command_id) const
		{
			std::string identity(algorithm_id);
			identity.push_back('\n');
			identity.append(command_id);
			return identity;
		}

		void refresh_custom_command_handlers()
		{
			m_custom_command_handlers.clear();
			auto& registry = m_mod_registry.classes<
				mngl::mods::tree::custom_command_handler>(
					mngl::mods::tree::custom_command_class_id);
			for (const auto& implementation : registry.implementations())
			{
				auto handler = registry.create(implementation.id);
				if (handler)
					m_custom_command_handlers.emplace(
						implementation.id, std::move(handler));
			}
		}

		void load_custom_command_catalogs(
			const std::filesystem::path& mods_directory)
		{
			m_custom_commands.clear();
			refresh_custom_command_handlers();
			std::vector<std::filesystem::path> catalog_paths;
			const auto shared_catalog =
				m_executable_directory / "data" / "tree_commands.json";
			std::error_code error;
			if (std::filesystem::is_regular_file(shared_catalog, error))
				catalog_paths.push_back(shared_catalog);
			error.clear();
			try
			{
				if (std::filesystem::is_directory(mods_directory, error))
				{
					for (const auto& package :
						std::filesystem::directory_iterator(mods_directory))
					{
						std::error_code package_error;
						if (!package.is_directory(package_error) || package_error)
							continue;
						const auto path = package.path() / "commands.json";
						if (std::filesystem::is_regular_file(path, package_error))
							catalog_paths.push_back(path);
					}
				}
			}
			catch (const std::exception& scan_error)
			{
				m_mod_load_errors.push_back(
					"Could not scan mod command catalogs: " +
					std::string(scan_error.what()));
			}
			if (catalog_paths.size() > 1)
				std::sort(catalog_paths.begin() + 1, catalog_paths.end());
			for (const auto& path : catalog_paths)
			{
				try
				{
					std::error_code size_error;
					const auto size = std::filesystem::file_size(path, size_error);
					if (size_error || size > 4 * 1024 * 1024)
						throw std::runtime_error("catalog is unreadable or exceeds 4 MiB");
					std::ifstream input(path, std::ios::binary);
					if (!input)
						throw std::runtime_error("cannot open catalog");
					const std::string source{
						std::istreambuf_iterator<char>(input),
						std::istreambuf_iterator<char>() };
					const auto catalog = mngl::mods::command_catalog::parse(source);
					for (const auto& command : catalog.commands)
					{
						if (command.name == "find" || command.name == "insert" ||
							command.name == "insert_random" || command.name == "erase" ||
							command.name == "merge" || command.name == "split")
							continue;
						const auto handler = m_custom_command_handlers.find(
							command.algorithm_id);
						if (handler == m_custom_command_handlers.end() ||
							!handler->second->supports_command(command.name))
							continue;
						const auto identity = custom_command_identity(
							command.algorithm_id, command.name);
						m_custom_commands[identity] = command;
					}
				}
				catch (const std::exception& catalog_error)
				{
					m_mod_load_errors.push_back(
						"Could not load command catalog '" + path_to_utf8(path) +
						"': " + catalog_error.what());
				}
			}
		}

		[[nodiscard]] mngl::console::command_result inspect_tree_invariants(
			std::string_view tree_id_or_file)
		{
			const auto& state = require_tree(tree_id_or_file);
			auto loaded = load_tree_state(state);
			const auto inspector = m_invariant_inspectors.find(state.algorithm_id);
			if (inspector == m_invariant_inspectors.end())
			{
				return {
					"tree_id=" + state.tree_id +
					"\nalgorithm_id=" + state.algorithm_id +
					"\ninspector_available=false\nalgorithm_validation=passed\n",
					0,
					{}
				};
			}
			const auto report = inspector->second->inspect(
				loaded.value, loaded.keys);
			const bool all_passed = !report.checks.empty() &&
				std::all_of(report.checks.begin(), report.checks.end(),
					[](const auto& check) { return check.passed; });
			std::string output = "tree_id=" + state.tree_id +
				"\nalgorithm_id=" + state.algorithm_id +
				"\ninspector_available=true\nall_passed=" +
				(all_passed ? "true\n" : "false\n");
			for (const auto& check : report.checks)
			{
				output += "check=" + check.name +
					"\npassed=" + (check.passed ? "true\n" : "false\n") +
					"detail=" + check.detail + "\n\n";
			}
			return { std::move(output), all_passed ? 0 : 1, {} };
		}

		[[nodiscard]] mngl::console::command_result describe_tree_node(
			std::string_view tree_id_or_file,
			std::int64_t node_index)
		{
			if (node_index < 0)
				throw std::out_of_range("Node index cannot be negative");
			const auto& state = require_tree(tree_id_or_file);
			auto loaded = load_tree_state(state);
			const auto inspector = m_invariant_inspectors.find(state.algorithm_id);
			if (inspector == m_invariant_inspectors.end())
				throw std::logic_error(
					"No node inspector is available for this tree algorithm");
			return {
				"tree_id=" + state.tree_id + "\n" +
				inspector->second->describe_node(
					loaded.value,
					loaded.keys,
					static_cast<std::size_t>(node_index)) + "\n",
				0,
				{}
			};
		}

		[[nodiscard]] mngl::console::command_result list_custom_tree_commands(
			std::string_view tree_id_or_file)
		{
			const auto& state = require_tree(tree_id_or_file);
			std::vector<const mngl::mods::command_catalog::command*> commands;
			for (const auto& [_, command] : m_custom_commands)
			{
				if (command.algorithm_id == state.algorithm_id)
					commands.push_back(&command);
			}
			std::sort(commands.begin(), commands.end(), [](const auto* left, const auto* right)
			{
				return left->name < right->name;
			});
			std::string output = "tree_id=" + state.tree_id +
				"\nalgorithm_id=" + state.algorithm_id +
				"\ncommand_count=" + std::to_string(commands.size()) + "\n";
			for (const auto* command : commands)
			{
				output += "command=" + command->name +
					"\nsignature=" + command->signature +
					"\ndescription=" + command->description + "\n";
				for (const auto& parameter : command->parameters)
				{
					output += "parameter=" + parameter.name +
						" type=" + parameter.type +
						" label=" + parameter.label;
					if (!parameter.hint.empty())
						output += " hint=" + parameter.hint;
					output.push_back('\n');
				}
				output.push_back('\n');
			}
			return { std::move(output), 0, {} };
		}

		[[nodiscard]] std::vector<mngl::mods::tree::command_argument>
			parse_custom_command_arguments(
				const mngl::mods::command_catalog::command& command,
				std::string_view arguments_json) const
		{
			const auto parsed = mngl::mods::command_catalog::detail::parser(
				arguments_json).parse();
			const auto* values = std::get_if<
				mngl::mods::command_catalog::detail::json_value::array>(
					&parsed.value);
			if (!values)
				throw std::invalid_argument("Command arguments must be a JSON array");
			if (values->size() != command.parameters.size())
				throw std::invalid_argument(
					"Expected " + std::to_string(command.parameters.size()) +
					" JSON arguments, received " + std::to_string(values->size()));

			using json_value = mngl::mods::command_catalog::detail::json_value;
			std::vector<mngl::mods::tree::command_argument> result;
			result.reserve(values->size());
			for (std::size_t i = 0; i < values->size(); ++i)
			{
				const auto& parameter = command.parameters[i];
				const auto& value = (*values)[i].value;
				if (parameter.type == "text" || parameter.type == "bit_string")
				{
					const auto* text = std::get_if<std::string>(&value);
					if (!text)
						throw std::invalid_argument(
							"Argument '" + parameter.name + "' must be a JSON string");
					result.emplace_back(*text);
				}
				else if (parameter.type == "int64")
				{
					if (const auto* integer = std::get_if<std::int64_t>(&value))
						result.emplace_back(*integer);
					else if (const auto* unsigned_integer =
						std::get_if<std::uint64_t>(&value);
						unsigned_integer && *unsigned_integer <=
							static_cast<std::uint64_t>(
								std::numeric_limits<std::int64_t>::max()))
						result.emplace_back(
							static_cast<std::int64_t>(*unsigned_integer));
					else
						throw std::invalid_argument(
							"Argument '" + parameter.name + "' must be an int64 JSON number");
				}
				else if (parameter.type == "double")
				{
					if (const auto* number = std::get_if<double>(&value))
						result.emplace_back(*number);
					else if (const auto* integer = std::get_if<std::int64_t>(&value))
						result.emplace_back(static_cast<double>(*integer));
					else if (const auto* unsigned_integer =
						std::get_if<std::uint64_t>(&value))
						result.emplace_back(static_cast<double>(*unsigned_integer));
					else
						throw std::invalid_argument(
							"Argument '" + parameter.name + "' must be a JSON number");
				}
				else if (parameter.type == "boolean")
				{
					const auto* boolean = std::get_if<bool>(&value);
					if (!boolean)
						throw std::invalid_argument(
							"Argument '" + parameter.name + "' must be a JSON boolean");
					result.emplace_back(*boolean);
				}
				else
				{
					throw std::logic_error(
						"Unsupported mod command parameter type '" + parameter.type + "'");
				}
			}
			return result;
		}

		[[nodiscard]] mngl::console::command_result run_custom_tree_command(
			std::string_view tree_id_or_file,
			std::string_view command_id,
			std::string_view arguments_json)
		{
			auto& state = require_tree(tree_id_or_file);
			const auto definition = m_custom_commands.find(
				custom_command_identity(state.algorithm_id, command_id));
			if (definition == m_custom_commands.end())
				throw std::invalid_argument(
					"Unknown custom command for tree algorithm '" +
					state.algorithm_id + "'");
			const auto handler = m_custom_command_handlers.find(state.algorithm_id);
			if (handler == m_custom_command_handlers.end() ||
				!handler->second->supports_command(command_id))
				throw std::logic_error("Custom command handler is unavailable");
			auto loaded = load_tree_state(state);
			auto arguments = parse_custom_command_arguments(
				definition->second, arguments_json);
			const auto result = handler->second->execute(
				command_id,
				loaded.value,
				loaded.keys,
				std::span<const mngl::mods::tree::command_argument>(arguments));
			if (std::any_of(
				result.highlighted_nodes.begin(),
				result.highlighted_nodes.end(),
				[&](std::size_t index) { return index >= loaded.value.nodes.size(); }))
				throw std::invalid_argument(
					"Custom command returned a node index outside the tree");
			if (result.changed)
			{
				if (!loaded.algorithm)
					throw std::logic_error(
						"A custom command cannot modify the built-in BST");
				loaded.algorithm->validate(loaded.value, loaded.keys);
				save_tree_state(state, loaded);
			}
			std::string output = "tree_id=" + state.tree_id +
				"\ncommand=" + std::string(command_id) +
				"\nchanged=" + (result.changed ? "true\n" : "false\n") +
				"saved=" + (result.changed ? "true\n" : "false\n") +
				"highlighted_nodes=";
			for (std::size_t i = 0; i < result.highlighted_nodes.size(); ++i)
			{
				if (i != 0) output.push_back(',');
				output += std::to_string(result.highlighted_nodes[i]);
			}
			output += "\nmessage=" + result.message + "\n";
			return { std::move(output), 0, {} };
		}

		[[nodiscard]] mngl::console::command_result get_tree_file(
			std::string_view file_name)
		{
			try
			{
				(void)register_tree_state(read_tree_file_state(file_name));
				return {
					describe_selected_tree(true),
					0,
					{}
				};
			}
			catch (const std::exception& exception)
			{
				return {
					{},
					1,
					"Could not load tree file '" + std::string(file_name) +
						"': " + exception.what() + "\n"
				};
			}
		}

		[[nodiscard]] current_tree_state read_tree_file_state(
			std::string_view file_name)
		{
			const auto path = path_from_utf8(file_name);
			std::error_code error;
			const auto size = std::filesystem::file_size(path, error);
			if (error)
			{
				throw std::runtime_error(
					"Could not inspect file: " + error.message());
			}
			if (size > tree_practicum::detail::max_archive_entry_size ||
				size > static_cast<std::uint64_t>(
					std::numeric_limits<std::streamsize>::max()))
			{
				throw std::length_error(
					"Tree file exceeds the 256 MiB read limit");
			}

			std::ifstream input(path, std::ios::binary);
			if (!input)
			{
				throw std::runtime_error("Could not open file");
			}
			std::string serialization(static_cast<std::size_t>(size), '\0');
			input.read(
				serialization.data(),
				static_cast<std::streamsize>(serialization.size()));
			if (!input || input.gcount() !=
				static_cast<std::streamsize>(serialization.size()))
			{
				throw std::runtime_error("Failed while reading file");
			}
			return parse_tree_file(file_name, serialization);
		}

		[[nodiscard]] current_tree_state parse_tree_file(
			std::string_view file_name,
			const std::string& serialization)
		{
			if (serialization.starts_with(
				mngl::mods::tree::builtin_bst_info.serialization_prefix))
			{
				auto parsed = bst_file_parser{ serialization }.parse();
				return {
					{},
					std::string(mngl::mods::tree::ids::bst),
					parsed.keys,
					std::string(file_name),
					serialization
				};
			}

			auto& algorithms = m_mod_registry.classes<
				mngl::mods::tree::algorithm>(
					mngl::mods::tree::algorithm_class_id);
			struct algorithm_candidate
			{
				std::string id;
				std::unique_ptr<mngl::mods::tree::algorithm> instance;
				std::size_t matched_prefix_size = 0;
			};
			std::vector<algorithm_candidate> candidates;
			for (const auto& implementation : algorithms.implementations())
			{
				if (implementation.id == mngl::mods::tree::ids::bst)
				{
					continue;
				}
				try
				{
					auto algorithm = algorithms.create(implementation.id);
					if (algorithm && valid_algorithm_info(
						implementation.id,
						algorithm->info()))
					{
						const auto& info = algorithm->info();
						std::size_t matched_prefix_size = 0;
						const auto consider_prefix = [&](std::string_view prefix)
						{
							if (!prefix.empty() &&
								serialization.starts_with(prefix))
							{
								matched_prefix_size = std::max(
									matched_prefix_size,
									prefix.size());
							}
						};
						consider_prefix(info.serialization_prefix);
						for (const auto prefix :
							info.legacy_serialization_prefixes)
						{
							consider_prefix(prefix);
						}
						if (matched_prefix_size != 0)
						{
							candidates.push_back({
								implementation.id,
								std::move(algorithm),
								matched_prefix_size
							});
						}
					}
				}
				catch (const std::exception&)
				{
					// A broken unrelated module should not prevent reading other files.
				}
			}
			std::sort(
				candidates.begin(),
				candidates.end(),
				[](const auto& left, const auto& right)
				{
					if (left.matched_prefix_size != right.matched_prefix_size)
					{
						return left.matched_prefix_size >
							right.matched_prefix_size;
					}
					return left.id < right.id;
				});
			if (!candidates.empty())
			{
				auto& candidate = candidates.front();
				auto document = candidate.instance->deserialize(serialization);
				if (document.algorithm_id != candidate.id)
				{
					throw std::invalid_argument(
						"The module parsed a different algorithm ID");
				}
				const auto keys = document.keys.value_or(
					mngl::mods::tree::default_key_kind_for(
						candidate.instance->info()));
				if (!mngl::mods::tree::supports(
					candidate.instance->info().supported_key_kinds,
					keys))
				{
					throw std::invalid_argument(
						"Tree file uses an unsupported key kind");
				}
				candidate.instance->validate(document.value, keys);
				return {
					{},
					candidate.id,
					keys,
					std::string(file_name),
					serialization
				};
			}
			throw std::invalid_argument(
				"No available algorithm recognizes this tree serialization prefix");
		}

		[[nodiscard]] bool same_file_path(
			std::string_view left,
			std::string_view right) const
		{
			const auto left_path = path_from_utf8(left);
			const auto right_path = path_from_utf8(right);
			std::error_code error;
			if (std::filesystem::equivalent(left_path, right_path, error) && !error)
			{
				return true;
			}
			error.clear();
			const auto left_absolute = std::filesystem::absolute(left_path, error);
			if (error)
			{
				return left_path.lexically_normal() ==
					right_path.lexically_normal();
			}
			error.clear();
			const auto right_absolute = std::filesystem::absolute(right_path, error);
			if (error)
			{
				return left_path.lexically_normal() ==
					right_path.lexically_normal();
			}
			const auto normalized_left = left_absolute.lexically_normal();
			const auto normalized_right = right_absolute.lexically_normal();
			if (normalized_left == normalized_right)
			{
				return true;
			}
#if defined(_WIN32)
			const auto& left_native = normalized_left.native();
			const auto& right_native = normalized_right.native();
			if (left_native.size() <=
				static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
				right_native.size() <=
				static_cast<std::size_t>(std::numeric_limits<int>::max()))
			{
				return CompareStringOrdinal(
					left_native.data(),
					static_cast<int>(left_native.size()),
					right_native.data(),
					static_cast<int>(right_native.size()),
					TRUE) == CSTR_EQUAL;
			}
#endif
			return false;
		}

		[[nodiscard]] std::string register_tree_state(current_tree_state state)
		{
			for (auto& [tree_id, open_tree] : m_open_trees)
			{
				if (same_file_path(open_tree.file_path, state.file_path))
				{
					state.tree_id = tree_id;
					open_tree = std::move(state);
					m_selected_tree_id = tree_id;
					return tree_id;
				}
			}

			state.tree_id = "tree-" + std::to_string(m_next_tree_id++);
			const std::string tree_id = state.tree_id;
			m_open_trees.emplace(tree_id, std::move(state));
			m_selected_tree_id = tree_id;
			return tree_id;
		}

		[[nodiscard]] current_tree_state& require_tree(std::string_view tree_id)
		{
			auto found = m_open_trees.find(std::string(tree_id));
			if (found == m_open_trees.end())
			{
				const std::string registered_id = register_tree_state(
					read_tree_file_state(tree_id));
				found = m_open_trees.find(registered_id);
			}
			m_selected_tree_id = found->first;
			return found->second;
		}

		[[nodiscard]] std::unique_ptr<mngl::mods::tree::algorithm>
			create_algorithm(std::string_view algorithm_id)
		{
			auto& algorithms = m_mod_registry.classes<
				mngl::mods::tree::algorithm>(
					mngl::mods::tree::algorithm_class_id);
				for (const auto& implementation : algorithms.implementations())
			{
				if (implementation.id != algorithm_id ||
					implementation.id == mngl::mods::tree::ids::bst)
				{
					continue;
				}
				auto algorithm = algorithms.create(implementation.id);
				if (algorithm && valid_algorithm_info(
					implementation.id,
					algorithm->info()))
				{
					return algorithm;
				}
			}
			throw std::invalid_argument(
				"Tree algorithm '" + std::string(algorithm_id) +
				"' is not currently available.");
		}

		[[nodiscard]] loaded_tree_state load_tree_state(
			const current_tree_state& state)
		{
			if (state.algorithm_id == mngl::mods::tree::ids::bst)
			{
				auto parsed = bst_file_parser{ state.serialization }.parse();
				return { std::move(parsed.value), parsed.keys, {} };
			}

			auto algorithm = create_algorithm(state.algorithm_id);
			auto document = algorithm->deserialize(state.serialization);
			if (document.algorithm_id != state.algorithm_id)
			{
				throw std::invalid_argument(
					"The module parsed a different algorithm ID");
			}
			const auto keys = document.keys.value_or(state.keys);
			if (!mngl::mods::tree::supports(
				algorithm->info().supported_key_kinds,
				keys))
			{
				throw std::invalid_argument(
					"Tree file uses an unsupported key kind");
			}
			algorithm->validate(document.value, keys);
			return { std::move(document.value), keys, std::move(algorithm) };
		}

		[[nodiscard]] static std::string normalize_big_integer(
			std::string_view value)
		{
			bool negative = false;
			if (!value.empty() && (value.front() == '+' || value.front() == '-'))
			{
				negative = value.front() == '-';
				value.remove_prefix(1);
			}
			if (value.empty() || !std::all_of(
				value.begin(), value.end(), [](char digit)
				{
					return digit >= '0' && digit <= '9';
				}))
			{
				throw std::invalid_argument(
					"Key must be a decimal integer");
			}
			const auto first_nonzero = value.find_first_not_of('0');
			if (first_nonzero == std::string_view::npos)
			{
				return "0";
			}
			value.remove_prefix(first_nonzero);
			std::string result;
			if (negative) result.push_back('-');
			result.append(value);
			return result;
		}

		[[nodiscard]] static mngl::mods::tree::key parse_cli_key(
			std::string_view input,
			mngl::mods::tree::key_kind kind)
		{
			using key_kind = mngl::mods::tree::key_kind;
			mngl::mods::tree::key result;
			switch (kind)
			{
			case key_kind::text:
				result.value = std::string(input);
				return result;
			case key_kind::signed_integer:
				{
					if (input.starts_with('+')) input.remove_prefix(1);
					std::int64_t value = 0;
					const auto parsed = std::from_chars(
						input.data(), input.data() + input.size(), value);
					if (input.empty() || parsed.ec != std::errc{} ||
						parsed.ptr != input.data() + input.size())
					{
						throw std::invalid_argument(
							"Key must be a signed 64-bit integer");
					}
					result.value = value;
					return result;
				}
			case key_kind::floating_point:
				{
					if (input.starts_with('+')) input.remove_prefix(1);
					double value = 0.0;
					const auto parsed = std::from_chars(
						input.data(),
						input.data() + input.size(),
						value,
						std::chars_format::general);
					if (input.empty() || parsed.ec != std::errc{} ||
						parsed.ptr != input.data() + input.size() ||
						!std::isfinite(value))
					{
						throw std::invalid_argument(
							"Key must be a finite number");
					}
					result.value = value;
					return result;
				}
			case key_kind::big_integer:
				result.value = mngl::mods::tree::big_integer_text{
					normalize_big_integer(input) };
				return result;
			}
			throw std::invalid_argument("Unsupported key type");
		}

		[[nodiscard]] static int compare_keys(
			const mngl::mods::tree::key& left,
			const mngl::mods::tree::key& right,
			mngl::mods::tree::key_kind kind)
		{
			using key_kind = mngl::mods::tree::key_kind;
			if (kind == key_kind::text)
			{
				const auto& left_value = std::get<std::string>(left.value);
				const auto& right_value = std::get<std::string>(right.value);
				return left_value < right_value ? -1 : left_value > right_value ? 1 : 0;
			}
			if (kind == key_kind::signed_integer)
			{
				const auto left_value = std::get<std::int64_t>(left.value);
				const auto right_value = std::get<std::int64_t>(right.value);
				return left_value < right_value ? -1 : left_value > right_value ? 1 : 0;
			}
			if (kind == key_kind::floating_point)
			{
				const auto left_value = std::get<double>(left.value);
				const auto right_value = std::get<double>(right.value);
				return left_value < right_value ? -1 : left_value > right_value ? 1 : 0;
			}

			const auto& left_value =
				std::get<mngl::mods::tree::big_integer_text>(left.value).decimal;
			const auto& right_value =
				std::get<mngl::mods::tree::big_integer_text>(right.value).decimal;
			const bool left_negative = left_value.front() == '-';
			const bool right_negative = right_value.front() == '-';
			if (left_negative != right_negative) return left_negative ? -1 : 1;
			const std::string_view left_magnitude = left_negative
				? std::string_view(left_value).substr(1)
				: std::string_view(left_value);
			const std::string_view right_magnitude = right_negative
				? std::string_view(right_value).substr(1)
				: std::string_view(right_value);
			int comparison = left_magnitude.size() < right_magnitude.size()
				? -1
				: left_magnitude.size() > right_magnitude.size() ? 1
				: left_magnitude < right_magnitude ? -1
				: left_magnitude > right_magnitude ? 1 : 0;
			return left_negative ? -comparison : comparison;
		}

		[[nodiscard]] static std::pair<bool, std::size_t> bst_find(
			const mngl::mods::tree::tree& value,
			const mngl::mods::tree::key& target,
			mngl::mods::tree::key_kind keys)
		{
			std::size_t current = value.root;
			std::size_t visited = 0;
			while (current != mngl::mods::tree::no_tree_node)
			{
				const auto& node = value.nodes.at(current);
				++visited;
				const int comparison = compare_keys(
					target, node.keys.front(), keys);
				if (comparison == 0) return { true, visited };
				current = node.binary_children[comparison < 0 ? 0 : 1];
			}
			return { false, visited };
		}

		[[nodiscard]] static bool bst_insert(
			mngl::mods::tree::tree& value,
			mngl::mods::tree::key insert_key,
			mngl::mods::tree::key_kind keys)
		{
			constexpr std::size_t maximum_node_count = 1'000'000;
			constexpr std::size_t maximum_depth = 1'024;
			if (value.nodes.size() >= maximum_node_count)
			{
				throw std::length_error("Tree reached the 1,000,000 node limit");
			}
			if (value.root == mngl::mods::tree::no_tree_node)
			{
				mngl::mods::tree::node node;
				node.keys.push_back(std::move(insert_key));
				node.leaf = true;
				value.nodes.push_back(std::move(node));
				value.root = 0;
				return true;
			}

			std::size_t current = value.root;
			std::size_t depth = 0;
			for (;;)
			{
				auto& current_node = value.nodes.at(current);
				const int comparison = compare_keys(
					insert_key, current_node.keys.front(), keys);
				if (comparison == 0) return false;
				const std::size_t side = comparison < 0 ? 0 : 1;
				const auto child = current_node.binary_children[side];
				if (child != mngl::mods::tree::no_tree_node)
				{
					if (++depth >= maximum_depth)
					{
						throw std::length_error("Tree reached the maximum depth");
					}
					current = child;
					continue;
				}
				if (depth + 1 >= maximum_depth)
				{
					throw std::length_error("Tree reached the maximum depth");
				}
				mngl::mods::tree::node node;
				node.keys.push_back(std::move(insert_key));
				node.leaf = true;
				const std::size_t new_index = value.nodes.size();
				value.nodes.push_back(std::move(node));
				value.nodes[current].binary_children[side] = new_index;
				value.nodes[current].children.push_back(new_index);
				value.nodes[current].leaf = false;
				return true;
			}
		}

		[[nodiscard]] static std::size_t bst_erase_recursive(
			mngl::mods::tree::tree& value,
			std::size_t current,
			const mngl::mods::tree::key& target,
			mngl::mods::tree::key_kind keys,
			bool& erased,
			std::size_t depth)
		{
			if (current == mngl::mods::tree::no_tree_node) return current;
			if (depth >= 1'024)
			{
				throw std::length_error("Tree reached the maximum depth");
			}
			auto& node = value.nodes.at(current);
			const int comparison = compare_keys(target, node.keys.front(), keys);
			if (comparison < 0)
			{
				const auto left = bst_erase_recursive(
					value,
					node.binary_children[0],
					target,
					keys,
					erased,
					depth + 1);
				node.binary_children[0] = left;
			}
			else if (comparison > 0)
			{
				const auto right = bst_erase_recursive(
					value,
					node.binary_children[1],
					target,
					keys,
					erased,
					depth + 1);
				node.binary_children[1] = right;
			}
			else
			{
				erased = true;
				const auto left = node.binary_children[0];
				const auto right = node.binary_children[1];
				if (left == mngl::mods::tree::no_tree_node) return right;
				if (right == mngl::mods::tree::no_tree_node) return left;
				std::size_t successor = right;
				while (value.nodes[successor].binary_children[0] !=
					mngl::mods::tree::no_tree_node)
				{
					successor = value.nodes[successor].binary_children[0];
				}
				const auto successor_key =
					value.nodes[successor].keys.front();
				node.keys.front() = successor_key;
				bool successor_erased = false;
				const auto new_right = bst_erase_recursive(
					value,
					right,
					successor_key,
					keys,
					successor_erased,
					depth + 1);
				node.binary_children[1] = new_right;
			}

			auto& updated = value.nodes[current];
			updated.children.clear();
			for (const auto child : updated.binary_children)
			{
				if (child != mngl::mods::tree::no_tree_node)
				{
					updated.children.push_back(child);
				}
			}
			updated.leaf = updated.children.empty();
			return current;
		}

		[[nodiscard]] static std::size_t compact_bst_node(
			const mngl::mods::tree::tree& source,
			std::size_t source_index,
			mngl::mods::tree::tree& destination,
			std::size_t depth)
		{
			if (source_index == mngl::mods::tree::no_tree_node)
			{
				return mngl::mods::tree::no_tree_node;
			}
			if (depth >= 1'024)
			{
				throw std::length_error("Tree reached the maximum depth");
			}
			const std::size_t destination_index = destination.nodes.size();
			destination.nodes.push_back(source.nodes.at(source_index));
			destination.nodes[destination_index].children.clear();
			const auto [left, right] = source.nodes[source_index].binary_children;
			const auto compact_left = compact_bst_node(
				source, left, destination, depth + 1);
			const auto compact_right = compact_bst_node(
				source, right, destination, depth + 1);
			auto& node = destination.nodes[destination_index];
			node.binary_children = { compact_left, compact_right };
			if (compact_left != mngl::mods::tree::no_tree_node)
				node.children.push_back(compact_left);
			if (compact_right != mngl::mods::tree::no_tree_node)
				node.children.push_back(compact_right);
			node.leaf = node.children.empty();
			return destination_index;
		}

		[[nodiscard]] static bool bst_erase(
			mngl::mods::tree::tree& value,
			const mngl::mods::tree::key& target,
			mngl::mods::tree::key_kind keys)
		{
			bool erased = false;
			value.root = bst_erase_recursive(
				value,
				value.root,
				target,
				keys,
				erased,
				0);
			if (erased)
			{
				mngl::mods::tree::tree compacted;
				compacted.root = compact_bst_node(
					value, value.root, compacted, 0);
				value = std::move(compacted);
			}
			return erased;
		}

		static void append_bst_serialization(
			const mngl::mods::tree::tree& value,
			std::size_t node_index,
			std::size_t depth,
			std::string& output)
		{
			if (node_index == mngl::mods::tree::no_tree_node)
			{
				output.push_back('#');
				return;
			}
			if (depth >= 1'024 || node_index >= value.nodes.size())
			{
				throw std::invalid_argument(
					"BST structure exceeds serialization limits");
			}
			const auto& node = value.nodes[node_index];
			if (node.keys.size() != 1)
			{
				throw std::invalid_argument(
					"BST nodes must contain exactly one key");
			}
			output.push_back('(');
			const auto& stored_key = node.keys.front().value;
			std::array<char, 128> buffer{};
			if (const auto* integer = std::get_if<std::int64_t>(&stored_key))
			{
				const auto converted = std::to_chars(
					buffer.data(), buffer.data() + buffer.size(), *integer);
				output.append(buffer.data(), converted.ptr);
			}
			else if (const auto* number = std::get_if<double>(&stored_key))
			{
				const auto converted = std::to_chars(
					buffer.data(), buffer.data() + buffer.size(),
					*number, std::chars_format::general);
				std::string token(buffer.data(), converted.ptr);
				if (token.find_first_of(".eE") == std::string::npos)
				{
					token += ".0";
				}
				output += token;
			}
			else
			{
				throw std::invalid_argument(
					"BST keys must be signed integers or finite numbers");
			}
			output += ",,";
			append_bst_serialization(
				value, node.binary_children[0], depth + 1, output);
			output.push_back(',');
			append_bst_serialization(
				value, node.binary_children[1], depth + 1, output);
			output.push_back(')');
		}

		[[nodiscard]] static std::string serialize_bst(
			const mngl::mods::tree::tree& value)
		{
			std::string serialization = "bst{";
			append_bst_serialization(value, value.root, 0, serialization);
			serialization.push_back('}');
			(void)bst_file_parser{ serialization }.parse();
			return serialization;
		}

		void save_tree_state(
			current_tree_state& state,
			loaded_tree_state& loaded)
		{
			std::string serialization;
			if (state.algorithm_id == mngl::mods::tree::ids::bst)
			{
				serialization = serialize_bst(loaded.value);
			}
			else
			{
				loaded.algorithm->validate(loaded.value, loaded.keys);
				serialization = loaded.algorithm->serialize(loaded.value);
				if (serialization.empty())
				{
					throw std::runtime_error(
						"Tree algorithm returned an empty serialization");
				}
			}
			tree_practicum::write_tree_file(
				path_from_utf8(state.file_path),
				serialization);
			state.serialization = std::move(serialization);
		}

		[[nodiscard]] mngl::console::command_result find_key(
			std::string_view tree_id,
			std::string_view key_text)
		{
			auto& state = require_tree(tree_id);
			auto loaded = load_tree_state(state);
			const auto key = parse_cli_key(key_text, loaded.keys);
			bool found = false;
			std::size_t visited_nodes = 0;
			bool changed = false;
			if (state.algorithm_id == mngl::mods::tree::ids::bst)
			{
				const auto result = bst_find(loaded.value, key, loaded.keys);
				found = result.first;
				visited_nodes = result.second;
			}
			else
			{
				if (!mngl::mods::tree::supports(
					loaded.algorithm->info().supported_operations,
					mngl::mods::tree::operation::find))
				{
					throw std::logic_error("Find is not supported by this algorithm");
				}
				auto result = loaded.algorithm->find(
					loaded.value, key, loaded.keys);
				found = result.found;
				visited_nodes = result.visited_nodes;
				changed = result.changed;
			}
			if (changed) save_tree_state(state, loaded);
			return {
				"tree_id=" + state.tree_id +
				"\nfound=" + (found ? "true" : "false") +
				"\nvisited_nodes=" + std::to_string(visited_nodes) +
				(changed ? "\nsaved=true\n" : "\nsaved=false\n"),
				0,
				{}
			};
		}

		[[nodiscard]] mngl::console::command_result insert_key(
			std::string_view tree_id,
			std::string_view key_text)
		{
			auto& state = require_tree(tree_id);
			auto loaded = load_tree_state(state);
			auto key = parse_cli_key(key_text, loaded.keys);
			bool inserted = false;
			bool changed = false;
			if (state.algorithm_id == mngl::mods::tree::ids::bst)
			{
				inserted = bst_insert(loaded.value, std::move(key), loaded.keys);
				changed = inserted;
			}
			else
			{
				if (!mngl::mods::tree::supports(
					loaded.algorithm->info().supported_operations,
					mngl::mods::tree::operation::insert))
				{
					throw std::logic_error("Insert is not supported by this algorithm");
				}
				auto result = loaded.algorithm->insert(
					loaded.value, std::move(key), loaded.keys, false);
				inserted = result.inserted;
				changed = result.changed;
			}
			if (changed) save_tree_state(state, loaded);
			return {
				"tree_id=" + state.tree_id +
				"\ninserted=" + (inserted ? "true" : "false") +
				"\nsaved=" + (changed ? "true\n" : "false\n"),
				0,
				{}
			};
		}

		[[nodiscard]] mngl::console::command_result erase_key(
			std::string_view tree_id,
			std::string_view key_text)
		{
			auto& state = require_tree(tree_id);
			auto loaded = load_tree_state(state);
			const auto key = parse_cli_key(key_text, loaded.keys);
			bool erased = false;
			bool changed = false;
			if (state.algorithm_id == mngl::mods::tree::ids::bst)
			{
				erased = bst_erase(loaded.value, key, loaded.keys);
				changed = erased;
			}
			else
			{
				if (!mngl::mods::tree::supports(
					loaded.algorithm->info().supported_operations,
					mngl::mods::tree::operation::erase))
				{
					throw std::logic_error("Erase is not supported by this algorithm");
				}
				const auto result = loaded.algorithm->erase(
					loaded.value, key, loaded.keys);
				erased = result.erased;
				changed = result.changed;
			}
			if (changed) save_tree_state(state, loaded);
			return {
				"tree_id=" + state.tree_id +
				"\nerased=" + (erased ? "true" : "false") +
				"\nsaved=" + (changed ? "true\n" : "false\n"),
				0,
				{}
			};
		}

		[[nodiscard]] mngl::console::command_result insert_random(
			std::string_view tree_id_or_file,
			std::int64_t requested_count,
			std::optional<std::int64_t> seed)
		{
			if (requested_count < 0 || requested_count > 1'000'000)
			{
				throw std::out_of_range(
					"count must be between 0 and 1,000,000");
			}
			auto& state = require_tree(tree_id_or_file);
			auto loaded = load_tree_state(state);
			const auto count = static_cast<std::uint64_t>(requested_count);
			std::mt19937_64 random_engine = [&]
			{
				if (seed)
				{
					return std::mt19937_64{
						static_cast<std::uint64_t>(*seed) };
				}
				std::random_device device;
				return std::mt19937_64{ device() };
			}();

			std::uint64_t inserted_count = 0;
			if (state.algorithm_id == mngl::mods::tree::ids::bst)
			{
				if (loaded.value.nodes.size() + count > 1'000'000)
				{
					throw std::length_error(
						"BST random insertion would exceed the 1,000,000 node limit");
				}
				std::uint64_t attempts = 0;
				const std::uint64_t attempt_limit = count * 32 + 1024;
				while (inserted_count < count && attempts < attempt_limit)
				{
					++attempts;
					mngl::mods::tree::key random_key;
					if (loaded.keys ==
						mngl::mods::tree::key_kind::signed_integer)
					{
						std::uniform_int_distribution<std::int64_t> distribution;
						random_key.value = distribution(random_engine);
					}
					else if (loaded.keys ==
						mngl::mods::tree::key_kind::floating_point)
					{
						const double unit = std::generate_canonical<
							double,
							std::numeric_limits<double>::digits>(random_engine);
						random_key.value = unit * 2.0 - 1.0;
					}
					else
					{
						throw std::logic_error(
							"BST random insertion supports integer and number keys");
					}
					if (bst_insert(
						loaded.value, std::move(random_key), loaded.keys))
					{
						++inserted_count;
					}
				}
				if (inserted_count != count)
				{
					throw std::runtime_error(
						"Could not generate the requested number of unique keys");
				}
			}
			else
			{
				if (!mngl::mods::tree::supports(
					loaded.algorithm->info().supported_operations,
					mngl::mods::tree::operation::random_insert))
				{
					throw std::logic_error(
						"Random insertion is not supported by this algorithm");
				}
				const auto result = loaded.algorithm->insert_many(
					loaded.value, count, loaded.keys, random_engine);
				inserted_count = result.inserted_count;
				if (inserted_count != count)
				{
					throw std::runtime_error(
						"Tree algorithm inserted a different number of unique keys than requested");
				}
			}

			if (inserted_count != 0)
			{
				save_tree_state(state, loaded);
			}
			return {
				"tree_id=" + state.tree_id +
				"\ninserted_count=" + std::to_string(inserted_count) +
				"\nsaved=" + (inserted_count != 0 ? "true\n" : "false\n"),
				0,
				{}
			};
		}

		[[nodiscard]] static std::size_t bst_minimum(
			const mngl::mods::tree::tree& value,
			std::size_t root)
		{
			while (root != mngl::mods::tree::no_tree_node &&
				value.nodes.at(root).binary_children[0] !=
					mngl::mods::tree::no_tree_node)
			{
				root = value.nodes[root].binary_children[0];
			}
			return root;
		}

		[[nodiscard]] static std::size_t bst_maximum(
			const mngl::mods::tree::tree& value,
			std::size_t root)
		{
			while (root != mngl::mods::tree::no_tree_node &&
				value.nodes.at(root).binary_children[1] !=
					mngl::mods::tree::no_tree_node)
			{
				root = value.nodes[root].binary_children[1];
			}
			return root;
		}

		static void rebuild_bst_node_children(
			mngl::mods::tree::node& node)
		{
			node.children.clear();
			for (const auto child : node.binary_children)
			{
				if (child != mngl::mods::tree::no_tree_node)
				{
					node.children.push_back(child);
				}
			}
			node.leaf = node.children.empty();
		}

		[[nodiscard]] static bool bst_merge(
			mngl::mods::tree::tree& destination,
			const mngl::mods::tree::tree& source,
			mngl::mods::tree::key_kind keys)
		{
			if (source.root == mngl::mods::tree::no_tree_node)
			{
				return false;
			}
			if (destination.root == mngl::mods::tree::no_tree_node)
			{
				destination = source;
				return true;
			}
			const auto destination_max = bst_maximum(
				destination, destination.root);
			const auto source_min = bst_minimum(source, source.root);
			if (compare_keys(
				destination.nodes[destination_max].keys.front(),
				source.nodes[source_min].keys.front(),
				keys) >= 0)
			{
				throw std::invalid_argument(
					"Merge requires every destination key to be below every source key");
			}
			if (destination.nodes.size() + source.nodes.size() > 1'000'000)
			{
				throw std::length_error(
					"Merge would exceed the 1,000,000 node limit");
			}

			const std::size_t offset = destination.nodes.size();
			auto merged = destination;
			for (const auto& source_node : source.nodes)
			{
				auto copy = source_node;
				for (auto& child : copy.binary_children)
				{
					if (child != mngl::mods::tree::no_tree_node) child += offset;
				}
				copy.children.clear();
				for (const auto child : copy.binary_children)
				{
					if (child != mngl::mods::tree::no_tree_node)
						copy.children.push_back(child);
				}
				copy.leaf = copy.children.empty();
				merged.nodes.push_back(std::move(copy));
			}
			const std::size_t merged_source_root = source.root + offset;
			const std::size_t merged_source_minimum = source_min + offset;
			merged.nodes[merged_source_minimum].binary_children[0] =
				destination.root;
			rebuild_bst_node_children(merged.nodes[merged_source_minimum]);
			merged.root = merged_source_root;
			destination = std::move(merged);
			return true;
		}

		[[nodiscard]] static std::size_t clone_bst_subtree(
			const mngl::mods::tree::tree& source,
			std::size_t source_index,
			mngl::mods::tree::tree& destination,
			std::size_t depth)
		{
			if (source_index == mngl::mods::tree::no_tree_node)
			{
				return mngl::mods::tree::no_tree_node;
			}
			if (depth >= 1'024 || destination.nodes.size() >= 1'000'000)
			{
				throw std::length_error("BST split exceeds structural limits");
			}
			const std::size_t index = destination.nodes.size();
			destination.nodes.push_back(source.nodes.at(source_index));
			destination.nodes[index].children.clear();
			const auto [left, right] = source.nodes[source_index].binary_children;
			const auto cloned_left = clone_bst_subtree(
				source, left, destination, depth + 1);
			const auto cloned_right = clone_bst_subtree(
				source, right, destination, depth + 1);
			auto& node = destination.nodes[index];
			node.binary_children = { cloned_left, cloned_right };
			rebuild_bst_node_children(node);
			return index;
		}

		[[nodiscard]] static std::size_t append_split_bst_node(
			const mngl::mods::tree::node& source_node,
			std::size_t left,
			std::size_t right,
			mngl::mods::tree::tree& destination)
		{
			if (destination.nodes.size() >= 1'000'000)
			{
				throw std::length_error("BST split exceeds the 1,000,000 node limit");
			}
			const std::size_t index = destination.nodes.size();
			destination.nodes.push_back(source_node);
			auto& node = destination.nodes[index];
			node.binary_children = { left, right };
			rebuild_bst_node_children(node);
			return index;
		}

		[[nodiscard]] static std::pair<std::size_t, std::size_t>
			bst_split_recursive(
				const mngl::mods::tree::tree& source,
				std::size_t source_index,
				const mngl::mods::tree::key& pivot,
				mngl::mods::tree::key_kind keys,
				mngl::mods::tree::tree& left_tree,
				mngl::mods::tree::tree& right_tree,
				std::size_t depth)
		{
			if (source_index == mngl::mods::tree::no_tree_node)
			{
				return {
					mngl::mods::tree::no_tree_node,
					mngl::mods::tree::no_tree_node
				};
			}
			if (depth >= 1'024)
			{
				throw std::length_error("BST split exceeds the maximum depth");
			}
			const auto& source_node = source.nodes.at(source_index);
			const auto [source_left, source_right] = source_node.binary_children;
			if (compare_keys(source_node.keys.front(), pivot, keys) < 0)
			{
				const auto left_subtree = clone_bst_subtree(
					source, source_left, left_tree, depth + 1);
				const auto [right_left, right_right] = bst_split_recursive(
					source,
					source_right,
					pivot,
					keys,
					left_tree,
					right_tree,
					depth + 1);
				const auto left_root = append_split_bst_node(
					source_node, left_subtree, right_left, left_tree);
				return { left_root, right_right };
			}

			const auto [left_left, left_right] = bst_split_recursive(
				source,
				source_left,
				pivot,
				keys,
				left_tree,
				right_tree,
				depth + 1);
			const auto right_subtree = clone_bst_subtree(
				source, source_right, right_tree, depth + 1);
			const auto right_root = append_split_bst_node(
				source_node, left_right, right_subtree, right_tree);
			return { left_left, right_root };
		}

		[[nodiscard]] mngl::console::command_result merge_trees(
			std::string_view destination_id_or_file,
			std::string_view source_id_or_file)
		{
			const std::string destination_id =
				require_tree(destination_id_or_file).tree_id;
			const std::string source_id =
				require_tree(source_id_or_file).tree_id;
			auto& destination_state = m_open_trees.at(destination_id);
			auto& source_state = m_open_trees.at(source_id);
			if (&destination_state == &source_state ||
				same_file_path(
					destination_state.file_path,
					source_state.file_path))
			{
				throw std::invalid_argument(
					"Merge requires two different tree files");
			}
			if (destination_state.algorithm_id != source_state.algorithm_id ||
				destination_state.keys != source_state.keys)
			{
				throw std::invalid_argument(
					"Merge requires the same algorithm and key kind on both trees");
			}
			auto destination = load_tree_state(destination_state);
			auto source = load_tree_state(source_state);
			bool changed = false;
			if (destination_state.algorithm_id == mngl::mods::tree::ids::bst)
			{
				changed = bst_merge(
					destination.value, source.value, destination.keys);
			}
			else
			{
				if (!mngl::mods::tree::supports(
					destination.algorithm->info().supported_operations,
					mngl::mods::tree::operation::merge))
				{
					throw std::logic_error("Merge is not supported by this algorithm");
				}
				changed = destination.algorithm->merge(
					destination.value, source.value, destination.keys);
			}
			if (changed) save_tree_state(destination_state, destination);
			return {
				"destination_tree_id=" + destination_state.tree_id +
				"\nsource_tree_id=" + source_state.tree_id +
				"\nchanged=" + (changed ? "true\n" : "false\n") +
				"destination_saved=" + (changed ? "true\n" : "false\n") +
				"source_changed=false\n",
				0,
				{}
			};
		}

		[[nodiscard]] mngl::console::command_result split_tree(
			std::string_view tree_id_or_file,
			std::string_view pivot_text,
			std::string_view left_file,
			std::string_view right_file)
		{
			auto& source_state = require_tree(tree_id_or_file);
			const auto left_path = path_from_utf8(left_file);
			const auto right_path = path_from_utf8(right_file);
			if (same_file_path(source_state.file_path, left_file) ||
				same_file_path(source_state.file_path, right_file) ||
				same_file_path(left_file, right_file))
			{
				throw std::invalid_argument(
					"Split output files must be different from each other and from the source");
			}
			for (const auto& [_, open_tree] : m_open_trees)
			{
				if (same_file_path(open_tree.file_path, left_file) ||
					same_file_path(open_tree.file_path, right_file))
					throw std::invalid_argument(
						"Split output path is already open as a tree file");
			}
			for (const auto* output_path : { &left_path, &right_path })
			{
				std::error_code error;
				if (std::filesystem::exists(*output_path, error))
					throw std::invalid_argument(
						"Split output files must not already exist");
				if (error)
					throw std::runtime_error(
						"Cannot inspect split output path: " + error.message());
			}
			const std::string source_session_id = source_state.tree_id;
			const std::string source_algorithm_id = source_state.algorithm_id;
			const auto source_key_kind = source_state.keys;
			auto source = load_tree_state(source_state);
			const auto pivot = parse_cli_key(pivot_text, source.keys);
			std::string left_serialization;
			std::string right_serialization;
			if (source_state.algorithm_id == mngl::mods::tree::ids::bst)
			{
				mngl::mods::tree::tree left_tree;
				mngl::mods::tree::tree right_tree;
				const auto roots = bst_split_recursive(
					source.value,
					source.value.root,
					pivot,
					source.keys,
					left_tree,
					right_tree,
					0);
				left_tree.root = roots.first;
				right_tree.root = roots.second;
				left_serialization = serialize_bst(left_tree);
				right_serialization = serialize_bst(right_tree);
			}
			else
			{
				if (!mngl::mods::tree::supports(
					source.algorithm->info().supported_operations,
					mngl::mods::tree::operation::split))
				{
					throw std::logic_error("Split is not supported by this algorithm");
				}
				auto [left_tree, right_tree] = source.algorithm->split(
					source.value, pivot, source.keys);
				source.algorithm->validate(left_tree, source.keys);
				source.algorithm->validate(right_tree, source.keys);
				left_serialization = source.algorithm->serialize(left_tree);
				right_serialization = source.algorithm->serialize(right_tree);
			}
			if (left_serialization.empty() || right_serialization.empty())
			{
				throw std::runtime_error(
					"Tree algorithm returned an empty split serialization");
			}

			bool left_output_written = false;
			try
			{
				tree_practicum::write_tree_file(
					left_path,
					left_serialization);
				left_output_written = true;
				tree_practicum::write_tree_file(
					right_path,
					right_serialization);
			}
			catch (...)
			{
				if (left_output_written)
				{
					std::error_code cleanup_error;
					std::filesystem::remove(left_path, cleanup_error);
					if (cleanup_error)
					{
						throw std::filesystem::filesystem_error(
							"Split failed and its partial output could not be removed",
							left_path,
							cleanup_error);
					}
				}
				throw;
			}
			const std::string left_id = register_tree_state({
				{}, source_algorithm_id, source_key_kind,
				std::string(left_file), std::move(left_serialization) });
			const std::string right_id = register_tree_state({
				{}, source_algorithm_id, source_key_kind,
				std::string(right_file), std::move(right_serialization) });
			return {
				"source_tree_id=" + source_session_id +
				"\nleft_tree_id=" + left_id +
				"\nright_tree_id=" + right_id +
				"\nleft_file=" + std::string(left_file) +
				"\nright_file=" + std::string(right_file) + "\n",
				0,
				{}
			};
		}

		[[nodiscard]] std::string list_tree_types()
		{
			std::vector<std::string> lines;
			lines.push_back(
				std::string(mngl::mods::tree::builtin_bst_info.id) + "\t" +
				std::string(mngl::mods::tree::builtin_bst_info.display_name) +
				" (keys: " + supported_key_kinds(
					mngl::mods::tree::builtin_bst_info) + ")");

			auto& algorithms = m_mod_registry.classes<
				mngl::mods::tree::algorithm>(
					mngl::mods::tree::algorithm_class_id);
			for (const auto& implementation : algorithms.implementations())
			{
				if (implementation.id == mngl::mods::tree::ids::bst)
				{
					continue;
				}
				try
				{
					auto algorithm = algorithms.create(implementation.id);
					if (!algorithm || !valid_algorithm_info(
						implementation.id,
						algorithm->info()))
					{
						continue;
					}
					lines.push_back(
						implementation.id + "\t" +
						std::string(algorithm->info().display_name) +
						" (keys: " + supported_key_kinds(algorithm->info()) + ")");
				}
				catch (const std::exception&)
				{
					// A broken optional module is omitted from the command list.
				}
			}
			std::sort(lines.begin(), lines.end());

			std::string output;
			for (const auto& line : lines)
			{
				output += line;
				output.push_back('\n');
			}
			if (!m_mod_load_errors.empty())
			{
				output += "\nSome mods could not be loaded:\n";
				for (const auto& error : m_mod_load_errors)
				{
					output += "  " + error + "\n";
				}
			}
			return output;
		}

		[[nodiscard]] mngl::console::command_result create_tree(
			std::string_view algorithm_id,
			std::string_view file_path_text,
			bool force)
		{
			if (file_path_text.empty())
				throw std::invalid_argument("Tree file path cannot be empty");
			const auto output_path = path_from_utf8(file_path_text);
			std::error_code path_error;
			const bool destination_exists = std::filesystem::exists(
				output_path, path_error);
			if (path_error)
				throw std::runtime_error(
					"Cannot inspect tree file destination: " + path_error.message());
			if (destination_exists && !force)
				throw std::invalid_argument(
					"Tree file already exists; pass --force to replace it");

			std::string serialization;
			mngl::mods::tree::key_kind selected_key_kind{};
			if (algorithm_id == mngl::mods::tree::ids::bst)
			{
				selected_key_kind =
					mngl::mods::tree::default_key_kind_for(
						mngl::mods::tree::builtin_bst_info);
				serialization = "bst{#}";
			}
			else
			{
				auto& algorithms = m_mod_registry.classes<
					mngl::mods::tree::algorithm>(
						mngl::mods::tree::algorithm_class_id);
				const auto implementations = algorithms.implementations();
				const auto available = std::find_if(
					implementations.begin(),
					implementations.end(),
					[algorithm_id](const auto& implementation)
					{
						return implementation.id == algorithm_id;
					});
				if (available == implementations.end())
				{
					return {
						{},
						2,
						"Unknown or unavailable tree algorithm '" +
							std::string(algorithm_id) +
							"'. Run 'tree.types' to list loaded algorithms.\n"
					};
				}
				auto algorithm = algorithms.create(algorithm_id);
				if (!algorithm || !valid_algorithm_info(
					algorithm_id,
					algorithm->info()))
				{
					return {
						{},
						2,
						"Unknown or unavailable tree algorithm '" +
							std::string(algorithm_id) +
							"'. Run 'tree.types' to list loaded algorithms.\n"
					};
				}
				selected_key_kind =
					mngl::mods::tree::default_key_kind_for(algorithm->info());
				auto empty_tree = algorithm->create_empty(selected_key_kind);
				algorithm->validate(empty_tree, selected_key_kind);
				serialization = algorithm->serialize(empty_tree);
			}

			if (serialization.empty())
			{
				return { {}, 1, "Tree algorithm returned an empty serialization.\n" };
			}

			tree_practicum::write_tree_file(output_path, serialization);
			const std::string tree_id = register_tree_state({
				{},
				std::string(algorithm_id),
				selected_key_kind,
				std::string(file_path_text),
				serialization
			});
			return {
				"tree_id=" + tree_id +
				"\nCreated empty tree '" + std::string(algorithm_id) +
					"' with key kind '" +
					std::string(key_kind_name(selected_key_kind)) +
					"' at '" + std::string(file_path_text) + "'.\n" +
					"replaced_existing=" +
					(destination_exists ? "true\n" : "false\n"),
				0,
				{}
			};
		}

		mngl::mods::registry m_mod_registry;
		mngl::mods::mod_loader m_mod_loader;
		mngl::console::registry m_commands;
		mngl::console::dispatcher m_dispatcher{ m_commands };
		std::filesystem::path m_executable_directory;
		std::vector<std::string> m_mod_load_errors;
		std::unordered_map<
			std::string,
			std::unique_ptr<mngl::mods::tree::invariant_inspector>>
			m_invariant_inspectors;
		std::unordered_map<
			std::string,
			std::unique_ptr<mngl::mods::tree::custom_command_handler>>
			m_custom_command_handlers;
		std::unordered_map<
			std::string,
			mngl::mods::command_catalog::command> m_custom_commands;
		std::unordered_map<std::string, current_tree_state> m_open_trees;
		std::optional<std::string> m_selected_tree_id;
		std::uint64_t m_next_tree_id = 1;
	};
}
