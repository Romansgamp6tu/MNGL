#include <mod_loader.hpp>
#include <tree_algorithm_api.hpp>
#include <tree_invariant_api.hpp>
#include <tree/avl.hpp>

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

	inline constexpr std::size_t maximum_node_count = 1'000'000;
	inline constexpr std::size_t maximum_tree_depth = 1'024;
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

	const algorithm_info avl_info{
		ids::avl,
		"AVL",
		"avl",
		"avl{",
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

	class avl_serialization_parser
	{
	public:
		explicit avl_serialization_parser(std::string_view source)
			: m_source(source)
		{
		}

		[[nodiscard]] document parse()
		{
			skip_whitespace();
			expect("avl{");
			tree result;
			result.root = parse_node(result, 0);
			expect_character('}');
			skip_whitespace();
			if (m_position != m_source.size())
			{
				fail("extra characters after the tree");
			}

			std::optional<key_kind> keys;
			if (!result.nodes.empty())
			{
				bool has_double = false;
				for (auto& current : result.nodes)
				{
					auto& raw = std::get<std::string>(current.keys.front().value);
					if (is_integer_token(raw))
					{
						std::int64_t integer = 0;
						if (!parse_integer(raw, integer))
						{
							fail("integer key is outside int64_t range");
						}
						current.keys.front().value = integer;
					}
					else
					{
						double floating = 0.0;
						if (!parse_double(raw, floating))
						{
							fail("key must be a finite number");
						}
						current.keys.front().value = floating;
						has_double = true;
					}
					current.keys.front().unquoted_serialized_value = false;
				}
				if (has_double)
				{
					for (auto& current : result.nodes)
					{
						auto& value = current.keys.front().value;
						if (const auto* integer = std::get_if<std::int64_t>(&value))
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
			return { std::string(ids::avl), std::move(result), keys };
		}

	private:
		[[noreturn]] void fail(std::string_view message) const
		{
			throw std::invalid_argument(
				"AVL serialization error at byte " +
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
				fail("expected avl{ prefix");
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

		[[nodiscard]] std::string read_node_header()
		{
			const std::size_t start = m_position;
			while (m_position < m_source.size() &&
				!m_source.substr(m_position).starts_with(",,"))
			{
				++m_position;
			}
			if (m_position == m_source.size())
			{
				fail("node header is incomplete");
			}
			const auto value = trim(m_source.substr(start, m_position - start));
			m_position += 2;
			const auto separator = value.rfind('|');
			if (separator == std::string_view::npos || separator == 0 ||
				separator + 1 == value.size())
			{
				fail("node must contain a key and stored height");
			}
			std::size_t height = 0;
			const auto height_text = trim(value.substr(separator + 1));
			const auto converted = std::from_chars(
				height_text.data(), height_text.data() + height_text.size(), height);
			if (converted.ec != std::errc{} ||
				converted.ptr != height_text.data() + height_text.size() ||
				height == 0)
			{
				fail("stored node height must be a positive integer");
			}
			return std::string(value);
		}

		static void synchronize_children(node& current)
		{
			current.children.clear();
			for (const auto child : current.binary_children)
			{
				if (child != no_tree_node)
				{
					current.children.push_back(child);
				}
			}
			current.leaf = current.children.empty();
		}

		[[nodiscard]] std::size_t parse_node(tree& result, std::size_t depth)
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
			if (!consume('('))
			{
				fail("expected a node or #");
			}
			const std::string header = read_node_header();
			if (result.nodes.size() >= maximum_node_count)
			{
				fail("maximum node count exceeded");
			}
			const auto index = result.nodes.size();
			const auto separator = header.find('|');
			std::size_t height = 0;
			const auto height_text = trim(
				std::string_view(header).substr(separator + 1));
			const auto converted = std::from_chars(
				height_text.data(), height_text.data() + height_text.size(), height);
			if (converted.ec != std::errc{} ||
				converted.ptr != height_text.data() + height_text.size() || height == 0)
			{
				fail("invalid stored node height");
			}
			node current;
			current.keys.push_back({
				std::string(trim(std::string_view(header).substr(0, separator))),
				std::to_string(height),
				true });
			result.nodes.push_back(std::move(current));
			const auto left = parse_node(result, depth + 1);
			expect_character(',');
			const auto right = parse_node(result, depth + 1);
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
					throw std::invalid_argument("AVL does not accept non-finite keys.");
				}
			}
			return *value;
		}
		throw std::invalid_argument("AVL key kind does not match the tree key kind.");
	}

	[[nodiscard]] bool supports_key_kind(key_kind keys) noexcept
	{
		return keys == key_kind::signed_integer || keys == key_kind::floating_point;
	}

	void require_supported_key_kind(key_kind keys)
	{
		if (!supports_key_kind(keys))
		{
			throw std::invalid_argument("AVL supports int64_t and finite double keys.");
		}
	}

	void synchronize_children(node& current)
	{
		current.children.clear();
		for (const auto child : current.binary_children)
		{
			if (child != no_tree_node)
			{
				current.children.push_back(child);
			}
		}
		current.leaf = current.children.empty();
	}

	template<class Value>
	void validate_typed_tree(const tree& value)
	{
		if (value.b_plus || value.fanout != 0 || value.nodes.size() > maximum_node_count)
		{
			throw std::invalid_argument("AVL model has invalid binary-tree metadata.");
		}
		if (value.nodes.empty())
		{
			if (value.root != no_tree_node)
			{
				throw std::invalid_argument("Empty AVL tree must use no_tree_node as its root.");
			}
			return;
		}
		if (value.root >= value.nodes.size())
		{
			throw std::invalid_argument("AVL root refers to a missing node.");
		}

		struct frame
		{
			std::size_t index;
			std::size_t lower;
			std::size_t upper;
			std::size_t depth;
			bool expanded;
		};
		std::vector<unsigned char> state(value.nodes.size(), 0);
		std::vector<std::size_t> heights(value.nodes.size(), 0);
		std::vector<frame> pending{{ value.root, no_tree_node, no_tree_node, 0, false }};
		std::size_t visited_count = 0;
		while (!pending.empty())
		{
			const auto current = pending.back();
			pending.pop_back();
			if (current.index >= value.nodes.size())
			{
				throw std::invalid_argument("AVL child refers to a missing node.");
			}
			const auto& current_node = value.nodes[current.index];
			if (!current.expanded)
			{
				if (current.depth >= maximum_tree_depth || state[current.index] != 0)
				{
					throw std::invalid_argument("AVL contains a cycle, repeated link, or excessive depth.");
				}
				state[current.index] = 1;
				++visited_count;
				if (current_node.keys.size() != 1 || current_node.red ||
					current_node.next_leaf != no_tree_node)
				{
					throw std::invalid_argument("AVL nodes must contain one key and binary-tree metadata.");
				}
				const auto current_key = key_value<Value>(current_node.keys.front());
				if (current.lower != no_tree_node &&
					!(key_value<Value>(value.nodes[current.lower].keys.front()) < current_key))
				{
					throw std::invalid_argument("AVL violates strict BST ordering.");
				}
				if (current.upper != no_tree_node &&
					!(current_key < key_value<Value>(value.nodes[current.upper].keys.front())))
				{
					throw std::invalid_argument("AVL violates strict BST ordering.");
				}
				const auto left = current_node.binary_children[0];
				const auto right = current_node.binary_children[1];
				const std::size_t child_count =
					static_cast<std::size_t>(left != no_tree_node) +
					static_cast<std::size_t>(right != no_tree_node);
				const auto first_child = left != no_tree_node ? left : right;
				if (current_node.children.size() != child_count ||
					current_node.leaf != (child_count == 0) ||
					(child_count > 0 && current_node.children[0] != first_child) ||
					(child_count > 1 && current_node.children[1] != right))
				{
					throw std::invalid_argument("AVL child list is inconsistent with binary links.");
				}
				for (const auto child : { left, right })
				{
					if (child != no_tree_node && child >= value.nodes.size())
					{
						throw std::invalid_argument("AVL child refers to a missing node.");
					}
				}
				pending.push_back({ current.index, current.lower, current.upper, current.depth, true });
				if (right != no_tree_node)
				{
					pending.push_back({ right, current.index, current.upper, current.depth + 1, false });
				}
				if (left != no_tree_node)
				{
					pending.push_back({ left, current.lower, current.index, current.depth + 1, false });
				}
				continue;
			}

			if (state[current.index] != 1)
			{
				throw std::invalid_argument("AVL has invalid traversal state.");
			}
			const auto left = current_node.binary_children[0];
			const auto right = current_node.binary_children[1];
			const auto left_height = left == no_tree_node ? 0 : heights[left];
			const auto right_height = right == no_tree_node ? 0 : heights[right];
			const auto expected_height = std::max(left_height, right_height) + 1;
			std::size_t stored_height = 0;
			const auto& detail = current_node.keys.front().detail;
			const auto converted = std::from_chars(
				detail.data(), detail.data() + detail.size(), stored_height);
			if (detail.empty() || converted.ec != std::errc{} ||
				converted.ptr != detail.data() + detail.size() || stored_height == 0)
			{
				throw std::invalid_argument("AVL stored height must be a positive integer.");
			}
			if (stored_height != expected_height)
			{
				throw std::invalid_argument("AVL stored height does not match its structure.");
			}
			const auto difference = left_height > right_height
				? left_height - right_height : right_height - left_height;
			if (difference > 1)
			{
				throw std::invalid_argument("AVL balance condition is violated.");
			}
			heights[current.index] = expected_height;
			state[current.index] = 2;
		}
		if (visited_count != value.nodes.size())
		{
			throw std::invalid_argument("AVL contains nodes unreachable from its root.");
		}
	}

	template<class Value>
	[[nodiscard]] avl<Value> make_native_tree(const tree& value)
	{
		using native_tree = avl<Value>;
		using native_node = typename native_tree::node_type;
		validate_typed_tree<Value>(value);
		if (value.nodes.empty())
		{
			return native_tree::from_validated_root(std::unique_ptr<native_node>{});
		}
		std::vector<std::unique_ptr<native_node>> nodes(value.nodes.size());
		std::vector<native_node*> raw_nodes(value.nodes.size(), nullptr);
		for (std::size_t i = 0; i < value.nodes.size(); ++i)
		{
			const auto& source = value.nodes[i];
			std::size_t height = 0;
			const auto& detail = source.keys.front().detail;
			const auto converted = std::from_chars(
				detail.data(), detail.data() + detail.size(), height);
			if (converted.ec != std::errc{} || converted.ptr != detail.data() + detail.size())
			{
				throw std::invalid_argument("Invalid AVL height metadata.");
			}
			nodes[i] = std::make_unique<native_node>(
				key_value<Value>(source.keys.front()), height);
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
		return native_tree::from_validated_root(std::move(nodes[value.root]));
	}

	template<class Value>
	[[nodiscard]] tree make_model_tree(const avl<Value>& source)
	{
		using native_node = typename avl<Value>::node_type;
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
			converted.keys.push_back({ current->value(),
				std::to_string(current->metadata()), false });
			if (current->left())
			{
				converted.binary_children[0] = pending.size();
				pending.push_back(current->left());
				result.nodes.emplace_back();
			}
			if (current->right())
			{
				converted.binary_children[1] = pending.size();
				pending.push_back(current->right());
				result.nodes.emplace_back();
			}
			synchronize_children(converted);
			result.nodes[index] = std::move(converted);
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
	[[nodiscard]] std::size_t locate_key(const tree& value, const key& search_key,
		std::vector<std::size_t>& path)
	{
		const auto requested = key_value<Value>(search_key);
		std::size_t current = value.root;
		while (current != no_tree_node)
		{
			path.push_back(current);
			const auto present = key_value<Value>(value.nodes[current].keys.front());
			if (requested < present)
			{
				current = value.nodes[current].binary_children[0];
			}
			else if (present < requested)
			{
				current = value.nodes[current].binary_children[1];
			}
			else
			{
				return current;
			}
		}
		return no_tree_node;
	}

	class avl_algorithm final : public algorithm
	{
	public:
		[[nodiscard]] const algorithm_info& info() const noexcept override
		{
			return avl_info;
		}

		[[nodiscard]] tree create_empty(key_kind keys) const override
		{
			require_supported_key_kind(keys);
			tree result;
			result.root = no_tree_node;
			return result;
		}

		[[nodiscard]] document deserialize(std::string_view source) const override
		{
			auto result = avl_serialization_parser{ source }.parse();
			validate(result.value, result.keys.value_or(key_kind::signed_integer));
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
			tree& value, const key& search_key, key_kind keys) const override
		{
			validate(value, keys);
			find_result result;
			if (keys == key_kind::signed_integer)
			{
				result.found = locate_key<std::int64_t>(value, search_key, result.path) != no_tree_node;
			}
			else
			{
				result.found = locate_key<double>(value, search_key, result.path) != no_tree_node;
			}
			result.visited_nodes = result.path.size();
			return result;
		}

		[[nodiscard]] operation_result insert(
			tree& value, key insert_key, key_kind keys, bool) const override
		{
			validate(value, keys);
			operation_result result;
			if (value.nodes.size() >= maximum_node_count)
			{
				std::vector<std::size_t> path;
				const bool already_present = keys == key_kind::signed_integer
					? locate_key<std::int64_t>(value, insert_key, path) != no_tree_node
					: locate_key<double>(value, insert_key, path) != no_tree_node;
				if (!already_present)
				{
					throw std::length_error("AVL node limit has been reached.");
				}
			}
			if (keys == key_kind::signed_integer)
			{
				auto native = make_native_tree<std::int64_t>(value);
				result.inserted = native.insert(key_value<std::int64_t>(insert_key));
				result.changed = result.inserted;
				if (result.changed)
				{
					value = make_model_tree(native);
				}
			}
			else
			{
				auto native = make_native_tree<double>(value);
				result.inserted = native.insert(key_value<double>(insert_key));
				result.changed = result.inserted;
				if (result.changed)
				{
					value = make_model_tree(native);
				}
			}
			return result;
		}

		[[nodiscard]] bulk_insert_result insert_many(
			tree& value, std::uint64_t requested_count, key_kind keys,
			std::mt19937_64& random_engine) const override
		{
			validate(value, keys);
			if (requested_count > maximum_node_count - value.nodes.size())
			{
				throw std::length_error("Mass insertion would exceed the AVL node limit.");
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
				value = make_model_tree(native);
				return { inserted, native.to_string() };
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
			value = make_model_tree(native);
			return { inserted, native.to_string() };
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
				auto native_left = make_native_tree<std::int64_t>(left);
				auto native_right = make_native_tree<std::int64_t>(right);
				native_left.merge(std::move(native_right));
				left = make_model_tree(native_left);
			}
			else
			{
				auto native_left = make_native_tree<double>(left);
				auto native_right = make_native_tree<double>(right);
				native_left.merge(std::move(native_right));
				left = make_model_tree(native_left);
			}
			return true;
		}

		[[nodiscard]] std::pair<tree, tree> split(
			const tree& value, const key& pivot, key_kind keys) const override
		{
			validate(value, keys);
			if (keys == key_kind::signed_integer)
			{
				auto native = make_native_tree<std::int64_t>(value);
				auto [left, right] = native.split(key_value<std::int64_t>(pivot));
				return { make_model_tree(left), make_model_tree(right) };
			}
			auto native = make_native_tree<double>(value);
			auto [left, right] = native.split(key_value<double>(pivot));
			return { make_model_tree(left), make_model_tree(right) };
		}

		[[nodiscard]] std::string serialize(const tree& value) const override
		{
			if (value.nodes.empty())
			{
				validate(value, key_kind::signed_integer);
				return "avl{#}";
			}
			if (value.nodes.front().keys.empty())
			{
				throw std::invalid_argument("AVL nodes must contain exactly one key.");
			}
			if (std::holds_alternative<std::int64_t>(value.nodes.front().keys.front().value))
			{
				return make_native_tree<std::int64_t>(value).to_string();
			}
			if (std::holds_alternative<double>(value.nodes.front().keys.front().value))
			{
				return make_native_tree<double>(value).to_string();
			}
			throw std::invalid_argument("AVL supports only int64_t and finite double keys.");
		}
	};

	class avl_invariant_inspector final : public invariant_inspector
	{
	public:
		[[nodiscard]] std::string_view algorithm_id() const noexcept override
		{
			return ids::avl;
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
						"AVL inspector supports int64_t and double keys.");
				report.checks = {
					{ "BST-порядок", true,
						"Ключи левого и правого поддеревьев строго упорядочены." },
					{ "Сохранённая высота", true,
						"Высота каждого узла совпадает с высотой его поддерева." },
					{ "Баланс AVL", true,
						"Для каждого узла разность высот детей не больше 1." }
				};
			}
			catch (const std::exception& error)
			{
				report.checks.push_back({
					"Инварианты AVL",
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
			if (node_index >= value.nodes.size() ||
				value.nodes[node_index].keys.size() != 1)
			{
				throw std::out_of_range("Invalid AVL node index.");
			}
			const auto read_height = [&](std::size_t index)
			{
				if (index == no_tree_node) return std::size_t{ 0 };
				if (index >= value.nodes.size() ||
					value.nodes[index].keys.size() != 1)
				{
					throw std::invalid_argument("Invalid AVL child node.");
				}
				const auto& detail = value.nodes[index].keys.front().detail;
				std::size_t height = 0;
				const auto parsed = std::from_chars(
					detail.data(), detail.data() + detail.size(), height);
				if (parsed.ec != std::errc{} ||
					parsed.ptr != detail.data() + detail.size())
				{
					throw std::invalid_argument("Invalid AVL stored height.");
				}
				return height;
			};
			const auto& current = value.nodes[node_index];
			const auto left_height = read_height(current.binary_children[0]);
			const auto right_height = read_height(current.binary_children[1]);
			const auto stored_height = read_height(node_index);
			const auto balance = static_cast<std::int64_t>(left_height) -
				static_cast<std::int64_t>(right_height);
			return std::format(
				"Ключ: {} · высота: {} · баланс: {} (левый {}, правый {})",
				std::holds_alternative<std::int64_t>(current.keys.front().value)
					? std::to_string(std::get<std::int64_t>(current.keys.front().value))
					: std::format("{}", std::get<double>(current.keys.front().value)),
				stored_height,
				balance,
				left_height,
				right_height);
		}
	};
}

extern "C" MNGL_MOD_EXPORT bool MNGL_MOD_CALL
mngl_mod_load_v1(mngl::mods::mod_context& context)
{
	const auto id = std::string(mngl::mods::tree::ids::avl);
	const bool algorithm_registered = context.add_implementation<
		mngl::mods::tree::algorithm>(
			mngl::mods::tree::algorithm_class_id,
			id,
			[] { return std::make_unique<avl_algorithm>(); });
	const bool inspector_registered = context.add_implementation<
		mngl::mods::tree::invariant_inspector>(
			mngl::mods::tree::invariant_inspector_class_id,
			id,
			[] { return std::make_unique<avl_invariant_inspector>(); });
	return algorithm_registered && inspector_registered;
}

extern "C" MNGL_MOD_EXPORT void MNGL_MOD_CALL mngl_mod_unload_v1()
{
}
