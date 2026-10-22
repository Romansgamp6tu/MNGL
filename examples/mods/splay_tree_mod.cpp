#include <mod_loader.hpp>
#include <tree_algorithm_api.hpp>
#include <tree_invariant_api.hpp>
#include <tree/splay_tree.hpp>

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
	using namespace mngl::mods::tree;

	inline constexpr tree_id tree_algorithm_id = "mngl.tree.splay";

	inline constexpr std::size_t maximum_node_count = 1'000'000;
	inline constexpr std::size_t maximum_tree_depth = 1'024;
	inline constexpr std::size_t maximum_animation_steps = 48;
	inline constexpr key_kind_mask supported_keys =
		key_kind_bit(key_kind::signed_integer) |
		key_kind_bit(key_kind::floating_point);
	inline constexpr operation_mask supported_operations =
		static_cast<operation_mask>(operation::find) |
		operation::insert |
		operation::random_insert |
		operation::erase |
		operation::merge |
		operation::split;
	inline constexpr operation_mask layout_transitions =
		static_cast<operation_mask>(operation::insert) |
		operation::erase |
		operation::merge |
		operation::split;

	const algorithm_info splay_info{
		tree_algorithm_id,
		"Splay",
		"splay",
		"splay{",
		supported_keys,
		supported_operations,
		{ layout_transitions,
			static_cast<operation_mask>(operation::insert) },
		{},
		3
	};

	[[nodiscard]] bool is_integer_token(std::string_view text) noexcept
	{
		if (text.empty())
		{
			return false;
		}
		if (text.front() == '+' || text.front() == '-')
		{
			text.remove_prefix(1);
		}
		return !text.empty() && std::all_of(
			text.begin(),
			text.end(),
			[](char character)
			{
				return character >= '0' && character <= '9';
			});
	}

	[[nodiscard]] bool parse_integer(
		std::string_view text,
		std::int64_t& value) noexcept
	{
		if (text.starts_with('+'))
		{
			text.remove_prefix(1);
		}
		if (text.empty())
		{
			return false;
		}
		const auto parsed = std::from_chars(
			text.data(),
			text.data() + text.size(),
			value);
		return parsed.ec == std::errc{} &&
			parsed.ptr == text.data() + text.size();
	}

	[[nodiscard]] bool parse_floating_point(
		std::string_view text,
		double& value) noexcept
	{
		if (text.starts_with('+'))
		{
			text.remove_prefix(1);
		}
		if (text.empty())
		{
			return false;
		}
		const auto parsed = std::from_chars(
			text.data(),
			text.data() + text.size(),
			value,
			std::chars_format::general);
		return parsed.ec == std::errc{} &&
			parsed.ptr == text.data() + text.size() &&
			std::isfinite(value);
	}

	[[nodiscard]] std::string_view trim(std::string_view text) noexcept
	{
		const auto first = text.find_first_not_of(" \t\r\n");
		if (first == std::string_view::npos)
		{
			return {};
		}
		const auto last = text.find_last_not_of(" \t\r\n");
		return text.substr(first, last - first + 1);
	}

	class splay_serialization_parser
	{
	public:
		explicit splay_serialization_parser(std::string_view source)
			: m_source(source)
		{
		}

		[[nodiscard]] document parse()
		{
			skip_whitespace();
			expect_prefix("splay{");
			tree result;
			result.root = parse_node(result, 0);
			expect_character('}');
			skip_whitespace();
			if (m_position != m_source.size())
			{
				fail("лишние символы после дерева");
			}

			std::optional<key_kind> keys;
			if (!result.nodes.empty())
			{
				bool has_floating_point = false;
				for (auto& node : result.nodes)
				{
					auto& raw = std::get<std::string>(
						node.keys.front().value);
					if (is_integer_token(raw))
					{
						std::int64_t value = 0;
						if (!parse_integer(raw, value))
						{
							fail("ключ выходит за диапазон int64_t");
						}
						node.keys.front().value = value;
					}
					else
					{
						double value = 0.0;
						if (!parse_floating_point(raw, value))
						{
							fail("ключ должен быть конечным числом");
						}
						node.keys.front().value = value;
						has_floating_point = true;
					}
					node.keys.front().unquoted_serialized_value = false;
				}

				if (has_floating_point)
				{
					for (auto& node : result.nodes)
					{
						auto& value = node.keys.front().value;
						if (const auto* integer =
							std::get_if<std::int64_t>(&value))
						{
							value = static_cast<double>(*integer);
						}
					}
					keys = key_kind::floating_point;
				}
				else
				{
					keys = key_kind::signed_integer;
				}
			}

			return {
				std::string(tree_algorithm_id),
				std::move(result),
				keys
			};
		}

	private:
		[[noreturn]] void fail(std::string_view message) const
		{
			throw std::invalid_argument(
				"Splay serialization error at byte " +
				std::to_string(m_position) + ": " + std::string(message));
		}

		void skip_whitespace() noexcept
		{
			while (m_position < m_source.size())
			{
				const char character = m_source[m_position];
				if (character != ' ' && character != '\t' &&
					character != '\r' && character != '\n')
				{
					break;
				}
				++m_position;
			}
		}

		[[nodiscard]] bool consume_character(char character) noexcept
		{
			if (m_position >= m_source.size() ||
				m_source[m_position] != character)
			{
				return false;
			}
			++m_position;
			return true;
		}

		void expect_prefix(std::string_view prefix)
		{
			if (!m_source.substr(m_position).starts_with(prefix))
			{
				fail("ожидался префикс splay{");
			}
			m_position += prefix.size();
		}

		void expect_character(char character)
		{
			skip_whitespace();
			if (!consume_character(character))
			{
				fail(std::string("ожидался символ '") + character + "'");
			}
		}

		[[nodiscard]] std::string read_until_double_comma()
		{
			const std::size_t start = m_position;
			while (m_position < m_source.size() &&
				!m_source.substr(m_position).starts_with(",,"))
			{
				++m_position;
			}
			if (m_position == m_source.size())
			{
				fail("сериализация не завершена");
			}
			const auto value = trim(m_source.substr(start, m_position - start));
			m_position += 2;
			if (value.empty())
			{
				fail("ключ узла пуст");
			}
			return std::string(value);
		}

		void synchronize_children(node& value)
		{
			value.children.clear();
			for (const auto child : value.binary_children)
			{
				if (child != no_tree_node)
				{
					value.children.push_back(child);
				}
			}
			value.leaf = value.children.empty();
		}

		[[nodiscard]] std::size_t parse_node(tree& result, std::size_t depth)
		{
			skip_whitespace();
			if (consume_character('#'))
			{
				return no_tree_node;
			}
			if (depth >= maximum_tree_depth)
			{
				fail("превышена максимальная глубина дерева");
			}
			if (!consume_character('('))
			{
				fail("ожидался узел или #");
			}

			const auto key_text = read_until_double_comma();
			if (result.nodes.size() >= maximum_node_count)
			{
				fail("превышен лимит узлов дерева");
			}
			const std::size_t index = result.nodes.size();
			node current;
			current.keys.push_back({ std::move(key_text), {}, true });
			result.nodes.push_back(std::move(current));

			const std::size_t left = parse_node(result, depth + 1);
			expect_character(',');
			const std::size_t right = parse_node(result, depth + 1);
			expect_character(')');
			result.nodes[index].binary_children = { left, right };
			synchronize_children(result.nodes[index]);
			return index;
		}

		std::string_view m_source;
		std::size_t m_position = 0;
	};

	template<class Value>
	[[nodiscard]] Value key_value(const key& source)
	{
		if (const auto* value = std::get_if<Value>(&source.value))
		{
			if constexpr (std::is_same_v<Value, double>)
			{
				if (!std::isfinite(*value))
				{
					throw std::invalid_argument(
						"Splay does not accept non-finite keys.");
				}
			}
			return *value;
		}
		throw std::invalid_argument(
			"Splay key kind does not match the tree key kind.");
	}

	[[nodiscard]] bool supports_key_kind(key_kind keys) noexcept
	{
		return keys == key_kind::signed_integer ||
			keys == key_kind::floating_point;
	}

	void require_supported_key_kind(key_kind keys)
	{
		if (!supports_key_kind(keys))
		{
			throw std::invalid_argument(
				"Splay supports int64_t and finite double keys.");
		}
	}

	void synchronize_children(node& value)
	{
		value.children.clear();
		for (const auto child : value.binary_children)
		{
			if (child != no_tree_node)
			{
				value.children.push_back(child);
			}
		}
		value.leaf = value.children.empty();
	}

	void set_binary_child(
		tree& value,
		std::size_t parent,
		std::size_t side,
		std::size_t child)
	{
		value.nodes[parent].binary_children[side] = child;
		synchronize_children(value.nodes[parent]);
	}

	template<class Value>
	void validate_typed_tree(const tree& value)
	{
		if (value.multiway || value.fanout != 0 ||
			value.nodes.size() > maximum_node_count)
		{
			throw std::invalid_argument(
				"Splay model has invalid binary-tree metadata.");
		}
		if (value.nodes.empty())
		{
			if (value.root != no_tree_node)
			{
				throw std::invalid_argument(
					"An empty Splay tree must use no_tree_node as its root.");
			}
			return;
		}
		if (value.root >= value.nodes.size())
		{
			throw std::invalid_argument(
				"Splay root refers to a missing node.");
		}

		struct frame
		{
			std::size_t index;
			std::size_t lower;
			std::size_t upper;
			std::size_t depth;
		};
		std::vector<unsigned char> visited(value.nodes.size(), 0);
		std::vector<frame> pending{{ value.root, no_tree_node,
			no_tree_node, 0 }};
		std::size_t visited_count = 0;
		while (!pending.empty())
		{
			const auto current = pending.back();
			pending.pop_back();
			if (current.index >= value.nodes.size() ||
				current.depth >= maximum_tree_depth ||
				visited[current.index] != 0)
			{
				throw std::invalid_argument(
					"Splay has an invalid node reference, cycle, or excessive depth.");
			}
			visited[current.index] = 1;
			++visited_count;

			const auto& current_node = value.nodes[current.index];
			if (current_node.keys.size() != 1 || current_node.emphasized ||
				current_node.next_leaf != no_tree_node)
			{
				throw std::invalid_argument(
					"Splay nodes must contain one key and use binary-tree metadata.");
			}
			const auto current_key = key_value<Value>(
				current_node.keys.front());
			if (current.lower != no_tree_node &&
				!(key_value<Value>(value.nodes[current.lower].keys.front()) <
					current_key))
			{
				throw std::invalid_argument(
					"Splay violates the strict BST ordering.");
			}
			if (current.upper != no_tree_node &&
				!(current_key <
					key_value<Value>(value.nodes[current.upper].keys.front())))
			{
				throw std::invalid_argument(
					"Splay violates the strict BST ordering.");
			}

			const auto left = current_node.binary_children[0];
			const auto right = current_node.binary_children[1];
			const std::size_t expected_child_count =
				static_cast<std::size_t>(left != no_tree_node) +
				static_cast<std::size_t>(right != no_tree_node);
			const std::size_t expected_first_child =
				left != no_tree_node ? left : right;
			if (current_node.children.size() != expected_child_count ||
				current_node.leaf != (expected_child_count == 0) ||
				(expected_child_count > 0 &&
					current_node.children[0] != expected_first_child) ||
				(expected_child_count > 1 &&
					current_node.children[1] != right))
			{
				throw std::invalid_argument(
					"Splay child list is inconsistent with its binary links.");
			}
			for (const auto child : { left, right })
			{
				if (child != no_tree_node && child >= value.nodes.size())
				{
					throw std::invalid_argument(
						"Splay child refers to a missing node.");
				}
			}
			if (right != no_tree_node)
			{
				pending.push_back({ right, current.index,
					current.upper, current.depth + 1 });
			}
			if (left != no_tree_node)
			{
				pending.push_back({ left, current.lower,
					current.index, current.depth + 1 });
			}
		}
		if (visited_count != value.nodes.size())
		{
			throw std::invalid_argument(
				"Splay contains nodes not reachable from its root.");
		}
	}

	template<class Value>
	[[nodiscard]] splay_tree<Value> make_native_tree(const tree& value)
	{
		using native_tree = splay_tree<Value>;
		using native_node = typename native_tree::node_type;
		validate_typed_tree<Value>(value);
		if (value.nodes.empty())
		{
			return native_tree::from_validated_root(
				std::unique_ptr<native_node>{});
		}

		std::vector<std::unique_ptr<native_node>> nodes(value.nodes.size());
		std::vector<native_node*> raw_nodes(value.nodes.size(), nullptr);
		for (std::size_t i = 0; i < value.nodes.size(); ++i)
		{
			nodes[i] = std::make_unique<native_node>(
				key_value<Value>(value.nodes[i].keys.front()));
			raw_nodes[i] = nodes[i].get();
		}
		for (std::size_t i = 0; i < value.nodes.size(); ++i)
		{
			const auto left = value.nodes[i].binary_children[0];
			const auto right = value.nodes[i].binary_children[1];
			if (left != no_tree_node)
			{
				raw_nodes[i]->set_left(std::move(nodes[left]));
			}
			if (right != no_tree_node)
			{
				raw_nodes[i]->set_right(std::move(nodes[right]));
			}
		}
		return native_tree::from_validated_root(
			std::move(nodes[value.root]));
	}

	template<class Value>
	[[nodiscard]] tree make_model_tree(const splay_tree<Value>& source)
	{
		using native_node = typename splay_tree<Value>::node_type;
		tree result;
		result.root = no_tree_node;
		if (!source.root())
		{
			return result;
		}

		std::vector<const native_node*> pending{ source.root() };
		result.root = 0;
		result.nodes.emplace_back();
		for (std::size_t index = 0; index < pending.size(); ++index)
		{
			const auto* current = pending[index];
			node converted;
			converted.keys.push_back({ current->value(), {}, false });
			if (current->left())
			{
				const auto child = pending.size();
				converted.binary_children[0] = child;
				pending.push_back(current->left());
				result.nodes.emplace_back();
			}
			if (current->right())
			{
				const auto child = pending.size();
				converted.binary_children[1] = child;
				pending.push_back(current->right());
				result.nodes.emplace_back();
			}
			synchronize_children(converted);
			result.nodes[index] = std::move(converted);
		}
		return result;
	}

	[[nodiscard]] bool compare_key_values(
		const key& left,
		const key& right,
		key_kind keys)
	{
		if (keys == key_kind::signed_integer)
		{
			return key_value<std::int64_t>(left) <
				key_value<std::int64_t>(right);
		}
		return key_value<double>(left) < key_value<double>(right);
	}

	void record_animation_event(
		std::vector<visual_event>& events,
		bool& capture_available,
		visual_event step)
	{
		if (!capture_available)
		{
			return;
		}
		if (events.size() >= maximum_animation_steps)
		{
			events.clear();
			capture_available = false;
			return;
		}
		events.push_back(std::move(step));
	}

	void rotate_node_up(
		tree& value,
		std::size_t parent,
		std::size_t child,
		std::size_t grandparent,
		std::vector<visual_event>* events = nullptr,
		bool* capture_available = nullptr)
	{
		std::size_t parent_side = 2;
		if (value.nodes[parent].binary_children[0] == child)
		{
			parent_side = 0;
		}
		else if (value.nodes[parent].binary_children[1] == child)
		{
			parent_side = 1;
		}
		else
		{
			throw std::logic_error(
				"Splay rotation target is not a child of its parent.");
		}

		std::size_t grandparent_side = 2;
		if (grandparent == no_tree_node)
		{
			if (value.root != parent)
			{
				throw std::logic_error(
					"Splay root rotation has a non-root parent.");
			}
		}
		else if (value.nodes[grandparent].binary_children[0] == parent)
		{
			grandparent_side = 0;
		}
		else if (value.nodes[grandparent].binary_children[1] == parent)
		{
			grandparent_side = 1;
		}
		else
		{
			throw std::logic_error(
				"Splay grandparent does not reference its parent.");
		}

		const auto moved_subtree =
			value.nodes[child].binary_children[1 - parent_side];
		set_binary_child(value, parent, parent_side, moved_subtree);
		set_binary_child(value, child, 1 - parent_side, parent);
		if (grandparent == no_tree_node)
		{
			value.root = child;
		}
		else
		{
			set_binary_child(
				value,
				grandparent,
				grandparent_side,
				child);
		}
		if (events && capture_available && *capture_available)
		{
			record_animation_event(
				*events,
				*capture_available,
				visual_event{
					visual_event::kind::rotate_node_up,
					child,
					parent,
					grandparent
				});
		}
	}

	void splay_node_to_root(
		tree& value,
		std::vector<std::size_t> ancestors,
		std::size_t node_index,
		std::vector<visual_event>* events = nullptr,
		bool* capture_available = nullptr)
	{
		while (!ancestors.empty())
		{
			const auto parent = ancestors.back();
			ancestors.pop_back();
			if (ancestors.empty())
			{
				rotate_node_up(
					value,
					parent,
					node_index,
					no_tree_node,
					events,
					capture_available);
				break;
			}

			const auto grandparent = ancestors.back();
			ancestors.pop_back();
			const auto great_grandparent = ancestors.empty()
				? no_tree_node
				: ancestors.back();

			std::size_t parent_side = 2;
			if (value.nodes[grandparent].binary_children[0] == parent)
			{
				parent_side = 0;
			}
			else if (value.nodes[grandparent].binary_children[1] == parent)
			{
				parent_side = 1;
			}
			else
			{
				throw std::logic_error(
					"Splay ancestry lost the parent-to-grandparent link.");
			}

			std::size_t node_side = 2;
			if (value.nodes[parent].binary_children[0] == node_index)
			{
				node_side = 0;
			}
			else if (value.nodes[parent].binary_children[1] == node_index)
			{
				node_side = 1;
			}
			else
			{
				throw std::logic_error(
					"Splay ancestry lost the node-to-parent link.");
			}

			if (parent_side == node_side)
			{
				rotate_node_up(
					value,
					grandparent,
					parent,
					great_grandparent,
					events,
					capture_available);
				rotate_node_up(
					value,
					parent,
					node_index,
					great_grandparent,
					events,
					capture_available);
			}
			else
			{
				rotate_node_up(
					value,
					parent,
					node_index,
					grandparent,
					events,
					capture_available);
				rotate_node_up(
					value,
					grandparent,
					node_index,
					great_grandparent,
					events,
					capture_available);
			}
		}

		if (value.root != node_index)
		{
			throw std::logic_error(
				"Splay operation did not move its target to the root.");
		}
	}

	[[nodiscard]] find_result search_without_splaying(
		const tree& value,
		const key& requested,
		key_kind keys)
	{
		find_result result;
		std::size_t current = value.root;
		while (current != no_tree_node)
		{
			result.path.push_back(current);
			++result.visited_nodes;
			const auto& present = value.nodes[current].keys.front();
			if (compare_key_values(requested, present, keys))
			{
				current = value.nodes[current].binary_children[0];
			}
			else if (compare_key_values(present, requested, keys))
			{
				current = value.nodes[current].binary_children[1];
			}
			else
			{
				result.found = true;
				return result;
			}
		}
		return result;
	}

	[[nodiscard]] std::size_t locate_splay_target(
		const tree& value,
		const key& requested,
		key_kind keys,
		std::vector<std::size_t>& ancestors)
	{
		if (value.nodes.empty())
		{
			return no_tree_node;
		}
		std::size_t current = value.root;
		while (current != no_tree_node)
		{
			const auto& present = value.nodes[current].keys.front();
			if (compare_key_values(requested, present, keys))
			{
				const auto child = value.nodes[current].binary_children[0];
				if (child == no_tree_node)
				{
					return current;
				}
				ancestors.push_back(current);
				current = child;
			}
			else if (compare_key_values(present, requested, keys))
			{
				const auto child = value.nodes[current].binary_children[1];
				if (child == no_tree_node)
				{
					return current;
				}
				ancestors.push_back(current);
				current = child;
			}
			else
			{
				return current;
			}
		}
		return no_tree_node;
	}

	template<class Value>
	[[nodiscard]] Value random_key(std::mt19937_64& random_engine)
	{
		if constexpr (std::is_same_v<Value, std::int64_t>)
		{
			return std::bit_cast<std::int64_t>(random_engine());
		}
		else
		{
			const auto unit_value = std::generate_canonical<
				double,
				std::numeric_limits<double>::digits>(random_engine);
			return unit_value * 2.0 - 1.0;
		}
	}

	class splay_algorithm final : public algorithm
	{
	public:
		[[nodiscard]] const algorithm_info& info() const noexcept override
		{
			return splay_info;
		}

		[[nodiscard]] tree create_empty(key_kind keys) const override
		{
			require_supported_key_kind(keys);
			tree result;
			result.root = no_tree_node;
			return result;
		}

		[[nodiscard]] document deserialize(
			std::string_view source) const override
		{
			auto result = splay_serialization_parser{ source }.parse();
			const auto keys = result.keys.value_or(key_kind::signed_integer);
			validate(result.value, keys);
			return result;
		}

		void validate(const tree& value, key_kind keys) const override
		{
			require_supported_key_kind(keys);
			if (keys == key_kind::signed_integer)
			{
				validate_typed_tree<std::int64_t>(value);
			}
			else
			{
				validate_typed_tree<double>(value);
			}
		}

		[[nodiscard]] find_result find(
			tree& value,
			const key& search_key,
			key_kind keys) const override
		{
			validate(value, keys);
			(void)key_value_for_kind(search_key, keys);
			auto initial = search_without_splaying(value, search_key, keys);
			if (initial.path.empty())
			{
				return initial;
			}
			const auto original_visited = initial.visited_nodes;
			const auto target = initial.path.back();
			if (target == value.root)
			{
				return initial;
			}
			initial.path.pop_back();
			splay_node_to_root(value, std::move(initial.path), target);
			auto result = search_without_splaying(value, search_key, keys);
			if (result.found != initial.found)
			{
				throw std::logic_error(
					"Splay search changed its result while restructuring the tree.");
			}
			result.visited_nodes = original_visited;
			result.changed = true;
			return result;
		}

		[[nodiscard]] operation_result insert(
			tree& value,
			key insert_key,
			key_kind keys,
			bool collect_animation_events) const override
		{
			validate(value, keys);
			(void)key_value_for_kind(insert_key, keys);
			operation_result result;
			bool capture_available = collect_animation_events;
			if (value.nodes.empty())
			{
				node root;
				root.keys.push_back(std::move(insert_key));
				synchronize_children(root);
				value.nodes.push_back(std::move(root));
				value.root = 0;
				result.inserted = true;
				result.changed = true;
				if (collect_animation_events)
				{
					record_animation_event(
						result.events,
						capture_available,
						visual_event{
							visual_event::kind::insert_node,
							0,
						no_tree_node,
						no_tree_node,
						0,
						value.nodes.front()
						});
				}
				result.animation_events_complete = capture_available;
				return result;
			}

			std::vector<std::size_t> ancestors;
		const auto target = locate_splay_target(
				value,
				insert_key,
				keys,
				ancestors);
			if (target == no_tree_node)
			{
				throw std::logic_error("Splay insertion did not find a target node.");
			}
			const bool duplicate =
				!compare_key_values(
					insert_key,
					value.nodes[target].keys.front(),
					keys) &&
				!compare_key_values(
					value.nodes[target].keys.front(),
					insert_key,
					keys);
			const bool changed = target != value.root;
			if (!duplicate)
			{
				if (value.nodes.size() >= maximum_node_count)
				{
					throw std::length_error(
						"Splay node limit has been reached.");
				}
				if (ancestors.size() + 1 >= maximum_tree_depth)
				{
					throw std::length_error(
						"Insertion would exceed the maximum Splay depth.");
				}
				const std::size_t side = compare_key_values(
					insert_key,
					value.nodes[target].keys.front(),
					keys) ? 0 : 1;
				const auto inserted_index = value.nodes.size();
				node inserted;
				inserted.keys.push_back(std::move(insert_key));
				synchronize_children(inserted);
				value.nodes.push_back(std::move(inserted));
				set_binary_child(value, target, side, inserted_index);
				if (collect_animation_events)
				{
					record_animation_event(
						result.events,
						capture_available,
						visual_event{
							visual_event::kind::insert_node,
							inserted_index,
							target,
							no_tree_node,
							side,
							value.nodes[inserted_index]
						});
				}
				ancestors.push_back(target);
				result.inserted = true;
				result.changed = true;
				splay_node_to_root(
					value,
					std::move(ancestors),
					inserted_index,
					collect_animation_events ? &result.events : nullptr,
					&capture_available);
			}
			else
			{
				result.changed = changed;
				splay_node_to_root(
					value,
					std::move(ancestors),
					target,
					collect_animation_events ? &result.events : nullptr,
					&capture_available);
			}
			result.animation_events_complete = capture_available;
			validate(value, keys);
			return result;
		}

		[[nodiscard]] bulk_insert_result insert_many(
			tree& value,
			std::uint64_t requested_count,
			key_kind keys,
			std::mt19937_64& random_engine) const override
		{
			validate(value, keys);
			if (requested_count == 0)
			{
				return {};
			}
			if (requested_count > maximum_node_count - value.nodes.size())
			{
				throw std::length_error(
					"Mass insertion would exceed the Splay node limit.");
			}
			if (keys == key_kind::signed_integer)
			{
				return insert_many_typed<std::int64_t>(
					value,
					requested_count,
					random_engine);
			}
			return insert_many_typed<double>(
				value,
				requested_count,
				random_engine);
		}

		[[nodiscard]] erase_result erase(
			tree& value,
			const key& erase_key,
			key_kind keys) const override
		{
			validate(value, keys);
			(void)key_value_for_kind(erase_key, keys);
			if (keys == key_kind::signed_integer)
			{
				return erase_typed<std::int64_t>(value, erase_key);
			}
			return erase_typed<double>(value, erase_key);
		}

		[[nodiscard]] bool merge(
			tree& left,
			const tree& right,
			key_kind keys) const override
		{
			validate(left, keys);
			validate(right, keys);
			if (right.nodes.empty())
			{
				return false;
			}
			if (keys == key_kind::signed_integer)
			{
				return merge_typed<std::int64_t>(left, right);
			}
			return merge_typed<double>(left, right);
		}

		[[nodiscard]] std::pair<tree, tree> split(
			const tree& value,
			const key& pivot,
			key_kind keys) const override
		{
			validate(value, keys);
			(void)key_value_for_kind(pivot, keys);
			if (keys == key_kind::signed_integer)
			{
				return split_typed<std::int64_t>(value, pivot);
			}
			return split_typed<double>(value, pivot);
		}

		[[nodiscard]] std::string serialize(const tree& value) const override
		{
			if (value.nodes.empty())
			{
				validate(value, key_kind::signed_integer);
				return "splay{#}";
			}
			if (value.nodes.front().keys.empty())
			{
				throw std::invalid_argument(
					"Splay nodes must contain exactly one key.");
			}
			const auto& first_key = value.nodes.front().keys.front();
			if (std::holds_alternative<std::int64_t>(first_key.value))
			{
				return make_native_tree<std::int64_t>(value).to_string();
			}
			if (std::holds_alternative<double>(first_key.value))
			{
				return make_native_tree<double>(value).to_string();
			}
			throw std::invalid_argument(
				"Splay supports only int64_t and finite double keys.");
		}

	private:
		static void key_value_for_kind(
			const key& source,
			key_kind keys)
		{
			if (keys == key_kind::signed_integer)
			{
				(void)key_value<std::int64_t>(source);
			}
			else
			{
				(void)key_value<double>(source);
			}
		}

		template<class Value>
		[[nodiscard]] bulk_insert_result insert_many_typed(
			tree& value,
			std::uint64_t requested_count,
			std::mt19937_64& random_engine) const
		{
			auto native = make_native_tree<Value>(value);
			std::uint64_t inserted_count = 0;
			while (inserted_count < requested_count)
			{
				if (native.insert(random_key<Value>(random_engine)))
				{
					++inserted_count;
				}
			}
			value = make_model_tree(native);
			return {
				inserted_count,
				std::optional<std::string>{ native.to_string() }
			};
		}

		template<class Value>
		[[nodiscard]] erase_result erase_typed(
			tree& value,
			const key& erase_key) const
		{
			auto native = make_native_tree<Value>(value);
			const auto before = native.to_string();
			const bool erased = native.erase(key_value<Value>(erase_key));
			const auto after = native.to_string();
			const bool changed = before != after;
			if (changed)
			{
				value = make_model_tree(native);
			}
			return { erased, changed };
		}

		template<class Value>
		[[nodiscard]] bool merge_typed(tree& left, const tree& right) const
		{
			auto native_left = make_native_tree<Value>(left);
			auto native_right = make_native_tree<Value>(right);
			native_left.merge(std::move(native_right));
			left = make_model_tree(native_left);
			return true;
		}

		template<class Value>
		[[nodiscard]] std::pair<tree, tree> split_typed(
			const tree& value,
			const key& pivot) const
		{
			auto native = make_native_tree<Value>(value);
			auto [left, right] = native.split(key_value<Value>(pivot));
			return {
				make_model_tree(left),
				make_model_tree(right)
			};
		}
	};

	class splay_invariant_inspector final : public invariant_inspector
	{
	public:
		[[nodiscard]] std::string_view algorithm_id() const noexcept override
		{
			return tree_algorithm_id;
		}

		[[nodiscard]] invariant_report inspect(
			const tree& value,
			key_kind keys) const override
		{
			invariant_report report;
			try
			{
				if (keys == key_kind::signed_integer)
					validate_typed_tree<std::int64_t>(value);
				else if (keys == key_kind::floating_point)
					validate_typed_tree<double>(value);
				else
					throw std::invalid_argument(
						"Splay inspector supports int64_t and double keys.");
				report.checks = {
					{ "BST-порядок", true,
						"Ключи строго упорядочены относительно корня и потомков." },
					{ "Структура дерева", true,
						"Все узлы достижимы, ссылки корректны, циклов нет." },
					{ "Корень Splay", true,
						"Корень соответствует текущему дереву; успешный доступ переносит цель в корень." }
				};
			}
			catch (const std::exception& error)
			{
				report.checks.push_back({
					"Инварианты Splay",
					false,
					std::format("Нарушение: {}", error.what()) });
			}
			return report;
		}

		[[nodiscard]] std::string describe_node(
			const tree& value,
			key_kind,
			std::size_t node_index) const override
		{
			if (node_index >= value.nodes.size())
				throw std::out_of_range("Invalid Splay node index.");
			const auto& current = value.nodes[node_index];
			if (current.keys.size() != 1)
				throw std::invalid_argument("Splay node must contain one key.");
			return std::format(
				"Ключ: {} · {} · левый потомок: {} · правый потомок: {}",
				key_text(current.keys.front()),
				node_index == value.root ? "корень" : "узел",
				child_key(value, current.binary_children[0]),
				child_key(value, current.binary_children[1]));
		}

	private:
		[[nodiscard]] static std::string key_text(const key& source)
		{
			if (const auto* integer =
				std::get_if<std::int64_t>(&source.value))
				return std::to_string(*integer);
			if (const auto* floating =
				std::get_if<double>(&source.value))
				return std::format("{}", *floating);
			throw std::invalid_argument("Unsupported Splay key type.");
		}

		[[nodiscard]] static std::string child_key(
			const tree& value,
			std::size_t child)
		{
			if (child == no_tree_node) return "∅";
			if (child >= value.nodes.size() ||
				value.nodes[child].keys.size() != 1)
				throw std::invalid_argument("Invalid Splay child reference.");
			return key_text(value.nodes[child].keys.front());
		}
	};
}

extern "C" MNGL_MOD_EXPORT bool MNGL_MOD_CALL
mngl_mod_load_v1(mngl::mods::mod_context& context)
{
	const auto id = std::string(tree_algorithm_id);
	const bool algorithm_registered =
		context.add_implementation<mngl::mods::tree::algorithm>(
		mngl::mods::tree::algorithm_class_id,
		id,
		[] { return std::make_unique<splay_algorithm>(); });
	const bool inspector_registered =
		context.add_implementation<mngl::mods::tree::invariant_inspector>(
			mngl::mods::tree::invariant_inspector_class_id,
			id,
			[] { return std::make_unique<splay_invariant_inspector>(); });
	return algorithm_registered && inspector_registered;
}

extern "C" MNGL_MOD_EXPORT void MNGL_MOD_CALL mngl_mod_unload_v1()
{
}
