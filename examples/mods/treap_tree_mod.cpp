#include <mod_loader.hpp>
#include <tree_algorithm_api.hpp>
#include <tree_invariant_api.hpp>
#include <tree/treap.hpp>

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

	inline constexpr tree_id tree_algorithm_id = "mngl.tree.treap";

	inline constexpr std::string_view mod_id =
		"mngl.tree-practicum.treap";
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

	const algorithm_info treap_info{
		tree_algorithm_id,
		"Treap",
		"treap",
		"treap{",
		supported_keys,
		supported_operations,
		{ layout_transitions, 0 },
		{ .key_detail_prefix = "p=" },
		4
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

	class treap_serialization_parser
	{
	public:
		explicit treap_serialization_parser(std::string_view source)
			: m_source(source)
		{
		}

		[[nodiscard]] document parse()
		{
			skip_whitespace();
			expect_prefix("treap{");
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
					auto& key = node.keys.front();
					auto& raw = std::get<std::string>(key.value);
					if (is_integer_token(raw))
					{
						std::int64_t value = 0;
						if (!parse_integer(raw, value))
						{
							fail("ключ выходит за диапазон int64_t");
						}
					key.value = value;
					}
					else
					{
						double value = 0.0;
						if (!parse_floating_point(raw, value))
						{
							fail("ключ должен быть конечным числом");
						}
						key.value = value;
						has_floating_point = true;
					}
					key.unquoted_serialized_value = false;
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

			document parsed{
				std::string(tree_algorithm_id),
				std::move(result),
				keys
			};
			return parsed;
		}

	private:
		[[noreturn]] void fail(std::string_view message) const
		{
			throw std::invalid_argument(
				"Treap serialization error at byte " +
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

		void expect_prefix(std::string_view prefix)
		{
			if (!m_source.substr(m_position).starts_with(prefix))
			{
				fail("ожидался префикс treap{");
			}
			m_position += prefix.size();
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

		void expect_character(char character)
		{
			skip_whitespace();
			if (!consume_character(character))
			{
				fail(std::string("ожидался символ '") + character + "'");
			}
		}

		[[nodiscard]] std::string read_until(std::string_view delimiter)
		{
			const std::size_t start = m_position;
			while (m_position < m_source.size() &&
				!m_source.substr(m_position).starts_with(delimiter))
			{
				++m_position;
			}
			if (m_position == m_source.size())
			{
				fail("сериализация не завершена");
			}
			std::string value(m_source.substr(start, m_position - start));
			m_position += delimiter.size();
			const auto first = value.find_first_not_of(" \t\r\n");
			if (first == std::string::npos)
			{
				return {};
			}
			const auto last = value.find_last_not_of(" \t\r\n");
			return value.substr(first, last - first + 1);
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

			const auto key_text = read_until("|");
			if (key_text.empty())
			{
				fail("ключ узла пуст");
			}
			const auto priority_text = read_until(",,");
			std::uint64_t priority = 0;
			const auto parsed_priority = std::from_chars(
				priority_text.data(),
				priority_text.data() + priority_text.size(),
				priority);
			if (priority_text.empty() ||
				parsed_priority.ec != std::errc{} ||
				parsed_priority.ptr !=
					priority_text.data() + priority_text.size())
			{
				fail("приоритет должен быть числом uint64_t");
			}
			if (result.nodes.size() >= maximum_node_count)
			{
				fail("превышен лимит узлов дерева");
			}

			const std::size_t index = result.nodes.size();
			node current;
			current.keys.push_back({ std::move(key_text),
				std::to_string(priority), false });
			result.nodes.push_back(std::move(current));

			const std::size_t left = parse_node(result, depth + 1);
			expect_character(',');
			const std::size_t right = parse_node(result, depth + 1);
			expect_character(')');
			result.nodes[index].binary_children = { left, right };
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
						"Treap does not accept non-finite keys.");
				}
			}
			return *value;
		}
		throw std::invalid_argument(
			"Treap key kind does not match the tree key kind.");
	}

	[[nodiscard]] std::uint64_t node_priority(const node& source)
	{
		if (source.keys.size() != 1)
		{
			throw std::invalid_argument(
				"Every Treap node must contain exactly one key.");
		}
		const auto& text = source.keys.front().detail;
		std::uint64_t result = 0;
		const auto parsed = std::from_chars(
			text.data(),
			text.data() + text.size(),
			result);
		if (text.empty() || parsed.ec != std::errc{} ||
			parsed.ptr != text.data() + text.size())
		{
			throw std::invalid_argument(
				"Every Treap node must have a uint64_t priority.");
		}
		return result;
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
				"Treap supports int64_t and finite double keys.");
		}
	}

	template<class Value>
	void validate_typed_tree(const tree& value)
	{
		if (value.multiway || value.fanout != 0 ||
			value.nodes.size() > maximum_node_count)
		{
			throw std::invalid_argument(
				"Treap model has invalid binary-tree metadata.");
		}
		if (value.nodes.empty())
		{
			if (value.root != no_tree_node)
			{
				throw std::invalid_argument(
					"An empty Treap must use no_tree_node as its root.");
			}
			return;
		}
		if (value.root >= value.nodes.size())
		{
			throw std::invalid_argument(
				"Treap root refers to a missing node.");
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
				current.depth >= maximum_tree_depth)
			{
				throw std::invalid_argument(
					"Treap has an invalid node reference or is too deep.");
			}
			if (visited[current.index] != 0)
			{
				throw std::invalid_argument(
					"Treap contains a cycle or a repeated child reference.");
			}
			visited[current.index] = 1;
			++visited_count;

			const auto& current_node = value.nodes[current.index];
			if (current_node.keys.size() != 1 ||
				!current_node.children.empty() ||
				current_node.leaf ||
				current_node.next_leaf != no_tree_node)
			{
				throw std::invalid_argument(
					"Treap nodes must use the binary-tree model.");
			}
			const Value current_key = key_value<Value>(
				current_node.keys.front());
			const auto current_priority = node_priority(current_node);
			if (current.lower != no_tree_node &&
				!(key_value<Value>(value.nodes[current.lower].keys.front()) <
					current_key))
			{
				throw std::invalid_argument(
					"Treap violates the strict BST ordering.");
			}
			if (current.upper != no_tree_node &&
				!(current_key <
					key_value<Value>(value.nodes[current.upper].keys.front())))
			{
				throw std::invalid_argument(
					"Treap violates the strict BST ordering.");
			}

			const auto left = current_node.binary_children[0];
			const auto right = current_node.binary_children[1];
			for (const auto child : { left, right })
			{
				if (child == no_tree_node)
				{
					continue;
				}
				if (child >= value.nodes.size())
				{
					throw std::invalid_argument(
						"Treap child refers to a missing node.");
				}
				if (current_priority > node_priority(value.nodes[child]))
				{
					throw std::invalid_argument(
						"Treap violates the min-heap priority ordering.");
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
				"Treap contains nodes that are not reachable from its root.");
		}
	}

	template<class Value>
	[[nodiscard]] ::treap<Value> make_native_tree(const tree& value)
	{
		using native_tree = ::treap<Value>;
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
			const auto& source = value.nodes[i];
			nodes[i] = std::make_unique<native_node>(
				key_value<Value>(source.keys.front()),
				node_priority(source));
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
	[[nodiscard]] tree make_model_tree(const ::treap<Value>& source)
	{
		using native_node = typename ::treap<Value>::node_type;
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
			converted.keys.push_back({
				key{ current->value(),
					std::to_string(current->metadata()), false }
			});
			result.nodes[index] = std::move(converted);
			if (current->left())
			{
				const auto child = pending.size();
				result.nodes[index].binary_children[0] = child;
				pending.push_back(current->left());
				result.nodes.emplace_back();
			}
			if (current->right())
			{
				const auto child = pending.size();
				result.nodes[index].binary_children[1] = child;
				pending.push_back(current->right());
				result.nodes.emplace_back();
			}
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
			const auto unit_value = std::generate_canonical<
				double,
				std::numeric_limits<double>::digits>(random_engine);
			return unit_value * 2.0 - 1.0;
		}
	}

	class treap_algorithm final : public algorithm
	{
	public:
		[[nodiscard]] const algorithm_info& info() const noexcept override
		{
			return treap_info;
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
			auto result = treap_serialization_parser{ source }.parse();
			const auto keys = result.keys.value_or(
				key_kind::signed_integer);
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
			find_result result;
			if (keys == key_kind::signed_integer)
			{
				find_typed<std::int64_t>(value, search_key, result);
			}
			else
			{
				find_typed<double>(value, search_key, result);
			}
			return result;
		}

		[[nodiscard]] operation_result insert(
			tree& value,
			key insert_key,
			key_kind keys,
			bool) const override
		{
			validate(value, keys);
			if (keys == key_kind::signed_integer)
			{
				return insert_typed<std::int64_t>(
					value,
					std::move(insert_key));
			}
			return insert_typed<double>(value, std::move(insert_key));
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
					"Mass insertion would exceed the Treap node limit.");
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
				return "treap{#}";
			}
			if (value.nodes.front().keys.empty())
			{
				throw std::invalid_argument(
					"Treap nodes must contain exactly one key.");
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
				"Treap supports only int64_t and finite double keys.");
		}

	private:
		template<class Value>
		static void find_typed(
			const tree& value,
			const key& search_key,
			find_result& result)
		{
			const Value requested = key_value<Value>(search_key);
			std::size_t current = value.root;
			while (current != no_tree_node)
			{
				result.path.push_back(current);
				++result.visited_nodes;
				const Value present = key_value<Value>(
					value.nodes[current].keys.front());
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
					result.found = true;
					return;
				}
			}
		}

		template<class Value>
		[[nodiscard]] static operation_result insert_typed(
			tree& value,
			key insert_key)
		{
			if (value.nodes.size() >= maximum_node_count)
			{
				throw std::length_error("Treap node limit has been reached.");
			}
			auto native = make_native_tree<Value>(value);
			const bool inserted = native.insert(key_value<Value>(insert_key));
			if (inserted)
			{
				value = make_model_tree(native);
			}
			return { inserted, inserted, true, {} };
		}

		template<class Value>
		[[nodiscard]] static bulk_insert_result insert_many_typed(
			tree& value,
			std::uint64_t requested_count,
			std::mt19937_64& random_engine)
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
		[[nodiscard]] static erase_result erase_typed(
			tree& value,
			const key& erase_key)
		{
			auto native = make_native_tree<Value>(value);
			const bool erased = native.erase(key_value<Value>(erase_key));
			if (erased)
			{
				value = make_model_tree(native);
			}
			return { erased, erased };
		}

		template<class Value>
		[[nodiscard]] static bool merge_typed(tree& left, const tree& right)
		{
			auto native_left = make_native_tree<Value>(left);
			auto native_right = make_native_tree<Value>(right);
			native_left.merge(std::move(native_right));
			left = make_model_tree(native_left);
			return true;
		}

		template<class Value>
		[[nodiscard]] static std::pair<tree, tree> split_typed(
			const tree& value,
			const key& pivot)
		{
			auto native = make_native_tree<Value>(value);
			auto [left, right] = native.split(key_value<Value>(pivot));
			return {
				make_model_tree(left),
				make_model_tree(right)
			};
		}
	};

	class treap_invariant_inspector final : public invariant_inspector
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
						"Treap inspector supports int64_t and double keys.");
				report.checks = {
					{ "BST-порядок", true,
						"Ключи строго возрастают от левого поддерева к правому." },
					{ "Min-heap приоритетов", true,
						"Приоритет каждого родителя не больше приоритетов его детей." },
					{ "Связность", true,
						"Все узлы достижимы из корня, циклов и повторных ссылок нет." }
				};
			}
			catch (const std::exception& error)
			{
				report.checks.push_back({
					"Инварианты Treap",
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
				throw std::out_of_range("Invalid Treap node index.");
			const auto& current = value.nodes[node_index];
			return std::format(
				"Ключ: {} · приоритет: {} · дети: {} / {}",
				key_text(current),
				node_priority(current),
				child_priority(value, current.binary_children[0]),
				child_priority(value, current.binary_children[1]));
		}

	private:
		[[nodiscard]] static std::string key_text(const node& source)
		{
			if (source.keys.size() != 1)
				throw std::invalid_argument("Treap node must contain one key.");
			if (const auto* integer =
				std::get_if<std::int64_t>(&source.keys.front().value))
				return std::to_string(*integer);
			if (const auto* floating =
				std::get_if<double>(&source.keys.front().value))
				return std::format("{}", *floating);
			throw std::invalid_argument("Unsupported Treap key type.");
		}

		[[nodiscard]] static std::string child_priority(
			const tree& value,
			std::size_t child)
		{
			if (child == no_tree_node) return "∅";
			if (child >= value.nodes.size())
				throw std::invalid_argument("Invalid Treap child index.");
			return std::to_string(node_priority(value.nodes[child]));
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
		[] { return std::make_unique<treap_algorithm>(); });
	const bool inspector_registered =
		context.add_implementation<mngl::mods::tree::invariant_inspector>(
			mngl::mods::tree::invariant_inspector_class_id,
			id,
			[] { return std::make_unique<treap_invariant_inspector>(); });
	return algorithm_registered && inspector_registered;
}

extern "C" MNGL_MOD_EXPORT void MNGL_MOD_CALL mngl_mod_unload_v1()
{
}
