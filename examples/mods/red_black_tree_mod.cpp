#include <mod_loader.hpp>
#include <tree_algorithm_api.hpp>
#include <tree_invariant_api.hpp>
#include <tree/rb_tree.hpp>

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

	const algorithm_info red_black_info{
		ids::red_black,
		"Красно-чёрное дерево",
		"red-black",
		"rb_tree{",
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

	class rb_tree_serialization_parser
	{
	public:
		explicit rb_tree_serialization_parser(std::string_view source)
			: m_source(source)
		{
		}

		[[nodiscard]] document parse()
		{
			skip_whitespace();
			expect("rb_tree{");
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
			return { std::string(ids::red_black), std::move(result), keys };
		}

	private:
		[[noreturn]] void fail(std::string_view message) const
		{
			throw std::invalid_argument(
				"Red-black tree serialization error at byte " +
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
				fail("expected rb_tree{ prefix");
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
				fail("node must contain a key and color");
			}
			unsigned color = 2;
			const auto color_text = trim(value.substr(separator + 1));
			const auto converted = std::from_chars(
				color_text.data(), color_text.data() + color_text.size(), color);
			if (converted.ec != std::errc{} ||
				converted.ptr != color_text.data() + color_text.size() || color > 1)
			{
				fail("node color must be 0 (red) or 1 (black)");
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
			unsigned color = 2;
			const auto color_text = trim(
				std::string_view(header).substr(separator + 1));
			const auto converted = std::from_chars(
				color_text.data(), color_text.data() + color_text.size(), color);
			if (converted.ec != std::errc{} ||
				converted.ptr != color_text.data() + color_text.size() || color > 1)
			{
				fail("invalid node color");
			}
			node current;
			current.keys.push_back({
				std::string(trim(std::string_view(header).substr(0, separator))),
				std::to_string(color),
				true });
			current.red = color == 0;
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
					throw std::invalid_argument("Red-black tree does not accept non-finite keys.");
				}
			}
			return *value;
		}
		throw std::invalid_argument("Red-black tree key kind does not match the tree key kind.");
	}

	[[nodiscard]] bool supports_key_kind(key_kind keys) noexcept
	{
		return keys == key_kind::signed_integer || keys == key_kind::floating_point;
	}

	void require_supported_key_kind(key_kind keys)
	{
		if (!supports_key_kind(keys))
		{
			throw std::invalid_argument("Red-black tree supports int64_t and finite double keys.");
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
			throw std::invalid_argument("Red-black tree model has invalid binary-tree metadata.");
		}
		if (value.nodes.empty())
		{
			if (value.root != no_tree_node)
			{
				throw std::invalid_argument("Empty red-black tree must use no_tree_node as its root.");
			}
			return;
		}
		if (value.root >= value.nodes.size())
		{
			throw std::invalid_argument("Red-black tree root refers to a missing node.");
		}

		auto node_is_red = [](const node& current) -> bool
		{
			if (current.keys.size() != 1)
			{
				throw std::invalid_argument("Red-black tree nodes must contain exactly one key.");
			}
			const auto& color = current.keys.front().detail;
			if (color != "0" && color != "1")
			{
				throw std::invalid_argument("Red-black node color must be 0 (red) or 1 (black).");
			}
			const bool red = color == "0";
			if (current.red != red)
			{
				throw std::invalid_argument("Red-black node color metadata is inconsistent.");
			}
			return red;
		};

		struct frame
		{
			std::size_t index;
			std::size_t lower;
			std::size_t upper;
			std::size_t depth;
			bool parent_red;
			bool expanded;
		};
		std::vector<unsigned char> state(value.nodes.size(), 0);
		std::vector<std::size_t> black_heights(value.nodes.size(), 0);
		std::vector<frame> pending{{ value.root, no_tree_node, no_tree_node, 0, false, false }};
		std::size_t visited_count = 0;
		if (node_is_red(value.nodes[value.root]))
		{
			throw std::invalid_argument("Red-black tree root must be black.");
		}

		while (!pending.empty())
		{
			const auto current = pending.back();
			pending.pop_back();
			if (current.index >= value.nodes.size())
			{
				throw std::invalid_argument("Red-black child refers to a missing node.");
			}
			const auto& current_node = value.nodes[current.index];
			if (!current.expanded)
			{
				if (current.depth >= maximum_tree_depth || state[current.index] != 0)
				{
					throw std::invalid_argument("Red-black tree contains a cycle, repeated link, or excessive depth.");
				}
				state[current.index] = 1;
				++visited_count;
				const bool red = node_is_red(current_node);
				if (current.parent_red && red)
				{
					throw std::invalid_argument("A red node cannot have a red parent.");
				}
				if (current_node.next_leaf != no_tree_node)
				{
					throw std::invalid_argument("Red-black nodes cannot contain B+ leaf metadata.");
				}
				const auto current_key = key_value<Value>(current_node.keys.front());
				if (current.lower != no_tree_node &&
					!(key_value<Value>(value.nodes[current.lower].keys.front()) < current_key))
				{
					throw std::invalid_argument("Red-black tree violates strict BST ordering.");
				}
				if (current.upper != no_tree_node &&
					!(current_key < key_value<Value>(value.nodes[current.upper].keys.front())))
				{
					throw std::invalid_argument("Red-black tree violates strict BST ordering.");
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
					throw std::invalid_argument("Red-black child list is inconsistent with binary links.");
				}
				for (const auto child : { left, right })
				{
					if (child != no_tree_node && child >= value.nodes.size())
					{
						throw std::invalid_argument("Red-black child refers to a missing node.");
					}
				}
				pending.push_back({ current.index, current.lower, current.upper, current.depth, current.parent_red, true });
				if (right != no_tree_node)
				{
					pending.push_back({ right, current.index, current.upper, current.depth + 1, red, false });
				}
				if (left != no_tree_node)
				{
					pending.push_back({ left, current.lower, current.index, current.depth + 1, red, false });
				}
				continue;
			}

			if (state[current.index] != 1)
			{
				throw std::invalid_argument("Red-black tree has invalid traversal state.");
			}
			const auto left = current_node.binary_children[0];
			const auto right = current_node.binary_children[1];
			const auto left_black_height = left == no_tree_node ? 0 : black_heights[left];
			const auto right_black_height = right == no_tree_node ? 0 : black_heights[right];
			if (left_black_height != right_black_height)
			{
				throw std::invalid_argument("Red-black paths have different black heights.");
			}
			black_heights[current.index] = left_black_height +
				(node_is_red(current_node) ? 0 : 1);
			state[current.index] = 2;
		}

		if (visited_count != value.nodes.size())
		{
			throw std::invalid_argument("Red-black tree contains nodes unreachable from its root.");
		}
	}
	template<class Value>
	[[nodiscard]] rb_tree<Value> make_native_tree(const tree& value)
	{
		using native_tree = rb_tree<Value>;
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
			const rb_color color = source.keys.front().detail == "0"
				? rb_color::red : rb_color::black;
			nodes[i] = std::make_unique<native_node>(
				key_value<Value>(source.keys.front()), color);
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
	[[nodiscard]] tree make_model_tree(const rb_tree<Value>& source)
	{
		using native_node = typename rb_tree<Value>::node_type;
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
				std::to_string(static_cast<int>(current->metadata())), false });
			converted.red = current->metadata() == rb_color::red;
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

	class rb_tree_algorithm final : public algorithm
	{
	public:
		[[nodiscard]] const algorithm_info& info() const noexcept override
		{
			return red_black_info;
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
			auto result = rb_tree_serialization_parser{ source }.parse();
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
					throw std::length_error("Red-black tree node limit has been reached.");
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
				throw std::length_error("Mass insertion would exceed the Red-black tree node limit.");
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
				return "rb_tree{#}";
			}
			if (value.nodes.front().keys.empty())
			{
				throw std::invalid_argument("Red-black tree nodes must contain exactly one key.");
			}
			if (std::holds_alternative<std::int64_t>(value.nodes.front().keys.front().value))
			{
				return make_native_tree<std::int64_t>(value).to_string();
			}
			if (std::holds_alternative<double>(value.nodes.front().keys.front().value))
			{
				return make_native_tree<double>(value).to_string();
			}
			throw std::invalid_argument("Red-black tree supports only int64_t and finite double keys.");
		}
	};

	class red_black_invariant_inspector final : public invariant_inspector
	{
	public:
		[[nodiscard]] std::string_view algorithm_id() const noexcept override
		{
			return ids::red_black;
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
						"Red-black inspector supports int64_t and double keys.");
				const auto black_height = value.nodes.empty()
					? std::size_t{ 0 }
					: subtree_black_height(value, value.root);
				report.checks = {
					{ "Корень чёрный", true,
						"Корневой узел имеет чёрный цвет." },
					{ "Нет красных соседей", true,
						"У красного узла нет красного родителя или ребёнка." },
					{ "Чёрная высота", true,
						std::format(
							"Пути имеют одинаковую чёрную высоту; BH корня = {} (null-листья не считаются).",
							black_height) },
					{ "BST-порядок", true,
						"Ключи сохраняют строгий порядок бинарного дерева поиска." }
				};
			}
			catch (const std::exception& error)
			{
				report.checks.push_back({
					"Инварианты RB",
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
				throw std::out_of_range("Invalid red-black node index.");
			}
			const auto& current = value.nodes[node_index];
			return std::format(
				"Цвет: {} · чёрная высота поддерева: {} · ключ: {}",
				current.red ? "красный" : "чёрный",
				subtree_black_height(value, node_index),
				key_text(current.keys.front()));
		}

	private:
		[[nodiscard]] static std::string key_text(const key& source)
		{
			if (const auto* value = std::get_if<std::int64_t>(&source.value))
				return std::to_string(*value);
			if (const auto* value = std::get_if<double>(&source.value))
				return std::format("{}", *value);
			return {};
		}

		[[nodiscard]] static std::size_t subtree_black_height(
			const tree& value,
			std::size_t subtree_root)
		{
			std::vector<std::size_t> order;
			std::vector<std::size_t> pending{ subtree_root };
			while (!pending.empty())
			{
				const auto index = pending.back();
				pending.pop_back();
				order.push_back(index);
				for (const auto child : value.nodes[index].binary_children)
					if (child != no_tree_node) pending.push_back(child);
			}
			std::vector<std::size_t> heights(value.nodes.size(), 0);
			for (auto iterator = order.rbegin(); iterator != order.rend(); ++iterator)
			{
				const auto index = *iterator;
				const auto& current = value.nodes[index];
				const auto left = current.binary_children[0];
				const auto right = current.binary_children[1];
				const auto left_height =
					left == no_tree_node ? 0 : heights[left];
				const auto right_height =
					right == no_tree_node ? 0 : heights[right];
				if (left_height != right_height)
				{
					throw std::logic_error(
						"Red-black subtree has inconsistent black height.");
				}
				heights[index] = left_height +
					(current.red ? 0 : 1);
			}
			return heights[subtree_root];
		}
	};
}

extern "C" MNGL_MOD_EXPORT bool MNGL_MOD_CALL
mngl_mod_load_v1(mngl::mods::mod_context& context)
{
	const auto id = std::string(mngl::mods::tree::ids::red_black);
	const bool algorithm_registered = context.add_implementation<
		mngl::mods::tree::algorithm>(
			mngl::mods::tree::algorithm_class_id,
			id,
			[] { return std::make_unique<rb_tree_algorithm>(); });
	const bool inspector_registered = context.add_implementation<
		mngl::mods::tree::invariant_inspector>(
			mngl::mods::tree::invariant_inspector_class_id,
			id,
			[] { return std::make_unique<red_black_invariant_inspector>(); });
	return algorithm_registered && inspector_registered;
}

extern "C" MNGL_MOD_EXPORT void MNGL_MOD_CALL mngl_mod_unload_v1()
{
}
