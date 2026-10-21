#include <mod_loader.hpp>
#include <tree_algorithm_api.hpp>
#include <tree_invariant_api.hpp>
#include <tree/b_plus_tree.hpp>
#include <tree/tree_serialization.hpp>

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
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

	inline constexpr std::size_t maximum_node_count = 1'000'000;
	inline constexpr std::size_t maximum_tree_depth = 1'024;
	inline constexpr std::size_t maximum_fanout = 1'024;
	inline constexpr std::size_t default_fanout = 8;
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

	const algorithm_info b_plus_info{
		ids::b_plus,
		"B+ tree",
		"b-plus",
		"b_plus_tree[",
		supported_keys,
		supported_operations,
		{ layout_transitions, 0 }
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
			text.begin(), text.end(), [](char character)
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
			text.data(), text.data() + text.size(), value);
		return parsed.ec == std::errc{} &&
			parsed.ptr == text.data() + text.size();
	}

	[[nodiscard]] bool parse_double(
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
			text.data(), text.data() + text.size(), value,
			std::chars_format::general);
		return parsed.ec == std::errc{} &&
			parsed.ptr == text.data() + text.size() && std::isfinite(value);
	}

	template<class Value>
	[[nodiscard]] Value key_value(const key& source)
	{
		if (const auto* value = std::get_if<Value>(&source.value))
		{
			if constexpr (std::is_same_v<Value, double>)
			{
				if (!std::isfinite(*value))
				{
					throw std::invalid_argument("B+ tree does not accept non-finite keys.");
				}
			}
			return *value;
		}
		throw std::invalid_argument("B+ tree key kind does not match the tree key kind.");
	}

	void require_supported_key_kind(key_kind keys)
	{
		if (keys != key_kind::signed_integer && keys != key_kind::floating_point)
		{
			throw std::invalid_argument("B+ tree supports int64_t and finite double keys.");
		}
	}

	class b_plus_serialization_parser
	{
	public:
		explicit b_plus_serialization_parser(std::string_view source)
			: m_source(source)
		{
		}

		[[nodiscard]] document parse()
		{
			skip_whitespace();
			if (m_source.substr(m_position).starts_with("b_plus_tree["))
			{
				expect("b_plus_tree[");
			}
			else if (m_source.substr(m_position).starts_with("b\\_plus\\_tree["))
			{
				expect("b\\_plus\\_tree[");
			}
			else
			{
				fail("unexpected serialization prefix");
			}
			expect("fanout=");
			const std::size_t start = m_position;
			while (m_position < m_source.size() &&
				m_source[m_position] >= '0' && m_source[m_position] <= '9')
			{
				++m_position;
			}
			if (start == m_position)
			{
				fail("fanout is missing");
			}
			const auto parsed_fanout = std::from_chars(
				m_source.data() + start,
				m_source.data() + m_position,
				m_result.fanout);
			if (parsed_fanout.ec != std::errc{} ||
				m_result.fanout < 3 || m_result.fanout > maximum_fanout)
			{
				fail("fanout must be between 3 and 1024");
			}
			expect_character(']');
			expect_character('{');
			m_result.root = parse_node(0);
			expect_character('}');
			skip_whitespace();
			if (m_position != m_source.size())
			{
				fail("extra characters after the tree");
			}
			normalize_keys();
			return {
				std::string(ids::b_plus),
				std::move(m_result),
				m_key_kind
			};
		}

	private:
		[[noreturn]] void fail(std::string_view message) const
		{
			throw std::invalid_argument(
				"B+ tree serialization error at byte " +
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

		void expect(std::string_view token)
		{
			if (!m_source.substr(m_position).starts_with(token))
			{
				fail("unexpected serialization prefix");
			}
			m_position += token.size();
		}

		[[nodiscard]] bool consume(char character) noexcept
		{
			if (m_position >= m_source.size() || m_source[m_position] != character)
			{
				return false;
			}
			++m_position;
			return true;
		}

		void expect_character(char character)
		{
			skip_whitespace();
			if (!consume(character))
			{
				fail("unexpected punctuation");
			}
		}

		[[nodiscard]] std::string read_key_token()
		{
			skip_whitespace();
			const std::size_t start = m_position;
			while (m_position < m_source.size() &&
				m_source[m_position] != ',' && m_source[m_position] != '}')
			{
				++m_position;
			}
			std::size_t end = m_position;
			while (end > start &&
				(m_source[end - 1] == ' ' || m_source[end - 1] == '\t' ||
					m_source[end - 1] == '\r' || m_source[end - 1] == '\n'))
			{
				--end;
			}
			if (start == end)
			{
				fail("empty key");
			}
			return std::string(m_source.substr(start, end - start));
		}

		[[nodiscard]] std::size_t append_node(node value)
		{
			if (m_result.nodes.size() >= maximum_node_count)
			{
				fail("maximum node count exceeded");
			}
			const std::size_t index = m_result.nodes.size();
			m_result.nodes.push_back(std::move(value));
			return index;
		}

		[[nodiscard]] std::size_t parse_node(std::size_t depth)
		{
			skip_whitespace();
			if (consume('#'))
			{
				return no_tree_node;
			}
			if (depth >= maximum_tree_depth)
			{
				fail("maximum tree depth exceeded");
			}
			if (m_position >= m_source.size() ||
				(m_source[m_position] != 'I' && m_source[m_position] != 'L'))
			{
				fail("expected an internal I or leaf L node");
			}
			node current;
			current.leaf = m_source[m_position++] == 'L';
			expect_character('{');
			skip_whitespace();
			if (!consume('}'))
			{
				while (true)
				{
					current.keys.push_back({ read_key_token(), {}, true });
					skip_whitespace();
					if (consume('}'))
					{
						break;
					}
					expect_character(',');
				}
			}
			const std::size_t index = append_node(std::move(current));
			if (m_result.nodes[index].leaf)
			{
				return index;
			}

			expect_character('[');
			skip_whitespace();
			if (!consume(']'))
			{
				while (true)
				{
					const auto child = parse_node(depth + 1);
					m_result.nodes[index].children.push_back(child);
					skip_whitespace();
					if (consume(']'))
					{
						break;
					}
					expect_character(',');
				}
			}
			if (m_result.nodes[index].children.size() !=
				m_result.nodes[index].keys.size() + 1 ||
				m_result.nodes[index].children.size() > m_result.fanout)
			{
				fail("internal node must have keys + 1 children, within fanout");
			}
			return index;
		}

		void normalize_keys()
		{
			bool has_double = false;
			for (auto& current : m_result.nodes)
			{
				for (auto& entry : current.keys)
				{
					auto& raw = std::get<std::string>(entry.value);
					if (is_integer_token(raw))
					{
						std::int64_t integer = 0;
						if (!parse_integer(raw, integer))
						{
							fail("integer key is outside int64_t range");
						}
						entry.value = integer;
					}
					else
					{
						double floating = 0.0;
						if (!parse_double(raw, floating))
						{
							fail("key must be a finite number");
						}
						entry.value = floating;
						has_double = true;
					}
					entry.unquoted_serialized_value = false;
				}
			}
			if (m_result.nodes.empty())
			{
				return;
			}
			if (has_double)
			{
				for (auto& current : m_result.nodes)
				{
					for (auto& entry : current.keys)
					{
						if (const auto* integer =
							std::get_if<std::int64_t>(&entry.value))
						{
							entry.value = static_cast<double>(*integer);
						}
					}
				}
				m_key_kind = key_kind::floating_point;
			}
			else
			{
				m_key_kind = key_kind::signed_integer;
			}
			link_leaves();
		}

		void link_leaves()
		{
			std::vector<std::size_t> pending{ m_result.root };
			std::vector<std::size_t> leaves;
			while (!pending.empty())
			{
				const auto index = pending.back();
				pending.pop_back();
				if (index >= m_result.nodes.size())
				{
					fail("node references a missing child");
				}
				const auto& current = m_result.nodes[index];
				if (current.leaf)
				{
					leaves.push_back(index);
				}
				else
				{
					for (auto child = current.children.rbegin();
						child != current.children.rend(); ++child)
					{
						pending.push_back(*child);
					}
				}
			}
			for (std::size_t i = 0; i + 1 < leaves.size(); ++i)
			{
				m_result.nodes[leaves[i]].next_leaf = leaves[i + 1];
			}
		}

		std::string_view m_source;
		std::size_t m_position = 0;
		tree m_result{ .root = no_tree_node, .b_plus = true };
		std::optional<key_kind> m_key_kind;
	};

	struct subtree_summary
	{
		std::int64_t minimum_integer = 0;
		std::int64_t maximum_integer = 0;
		double minimum_double = 0.0;
		double maximum_double = 0.0;
		std::size_t leaf_depth = 0;
	};

	template<class Value>
	[[nodiscard]] Value node_key_value(const node& source, std::size_t index)
	{
		if (index >= source.keys.size())
		{
			throw std::invalid_argument("B+ node contains an invalid key index.");
		}
		return key_value<Value>(source.keys[index]);
	}

	template<class Value>
	void validate_tree(const tree& value)
	{
		if (!value.b_plus || value.fanout < 3 ||
			value.fanout > maximum_fanout ||
			value.nodes.size() > maximum_node_count)
		{
			throw std::invalid_argument("B+ tree model has invalid metadata.");
		}
		if (value.nodes.empty())
		{
			if (value.root != no_tree_node)
			{
				throw std::invalid_argument("Empty B+ tree must use no_tree_node as its root.");
			}
			return;
		}
		if (value.root >= value.nodes.size())
		{
			throw std::invalid_argument("B+ tree root refers to a missing node.");
		}

		std::vector<unsigned char> state(value.nodes.size(), 0);
		std::vector<std::size_t> leaves;
		std::size_t visited_count = 0;
		std::size_t stored_key_count = 0;
		const auto equal = [](Value left, Value right)
		{
			return !(left < right) && !(right < left);
		};

		std::function<subtree_summary(std::size_t, std::size_t, bool)> visit;
		visit = [&](std::size_t index, std::size_t depth, bool is_root)
			-> subtree_summary
		{
			if (index >= value.nodes.size() || depth >= maximum_tree_depth ||
				state[index] != 0)
			{
				throw std::invalid_argument(
					"B+ tree contains a missing link, cycle, repeated node, or excessive depth.");
			}
			state[index] = 1;
			++visited_count;
			const auto& current = value.nodes[index];
			if (current.binary_children[0] != no_tree_node ||
				current.binary_children[1] != no_tree_node || current.red)
			{
				throw std::invalid_argument("B+ nodes cannot contain binary-tree metadata.");
			}
			for (std::size_t i = 1; i < current.keys.size(); ++i)
			{
				if (!(node_key_value<Value>(current, i - 1) <
					node_key_value<Value>(current, i)))
				{
					throw std::invalid_argument("B+ node keys must be strictly increasing.");
				}
			}

			if (current.leaf)
			{
				const std::size_t minimum_keys = is_root ? 1 : value.fanout / 2;
				if (!current.children.empty() || current.keys.size() < minimum_keys ||
					current.keys.size() >= value.fanout)
				{
					throw std::invalid_argument("B+ leaf violates its occupancy limits.");
				}
				stored_key_count += current.keys.size();
				if (stored_key_count > maximum_node_count)
				{
					throw std::length_error("B+ tree key limit has been reached.");
				}
				leaves.push_back(index);
				state[index] = 2;
				const Value first = node_key_value<Value>(current, 0);
				const Value last = node_key_value<Value>(current, current.keys.size() - 1);
				if constexpr (std::is_same_v<Value, std::int64_t>)
				{
					return { first, last, 0.0, 0.0, depth };
				}
				else
				{
					return { 0, 0, first, last, depth };
				}
			}

			const std::size_t child_count = current.children.size();
			const std::size_t minimum_children = is_root
				? 2
				: value.fanout / 2 + value.fanout % 2;
			if (child_count != current.keys.size() + 1 ||
				child_count < minimum_children || child_count > value.fanout ||
				current.next_leaf != no_tree_node)
			{
				throw std::invalid_argument("B+ internal node violates its child limits.");
			}

			std::vector<subtree_summary> child_summaries;
			child_summaries.reserve(child_count);
			for (const auto child : current.children)
			{
				child_summaries.push_back(visit(child, depth + 1, false));
			}
			for (std::size_t i = 0; i < child_summaries.size(); ++i)
			{
				if (child_summaries[i].leaf_depth != child_summaries.front().leaf_depth)
				{
					throw std::invalid_argument("All B+ leaves must have the same depth.");
				}
				if (i == 0)
				{
					continue;
				}
				const Value separator = node_key_value<Value>(current, i - 1);
				Value right_minimum{};
				Value left_maximum{};
				if constexpr (std::is_same_v<Value, std::int64_t>)
				{
					right_minimum = child_summaries[i].minimum_integer;
					left_maximum = child_summaries[i - 1].maximum_integer;
				}
				else
				{
					right_minimum = child_summaries[i].minimum_double;
					left_maximum = child_summaries[i - 1].maximum_double;
				}
				if (!equal(right_minimum, separator) || !(left_maximum < separator))
				{
					throw std::invalid_argument(
						"B+ separator must equal the right subtree minimum and exceed the left subtree maximum.");
				}
			}
			state[index] = 2;
			auto summary = child_summaries.front();
			if constexpr (std::is_same_v<Value, std::int64_t>)
			{
				summary.maximum_integer = child_summaries.back().maximum_integer;
			}
			else
			{
				summary.maximum_double = child_summaries.back().maximum_double;
			}
			return summary;
		};

		(void)visit(value.root, 0, true);
		if (visited_count != value.nodes.size())
		{
			throw std::invalid_argument("B+ tree contains nodes unreachable from its root.");
		}
		for (std::size_t i = 0; i < leaves.size(); ++i)
		{
			const auto expected_next = i + 1 < leaves.size()
				? leaves[i + 1]
				: no_tree_node;
			if (value.nodes[leaves[i]].next_leaf != expected_next)
			{
				throw std::invalid_argument("B+ leaf chain does not match child order.");
			}
		}
	}

	template<class Value>
	[[nodiscard]] b_plus_tree<Value> make_native_tree(const tree& value)
	{
		validate_tree<Value>(value);
		b_plus_tree<Value> result(value.fanout);
		if (value.nodes.empty())
		{
			return result;
		}
		std::size_t leaf_index = value.root;
		while (!value.nodes[leaf_index].leaf)
		{
			leaf_index = value.nodes[leaf_index].children.front();
		}
		while (leaf_index != no_tree_node)
		{
			const auto& leaf = value.nodes[leaf_index];
			for (const auto& entry : leaf.keys)
			{
				if (!result.insert(key_value<Value>(entry)))
				{
					throw std::invalid_argument("B+ tree contains a duplicate key.");
				}
			}
			leaf_index = leaf.next_leaf;
		}
		return result;
	}

	template<class Value>
	[[nodiscard]] tree make_model_tree(const b_plus_tree<Value>& source)
	{
		using native_node = typename b_plus_tree<Value>::node_type;
		tree result;
		result.root = no_tree_node;
		result.fanout = source.max_children();
		result.b_plus = true;
		if (!source.root())
		{
			return result;
		}

		std::vector<const native_node*> pending{ source.root() };
		result.root = 0;
		result.nodes.emplace_back();
		std::vector<std::size_t> leaves;
		for (std::size_t index = 0; index < pending.size(); ++index)
		{
			const auto* current = pending[index];
			node converted;
			converted.leaf = current->is_leaf();
			for (const auto& value : current->keys())
			{
				converted.keys.push_back({ value, {}, false });
			}
			if (current->is_leaf())
			{
				leaves.push_back(index);
			}
			else
			{
				for (std::size_t child_index = 0;
					child_index < current->child_count(); ++child_index)
				{
					const auto* child = current->child(child_index);
					const auto next_index = pending.size();
					pending.push_back(child);
					result.nodes.emplace_back();
					converted.children.push_back(next_index);
				}
			}
			result.nodes[index] = std::move(converted);
		}
		for (std::size_t i = 0; i < leaves.size(); ++i)
		{
			if (i + 1 < leaves.size())
			{
				result.nodes[leaves[i]].next_leaf = leaves[i + 1];
			}
		}
		if (result.nodes.size() > maximum_node_count)
		{
			throw std::length_error("B+ tree node limit has been reached.");
		}
		return result;
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
			const auto unit = std::generate_canonical<
				double, std::numeric_limits<double>::digits>(random_engine);
			return unit * 2.0 - 1.0;
		}
	}

	template<class Value>
	[[nodiscard]] std::size_t key_count(const tree& value)
	{
		std::size_t count = 0;
		for (const auto& current : value.nodes)
		{
			if (current.leaf)
			{
				count += current.keys.size();
			}
		}
		return count;
	}

	template<class Value>
	[[nodiscard]] find_result find_key(
		tree& value,
		const key& search_key)
	{
		const Value requested = key_value<Value>(search_key);
		find_result result;
		std::size_t current = value.root;
		while (current != no_tree_node)
		{
			result.path.push_back(current);
			const auto& present = value.nodes[current];
			if (present.leaf)
			{
				result.found = std::any_of(
					present.keys.begin(), present.keys.end(),
					[&](const auto& entry)
					{
						return !(requested < key_value<Value>(entry)) &&
							!(key_value<Value>(entry) < requested);
					});
				break;
			}
			std::size_t child = 0;
			while (child < present.keys.size() &&
				!(requested < key_value<Value>(present.keys[child])))
			{
				++child;
			}
			current = present.children.at(child);
		}
		result.visited_nodes = result.path.size();
		return result;
	}

	template<class Value>
	void write_model_node(std::ostream& output, const tree& value, std::size_t index)
	{
		if (index >= value.nodes.size())
		{
			throw std::invalid_argument("B+ node refers to a missing child.");
		}
		const auto& current = value.nodes[index];
		output << (current.leaf ? "L{" : "I{");
		for (std::size_t i = 0; i < current.keys.size(); ++i)
		{
			if (i != 0)
			{
				output << ',';
			}
			tree_serialization::write_text_value(
				output, key_value<Value>(current.keys[i]));
		}
		output << '}';
		if (!current.leaf)
		{
			output << '[';
			for (std::size_t i = 0; i < current.children.size(); ++i)
			{
				if (i != 0)
				{
					output << ',';
				}
				write_model_node<Value>(output, value, current.children[i]);
			}
			output << ']';
		}
	}

	template<class Value>
	[[nodiscard]] std::string serialize_tree(const tree& value)
	{
		validate_tree<Value>(value);
		std::ostringstream output;
		output << "b_plus_tree[fanout=" << value.fanout << "]{";
		if (value.root == no_tree_node)
		{
			output << '#';
		}
		else
		{
			write_model_node<Value>(output, value, value.root);
		}
		output << '}';
		return output.str();
	}

	class b_plus_algorithm final : public algorithm
	{
	public:
		[[nodiscard]] const algorithm_info& info() const noexcept override
		{
			return b_plus_info;
		}

		[[nodiscard]] tree create_empty(key_kind keys) const override
		{
			require_supported_key_kind(keys);
			tree result;
			result.root = no_tree_node;
			result.fanout = default_fanout;
			result.b_plus = true;
			return result;
		}

		[[nodiscard]] document deserialize(std::string_view source) const override
		{
			auto result = b_plus_serialization_parser{ source }.parse();
			validate(result.value, result.keys.value_or(key_kind::signed_integer));
			return result;
		}

		void validate(const tree& value, key_kind keys) const override
		{
			require_supported_key_kind(keys);
			if (keys == key_kind::signed_integer)
			{
				validate_tree<std::int64_t>(value);
			}
			else
			{
				validate_tree<double>(value);
			}
		}

		[[nodiscard]] find_result find(
			tree& value, const key& search_key, key_kind keys) const override
		{
			validate(value, keys);
			return keys == key_kind::signed_integer
				? find_key<std::int64_t>(value, search_key)
				: find_key<double>(value, search_key);
		}

		[[nodiscard]] operation_result insert(
			tree& value, key insert_key, key_kind keys, bool) const override
		{
			validate(value, keys);
			const auto current_count = key_count<std::int64_t>(value);
			if (current_count >= maximum_node_count)
			{
				const auto existing = keys == key_kind::signed_integer
					? find_key<std::int64_t>(value, insert_key).found
					: find_key<double>(value, insert_key).found;
				if (!existing)
				{
					throw std::length_error("B+ tree node limit has been reached.");
				}
			}
			if (keys == key_kind::signed_integer)
			{
				auto native = make_native_tree<std::int64_t>(value);
				const bool inserted = native.insert(key_value<std::int64_t>(insert_key));
				if (inserted)
				{
					value = make_model_tree(native);
				}
				return { inserted, inserted, true, {} };
			}
			auto native = make_native_tree<double>(value);
			const bool inserted = native.insert(key_value<double>(insert_key));
			if (inserted)
			{
				value = make_model_tree(native);
			}
			return { inserted, inserted, true, {} };
		}

		[[nodiscard]] bulk_insert_result insert_many(
			tree& value, std::uint64_t requested_count, key_kind keys,
			std::mt19937_64& random_engine) const override
		{
			validate(value, keys);
			const auto current_count = key_count<std::int64_t>(value);
			if (current_count > maximum_node_count ||
				requested_count > maximum_node_count - current_count)
			{
				throw std::length_error("Mass insertion would exceed the B+ tree node limit.");
			}
			if (requested_count == 0)
			{
				return {};
			}
			if (keys == key_kind::signed_integer)
			{
				auto native = make_native_tree<std::int64_t>(value);
				std::uint64_t inserted = 0;
				while (inserted < requested_count)
				{
					if (native.insert(random_key<std::int64_t>(random_engine)))
					{
						++inserted;
					}
				}
				const auto serialization = native.to_string();
				value = make_model_tree(native);
				return { inserted, serialization };
			}
			auto native = make_native_tree<double>(value);
			std::uint64_t inserted = 0;
			while (inserted < requested_count)
			{
				if (native.insert(random_key<double>(random_engine)))
				{
					++inserted;
				}
			}
			const auto serialization = native.to_string();
			value = make_model_tree(native);
			return { inserted, serialization };
		}

		[[nodiscard]] erase_result erase(
			tree& value, const key& erase_key, key_kind keys) const override
		{
			validate(value, keys);
			if (keys == key_kind::signed_integer)
			{
				auto native = make_native_tree<std::int64_t>(value);
				const bool erased = native.erase(key_value<std::int64_t>(erase_key));
				if (erased)
				{
					value = make_model_tree(native);
				}
				return { erased, erased };
			}
			auto native = make_native_tree<double>(value);
			const bool erased = native.erase(key_value<double>(erase_key));
			if (erased)
			{
				value = make_model_tree(native);
			}
			return { erased, erased };
		}

		[[nodiscard]] bool merge(
			tree& left, const tree& right, key_kind keys) const override
		{
			validate(left, keys);
			validate(right, keys);
			if (right.nodes.empty())
			{
				return false;
			}
			if (keys == key_kind::signed_integer)
			{
				merge_typed<std::int64_t>(left, right);
			}
			else
			{
				merge_typed<double>(left, right);
			}
			return true;
		}

		[[nodiscard]] std::pair<tree, tree> split(
			const tree& value, const key& pivot, key_kind keys) const override
		{
			validate(value, keys);
			if (keys == key_kind::signed_integer)
			{
				return split_typed<std::int64_t>(value, key_value<std::int64_t>(pivot));
			}
			return split_typed<double>(value, key_value<double>(pivot));
		}

		[[nodiscard]] std::string serialize(const tree& value) const override
		{
			if (value.nodes.empty())
			{
				return serialize_tree<std::int64_t>(value);
			}
			for (const auto& current : value.nodes)
			{
				if (!current.keys.empty())
				{
					if (std::holds_alternative<std::int64_t>(current.keys.front().value))
					{
						return serialize_tree<std::int64_t>(value);
					}
					return serialize_tree<double>(value);
				}
			}
			return serialize_tree<std::int64_t>(value);
		}

	private:
		template<class Value>
		static void merge_typed(tree& left, const tree& right)
		{
			const auto left_count = key_count<Value>(left);
			const auto right_count = key_count<Value>(right);
			if (left_count != 0 && right_count != 0)
			{
				const Value maximum_left = last_key<Value>(left);
				const Value minimum_right = first_key<Value>(right);
				if (!(maximum_left < minimum_right))
				{
					throw std::invalid_argument(
						"B+ merge requires every left key to precede every right key.");
				}
			}
			auto native = make_native_tree<Value>(left);
			for_each_value<Value>(right, [&](Value value)
			{
				if (!native.insert(value))
				{
					throw std::invalid_argument("B+ merge encountered a duplicate key.");
				}
			});
			left = make_model_tree(native);
		}

		template<class Value>
		[[nodiscard]] static std::pair<tree, tree> split_typed(
			const tree& source, Value pivot)
		{
			b_plus_tree<Value> left(source.fanout);
			b_plus_tree<Value> right(source.fanout);
			for_each_value<Value>(source, [&](Value value)
			{
				auto& destination = value < pivot ? left : right;
				if (!destination.insert(value))
				{
					throw std::invalid_argument("B+ split encountered a duplicate key.");
				}
			});
			return { make_model_tree(left), make_model_tree(right) };
		}

		template<class Value, class Visitor>
		static void for_each_value(const tree& value, Visitor&& visitor)
		{
			if (value.nodes.empty())
			{
				return;
			}
			std::size_t leaf_index = value.root;
			while (!value.nodes[leaf_index].leaf)
			{
				leaf_index = value.nodes[leaf_index].children.front();
			}
			while (leaf_index != no_tree_node)
			{
				const auto& leaf = value.nodes[leaf_index];
				for (const auto& entry : leaf.keys)
				{
					visitor(key_value<Value>(entry));
				}
				leaf_index = leaf.next_leaf;
			}
		}

		template<class Value>
		[[nodiscard]] static Value first_key(const tree& value)
		{
			Value result{};
			bool found = false;
			for_each_value<Value>(value, [&](Value current)
			{
				if (!found)
				{
					result = current;
					found = true;
				}
			});
			return result;
		}

		template<class Value>
		[[nodiscard]] static Value last_key(const tree& value)
		{
			Value result{};
			for_each_value<Value>(value, [&](Value current) { result = current; });
			return result;
		}
	};

	class b_plus_invariant_inspector final : public invariant_inspector
	{
	public:
		[[nodiscard]] std::string_view algorithm_id() const noexcept override
		{
			return ids::b_plus;
		}

		[[nodiscard]] invariant_report inspect(
			const tree& value,
			key_kind keys) const override
		{
			invariant_report report;
			try
			{
				if (keys == key_kind::signed_integer)
					validate_tree<std::int64_t>(value);
				else if (keys == key_kind::floating_point)
					validate_tree<double>(value);
				else
					throw std::invalid_argument(
						"B+ inspector supports int64_t and double keys.");
				std::size_t leaf_count = 0;
				std::size_t internal_count = 0;
				for (const auto& current : value.nodes)
				{
					if (current.leaf)
						++leaf_count;
					else
						++internal_count;
				}
				report.checks = {
					{ "Заполнение узлов", true,
						std::format(
							"Fanout = {}; проверено {} листьев и {} внутренних узлов.",
							value.fanout,
							leaf_count,
							internal_count) },
					{ "Уровни и разделители", true,
						"Листья на одной глубине, разделители совпадают с границами детей." },
					{ "Цепочка листьев", true,
						"Ссылки next_leaf идут в порядке дерева и завершаются пустой ссылкой." }
				};
			}
			catch (const std::exception& error)
			{
				report.checks.push_back({
					"Инварианты B+",
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
			if (node_index >= value.nodes.size() || value.fanout < 3)
				throw std::out_of_range("Invalid B+ node index or fanout.");
			const auto& current = value.nodes[node_index];
			if (current.leaf)
			{
				const auto minimum = node_index == value.root
					? std::size_t{ 1 }
					: value.fanout / 2;
				return std::format(
					"Лист: заполнение {} / {} ключей · минимум {} · next_leaf {}",
					current.keys.size(),
					value.fanout - 1,
					minimum,
					current.next_leaf == no_tree_node
						? std::string("∅")
						: std::to_string(current.next_leaf));
			}
			const auto minimum_children = node_index == value.root
				? std::size_t{ 2 }
				: value.fanout / 2 + value.fanout % 2;
			return std::format(
				"Внутренний узел: заполнение {} / {} детей · минимум {} · разделителей {}",
				current.children.size(),
				value.fanout,
				minimum_children,
				current.keys.size());
		}
	};
}

extern "C" MNGL_MOD_EXPORT bool MNGL_MOD_CALL
mngl_mod_load_v1(mngl::mods::mod_context& context)
{
	const auto id = std::string(mngl::mods::tree::ids::b_plus);
	const bool algorithm_registered = context.add_implementation<
		mngl::mods::tree::algorithm>(
			mngl::mods::tree::algorithm_class_id,
			id,
			[] { return std::make_unique<b_plus_algorithm>(); });
	const bool inspector_registered = context.add_implementation<
		mngl::mods::tree::invariant_inspector>(
			mngl::mods::tree::invariant_inspector_class_id,
			id,
			[] { return std::make_unique<b_plus_invariant_inspector>(); });
	return algorithm_registered && inspector_registered;
}

extern "C" MNGL_MOD_EXPORT void MNGL_MOD_CALL mngl_mod_unload_v1()
{
}
