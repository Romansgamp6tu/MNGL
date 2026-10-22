#include <mod_loader.hpp>
#include <tree_algorithm_api.hpp>
#include <tree_custom_command_api.hpp>
#include <tree_invariant_api.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
	using namespace mngl::mods::tree;

	inline constexpr tree_id tree_algorithm_id = "mngl.tree.bitwise-trie";

	inline constexpr std::size_t maximum_key_count = 500'000;
	inline constexpr std::size_t maximum_node_count = 1'000'000;
	inline constexpr std::uint64_t sign_bit = std::uint64_t{ 1 } << 63;
	inline constexpr key_kind_mask supported_keys =
		key_kind_bit(key_kind::signed_integer);
	inline constexpr operation_mask supported_operations =
		static_cast<operation_mask>(operation::find) |
		operation::insert |
		operation::random_insert |
		operation::erase;
	inline constexpr operation_mask layout_transitions =
		static_cast<operation_mask>(operation::insert) |
		operation::erase;

	const algorithm_info bitwise_trie_info{
		tree_algorithm_id,
		"Битовый бор (Patricia)",
		"bitwise-trie",
		"bitwise_trie{",
		supported_keys,
		supported_operations,
		{ layout_transitions, 0 },
		{},
		6
	};

	[[nodiscard]] std::uint64_t encode(std::int64_t value) noexcept
	{
		return std::bit_cast<std::uint64_t>(value) ^ sign_bit;
	}

	void require_integer_keys(key_kind keys)
	{
		if (keys != key_kind::signed_integer)
		{
			throw std::invalid_argument(
				"Bitwise Trie supports signed int64_t keys only.");
		}
	}

	[[nodiscard]] std::int64_t key_value(const key& source)
	{
		if (const auto* value = std::get_if<std::int64_t>(&source.value))
		{
			return *value;
		}
		throw std::invalid_argument(
			"Bitwise Trie key kind does not match the tree key kind.");
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

	[[nodiscard]] std::vector<std::int64_t> collect_keys(const tree& value)
	{
		std::vector<std::int64_t> result;
		result.reserve(value.nodes.size() / 2 + 1);
		for (const auto& current : value.nodes)
		{
			if (current.leaf && current.keys.size() == 1)
			{
				if (const auto* item = std::get_if<std::int64_t>(
					&current.keys.front().value))
				{
					result.push_back(*item);
				}
			}
		}
		return result;
	}

	struct encoded_key
	{
		std::uint64_t bits;
		std::int64_t value;
	};

	[[nodiscard]] tree build_tree(std::vector<std::int64_t> keys)
	{
		if (keys.size() > maximum_key_count)
		{
			throw std::length_error("Bitwise Trie key limit exceeded.");
		}
		std::vector<encoded_key> ordered;
		ordered.reserve(keys.size());
		for (const auto value : keys)
		{
			ordered.push_back({ encode(value), value });
		}
		std::sort(
			ordered.begin(), ordered.end(),
			[](const auto& left, const auto& right)
			{
				return left.bits < right.bits;
			});
		if (std::adjacent_find(
			ordered.begin(), ordered.end(),
			[](const auto& left, const auto& right)
			{
				return left.bits == right.bits;
			}) != ordered.end())
		{
			throw std::invalid_argument("Bitwise Trie keys must be unique.");
		}

		tree result;
		result.root = no_tree_node;
		if (ordered.empty())
		{
			return result;
		}
		result.nodes.reserve(ordered.size() * 2 - 1);

		const auto build_range = [&](auto&& self,
			std::size_t first,
			std::size_t last) -> std::size_t
		{
			if (last - first == 1)
			{
				const auto index = result.nodes.size();
				node leaf;
				leaf.keys.push_back({ ordered[first].value, {}, false });
				leaf.leaf = true;
				result.nodes.push_back(std::move(leaf));
				return index;
			}

			const auto difference = ordered[first].bits ^ ordered[last - 1].bits;
			if (difference == 0)
			{
				throw std::logic_error(
					"Bitwise Trie builder received duplicate keys.");
			}
			const auto bit_index = static_cast<unsigned>(
				63 - std::countl_zero(difference));
			const auto bit_mask = std::uint64_t{ 1 } << bit_index;
			const auto split = std::find_if(
				ordered.begin() + static_cast<std::ptrdiff_t>(first),
				ordered.begin() + static_cast<std::ptrdiff_t>(last),
				[bit_mask](const auto& item)
				{
					return (item.bits & bit_mask) != 0;
				});
			const auto middle = static_cast<std::size_t>(
				std::distance(ordered.begin(), split));
			if (middle == first || middle == last)
			{
				throw std::logic_error(
					"Bitwise Trie critical bit did not split the key range.");
			}

			const auto index = result.nodes.size();
			node branch;
			const auto bit_label = std::format("bit {}", bit_index);
			branch.keys.push_back({
				bit_label,
				std::format("branch bit {}; left=0, right=1", bit_index),
				false });
			result.nodes.push_back(std::move(branch));
			const auto left = self(self, first, middle);
			const auto right = self(self, middle, last);
			result.nodes[index].binary_children = { left, right };
			synchronize_children(result.nodes[index]);
			return index;
		};

		result.root = build_range(build_range, 0, ordered.size());
		if (result.nodes.size() > maximum_node_count)
		{
			throw std::length_error("Bitwise Trie node limit exceeded.");
		}
		return result;
	}

	[[nodiscard]] unsigned parse_branch_bit(const node& current)
	{
		if (current.keys.size() != 1 ||
			!std::holds_alternative<std::string>(current.keys.front().value))
		{
			throw std::invalid_argument(
				"Bitwise Trie branch node must contain one label.");
		}
		const std::string_view detail = current.keys.front().detail;
		constexpr std::string_view prefix = "branch bit ";
		if (!detail.starts_with(prefix))
		{
			throw std::invalid_argument(
				"Bitwise Trie branch label has no bit index.");
		}
		const auto number = detail.substr(prefix.size());
		const auto end = number.find_first_not_of("0123456789");
		const auto digits = number.substr(0, end);
		unsigned result = 0;
		const auto parsed = std::from_chars(
			digits.data(), digits.data() + digits.size(), result);
		if (digits.empty() || parsed.ec != std::errc{} ||
			parsed.ptr != digits.data() + digits.size() || result > 63)
		{
			throw std::invalid_argument(
				"Bitwise Trie branch bit must be between 0 and 63.");
		}
		return result;
	}

	struct subtree_range
	{
		std::uint64_t minimum;
		std::uint64_t maximum;
	};

	void validate_tree(const tree& value, key_kind keys)
	{
		require_integer_keys(keys);
		if (value.multiway || value.fanout != 0 ||
			value.nodes.size() > maximum_node_count)
		{
			throw std::invalid_argument("Invalid Bitwise Trie metadata.");
		}
		if (value.nodes.empty())
		{
			if (value.root != no_tree_node)
			{
				throw std::invalid_argument(
					"Empty Bitwise Trie must use the empty root sentinel.");
			}
			return;
		}
		if (value.root >= value.nodes.size())
		{
			throw std::invalid_argument("Bitwise Trie root is out of range.");
		}

		std::vector<bool> visited(value.nodes.size(), false);
		std::unordered_set<std::int64_t> seen_keys;
		seen_keys.reserve(value.nodes.size() / 2 + 1);
		std::size_t leaf_count = 0;
		const auto validate_subtree = [&](auto&& self,
			std::size_t index) -> subtree_range
		{
			if (index >= value.nodes.size() || visited[index])
			{
				throw std::invalid_argument(
					"Bitwise Trie contains an invalid child or a cycle.");
			}
			visited[index] = true;
			const auto& current = value.nodes[index];
			if (current.emphasized || current.next_leaf != no_tree_node)
			{
				throw std::invalid_argument(
					"Bitwise Trie node contains unsupported metadata.");
			}

			const bool leaf = current.binary_children[0] == no_tree_node &&
				current.binary_children[1] == no_tree_node;
			if (leaf)
			{
				if (!current.leaf || !current.children.empty() ||
					current.keys.size() != 1)
				{
					throw std::invalid_argument(
						"Bitwise Trie leaf must contain one key and no children.");
				}
				const auto* stored = std::get_if<std::int64_t>(
					&current.keys.front().value);
				if (!stored || !seen_keys.insert(*stored).second)
				{
					throw std::invalid_argument(
						"Bitwise Trie leaves must contain unique int64_t keys.");
				}
				++leaf_count;
				const auto bits = encode(*stored);
				return { bits, bits };
			}

			const auto left_index = current.binary_children[0];
			const auto right_index = current.binary_children[1];
			if (current.leaf || left_index == no_tree_node ||
				right_index == no_tree_node ||
				current.children.size() != 2 ||
				current.children[0] != left_index ||
				current.children[1] != right_index)
			{
				throw std::invalid_argument(
					"Bitwise Trie branch must have ordered zero and one children.");
			}
			const unsigned bit_index = parse_branch_bit(current);
			if (std::get<std::string>(current.keys.front().value) !=
				std::format("bit {}", bit_index))
			{
				throw std::invalid_argument(
					"Bitwise Trie branch label does not match its bit index.");
			}
			const auto left = self(self, left_index);
			const auto right = self(self, right_index);
			const auto bit_mask = std::uint64_t{ 1 } << bit_index;
			if ((left.minimum & bit_mask) != 0 ||
				(left.maximum & bit_mask) != 0 ||
				(right.minimum & bit_mask) == 0 ||
				(right.maximum & bit_mask) == 0)
			{
				throw std::invalid_argument(
					"Bitwise Trie keys do not match their branch bits.");
			}
			const auto minimum = std::min(left.minimum, right.minimum);
			const auto maximum = std::max(left.maximum, right.maximum);
			const auto difference = minimum ^ maximum;
			if (difference == 0)
			{
				throw std::invalid_argument(
					"Bitwise Trie branch does not separate distinct keys.");
			}
			const auto critical_bit = static_cast<unsigned>(
				63 - std::countl_zero(difference));
			if (critical_bit != bit_index)
			{
				throw std::invalid_argument(
					"Bitwise Trie branch does not use its highest differing bit.");
			}
			return { minimum, maximum };
		};

		(void)validate_subtree(validate_subtree, value.root);
		if (leaf_count > maximum_key_count ||
			std::find(visited.begin(), visited.end(), false) != visited.end())
		{
			throw std::invalid_argument(
				"Bitwise Trie contains unreachable nodes or too many keys.");
		}
	}

	[[nodiscard]] bool parse_integer(
		std::string_view input,
		std::int64_t& value) noexcept
	{
		while (!input.empty() && (input.front() == ' ' || input.front() == '\t' ||
			input.front() == '\r' || input.front() == '\n'))
		{
			input.remove_prefix(1);
		}
		while (!input.empty() && (input.back() == ' ' || input.back() == '\t' ||
			input.back() == '\r' || input.back() == '\n'))
		{
			input.remove_suffix(1);
		}
		if (input.starts_with('+')) input.remove_prefix(1);
		if (input.empty()) return false;
		const auto parsed = std::from_chars(
			input.data(), input.data() + input.size(), value);
		return parsed.ec == std::errc{} &&
			parsed.ptr == input.data() + input.size();
	}

	class serialization_parser
	{
	public:
		explicit serialization_parser(std::string_view source) : m_source(source) {}

		[[nodiscard]] document parse()
		{
			skip_space();
			expect("bitwise_trie{");
			std::vector<std::int64_t> keys;
			std::unordered_set<std::int64_t> seen;
			if (!take('}'))
			{
				while (true)
				{
					skip_space();
					const auto start = m_position;
					while (m_position < m_source.size() &&
						m_source[m_position] != ',' && m_source[m_position] != '}')
					{
						++m_position;
					}
					std::int64_t value = 0;
					if (!parse_integer(m_source.substr(start, m_position - start), value))
					{
						fail("invalid int64_t key");
					}
					if (!seen.insert(value).second)
					{
						fail("duplicate key");
					}
					keys.push_back(value);
					if (keys.size() > maximum_key_count)
					{
						fail("maximum key count exceeded");
					}
					skip_space();
					if (take('}')) break;
					expect(',');
				}
			}
			skip_space();
			if (m_position != m_source.size())
			{
				fail("unexpected data after tree");
			}
			return {
				std::string(tree_algorithm_id),
				build_tree(std::move(keys)),
				key_kind::signed_integer
			};
		}

	private:
		[[noreturn]] void fail(std::string_view message) const
		{
			throw std::invalid_argument(
				"Bitwise Trie serialization at byte " +
				std::to_string(m_position) + ": " + std::string(message));
		}

		void skip_space()
		{
			while (m_position < m_source.size() &&
				(m_source[m_position] == ' ' || m_source[m_position] == '\t' ||
					m_source[m_position] == '\r' || m_source[m_position] == '\n'))
			{
				++m_position;
			}
		}

		bool take(char character)
		{
			if (m_position < m_source.size() && m_source[m_position] == character)
			{
				++m_position;
				return true;
			}
			return false;
		}

		void expect(char character)
		{
			if (!take(character))
			{
				fail(std::string("expected '") + character + "'");
			}
		}

		void expect(std::string_view token)
		{
			if (m_source.substr(m_position, token.size()) != token)
			{
				fail("invalid tree prefix");
			}
			m_position += token.size();
		}

		std::string_view m_source;
		std::size_t m_position = 0;
	};

	class bitwise_trie_algorithm final : public algorithm
	{
	public:
		[[nodiscard]] const algorithm_info& info() const noexcept override
		{
			return bitwise_trie_info;
		}

		[[nodiscard]] tree create_empty(key_kind keys) const override
		{
			require_integer_keys(keys);
			tree result;
			result.root = no_tree_node;
			return result;
		}

		[[nodiscard]] document deserialize(
			std::string_view source) const override
		{
			return serialization_parser(source).parse();
		}

		void validate(const tree& value, key_kind keys) const override
		{
			validate_tree(value, keys);
		}

		[[nodiscard]] find_result find(
			tree& value,
			const key& search_key,
			key_kind keys) const override
		{
			validate_tree(value, keys);
			const auto target = key_value(search_key);
			find_result result;
			if (value.root == no_tree_node)
			{
				return result;
			}
			const auto target_bits = encode(target);
			auto index = value.root;
			while (index != no_tree_node)
			{
				result.path.push_back(index);
				++result.visited_nodes;
				const auto& current = value.nodes[index];
				if (current.leaf)
				{
					result.found =
						std::get<std::int64_t>(current.keys.front().value) == target;
					return result;
				}
				const auto bit_index = parse_branch_bit(current);
				const auto side = static_cast<std::size_t>(
					((target_bits >> bit_index) & 1u) != 0);
				index = current.binary_children[side];
			}
			return result;
		}

		[[nodiscard]] operation_result insert(
			tree& value,
			key insert_key,
			key_kind keys,
			bool collect_animation_events) const override
		{
			(void)collect_animation_events;
			validate_tree(value, keys);
			const auto inserted_value = key_value(insert_key);
			auto values = collect_keys(value);
			if (std::find(values.begin(), values.end(), inserted_value) != values.end())
			{
				return {};
			}
			if (values.size() >= maximum_key_count)
			{
				throw std::length_error("Bitwise Trie key limit exceeded.");
			}
			values.push_back(inserted_value);
			value = build_tree(std::move(values));
			return { true, true, true, {} };
		}

		[[nodiscard]] bulk_insert_result insert_many(
			tree& value,
			std::uint64_t requested_count,
			key_kind keys,
			std::mt19937_64& random_engine) const override
		{
			validate_tree(value, keys);
			if (requested_count > 100'000)
			{
				throw std::length_error(
					"Bitwise Trie bulk insertion is limited to 100000 keys per operation.");
			}
			auto values = collect_keys(value);
			if (requested_count > maximum_key_count - values.size())
			{
				throw std::length_error("Bitwise Trie key limit exceeded.");
			}
			std::unordered_set<std::int64_t> seen(
				values.begin(), values.end());
			seen.reserve(values.size() + static_cast<std::size_t>(requested_count));
			std::uniform_int_distribution<std::int64_t> distribution(
				std::numeric_limits<std::int64_t>::min(),
				std::numeric_limits<std::int64_t>::max());
			std::uint64_t inserted = 0;
			while (inserted < requested_count)
			{
				const auto candidate = distribution(random_engine);
				if (seen.insert(candidate).second)
				{
					values.push_back(candidate);
					++inserted;
				}
			}
			if (inserted != 0)
			{
				value = build_tree(std::move(values));
			}
			return { inserted, serialize(value) };
		}

		[[nodiscard]] erase_result erase(
			tree& value,
			const key& erase_key,
			key_kind keys) const override
		{
			validate_tree(value, keys);
			const auto target = key_value(erase_key);
			auto values = collect_keys(value);
			const auto found = std::find(values.begin(), values.end(), target);
			if (found == values.end())
			{
				return {};
			}
			values.erase(found);
			value = build_tree(std::move(values));
			return { true, true };
		}

		[[nodiscard]] std::string serialize(const tree& value) const override
		{
			validate_tree(value, key_kind::signed_integer);
			auto values = collect_keys(value);
			std::sort(values.begin(), values.end(),
				[](std::int64_t left, std::int64_t right)
				{
					return encode(left) < encode(right);
				});
			std::string result = "bitwise_trie{";
			for (std::size_t index = 0; index < values.size(); ++index)
			{
				if (index != 0) result.push_back(',');
				result += std::to_string(values[index]);
			}
			result.push_back('}');
			return result;
		}
	};

	class bitwise_trie_command_handler final : public custom_command_handler
	{
	public:
		[[nodiscard]] bool supports_command(
			std::string_view command_id) const noexcept override
		{
			return command_id == "prefix_search";
		}

		[[nodiscard]] command_result execute(
			std::string_view command_id,
			tree& value,
			key_kind keys,
			std::span<const command_argument> arguments) const override
		{
			if (!supports_command(command_id))
			{
				throw std::invalid_argument(
					"Bitwise Trie does not provide this custom command.");
			}
			if (arguments.size() != 1 ||
				!std::holds_alternative<std::string>(arguments.front()))
			{
				throw std::invalid_argument(
					"prefix_search expects one binary string argument.");
			}
			const auto& prefix = std::get<std::string>(arguments.front());
			if (prefix.empty() || prefix.size() > 64)
			{
				throw std::invalid_argument(
					"Bit prefix length must be between 1 and 64.");
			}

			std::uint64_t prefix_value = 0;
			for (const char bit : prefix)
			{
				if (bit != '0' && bit != '1')
				{
					throw std::invalid_argument(
						"Bit prefix can contain only 0 and 1.");
				}
				prefix_value = (prefix_value << 1) | (bit == '1' ? 1u : 0u);
			}

			validate_tree(value, keys);
			command_result result;
			if (value.root == no_tree_node)
			{
				result.message = "Префикс «" + prefix +
					"»: совпадений нет, дерево пусто.";
				return result;
			}

			const unsigned lowest_tested_bit = static_cast<unsigned>(
				64 - prefix.size());
			std::size_t subtree_root = value.root;
			while (subtree_root != no_tree_node)
			{
				result.highlighted_nodes.push_back(subtree_root);
				const auto& current = value.nodes[subtree_root];
				if (current.leaf)
				{
					break;
				}
				const unsigned branch_bit = parse_branch_bit(current);
				if (branch_bit < lowest_tested_bit)
				{
					break;
				}
				const auto prefix_offset = 63u - branch_bit;
				const auto side = static_cast<std::size_t>(
					prefix[prefix_offset] == '1');
				subtree_root = current.binary_children[side];
			}

			std::vector<std::size_t> pending{ subtree_root };
			std::size_t match_count = 0;
			while (!pending.empty())
			{
				const auto index = pending.back();
				pending.pop_back();
				if (index == no_tree_node || index >= value.nodes.size())
				{
					throw std::invalid_argument(
						"Bitwise Trie contains an invalid node index.");
				}
				const auto& current = value.nodes[index];
				if (current.leaf)
				{
					const auto leaf_value = std::get<std::int64_t>(
						current.keys.front().value);
					const auto encoded = encode(leaf_value);
					if ((encoded >> lowest_tested_bit) == prefix_value)
					{
						result.highlighted_nodes.push_back(index);
						++match_count;
					}
					continue;
				}
				pending.push_back(current.binary_children[0]);
				pending.push_back(current.binary_children[1]);
			}

			result.message = std::format(
				"Префикс «{}»: найдено {} ключей. Подсвечены ветви поиска и листья совпадений.",
				prefix,
				match_count);
			return result;
		}
	};

	class bitwise_trie_invariant_inspector final : public invariant_inspector
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
				validate_tree(value, keys);
				std::size_t leaves = 0;
				std::size_t branches = 0;
				for (const auto& current : value.nodes)
				{
					if (current.leaf)
						++leaves;
					else
						++branches;
				}
				report.checks = {
					{ "Сжатые критические биты", true,
						std::format(
							"{} внутренних ветвей; каждая использует старший различающийся бит.",
							branches) },
					{ "Уникальные int64-листья", true,
						std::format("{} листьев содержат уникальные ключи.", leaves) },
					{ "Связность Patricia", true,
						"Все узлы достижимы, каждый внутренний узел имеет ветви 0 и 1." }
				};
			}
			catch (const std::exception& error)
			{
				report.checks.push_back({
					"Инварианты Patricia",
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
				throw std::out_of_range("Invalid Patricia node index.");
			const auto& current = value.nodes[node_index];
			if (current.leaf)
			{
				if (current.keys.size() != 1 ||
					!std::holds_alternative<std::int64_t>(
						current.keys.front().value))
				{
					throw std::invalid_argument(
						"Patricia leaf must contain one int64 key.");
				}
				return std::format(
					"Лист Patricia · ключ {}",
					std::get<std::int64_t>(current.keys.front().value));
			}
			const auto bit_index = parse_branch_bit(current);
			return std::format(
				"Внутренняя вершина · критический бит {} · 0 → левый, 1 → правый",
				bit_index);
		}
	};
}

extern "C" MNGL_MOD_EXPORT bool MNGL_MOD_CALL
mngl_mod_load_v1(mngl::mods::mod_context& context)
{
	const bool algorithm_registered =
		context.add_implementation<mngl::mods::tree::algorithm>(
		mngl::mods::tree::algorithm_class_id,
		std::string(tree_algorithm_id),
		[] { return std::make_unique<bitwise_trie_algorithm>(); });
	const bool command_handler_registered =
		context.add_implementation<
			mngl::mods::tree::custom_command_handler>(
				mngl::mods::tree::custom_command_class_id,
				std::string(tree_algorithm_id),
				[] { return std::make_unique<bitwise_trie_command_handler>(); });
	const bool inspector_registered = context.add_implementation<
		mngl::mods::tree::invariant_inspector>(
			mngl::mods::tree::invariant_inspector_class_id,
			std::string(tree_algorithm_id),
			[] { return std::make_unique<bitwise_trie_invariant_inspector>(); });
	return algorithm_registered && command_handler_registered &&
		inspector_registered;
}

extern "C" MNGL_MOD_EXPORT void MNGL_MOD_CALL mngl_mod_unload_v1()
{
}
