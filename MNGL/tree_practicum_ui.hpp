#pragma once

#include "event.hpp"
#include "imgui_compat.hpp"
#include "logger.h"
#include "render_window.hpp"
#include "tree_practicum_storage.hpp"
#include "tree_renderer.hpp"
#include "undo_redo_stack.hpp"
#include "../tree/avl.hpp"
#include "../tree/b_plus_tree.hpp"
#include "../tree/bst.hpp"
#include "../tree/rb_tree.hpp"
#include "../tree/splay_tree.hpp"
#include "../tree/treap.hpp"

#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace tree_practicum
{
	class tree_practicum_ui
	{
		struct workspace_entry
		{
			std::string_view name;
			std::string_view description;
			bool snapshot;
		};

		struct function_entry
		{
			std::size_t tree_index;
			std::string_view name;
			std::string_view signature;
			std::string_view description;
		};

		struct explorer_entry
		{
			std::filesystem::path path;
			bool directory;
		};

		struct tree_document_state
		{
			std::filesystem::path local_path;
			std::string display_name;
			std::uint64_t current_state_id = 0;
			std::uint64_t saved_state_id = 0;
		};

		enum class file_dialog_mode
		{
			open,
			load_tree,
			save_tree,
			save_session,
			rename_local_file,
			rename_archive_entry,
			save_copy
		};

		enum class workspace_action_kind
		{
			none,
			new_workspace,
			open_file,
			load_history
		};

		enum class tree_tool_operation
		{
			find,
			insert,
			random_insert,
			erase,
			merge,
			split
		};

		struct pending_workspace_action
		{
			workspace_action_kind kind = workspace_action_kind::none;
			std::filesystem::path path;
			std::string archive_member_path;
		};

		struct session_manifest
		{
			std::uint32_t version = 0;
			std::string name;
			std::string current_tree_path;
			std::vector<std::string> history_paths;
			std::vector<std::string> tree_paths;
		};

		inline static std::mutex pending_file_drops_mutex;
		inline static std::vector<std::string> pending_file_drops;
		inline static std::string pending_file_dialog_error;
		inline static std::atomic_uint64_t next_tree_animation_source_id{ 1 };
		inline static std::mutex tree_animation_queues_mutex;
		inline static constexpr SDL_DialogFileFilter tree_file_dialog_filter{
			"Tree serialization",
			"tree"
		};

		inline static constexpr std::size_t no_visual_node =
			std::numeric_limits<std::size_t>::max();
		inline static constexpr float visual_node_width = 156.0f;
		inline static constexpr float visual_node_height = 54.0f;
		inline static constexpr std::size_t maximum_animated_tree_nodes = 512;
		inline static constexpr std::size_t maximum_tree_animation_steps = 48;
		inline static constexpr float tree_animation_step_seconds = 0.14f;
		inline static constexpr float tree_layout_animation_seconds = 0.38f;

		struct visual_key
		{
			struct big_integer_text
			{
				std::string decimal;
			};

			using value_type = std::variant<
				std::string,
				double,
				std::int64_t,
				big_integer_text>;

			value_type value;
			std::string detail;
			bool unquoted_serialized_value = false;
		};

		struct visual_node
		{
			std::vector<visual_key> keys;
			std::vector<std::size_t> children;
			std::array<std::size_t, 2> binary_children{
				no_visual_node,
				no_visual_node
			};
			bool leaf = false;
			bool red = false;
			std::size_t next_leaf = no_visual_node;
		};

		struct visual_tree
		{
			std::vector<visual_node> nodes;
			std::size_t root = 0;
			std::size_t fanout = 0;
			bool b_plus = false;
		};

		struct tree_preview_geometry
		{
			mngl::two_d::tree_draw_model model;
			mngl::two_d::tree_layout layout;
			mngl::two_d::tree_draw_options draw_options;
			std::uint64_t revision = 0;
			std::uint64_t source_tree_signature = 0;
			std::size_t animation_step = 0;
			int tree_index = -1;
			double tree_width = 0.0;
			double tree_height = 0.0;
			float leaf_link_space = 0.0f;
			bool current_tree_loaded = false;
			bool showing_animation = false;
		};

		struct tree_canvas_view_state
		{
			std::optional<mngl::texture> texture;
			std::optional<visual_key> context_menu_key;
			std::optional<std::size_t> context_menu_node;
			int tree_index = -1;
			double zoom = 1.0;
			glm::dvec2 pan{};
			bool view_initialized = false;
			bool fit_requested = true;
		};

		struct tree_comparison_pane
		{
			std::string archive_member_path;
			std::optional<visual_tree> tree;
			std::size_t tree_index = 0;
			tree_canvas_view_state canvas;
		};

		struct tree_preview_cache_state
		{
			std::shared_ptr<const tree_preview_geometry> geometry;
			std::shared_ptr<const tree_preview_geometry>
				visibility_geometry;
			std::shared_ptr<const mngl::two_d::tree_draw_visibility>
				visibility;
			mngl::two_d::tree_draw_area visibility_bounds{};
			std::shared_ptr<const tree_preview_geometry>
				visibility_request_geometry;
			std::uint64_t visibility_request_id = 0;
			mngl::two_d::tree_draw_area visibility_request_bounds{};
			mngl::two_d::tree_draw_culler culler;
		};

		struct tree_visual_event
		{
			enum class kind
			{
				insert_node,
				rotate_node_up
			};

			kind action;
			std::size_t node_index;
			std::size_t parent_index = no_visual_node;
			std::size_t grandparent_index = no_visual_node;
			std::size_t side = 0;
			visual_node inserted_node;

			tree_visual_event(
				kind action,
				std::size_t node,
				std::size_t parent = no_visual_node,
				std::size_t grandparent = no_visual_node,
				std::size_t child_side = 0,
				visual_node added_node = {})
				: action(action),
				node_index(node),
				parent_index(parent),
				grandparent_index(grandparent),
				side(child_side),
				inserted_node(std::move(added_node))
			{
			}
		};

		struct tree_operation_result
		{
			bool inserted = false;
			bool changed = false;
			bool animation_events_complete = true;
			std::vector<tree_visual_event> events;
		};

		struct tree_bulk_insert_result
		{
			std::uint64_t inserted_count = 0;
			std::optional<std::string> serialization;
		};

		struct tree_erase_result
		{
			bool erased = false;
			bool changed = false;
		};

		struct tree_find_result
		{
			bool found = false;
			bool changed = false;
			std::size_t visited_nodes = 0;
			std::vector<std::size_t> path;
		};

		enum class search_tree_key_kind
		{
			text,
			floating_point,
			signed_integer,
			big_integer
		};

		struct tree_native_change_result
		{
			bool operation_succeeded = false;
			bool changed = false;
			std::string serialization;
		};

		struct tree_animation_event : mngl::eventBase
		{
			std::uint64_t source_id;
			tree_visual_event step;

			tree_animation_event(
				std::uint64_t source,
				tree_visual_event animation_step)
				: mngl::eventBase{ mngl::Events::user, 0 },
				source_id(source),
				step(std::move(animation_step))
			{
			}
		};

		struct tree_animation_event_queue
		{
			std::mutex mutex;
			std::vector<tree_visual_event> events;
		};

		enum class tree_animation_mode : unsigned char
		{
			splay_steps,
			layout_transition
		};

		struct tree_animation_state
		{
			tree_animation_mode mode = tree_animation_mode::splay_steps;
			visual_tree tree;
			std::vector<tree_visual_event> events;
			std::vector<glm::dvec2> transition_start_centers;
			glm::dvec2 transition_start_layout_size{};
			std::optional<std::size_t> emphasized_node;
			int tree_index = -1;
			std::size_t next_event = 0;
			float elapsed_seconds = 0.0f;
			std::optional<tree_visual_event> active_event;
		};

		inline static std::unordered_map<
			std::uint64_t,
			std::weak_ptr<tree_animation_event_queue>>
			tree_animation_event_queues;

		class tree_serialization_error : public std::runtime_error
		{
		public:
			using std::runtime_error::runtime_error;
		};

		struct visual_layout_node
		{
			std::vector<float> cell_widths;
			std::vector<std::string> display_values;
			std::vector<std::string> display_details;
			std::vector<float> value_scales;
			std::vector<float> detail_scales;
			float width = 0.0f;
			float subtree_width = 0.0f;
			float center_x = 0.0f;
			float center_y = 0.0f;
			std::size_t depth = 0;
		};

		struct pending_tree_file_requests
		{
			std::vector<std::string> paths;
			std::string dialog_error;
		};

		struct interface_state
		{
			int selected_workspace = 0;
			int active_tree = 0;
			std::array<char, 1024> key_input{};
			std::uint64_t random_count = 0;

			bool operator==(const interface_state&) const = default;

			[[nodiscard]] std::size_t history_bytes() const noexcept
			{
				return sizeof(*this);
			}
		};

		struct archive_entry_history_delta
		{
			std::vector<tree_practicum::archive_entry> added_entries;
		};

		struct tree_history_snapshot
		{
			std::string current_tree_name;
			std::string current_tree_serialization;
			std::string current_archive_member_path;
			std::filesystem::path current_tree_path;
			std::string merge_source_archive_path;
			std::array<std::string, 2> split_result_member_paths;
			std::string split_pivot_label;
			std::optional<search_tree_key_kind> loaded_search_tree_key_kind;
			std::optional<search_tree_key_kind> split_result_key_kind;
			std::shared_ptr<const archive_entry_history_delta> archive_entry_delta;
			std::uint64_t document_state_id = 0;
			int active_tree = 0;
			int loaded_tree_index = -1;
			bool loaded_search_tree_valid = false;
			bool archive_entries_present = false;

			[[nodiscard]] std::size_t history_bytes() const noexcept
			{
				std::size_t bytes = sizeof(*this);
				const auto add = [&bytes](std::size_t value)
				{
					const auto maximum =
						std::numeric_limits<std::size_t>::max();
					bytes = value > maximum - bytes
						? maximum
						: bytes + value;
				};
				add(current_tree_name.size());
				add(current_tree_serialization.size());
				add(current_archive_member_path.size());
				add(merge_source_archive_path.size());
				add(split_pivot_label.size());
				add(current_tree_path.native().size() *
					sizeof(std::filesystem::path::value_type));
				for (const auto& path : split_result_member_paths)
				{
					add(path.size());
				}
				if (archive_entry_delta)
				{
					for (const auto& entry : archive_entry_delta->added_entries)
					{
						add(sizeof(entry));
						add(entry.path.size());
						add(entry.contents.size());
					}
				}
				return bytes;
			}
		};

		struct tree_history_transition
		{
			tree_history_snapshot before;
			tree_history_snapshot after;

			[[nodiscard]] std::size_t history_bytes() const noexcept
			{
				const auto before_bytes = before.history_bytes();
				const auto after_bytes = after.history_bytes();
				const auto maximum =
					std::numeric_limits<std::size_t>::max();
				return after_bytes > maximum - before_bytes
					? maximum
					: before_bytes + after_bytes;
			}
		};

		struct tree_history_state
		{
			std::shared_ptr<const tree_history_transition> transition;
			bool is_after = false;

			bool operator==(const tree_history_state&) const = default;

			[[nodiscard]] std::size_t history_bytes() const noexcept
			{
				if (!transition)
				{
					return sizeof(*this);
				}
				const auto transition_bytes =
					transition->history_bytes();
				const auto maximum =
					std::numeric_limits<std::size_t>::max();
				return transition_bytes > maximum - sizeof(*this)
					? maximum
					: sizeof(*this) + transition_bytes;
			}
		};

		struct history_state
		{
			std::variant<interface_state, tree_history_state> value;

			history_state(interface_state state)
				: value(std::move(state))
			{
			}

			history_state(tree_history_state state)
				: value(std::move(state))
			{
			}

			bool operator==(const history_state&) const = default;

			[[nodiscard]] std::size_t history_bytes() const noexcept
			{
				return std::visit(
					[](const auto& state)
					{
						return state.history_bytes();
					},
					value);
			}
		};

		inline static constexpr std::size_t undo_history_limit = 100;
		inline static constexpr std::size_t undo_history_memory_limit_bytes =
			64u * 1024u * 1024u;
		inline static constexpr std::size_t max_history_file_size =
			128u * 1024u * 1024u;
		inline static constexpr std::string_view history_file_magic =
			"MNGL-HTREE";
		inline static constexpr std::uint32_t history_file_version = 1;
		using history_stack_type = undo_redo_stack<
			history_state,
			undo_history_limit,
			undo_history_memory_limit_bytes>;

		struct history_document
		{
			std::uint64_t current_document_state_id = 0;
			std::uint64_t next_document_state_id = 1;
			std::uint64_t current_tree_checksum = 0;
			interface_state current_interface;
			tree_history_snapshot current_tree;
			history_stack_type history;
			std::vector<history_state> undo_states;
			std::vector<history_state> redo_states;
		};

		struct history_binary_writer
		{
			std::string data;

			void write_bytes(std::string_view value)
			{
				if (value.size() > max_history_file_size - data.size())
				{
					throw std::length_error(
						"Serialized undo history exceeds the supported size limit");
				}
				data.append(value);
			}

			template<typename Unsigned>
			void write_unsigned(Unsigned value)
			{
				static_assert(std::is_unsigned_v<Unsigned>);
				if (sizeof(Unsigned) > max_history_file_size - data.size())
				{
					throw std::length_error(
						"Serialized undo history exceeds the supported size limit");
				}
				for (std::size_t i = 0; i < sizeof(Unsigned); ++i)
				{
					data.push_back(static_cast<char>(value & 0xFFu));
					value >>= 8u;
				}
			}

			void write_signed(std::int32_t value)
			{
				write_unsigned(std::bit_cast<std::uint32_t>(value));
			}

			void write_bool(bool value)
			{
				write_unsigned<std::uint8_t>(value ? 1u : 0u);
			}

			void write_string(std::string_view value)
			{
				write_unsigned<std::uint64_t>(
					static_cast<std::uint64_t>(value.size()));
				write_bytes(value);
			}
		};

		struct history_binary_reader
		{
			explicit history_binary_reader(std::string_view contents)
				: contents(contents)
			{
				if (contents.size() > max_history_file_size)
				{
					throw std::length_error(
						"Undo history file exceeds the supported size limit");
				}
			}

			template<typename Unsigned>
			[[nodiscard]] Unsigned read_unsigned()
			{
				static_assert(std::is_unsigned_v<Unsigned>);
				if (sizeof(Unsigned) > contents.size() - offset)
				{
					throw std::invalid_argument(
						"Undo history file is truncated");
				}

				Unsigned value = 0;
				for (std::size_t i = 0; i < sizeof(Unsigned); ++i)
				{
					value |= static_cast<Unsigned>(
						static_cast<unsigned char>(contents[offset++])) <<
						(i * 8u);
				}
				return value;
			}

			[[nodiscard]] std::int32_t read_signed()
			{
				return std::bit_cast<std::int32_t>(
					read_unsigned<std::uint32_t>());
			}

			[[nodiscard]] bool read_bool()
			{
				const auto value = read_unsigned<std::uint8_t>();
				if (value > 1u)
				{
					throw std::invalid_argument(
						"Undo history contains an invalid boolean value");
				}
				return value != 0;
			}

			[[nodiscard]] std::string read_string()
			{
				const auto size = read_unsigned<std::uint64_t>();
				if (size > tree_practicum::detail::max_archive_entry_size ||
					size > contents.size() - offset)
				{
					throw std::invalid_argument(
						"Undo history contains an invalid string length");
				}
				const auto value = contents.substr(
					offset,
					static_cast<std::size_t>(size));
				offset += static_cast<std::size_t>(size);
				return std::string(value);
			}

			void read_bytes(char* destination, std::size_t size)
			{
				if (size > contents.size() - offset)
				{
					throw std::invalid_argument(
						"Undo history file is truncated");
				}
				std::copy_n(contents.data() + offset, size, destination);
				offset += size;
			}

			[[nodiscard]] std::string_view read_bytes(std::size_t size)
			{
				if (size > contents.size() - offset)
				{
					throw std::invalid_argument(
						"Undo history file is truncated");
				}
				const auto value = contents.substr(offset, size);
				offset += size;
				return value;
			}

			[[nodiscard]] bool finished() const noexcept
			{
				return offset == contents.size();
			}

		private:
			std::string_view contents;
			std::size_t offset = 0;
		};

		inline static constexpr std::array<std::string_view, 6> tree_names{
			"BST",
			"AVL",
			"Красно-чёрное",
			"Splay",
			"Декартово (Treap)",
			"B+ tree"
		};
		inline static constexpr std::array<std::string_view, 6> tree_file_stems{
			"bst",
			"avl",
			"red-black",
			"splay",
			"treap",
			"b-plus"
		};

		inline static constexpr std::array<workspace_entry, 4> workspace_entries{ {
			{ "Черновик", "Текущая рабочая область", false },
			{ "Демо: семь ключей", "Демонстрационный сценарий", false },
			{ "До split", "Снимок демонстрационного дерева", true },
			{ "После split", "Снимок результата", true }
		} };

		inline static constexpr std::array<function_entry, 36> functions{ {
			{ 0, "find", "find(key: int64_t | double) [F]", "Находит точный ключ и подсвечивает путь до узла. BST не меняется." },
			{ 0, "insert", "insert(key: int64_t | double) [A / Ins]", "Добавляет уникальный ключ, не меняя порядок BST. Дерево не балансируется, поэтому стоимость зависит от высоты; дубликат не добавляется." },
			{ 0, "erase", "erase(key: int64_t | double) [D / Del]", "Удаляет ключ, заменяя узел с двумя потомками его следующим ключом по порядку. Структура остаётся BST, но балансировка не выполняется." },
			{ 0, "merge", "merge(source: .tree) [M]", "Добавляет в текущее BST все ключи выбранного дерева-источника. Тип дерева и тип ключей должны совпадать, а каждый ключ текущего дерева должен быть меньше каждого ключа источника. Источник сохраняется." },
			{ 0, "split", "split(pivot: int64_t | double) [C]", "Разделяет BST без повторной вставки: левая часть содержит ключи < pivot, правая — ключи >= pivot. Обе части сохраняются и открываются отдельными вкладками." },
			{ 0, "insert_random", "insert_random(count: uint64_t) [R]", "Добавляет указанное число уникальных случайных ключей без анимации. Алиасы: mass insert, random test, mass random insert." },
			{ 1, "find", "find(key: int64_t | double) [F]", "Находит точный ключ и подсвечивает путь до узла. AVL не меняется." },
			{ 1, "insert", "insert(key: int64_t | double) [A / Ins]", "Добавляет уникальный ключ в AVL и обновляет высоты с поворотами, чтобы для каждого узла разница высот поддеревьев была не больше единицы." },
			{ 1, "erase", "erase(key: int64_t | double) [D / Del]", "Удаляет ключ и перебалансирует AVL на пути к корню; сохранённые высоты и условие балансировки обновляются." },
			{ 1, "merge", "merge(source: .tree) [M]", "Объединяет текущее AVL с выбранным источником за счёт join-операции. Нужны тот же тип дерева и ключей и диапазоны max(current) < min(source); источник остаётся в архиве." },
			{ 1, "split", "split(pivot: int64_t | double) [C]", "Делит AVL с сохранением баланса: левая часть содержит ключи < pivot, правая — ключи >= pivot. Результаты сохраняются и показываются отдельными вкладками." },
			{ 1, "insert_random", "insert_random(count: uint64_t) [R]", "Добавляет указанное число уникальных случайных ключей без анимации. Алиасы: mass insert, random test, mass random insert." },
			{ 2, "find", "find(key: int64_t | double) [F]", "Находит точный ключ и подсвечивает путь до узла. Красно-чёрное дерево не меняется." },
			{ 2, "insert", "insert(key: int64_t | double) [A / Ins]", "Добавляет уникальный ключ в красно-чёрное дерево, перекрашивает узлы и выполняет повороты для сохранения чёрной высоты и ограничения на красные узлы." },
			{ 2, "erase", "erase(key: int64_t | double) [D / Del]", "Удаляет ключ и выполняет fix-up: восстанавливает чёрную высоту, цвета и баланс красно-чёрного дерева." },
			{ 2, "merge", "merge(source: .tree) [M]", "Объединяет два красно-чёрных дерева с непересекающимися упорядоченными диапазонами: max(current) < min(source). Источник не изменяется." },
			{ 2, "split", "split(pivot: int64_t | double) [C]", "Разделяет красно-чёрное дерево с восстановлением цветов и чёрной высоты: слева < pivot, справа >= pivot. Части доступны в отдельных вкладках." },
			{ 2, "insert_random", "insert_random(count: uint64_t) [R]", "Добавляет указанное число уникальных случайных ключей без анимации. Алиасы: mass insert, random test, mass random insert." },
			{ 3, "find", "find(key: int64_t | double) [F]", "Находит ключ и подсвечивает путь. Найденный или последний посещённый узел перемещается в корень Splay." },
			{ 3, "insert", "insert(key: int64_t | double) [A / Ins]", "Добавляет уникальный ключ и поднимает его в корень последовательностью zig, zig-zig и zig-zag. Повторный ключ не добавляется, но тоже может быть поднят в корень." },
			{ 3, "erase", "erase(key: int64_t | double) [D / Del]", "Удаляет ключ. При успешном удалении левое и правое поддеревья соединяются через максимальный узел левой части; поиск может изменить форму Splay даже если ключа нет." },
			{ 3, "merge", "merge(source: .tree) [M]", "Соединяет упорядоченные Splay-деревья, подняв максимум левой части и присоединив справа источник. Требуется max(current) < min(source); источник сохраняется." },
			{ 3, "split", "split(pivot: int64_t | double) [C]", "Разделяет Splay-дерево по границе < pivot / >= pivot. Каждая часть сохраняет корректную структуру поиска и открывается отдельной вкладкой." },
			{ 3, "insert_random", "insert_random(count: uint64_t) [R]", "Добавляет указанное число уникальных случайных ключей без анимации. Алиасы: mass insert, random test, mass random insert." },
			{ 4, "find", "find(key: int64_t | double) [F]", "Находит точный ключ и подсвечивает путь до узла. Treap не меняется." },
			{ 4, "insert", "insert(key: int64_t | double) [A / Ins]", "Добавляет уникальный ключ с новым случайным приоритетом и поворотами восстанавливает min-heap порядок Treap поверх порядка BST." },
			{ 4, "erase", "erase(key: int64_t | double) [D / Del]", "Удаляет ключ поворотами вниз по приоритетам, сохраняя одновременно BST-порядок и min-heap инвариант Treap." },
			{ 4, "merge", "merge(source: .tree) [M]", "Объединяет Treap линейным слиянием по приоритетам при условии max(current) < min(source). Приоритеты существующих узлов сохраняются; источник остаётся доступен." },
			{ 4, "split", "split(pivot: int64_t | double) [C]", "Разделяет Treap на ключи < pivot и >= pivot без повторной генерации приоритетов; обе части сохраняют BST- и heap-инварианты." },
			{ 4, "insert_random", "insert_random(count: uint64_t) [R]", "Добавляет указанное число уникальных случайных ключей без анимации. Алиасы: mass insert, random test, mass random insert." },
			{ 5, "find", "find(key: int64_t | double) [F]", "Ищет точный ключ в листьях B+ дерева и подсвечивает путь через узлы-разделители. Дерево не меняется." },
			{ 5, "insert", "insert(key: int64_t | double) [A / Ins]", "Добавляет уникальный ключ в лист B+ дерева. При переполнении лист разделяется, а разделители и связи листьев обновляются." },
			{ 5, "erase", "erase(key: int64_t | double) [D / Del]", "Удаляет ключ из листа. Недозаполненные узлы заимствуют ключи у соседей или объединяются; родительские разделители пересчитываются." },
			{ 5, "merge", "merge(source: .tree) [M]", "Перестраивает B+ дерево из ключей текущего дерева и источника, если max(current) < min(source). Fanout текущего дерева сохраняется, исходная запись остаётся нетронутой." },
			{ 5, "split", "split(pivot: int64_t | double) [C]", "Перестраивает две B+ структуры с тем же fanout: листья слева содержат ключи < pivot, справа — ключи >= pivot. Результаты сохраняются в отдельных вкладках." },
			{ 5, "insert_random", "insert_random(count: uint64_t) [R]", "Добавляет указанное число уникальных случайных ключей без анимации. Алиасы: mass insert, random test, mass random insert." }
		} };

	public:
		tree_practicum_ui(
			mngl::render_window& render_window,
			mngl::two_d::font& tree_font,
			SDL_Window* native_window = nullptr)
			: m_render_window(render_window)
			, m_tree_font(tree_font)
			, m_native_window(
				native_window ? native_window : render_window.raw())
		{
			register_tree_animation_queue();
			register_file_drop_listener();
			std::error_code error;
			m_explorer_directory = std::filesystem::current_path(error);
			const std::string initial_directory_error = error
				? "Не удалось определить рабочую папку: " + error.message()
				: std::string{};
			if (error)
			{
				m_explorer_directory = ".";
			}
			set_path_buffer(m_explorer_path, path_to_utf8(m_explorer_directory));
			set_path_buffer(m_key_input, "42");
			begin_new_workspace();
			m_show_demo_trees = false;
			refresh_explorer();
			if (!initial_directory_error.empty())
			{
				m_explorer_error = initial_directory_error;
				logger.error("Unable to determine the initial workspace directory: ",
					initial_directory_error);
			}
			logger.info("Tree Practicum workspace initialized.");
		}

		~tree_practicum_ui() noexcept
		{
			std::lock_guard lock(tree_animation_queues_mutex);
			tree_animation_event_queues.erase(m_tree_animation_source_id);
		}

		tree_practicum_ui(const tree_practicum_ui&) = delete;
		tree_practicum_ui& operator=(const tree_practicum_ui&) = delete;
		tree_practicum_ui(tree_practicum_ui&&) = delete;
		tree_practicum_ui& operator=(tree_practicum_ui&&) = delete;

		void draw(mngl::imgui_context::frame_scope& frame)
		{
			m_hovered_tree_key.reset();
			m_hovered_tree_comparison_pane.reset();
			process_pending_tree_files();
			draw_menu(frame);
			ImGui::DockSpaceOverViewport();

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			if (!viewport)
			{
				commit_pending_input_edits();
				process_keyboard_shortcuts();
				return;
			}

			const ImVec2 origin = viewport->WorkPos;
			const ImVec2 area = viewport->WorkSize;
			if (area.x <= 0.0f || area.y <= 0.0f)
			{
				commit_pending_input_edits();
				process_keyboard_shortcuts();
				return;
			}

			const float explorer_width = area.x * 0.20f;
			const float functions_width = area.x * 0.25f;
			const float info_height = area.y * 0.25f;
			const float top_height = area.y - info_height;
			const float workspace_width = area.x - explorer_width - functions_width;
			const ImVec2 workspace_origin{
				origin.x + explorer_width,
				origin.y
			};
			if (m_show_explorer)
			{
				draw_explorer(
					frame,
					origin,
					{ explorer_width, area.y });
			}
			if (m_show_workspace)
			{
				draw_workspace(
					frame,
					workspace_origin,
					{ workspace_width, top_height });
			}
			if (m_show_functions)
			{
				draw_functions(
					frame,
					{ origin.x + explorer_width + workspace_width, origin.y },
					{ functions_width, top_height });
			}
			if (m_show_information)
			{
				draw_information(
					frame,
					{ origin.x + explorer_width, origin.y + top_height },
					{ area.x - explorer_width, info_height });
			}
			if (m_show_tools)
			{
				draw_tools(
					frame,
					origin,
					area);
			}
			else
			{
				commit_pending_input_edits();
			}
			draw_tool_parameters(frame);
			draw_file_dialog();
			draw_file_delete_dialog();
			draw_unsaved_changes_dialog();
			process_keyboard_shortcuts();
		}

	private:
		[[nodiscard]] interface_state current_interface_state() const noexcept
		{
			return {
				m_selected_workspace,
				m_new_tree_type,
				m_key_input,
				m_random_count
			};
		}

		static void validate_interface_state(const interface_state& state)
		{
			if (state.selected_workspace < 0 ||
				static_cast<std::size_t>(state.selected_workspace) >=
					workspace_entries.size() ||
				state.active_tree < 0 ||
				static_cast<std::size_t>(state.active_tree) >=
					tree_names.size() ||
				std::find(
					state.key_input.begin(),
					state.key_input.end(),
					'\0') == state.key_input.end())
			{
				throw std::invalid_argument(
					"Undo history contains an invalid interface state");
			}
		}

		static void validate_tree_member_path(std::string_view path)
		{
			if (path.empty())
			{
				return;
			}
			if (tree_practicum::detail::normalize_archive_path(path) != path ||
				archive_extension(path) != ".tree")
			{
				throw std::invalid_argument(
					"Undo history contains an invalid tree member path");
			}
		}

		static void write_interface_state(
			history_binary_writer& writer,
			const interface_state& state)
		{
			validate_interface_state(state);
			writer.write_signed(state.selected_workspace);
			writer.write_signed(state.active_tree);
			writer.write_bytes(std::string_view(
				state.key_input.data(),
				state.key_input.size()));
			writer.write_unsigned(state.random_count);
		}

		[[nodiscard]] static interface_state read_interface_state(
			history_binary_reader& reader)
		{
			interface_state state;
			state.selected_workspace = reader.read_signed();
			state.active_tree = reader.read_signed();
			reader.read_bytes(
				state.key_input.data(),
				state.key_input.size());
			state.random_count =
				reader.read_unsigned<std::uint64_t>();
			validate_interface_state(state);
			return state;
		}

		static void write_key_kind(
			history_binary_writer& writer,
			const std::optional<search_tree_key_kind>& kind)
		{
			writer.write_bool(kind.has_value());
			if (kind)
			{
				writer.write_unsigned<std::uint8_t>(
					static_cast<std::uint8_t>(*kind));
			}
		}

		[[nodiscard]] static std::optional<search_tree_key_kind>
			read_key_kind(history_binary_reader& reader)
		{
			if (!reader.read_bool())
			{
				return std::nullopt;
			}
			const auto value = reader.read_unsigned<std::uint8_t>();
			if (value > static_cast<std::uint8_t>(
				search_tree_key_kind::big_integer))
			{
				throw std::invalid_argument(
					"Undo history contains an invalid key type");
			}
			return static_cast<search_tree_key_kind>(value);
		}

		static void validate_tree_history_snapshot(
			const tree_history_snapshot& state)
		{
			if (state.active_tree < 0 ||
				static_cast<std::size_t>(state.active_tree) >=
					tree_names.size() ||
				state.loaded_tree_index < -1 ||
				state.loaded_tree_index >=
					static_cast<int>(tree_names.size()) ||
				(state.loaded_search_tree_valid &&
					state.loaded_tree_index < 0) ||
				(state.archive_entries_present &&
					!state.archive_entry_delta))
			{
				throw std::invalid_argument(
					"Undo history contains an invalid tree state");
			}
			validate_tree_member_path(state.current_archive_member_path);
			validate_tree_member_path(state.merge_source_archive_path);
			for (const auto& path : state.split_result_member_paths)
			{
				validate_tree_member_path(path);
			}
			if (!state.current_tree_name.empty() &&
				state.current_archive_member_path.empty())
			{
				throw std::invalid_argument(
					"Undo history tree state is missing its archive member path");
			}
			if (state.archive_entry_delta)
			{
				if (state.archive_entry_delta->added_entries.size() > 2)
				{
					throw std::invalid_argument(
						"Undo history contains too many added archive entries");
				}
				for (const auto& entry :
					state.archive_entry_delta->added_entries)
				{
					validate_tree_member_path(entry.path);
					if (entry.path.empty() ||
						entry.contents.size() >
							tree_practicum::detail::max_archive_entry_size)
					{
						throw std::invalid_argument(
							"Undo history contains an invalid added archive entry");
					}
				}
			}
		}

		static void write_tree_history_snapshot(
			history_binary_writer& writer,
			const tree_history_snapshot& state)
		{
			validate_tree_history_snapshot(state);
			writer.write_string(state.current_tree_name);
			writer.write_string(state.current_tree_serialization);
			writer.write_string(state.current_archive_member_path);
			writer.write_string(path_to_utf8(state.current_tree_path));
			writer.write_string(state.merge_source_archive_path);
			for (const auto& path : state.split_result_member_paths)
			{
				writer.write_string(path);
			}
			writer.write_string(state.split_pivot_label);
			write_key_kind(writer, state.loaded_search_tree_key_kind);
			write_key_kind(writer, state.split_result_key_kind);
			writer.write_unsigned(state.document_state_id);
			writer.write_signed(state.active_tree);
			writer.write_signed(state.loaded_tree_index);
			writer.write_bool(state.loaded_search_tree_valid);
			writer.write_bool(state.archive_entries_present);
			writer.write_bool(static_cast<bool>(state.archive_entry_delta));
			if (state.archive_entry_delta)
			{
				writer.write_unsigned<std::uint32_t>(
					static_cast<std::uint32_t>(
						state.archive_entry_delta->added_entries.size()));
				for (const auto& entry :
					state.archive_entry_delta->added_entries)
				{
					writer.write_string(entry.path);
					writer.write_string(entry.contents);
				}
			}
		}

		[[nodiscard]] static tree_history_snapshot
			read_tree_history_snapshot(history_binary_reader& reader)
		{
			tree_history_snapshot state;
			state.current_tree_name = reader.read_string();
			state.current_tree_serialization = reader.read_string();
			state.current_archive_member_path = reader.read_string();
			state.current_tree_path = path_from_utf8(reader.read_string());
			state.merge_source_archive_path = reader.read_string();
			for (auto& path : state.split_result_member_paths)
			{
				path = reader.read_string();
			}
			state.split_pivot_label = reader.read_string();
			state.loaded_search_tree_key_kind = read_key_kind(reader);
			state.split_result_key_kind = read_key_kind(reader);
			state.document_state_id =
				reader.read_unsigned<std::uint64_t>();
			state.active_tree = reader.read_signed();
			state.loaded_tree_index = reader.read_signed();
			state.loaded_search_tree_valid = reader.read_bool();
			state.archive_entries_present = reader.read_bool();
			if (reader.read_bool())
			{
				const auto count =
					reader.read_unsigned<std::uint32_t>();
				if (count > 2)
				{
					throw std::invalid_argument(
						"Undo history contains too many added archive entries");
				}
				auto delta =
					std::make_shared<archive_entry_history_delta>();
				delta->added_entries.reserve(count);
				for (std::uint32_t i = 0; i < count; ++i)
				{
					delta->added_entries.push_back({
						reader.read_string(),
						reader.read_string()
					});
				}
				state.archive_entry_delta = std::move(delta);
			}
			validate_tree_history_snapshot(state);
			return state;
		}

		static void write_history_state(
			history_binary_writer& writer,
			const history_state& state)
		{
			if (const auto* interface_value =
				std::get_if<interface_state>(&state.value))
			{
				writer.write_unsigned<std::uint8_t>(0);
				write_interface_state(writer, *interface_value);
				return;
			}

			const auto& tree = std::get<tree_history_state>(state.value);
			if (!tree.transition)
			{
				throw std::logic_error(
					"Undo history contains a missing tree transition");
			}
			writer.write_unsigned<std::uint8_t>(1);
			writer.write_bool(tree.is_after);
			write_tree_history_snapshot(
				writer,
				tree.transition->before);
			write_tree_history_snapshot(
				writer,
				tree.transition->after);
		}

		[[nodiscard]] static history_state read_history_state(
			history_binary_reader& reader)
		{
			const auto kind = reader.read_unsigned<std::uint8_t>();
			if (kind == 0)
			{
				return history_state{ read_interface_state(reader) };
			}
			if (kind != 1)
			{
				throw std::invalid_argument(
					"Undo history contains an unknown state type");
			}

			tree_history_state state;
			state.is_after = reader.read_bool();
			auto transition = std::make_shared<tree_history_transition>();
			transition->before = read_tree_history_snapshot(reader);
			transition->after = read_tree_history_snapshot(reader);
			if (transition->before.document_state_id ==
				transition->after.document_state_id)
			{
				throw std::invalid_argument(
					"Undo history contains an unchanged tree transition");
			}
			state.transition = std::move(transition);
			return history_state{ std::move(state) };
		}

		[[nodiscard]] static std::uint64_t
			history_string_checksum(std::string_view value) noexcept
		{
			std::uint64_t checksum = 14695981039346656037ull;
			for (const unsigned char character : value)
			{
				checksum ^= character;
				checksum *= 1099511628211ull;
			}
			return checksum;
		}

		[[nodiscard]] static std::string serialize_history_document(
			const history_stack_type& history,
			const interface_state& current_interface,
			tree_history_snapshot current_tree,
			std::uint64_t current_document_state_id,
			std::uint64_t next_document_state_id)
		{
			if (next_document_state_id <= current_document_state_id)
			{
				throw std::logic_error(
					"Undo history state identifiers are inconsistent");
			}
			const auto current_tree_checksum =
				current_tree.current_archive_member_path.empty()
					? 0u
					: history_string_checksum(
						current_tree.current_tree_serialization);
			if (current_tree.current_archive_member_path.empty() &&
				!current_tree.current_tree_serialization.empty())
			{
				throw std::logic_error(
					"Current tree serialization has no archive member");
			}
			current_tree.current_tree_serialization.clear();
			current_tree.document_state_id = current_document_state_id;
			validate_interface_state(current_interface);
			validate_tree_history_snapshot(current_tree);

			history_binary_writer writer;
			writer.data.reserve(std::min(
				max_history_file_size,
				history.history_bytes() + 4096u));
			writer.write_bytes(history_file_magic);
			writer.write_unsigned(history_file_version);
			writer.write_unsigned(current_document_state_id);
			writer.write_unsigned(next_document_state_id);
			writer.write_unsigned(current_tree_checksum);
			write_interface_state(writer, current_interface);
			write_tree_history_snapshot(writer, current_tree);
			writer.write_unsigned<std::uint32_t>(
				static_cast<std::uint32_t>(history.undo_states().size()));
			for (const auto& state : history.undo_states())
			{
				write_history_state(writer, state);
			}
			writer.write_unsigned<std::uint32_t>(
				static_cast<std::uint32_t>(history.redo_states().size()));
			for (const auto& state : history.redo_states())
			{
				write_history_state(writer, state);
			}
			return std::move(writer.data);
		}

		[[nodiscard]] static history_document deserialize_history_document(
			std::string_view contents)
		{
			history_binary_reader reader(contents);
			if (reader.read_bytes(history_file_magic.size()) !=
				history_file_magic)
			{
				throw std::invalid_argument(
					"Undo history file has an invalid signature");
			}
			const auto version = reader.read_unsigned<std::uint32_t>();
			if (version != history_file_version)
			{
				throw std::invalid_argument(
					"Undo history file version is not supported");
			}

			history_document document;
			document.current_document_state_id =
				reader.read_unsigned<std::uint64_t>();
			document.next_document_state_id =
				reader.read_unsigned<std::uint64_t>();
			document.current_tree_checksum =
				reader.read_unsigned<std::uint64_t>();
			document.current_interface =
				read_interface_state(reader);
			document.current_tree =
				read_tree_history_snapshot(reader);
			if (!document.current_tree.current_tree_serialization.empty() ||
				document.current_tree.document_state_id !=
					document.current_document_state_id)
			{
				throw std::invalid_argument(
					"Undo history current state is inconsistent");
			}

			const auto undo_count =
				reader.read_unsigned<std::uint32_t>();
			if (undo_count > undo_history_limit)
			{
				throw std::length_error(
					"Undo history contains too many states");
			}
			document.undo_states.reserve(undo_count);
			for (std::uint32_t i = 0; i < undo_count; ++i)
			{
				document.undo_states.push_back(read_history_state(reader));
			}

			const auto redo_count =
				reader.read_unsigned<std::uint32_t>();
			if (redo_count > undo_history_limit - undo_count)
			{
				throw std::length_error(
					"Undo history contains too many states");
			}
			document.redo_states.reserve(redo_count);
			for (std::uint32_t i = 0; i < redo_count; ++i)
			{
				document.redo_states.push_back(read_history_state(reader));
			}
			if (!reader.finished())
			{
				throw std::invalid_argument(
					"Undo history file contains trailing data");
			}
			if (document.next_document_state_id <=
				document.current_document_state_id)
			{
				throw std::invalid_argument(
					"Undo history state identifiers are inconsistent");
			}

			std::uint64_t maximum_state_id =
				document.current_document_state_id;
			const auto inspect_state_id =
				[&maximum_state_id](const history_state& state)
			{
				if (const auto* tree =
					std::get_if<tree_history_state>(&state.value))
				{
					if (!tree->transition)
					{
						throw std::invalid_argument(
							"Undo history contains a missing tree transition");
					}
					maximum_state_id = std::max(
						maximum_state_id,
						std::max(
							tree->transition->before.document_state_id,
							tree->transition->after.document_state_id));
				}
			};
			for (const auto& state : document.undo_states)
			{
				inspect_state_id(state);
			}
			for (const auto& state : document.redo_states)
			{
				inspect_state_id(state);
			}
			if (document.next_document_state_id <= maximum_state_id)
			{
				throw std::invalid_argument(
					"Undo history next state identifier is inconsistent");
			}

			document.history.replace_states(
				std::move(document.undo_states),
				std::move(document.redo_states));
			return document;
		}

		void register_tree_animation_queue()
		{
			[[maybe_unused]] static auto tree_animation_subscription =
				mngl::event_manager::on<tree_animation_event>(
					[](const tree_animation_event& event)
					{
						std::shared_ptr<tree_animation_event_queue> queue;
						{
							std::lock_guard lock(
								tree_animation_queues_mutex);
							const auto found =
								tree_animation_event_queues.find(
									event.source_id);
							if (found == tree_animation_event_queues.end())
							{
								return;
							}
							queue = found->second.lock();
							if (!queue)
							{
								tree_animation_event_queues.erase(found);
								return;
							}
						}

						std::lock_guard lock(queue->mutex);
						queue->events.push_back(event.step);
					});

			std::lock_guard lock(tree_animation_queues_mutex);
			tree_animation_event_queues[m_tree_animation_source_id] =
				m_tree_animation_event_queue;
		}

		void clear_pending_tree_animation_events()
		{
			std::lock_guard lock(m_tree_animation_event_queue->mutex);
			m_tree_animation_event_queue->events.clear();
		}

		void cancel_tree_animation()
		{
			m_tree_animation.reset();
			clear_pending_tree_animation_events();
		}

		[[nodiscard]] std::vector<tree_visual_event>
			take_pending_tree_animation_events()
		{
			std::lock_guard lock(m_tree_animation_event_queue->mutex);
			std::vector<tree_visual_event> events;
			events.swap(m_tree_animation_event_queue->events);
			return events;
		}

		void publish_tree_animation_events(
			const std::vector<tree_visual_event>& events)
		{
			for (const auto& step : events)
			{
				mngl::event_manager::emit(tree_animation_event{
					m_tree_animation_source_id,
					step
				});
			}
		}

		static void apply_tree_visual_event(
			visual_tree& tree,
			const tree_visual_event& step)
		{
			if (step.action == tree_visual_event::kind::insert_node)
			{
				if (step.node_index != tree.nodes.size())
				{
					throw std::logic_error(
						"Индекс нового узла в анимации Splay не совпадает с размером дерева.");
				}
				if (step.parent_index == no_visual_node)
				{
					if (!tree.nodes.empty() ||
						step.node_index != 0 ||
						step.inserted_node.binary_children[0] != no_visual_node ||
						step.inserted_node.binary_children[1] != no_visual_node)
					{
						throw std::logic_error(
							"Некорректный шаг вставки корня в анимации Splay.");
					}
					tree.nodes.push_back(step.inserted_node);
					tree.root = step.node_index;
					return;
				}

				if (step.parent_index >= tree.nodes.size() ||
					step.side >= 2 ||
					tree.nodes[step.parent_index].binary_children[step.side] !=
						no_visual_node ||
					step.inserted_node.binary_children[0] != no_visual_node ||
					step.inserted_node.binary_children[1] != no_visual_node)
				{
					throw std::logic_error(
						"Некорректный шаг вставки узла в анимации Splay.");
				}
				tree.nodes.push_back(step.inserted_node);
				set_binary_child(
					tree,
					step.parent_index,
					step.side,
					step.node_index);
				return;
			}

			if (step.node_index >= tree.nodes.size() ||
				step.parent_index >= tree.nodes.size() ||
				(step.grandparent_index != no_visual_node &&
					step.grandparent_index >= tree.nodes.size()))
			{
				throw std::logic_error(
					"Splay-анимация содержит ссылку на отсутствующий узел.");
			}
			rotate_splay_node_up(
				tree,
				step.parent_index,
				step.node_index,
				step.grandparent_index);
		}

		void advance_tree_animation(int tree_index, float delta_time)
		{
			if (!m_tree_animation.has_value())
			{
				return;
			}
			if (m_tree_animation->tree_index != tree_index)
			{
				cancel_tree_animation();
				return;
			}
			if (!std::isfinite(delta_time) || delta_time < 0.0f)
			{
				delta_time = 0.0f;
			}

			auto& animation = m_tree_animation.value();
			animation.elapsed_seconds += delta_time;
			if (animation.mode ==
				tree_animation_mode::layout_transition)
			{
				if (animation.elapsed_seconds >=
					tree_layout_animation_seconds)
				{
					m_tree_animation.reset();
				}
				return;
			}
			if (animation.next_event >= animation.events.size())
			{
				if (animation.elapsed_seconds >=
					tree_animation_step_seconds)
				{
					m_tree_animation.reset();
				}
				return;
			}
			if (animation.elapsed_seconds < tree_animation_step_seconds)
			{
				return;
			}

			animation.elapsed_seconds -= tree_animation_step_seconds;
			const auto& step = animation.events[animation.next_event++];
			try
			{
				apply_tree_visual_event(animation.tree, step);
				animation.active_event = step;
			}
			catch (const std::exception& error)
			{
				m_tree_operation_error = std::format(
					"Анимация Splay остановлена: {}",
					error.what());
				logger.error(
					"Unable to process a Splay animation event: ",
					error.what());
				cancel_tree_animation();
			}
		}

		void restore_interface_state(const interface_state& state)
		{
			m_selected_workspace = state.selected_workspace;
			m_new_tree_type = state.active_tree;
			clear_tree_find_result();
			m_key_input = state.key_input;
			m_random_count = state.random_count;
			m_key_input_edit_start.reset();
			m_random_count_edit_start.reset();
		}

		void record_interface_change(interface_state previous)
		{
			m_history.record(
				history_state{ std::move(previous) },
				history_state{ current_interface_state() });
		}

		[[nodiscard]] tree_history_snapshot
			capture_tree_history_state() const
		{
			tree_history_snapshot state;
			state.current_tree_name = m_current_tree_name;
			state.current_tree_serialization = m_current_tree_serialization;
			state.current_archive_member_path =
				m_current_archive_member_path;
			state.current_tree_path = m_current_tree_path;
			state.merge_source_archive_path = m_merge_source_archive_path;
			state.split_result_member_paths = m_split_result_member_paths;
			state.split_pivot_label = m_split_pivot_label;
			state.loaded_search_tree_key_kind =
				m_loaded_search_tree_key_kind;
			state.split_result_key_kind = m_split_result_key_kind;
			state.document_state_id = m_current_document_state_id;
			state.active_tree = m_active_tree;
			state.loaded_tree_index = m_loaded_tree_index;
			state.loaded_search_tree_valid =
				m_loaded_search_tree_valid;
			return state;
		}

		[[nodiscard]] bool archive_history_entries_present(
			const archive_entry_history_delta& delta) const
		{
			return std::all_of(
				delta.added_entries.begin(),
				delta.added_entries.end(),
				[this](const auto& expected)
				{
					const auto expected_path =
						lowercase_ascii(expected.path);
					return std::any_of(
						m_session_entries.begin(),
						m_session_entries.end(),
						[&](const auto& entry)
						{
							return lowercase_ascii(entry.path) ==
								expected_path;
						});
				});
		}

		[[nodiscard]] history_state capture_history_counterpart(
			const history_state& target) const
		{
			return std::visit(
				[this](const auto& expected) -> history_state
				{
					using state_type =
						std::remove_cvref_t<decltype(expected)>;
					if constexpr (std::is_same_v<
						state_type,
						interface_state>)
					{
						return history_state{
							current_interface_state()
						};
					}
					else
					{
						if (!expected.transition)
						{
							throw std::logic_error(
								"В истории отсутствует снимок операции с деревом.");
						}
						tree_history_state counterpart;
						counterpart.transition = expected.transition;
						counterpart.is_after = !expected.is_after;
						return history_state{
							std::move(counterpart)
						};
					}
				},
				target.value);
		}

		void restore_tree_history_state(
			const tree_history_snapshot& state)
		{
			sync_comparison_pane_with_active_tree(true);
			cancel_tree_animation();
			clear_tree_find_result();
			if (state.archive_entry_delta)
			{
				for (const auto& entry :
					state.archive_entry_delta->added_entries)
				{
					if (state.archive_entries_present)
					{
						set_archive_entry(entry.path, entry.contents);
					}
					else
					{
						const auto path = lowercase_ascii(entry.path);
						m_session_entries.erase(
							std::remove_if(
								m_session_entries.begin(),
								m_session_entries.end(),
								[&](const auto& current)
								{
									return lowercase_ascii(current.path) ==
										path;
								}),
							m_session_entries.end());
					}
				}
			}
			std::erase_if(
				m_open_tree_tabs,
				[this](const auto& path)
				{
					return find_archive_entry(
						m_session_entries,
						path) == nullptr;
				});
			std::erase_if(
				m_tree_documents,
				[this](const auto& item)
				{
					return find_archive_entry(
						m_session_entries,
						item.first) == nullptr;
				});
			if (!state.current_archive_member_path.empty())
			{
				set_archive_entry(
					state.current_archive_member_path,
					state.current_tree_serialization);
			}
			if (state.current_archive_member_path.empty())
			{
				clear_current_tree_state(false);
			}
			else
			{
				create_window_tree(
					state.current_tree_name,
					state.current_tree_serialization);
			}
			m_current_tree_path = state.current_tree_path;
			m_current_archive_member_path =
				state.current_archive_member_path;
			m_merge_source_archive_path =
				state.merge_source_archive_path;
			m_split_result_member_paths =
				state.split_result_member_paths;
			m_split_result_key_kind = state.split_result_key_kind;
			m_split_pivot_label = state.split_pivot_label;
			m_active_tree = state.active_tree;
			m_loaded_tree_index = state.loaded_tree_index;
			m_loaded_search_tree_key_kind =
				state.loaded_search_tree_key_kind;
			m_loaded_search_tree_valid =
				state.loaded_search_tree_valid;
			m_current_document_state_id =
				state.document_state_id;
			if (!m_current_archive_member_path.empty())
			{
				register_tree_document(
					m_current_archive_member_path,
					m_current_tree_path);
				auto& document = m_tree_documents.at(
					m_current_archive_member_path);
				document.current_state_id =
					m_current_document_state_id;
				m_saved_document_state_id =
					document.saved_state_id;
			}
			else
			{
				m_saved_document_state_id = 0;
			}
			m_tree_dirty = m_current_document_state_id !=
				m_saved_document_state_id;
			m_selected_archive_entry = -1;
			for (std::size_t i = 0; i < m_session_entries.size(); ++i)
			{
				if (m_session_entries[i].path ==
					m_current_archive_member_path)
				{
					if (i <= static_cast<std::size_t>(
						std::numeric_limits<int>::max()))
					{
						m_selected_archive_entry =
							static_cast<int>(i);
					}
					break;
				}
			}
			update_session_metadata();
		}

		void restore_history_state(const history_state& state)
		{
			std::visit(
				[this](const auto& previous)
				{
					using state_type =
						std::remove_cvref_t<decltype(previous)>;
					if constexpr (std::is_same_v<
						state_type,
						interface_state>)
					{
						restore_interface_state(previous);
					}
					else
					{
						if (!previous.transition)
						{
							throw std::logic_error(
								"В истории отсутствует снимок операции с деревом.");
						}
						restore_tree_history_state(
							previous.is_after
								? previous.transition->after
								: previous.transition->before);
					}
				},
				state.value);
		}

		void reset_history()
		{
			m_history.clear();
			m_history_warning.clear();
			m_current_document_state_id = 0;
			m_saved_document_state_id = 0;
			m_next_document_state_id = 1;
		}

		void record_tree_history_change(
			tree_history_snapshot previous,
			std::shared_ptr<const archive_entry_history_delta>
				archive_entry_delta = {})
		{
			invalidate_tree_preview_cache();
			previous.document_state_id = m_current_document_state_id;
			previous.archive_entry_delta = archive_entry_delta;
			previous.archive_entries_present = false;
			m_current_document_state_id = m_next_document_state_id++;
			m_tree_dirty = m_current_document_state_id !=
				m_saved_document_state_id;
			remember_active_tree_document();

			auto current = capture_tree_history_state();
			current.document_state_id = m_current_document_state_id;
			current.archive_entry_delta = std::move(archive_entry_delta);
			current.archive_entries_present =
				current.archive_entry_delta &&
				archive_history_entries_present(
					*current.archive_entry_delta);
			if (current.archive_entry_delta &&
				!current.archive_entries_present)
			{
				throw std::logic_error(
					"Не удалось сохранить новые записи архива в истории отмены.");
			}

			auto transition =
				std::make_shared<tree_history_transition>();
			transition->before = std::move(previous);
			transition->after = std::move(current);
			const std::shared_ptr<const tree_history_transition>
				shared_transition = std::move(transition);
			tree_history_state undo_state;
			undo_state.transition = shared_transition;
			tree_history_state redo_state;
			redo_state.transition = shared_transition;
			redo_state.is_after = true;
			const auto result = m_history.record(
				history_state{ std::move(undo_state) },
				history_state{ std::move(redo_state) });
			if (result ==
				history_stack_type::record_result::state_too_large)
			{
				m_history_warning =
					"Последнее изменение больше лимита истории 64 МиБ; "
					"история отмены очищена, чтобы не применить неверное состояние.";
				logger.warning(
					"Undo history was cleared because the latest tree state "
					"exceeded the 64 MiB memory limit.");
			}
			else if (result ==
				history_stack_type::record_result::recorded)
			{
				m_history_warning.clear();
			}
		}

		void commit_pending_input_edits()
		{
			if (m_key_input_edit_start)
			{
				record_interface_change(*m_key_input_edit_start);
				m_key_input_edit_start.reset();
			}
			if (m_random_count_edit_start)
			{
				record_interface_change(*m_random_count_edit_start);
				m_random_count_edit_start.reset();
			}
		}

		void track_input_edit(
			std::optional<interface_state>& edit_start,
			const interface_state& state_before_widget,
			bool activated,
			bool deactivated_after_edit,
			bool deactivated)
		{
			if (activated)
			{
				edit_start = state_before_widget;
			}
			if (deactivated_after_edit)
			{
				record_interface_change(
					edit_start.value_or(state_before_widget));
			}
			if (deactivated)
			{
				edit_start.reset();
			}
		}

		void select_tree(int tree_index)
		{
			if (tree_index == m_new_tree_type)
			{
				return;
			}
			commit_pending_input_edits();
			const auto previous = current_interface_state();
			m_new_tree_type = tree_index;
			record_interface_change(previous);
		}

		void clear_tree_find_result() noexcept
		{
			m_tree_find_path.clear();
			m_tree_find_status.clear();
			m_tree_find_tree_index = -1;
			m_tree_find_result_valid = false;
			m_tree_find_found = false;
		}

		bool undo_history_state()
		{
			if (ImGui::GetIO().WantTextInput ||
				m_key_input_edit_start ||
				m_random_count_edit_start)
			{
				return false;
			}

			const auto* target = m_history.undo_target();
			if (!target)
			{
				return false;
			}
			auto previous = m_history.undo(
				capture_history_counterpart(*target));
			if (!previous)
			{
				m_history_warning =
					"Не удалось безопасно сохранить состояние для повтора.";
				return false;
			}
			const bool tree_change = std::holds_alternative<
				tree_history_state>(previous->value);
			restore_history_state(*previous);
			m_last_action = tree_change
				? "Отменена последняя операция над деревом."
				: "Отменено последнее изменение интерфейса.";
			logger.info(
				tree_change
					? "Undo: restored the previous tree state."
					: "Undo: restored the previous interface state.");
			return true;
		}

		bool redo_history_state()
		{
			if (ImGui::GetIO().WantTextInput ||
				m_key_input_edit_start ||
				m_random_count_edit_start)
			{
				return false;
			}

			const auto* target = m_history.redo_target();
			if (!target)
			{
				return false;
			}
			auto next = m_history.redo(
				capture_history_counterpart(*target));
			if (!next)
			{
				m_history_warning =
					"Не удалось безопасно сохранить состояние для отмены.";
				return false;
			}
			const bool tree_change = std::holds_alternative<
				tree_history_state>(next->value);
			restore_history_state(*next);
			m_last_action = tree_change
				? "Повторена последняя операция над деревом."
				: "Повторено последнее изменение интерфейса.";
			logger.info(
				tree_change
					? "Redo: restored the next tree state."
					: "Redo: restored the next interface state.");
			return true;
		}

		void process_keyboard_shortcuts()
		{
			const ImGuiIO& io = ImGui::GetIO();
			const bool modal_dialog_open =
				m_file_dialog_open || m_unsaved_changes_dialog_open;
			const bool text_input_active = io.WantTextInput ||
				m_key_input_edit_start ||
				m_random_count_edit_start;
			const bool operation_shortcuts_enabled =
				!text_input_active &&
				!ImGui::IsAnyItemActive() &&
				!modal_dialog_open &&
				!m_active_tree_tool.has_value() &&
				!io.KeyCtrl &&
				!io.KeyShift &&
				!io.KeyAlt &&
				!io.KeySuper;
			if (operation_shortcuts_enabled)
			{
				if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) &&
					m_hovered_tree_key)
				{
					auto key =
						std::move(m_hovered_tree_key);
					auto comparison_pane =
						std::move(m_hovered_tree_comparison_pane);
					m_hovered_tree_key.reset();
					m_hovered_tree_comparison_pane.reset();
					invoke_tree_tool(
						tree_tool_operation::erase,
						std::move(key),
						std::move(comparison_pane));
					return;
				}
				if (ImGui::IsKeyPressed(ImGuiKey_F, false))
				{
					invoke_tree_tool(tree_tool_operation::find);
					return;
				}
				if (ImGui::IsKeyPressed(ImGuiKey_R, false))
				{
					invoke_tree_tool(
						tree_tool_operation::random_insert);
					return;
				}
				if (ImGui::IsKeyPressed(ImGuiKey_A, false) ||
					ImGui::IsKeyPressed(ImGuiKey_Insert, false))
				{
					invoke_tree_tool(tree_tool_operation::insert);
					return;
				}
				if (ImGui::IsKeyPressed(ImGuiKey_D, false) ||
					ImGui::IsKeyPressed(ImGuiKey_Delete, false))
				{
					auto key = std::move(m_hovered_tree_key);
					auto comparison_pane =
						std::move(m_hovered_tree_comparison_pane);
					m_hovered_tree_key.reset();
					m_hovered_tree_comparison_pane.reset();
					invoke_tree_tool(
						tree_tool_operation::erase,
						std::move(key),
						std::move(comparison_pane));
					return;
				}
				if (ImGui::IsKeyPressed(ImGuiKey_M, false))
				{
					invoke_tree_tool(tree_tool_operation::merge);
					return;
				}
				if (ImGui::IsKeyPressed(ImGuiKey_C, false))
				{
					auto key = std::move(m_hovered_tree_key);
					auto comparison_pane =
						std::move(m_hovered_tree_comparison_pane);
					m_hovered_tree_key.reset();
					m_hovered_tree_comparison_pane.reset();
					invoke_tree_tool(
						tree_tool_operation::split,
						std::move(key),
						std::move(comparison_pane));
					return;
				}
			}

			if (text_input_active ||
				modal_dialog_open ||
				!io.KeyCtrl ||
				!ImGui::IsKeyPressed(ImGuiKey_Z))
			{
				return;
			}

			if (io.KeyShift)
			{
				(void)redo_history_state();
			}
			else
			{
				(void)undo_history_state();
			}
		}

		void draw_menu(mngl::imgui_context::frame_scope& frame)
		{
			const ImGuiIO& io = ImGui::GetIO();
			const bool modal_dialog_open =
				m_file_dialog_open || m_unsaved_changes_dialog_open;
			if (!modal_dialog_open)
			{
				if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P))
				{
					m_show_functions = true;
					m_focus_function_search = true;
				}
				if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
				{
					request_file_dialog(file_dialog_mode::open);
				}
				if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
				{
					(void)save_current();
				}
			}

			const auto menu_bar = frame.main_menu_bar();
			if (!menu_bar)
			{
				return;
			}

			if (ImGui::BeginMenu("Файл"))
			{
				if (ImGui::MenuItem("Новая рабочая область"))
				{
					begin_new_workspace();
				}
				if (ImGui::MenuItem("Новое дерево"))
				{
					(void)perform_io([&]
					{
						create_new_tree();
					});
				}
				if (ImGui::MenuItem("Открыть..."))
				{
					request_file_dialog(file_dialog_mode::open);
				}
				if (ImGui::MenuItem("Сохранить"))
				{
					(void)save_current();
				}
				if (ImGui::MenuItem("Сохранить дерево как .tree..."))
				{
					if (m_current_tree_name.empty())
					{
						m_last_action = "Сначала откройте сериализованное дерево.";
					}
					else
					{
						const auto default_path = m_current_tree_path.empty()
							? m_explorer_directory /
								path_from_utf8(m_current_tree_name + ".tree")
							: m_current_tree_path;
						request_file_dialog(
							file_dialog_mode::save_tree,
							default_path);
					}
				}
				if (ImGui::MenuItem("Сохранить рабочую область как .stree..."))
				{
					request_file_dialog(
						file_dialog_mode::save_session,
						m_explorer_directory / "workspace.stree");
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Правка"))
			{
				if (ImGui::MenuItem(
					"Отменить",
					"Ctrl+Z",
					false,
					m_history.can_undo() &&
						!io.WantTextInput &&
						!m_key_input_edit_start &&
						!m_random_count_edit_start))
				{
					(void)undo_history_state();
				}
				if (ImGui::MenuItem(
					"Повторить",
					"Ctrl+Shift+Z",
					false,
					m_history.can_redo() &&
						!io.WantTextInput &&
						!m_key_input_edit_start &&
						!m_random_count_edit_start))
				{
					(void)redo_history_state();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Настройки"))
			{
				ImGui::MenuItem(
					"Сразу выполнять Erase/Split по наведённому ключу",
					nullptr,
					&m_execute_hovered_key_tools_immediately);
				ImGui::TextDisabled(
					m_execute_hovered_key_tools_immediately
						? "Выключите для подтверждения в окне параметров."
						: "Включите для немедленного выполнения.");
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Вид"))
			{
				ImGui::MenuItem("Проводник", nullptr, &m_show_explorer);
				ImGui::MenuItem("Окно дерева", nullptr, &m_show_workspace);
				ImGui::MenuItem("Панель операций", nullptr, &m_show_tools);
				ImGui::MenuItem("Поиск функций", nullptr, &m_show_functions);
				ImGui::MenuItem("Информация", nullptr, &m_show_information);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Тип нового дерева"))
			{
				for (std::size_t i = 0; i < tree_names.size(); ++i)
				{
					if (ImGui::MenuItem(
						tree_names[i].data(),
						nullptr,
						m_new_tree_type == static_cast<int>(i)))
					{
						select_tree(static_cast<int>(i));
					}
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Инструменты"))
			{
				const bool can_operate =
					can_insert_into_selected_search_tree();
				if (ImGui::MenuItem(
					"Найти ключ",
					"F",
					false,
					can_operate))
				{
					invoke_tree_tool(tree_tool_operation::find);
				}
				if (ImGui::MenuItem(
					"Вставить ключ",
					"A / Ins",
					false,
					can_operate))
				{
					invoke_tree_tool(tree_tool_operation::insert);
				}
				if (ImGui::MenuItem(
					"Случайная вставка",
					"R",
					false,
					can_operate))
				{
					invoke_tree_tool(
						tree_tool_operation::random_insert);
				}
				if (ImGui::MenuItem(
					"Удалить ключ",
					"D / Del",
					false,
					can_operate))
				{
					invoke_tree_tool(tree_tool_operation::erase);
				}
				if (ImGui::MenuItem(
					"Разделить дерево",
					"C",
					false,
					can_operate))
				{
					invoke_tree_tool(tree_tool_operation::split);
				}
				if (ImGui::MenuItem(
					"Объединить с источником",
					"M",
					false,
					can_operate))
				{
					invoke_tree_tool(tree_tool_operation::merge);
				}
				if (!can_operate)
				{
					ImGui::TextDisabled(
						"Для операций выберите корректное дерево с ключами int64_t или double.");
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Справка"))
			{
				ImGui::MenuItem("Прототип интерфейса", nullptr, false, false);
				ImGui::MenuItem(
					"Find, insert, random insert, erase, merge и split доступны для шести типов деревьев",
					nullptr,
					false,
					false);
				ImGui::EndMenu();
			}
		}

		static void set_initial_window_rect(ImVec2 position, ImVec2 size)
		{
			ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
		}

		static bool contains_text(
			std::string_view value,
			std::string_view query) noexcept
		{
			if (query.empty())
			{
				return true;
			}
			if (query.size() > value.size())
			{
				return false;
			}

			const auto next_codepoint = [](std::string_view text, std::size_t& offset)
			{
				const std::size_t start = offset;
				const auto first = static_cast<unsigned char>(text[offset++]);
				if (first < 0x80u)
				{
					return static_cast<std::uint32_t>(first);
				}

				std::uint32_t codepoint = 0;
				std::size_t continuation_count = 0;
				if ((first & 0xE0u) == 0xC0u)
				{
					codepoint = first & 0x1Fu;
					continuation_count = 1;
				}
				else if ((first & 0xF0u) == 0xE0u)
				{
					codepoint = first & 0x0Fu;
					continuation_count = 2;
				}
				else if ((first & 0xF8u) == 0xF0u)
				{
					codepoint = first & 0x07u;
					continuation_count = 3;
				}
				else
				{
					return 0xFFFDu;
				}

				if (offset + continuation_count > text.size())
				{
					offset = start + 1;
					return 0xFFFDu;
				}
				for (std::size_t i = 0; i < continuation_count; ++i)
				{
					const auto continuation =
						static_cast<unsigned char>(text[offset++]);
					if ((continuation & 0xC0u) != 0x80u)
					{
						offset = start + 1;
						return 0xFFFDu;
					}
					codepoint = (codepoint << 6) | (continuation & 0x3Fu);
				}
				return codepoint;
			};

			const auto fold_codepoint = [](std::uint32_t codepoint)
			{
				if (codepoint >= 'A' && codepoint <= 'Z')
				{
					return codepoint + ('a' - 'A');
				}
				if (codepoint == 0x0401u)
				{
					return 0x0451u;
				}
				if (codepoint >= 0x0410u && codepoint <= 0x042Fu)
				{
					return codepoint + 0x20u;
				}
				if (codepoint >= 0x0400u && codepoint <= 0x040Fu)
				{
					return codepoint + 0x50u;
				}
				return codepoint;
			};

			for (std::size_t start = 0; start < value.size();)
			{
				std::size_t value_offset = start;
				std::size_t query_offset = 0;
				bool matches = true;
				while (query_offset < query.size())
				{
					if (value_offset >= value.size() ||
						fold_codepoint(next_codepoint(value, value_offset)) !=
						fold_codepoint(next_codepoint(query, query_offset)))
					{
						matches = false;
						break;
					}
				}
				if (matches)
				{
					return true;
				}
				(void)next_codepoint(value, start);
			}
			return false;
		}

		static bool equals_ascii_case_insensitive(
			std::string_view left,
			std::string_view right) noexcept
		{
			if (left.size() != right.size())
			{
				return false;
			}
			for (std::size_t i = 0; i < left.size(); ++i)
			{
				const auto fold = [](char value)
				{
					return value >= 'A' && value <= 'Z'
						? static_cast<char>(value + ('a' - 'A'))
						: value;
				};
				if (fold(left[i]) != fold(right[i]))
				{
					return false;
				}
			}
			return true;
		}

		static bool alias_matches_query(
			std::string_view alias,
			std::string_view query) noexcept
		{
			return equals_ascii_case_insensitive(alias, query) ||
				(query.size() >= 3 && contains_text(alias, query));
		}

		static bool function_has_alias(
			std::string_view function_name,
			std::string_view query) noexcept
		{
			if (query.empty())
			{
				return false;
			}
			if (function_name == "insert")
			{
				return alias_matches_query("ins", query) ||
					alias_matches_query("a", query) ||
					alias_matches_query("add", query) ||
					alias_matches_query("put", query);
			}
			if (function_name == "erase")
			{
				return alias_matches_query("d", query) ||
					alias_matches_query("del", query) ||
					alias_matches_query("delete", query);
			}
			if (function_name == "merge")
			{
				return alias_matches_query("m", query) ||
					(!equals_ascii_case_insensitive(query, "mass insert") &&
						alias_matches_query("fast mass insert", query));
			}
			if (function_name == "insert_random")
			{
				return alias_matches_query("r", query) ||
					alias_matches_query("mass insert", query) ||
					alias_matches_query("random test", query) ||
					alias_matches_query("mass random insert", query);
			}
			if (function_name == "split")
			{
				return alias_matches_query("c", query) ||
					alias_matches_query("cut", query);
			}
			return false;
		}

		static bool function_matches_query(
			const function_entry& function,
			std::string_view query) noexcept
		{
			if (function_has_alias(function.name, query))
			{
				return true;
			}

			return contains_text(function.name, query) ||
				contains_text(function.signature, query) ||
				contains_text(tree_names[function.tree_index], query) ||
				contains_text(function.description, query);
		}

		static std::string path_to_utf8(const std::filesystem::path& path)
		{
			const auto encoded = path.u8string();
			return {
				reinterpret_cast<const char*>(encoded.data()),
				encoded.size()
			};
		}

		[[nodiscard]] static std::filesystem::path path_from_utf8(
			std::string_view value)
		{
			return std::filesystem::path(std::u8string(
				reinterpret_cast<const char8_t*>(value.data()),
				value.size()));
		}

		template<std::size_t Size>
		static void set_path_buffer(
			std::array<char, Size>& buffer,
			std::string_view value)
		{
			buffer.fill('\0');
			auto count = std::min(value.size(), buffer.size() - 1);
			if (count < value.size())
			{
				while (count > 0 &&
					(static_cast<unsigned char>(value[count]) & 0xC0u) == 0x80u)
				{
					--count;
				}
			}
			std::copy_n(value.begin(), count, buffer.begin());
		}

		[[nodiscard]] static std::string lowercase_ascii(std::string value)
		{
			std::transform(
				value.begin(),
				value.end(),
				value.begin(),
				[](unsigned char character)
				{
					return character >= 'A' && character <= 'Z'
						? static_cast<char>(character + ('a' - 'A'))
						: static_cast<char>(character);
				});
			return value;
		}

		[[nodiscard]] static std::string file_extension(
			const std::filesystem::path& path)
		{
			return lowercase_ascii(path_to_utf8(path.extension()));
		}

		[[nodiscard]] static std::string archive_extension(
			std::string_view path)
		{
			const auto separator = path.find_last_of('/');
			const auto dot = path.find_last_of('.');
			if (dot == std::string_view::npos ||
				(separator != std::string_view::npos && dot < separator))
			{
				return {};
			}
			return lowercase_ascii(std::string(path.substr(dot)));
		}

		[[nodiscard]] static std::string safe_archive_component(
			std::string_view value)
		{
			std::string result;
			result.reserve(value.size());
			for (const unsigned char character : value)
			{
				result.push_back(
					character == '/' || character == '\\' || character == ':' ||
						character < 0x20u || character == 0x7Fu
						? '_'
						: static_cast<char>(character));
			}
			while (!result.empty() &&
				(result.back() == '.' || result.back() == ' '))
			{
				result.pop_back();
			}
			if (result.empty() || result == "." || result == "..")
			{
				return "tree";
			}
			if (result.size() > 87)
			{
				std::size_t length = 87;
				while (length > 0 &&
					(static_cast<unsigned char>(result[length]) & 0xC0u) == 0x80u)
				{
					--length;
				}
				result.resize(length);
			}
			return result;
		}

		template<typename Action>
		bool perform_io(Action&& action)
		{
			try
			{
				std::forward<Action>(action)();
				m_file_error.clear();
				return true;
			}
			catch (const std::exception& error)
			{
				m_file_error = error.what();
				m_last_action = std::format("Ошибка файловой операции: {}", error.what());
				logger.error("File operation failed: ", error.what());
				return false;
			}
		}

		static void register_file_drop_listener()
		{
			[[maybe_unused]] static auto file_drop_subscription =
				mngl::event_manager::on<mngl::dropFile>(
					[](const mngl::dropFile& event)
					{
						enqueue_tree_file_path(event.data);
					});
		}

		static void enqueue_tree_file_path(std::string_view path)
		{
			if (path.empty())
			{
				return;
			}
			try
			{
				std::lock_guard lock(pending_file_drops_mutex);
				pending_file_drops.emplace_back(path);
			}
			catch (const std::bad_alloc&)
			{
				SDL_LogError(
					SDL_LOG_CATEGORY_APPLICATION,
					"Unable to queue a selected or dropped tree file.");
			}
		}

		static void enqueue_file_dialog_error(std::string_view error)
		{
			try
			{
				std::lock_guard lock(pending_file_drops_mutex);
				pending_file_dialog_error.assign(error);
			}
			catch (const std::bad_alloc&)
			{
				SDL_LogError(
					SDL_LOG_CATEGORY_APPLICATION,
					"Unable to queue the native file dialog error.");
			}
		}

		static void SDLCALL tree_file_dialog_callback(
			void* userdata,
			const char* const* filelist,
			int filter)
		{
			(void)userdata;
			(void)filter;
			if (!filelist)
			{
				const char* error = SDL_GetError();
				enqueue_file_dialog_error(
					error && *error
						? std::string_view(error)
						: std::string_view("Не удалось открыть диалог выбора файла."));
				return;
			}
			for (std::size_t i = 0; filelist[i]; ++i)
			{
				enqueue_tree_file_path(filelist[i]);
			}
		}

		static pending_tree_file_requests take_pending_tree_file_requests()
		{
			pending_tree_file_requests result;
			std::lock_guard lock(pending_file_drops_mutex);
			result.paths.swap(pending_file_drops);
			result.dialog_error.swap(pending_file_dialog_error);
			return result;
		}

		void process_pending_tree_files()
		{
			auto requests = take_pending_tree_file_requests();
			if (requests.paths.empty())
			{
				if (!requests.dialog_error.empty())
				{
					m_file_error = std::move(requests.dialog_error);
					m_last_action = std::format(
						"Ошибка диалога выбора файла: {}",
						m_file_error);
				}
				return;
			}
			if (requests.paths.size() != 1)
			{
				m_file_error =
					"За один раз можно загрузить только один файл .tree.";
				m_last_action = m_file_error;
				return;
			}

			(void)perform_io([&]
			{
				const auto path = path_from_utf8(requests.paths.front());
				if (file_extension(path) != ".tree")
				{
					throw std::invalid_argument(
						"Для загрузки выберите или перетащите файл с расширением .tree.");
				}
				open_file(path);
			});
		}

		void request_tree_file_picker()
		{
			if (!m_native_window)
			{
				request_file_dialog(file_dialog_mode::load_tree);
				return;
			}

			m_tree_dialog_default_location =
				path_to_utf8(m_explorer_directory);
			m_last_action = "Ожидание выбора файла .tree...";
			SDL_ShowOpenFileDialog(
				tree_file_dialog_callback,
				nullptr,
				m_native_window,
				&tree_file_dialog_filter,
				1,
				m_tree_dialog_default_location.c_str(),
				false);
		}

		void request_file_dialog(
			file_dialog_mode mode,
			const std::filesystem::path& default_path = {})
		{
			m_file_dialog_mode = mode;
			const auto initial_path = default_path.empty()
				? m_explorer_directory
				: default_path;
			set_path_buffer(m_file_dialog_path, path_to_utf8(initial_path));
			m_file_dialog_error.clear();
			m_file_dialog_open = true;
			m_file_dialog_requested = true;
		}

		void request_local_file_rename(
			const std::filesystem::path& path)
		{
			m_file_dialog_source_path = path;
			request_file_dialog(file_dialog_mode::rename_local_file, path);
		}

		void request_local_file_delete(
			const std::filesystem::path& path)
		{
			m_pending_file_delete_path = path;
			m_pending_archive_delete_path.clear();
			m_pending_delete_is_archive_entry = false;
			m_file_error.clear();
			m_file_delete_dialog_open = true;
			m_file_delete_dialog_requested = true;
		}

		void request_archive_entry_rename(std::string_view archive_path)
		{
			m_file_dialog_archive_source_path = archive_path;
			request_file_dialog(
				file_dialog_mode::rename_archive_entry,
				path_from_utf8(archive_path));
		}

		void request_archive_entry_delete(std::string_view archive_path)
		{
			m_pending_file_delete_path.reset();
			m_pending_archive_delete_path = archive_path;
			m_pending_delete_is_archive_entry = true;
			m_file_error.clear();
			m_file_delete_dialog_open = true;
			m_file_delete_dialog_requested = true;
		}

		[[nodiscard]] static std::filesystem::path suggested_copy_path(
			const std::filesystem::path& source)
		{
			auto filename = source.stem();
			filename += path_from_utf8("_copy");
			filename += source.extension();
			return source.parent_path() / filename;
		}

		void begin_local_file_save_as(
			const std::filesystem::path& path)
		{
			const auto extension = file_extension(path);
			if (extension == ".tree")
			{
				const bool already_open = !m_current_tree_path.empty() &&
					local_tree_document_identity(m_current_tree_path) ==
						local_tree_document_identity(path);
				if (!already_open && !perform_io([&]
				{
					open_tree_file(path);
				}))
				{
					return;
				}
				request_file_dialog(
					file_dialog_mode::save_tree,
					suggested_copy_path(path));
				return;
			}

			const auto identity = local_tree_document_identity(path);
			if ((extension == ".stree" || extension == ".tar") &&
				!m_session_path.empty() &&
				local_tree_document_identity(m_session_path) == identity)
			{
				request_file_dialog(
					file_dialog_mode::save_session,
					suggested_copy_path(path));
				return;
			}

			m_file_dialog_source_path = path;
			request_file_dialog(
				file_dialog_mode::save_copy,
				suggested_copy_path(path));
			m_file_dialog_copy_from_memory = false;
			m_file_dialog_copy_contents.clear();
			m_file_dialog_required_extension = extension;
		}

		void begin_archive_entry_save_as(std::string_view archive_path)
		{
			const auto* entry = find_archive_entry(
				m_session_entries,
				archive_path);
			if (!entry)
			{
				throw std::invalid_argument(
					"Запись рабочей области больше недоступна.");
			}
			const std::string contents =
				entry->path == m_current_archive_member_path
				? m_current_tree_serialization
				: entry->contents;
			const auto separator = entry->path.find_last_of('/');
			const auto filename = std::string_view(entry->path).substr(
				separator == std::string::npos ? 0 : separator + 1);
			const auto suggested_path = suggested_copy_path(
				m_explorer_directory / path_from_utf8(filename));
			request_file_dialog(
				file_dialog_mode::save_copy,
				suggested_path);
			m_file_dialog_source_path.clear();
			m_file_dialog_copy_from_memory = true;
			m_file_dialog_copy_contents = contents;
			m_file_dialog_required_extension = archive_extension(entry->path);
		}

		void mark_workspace_structure_dirty()
		{
			invalidate_tree_preview_cache();
			std::erase_if(
				m_session_entries,
				[](const auto& entry)
				{
					return archive_extension(entry.path) == ".htree";
				});
			reset_history();
			for (auto& [archive_path, document] : m_tree_documents)
			{
				(void)archive_path;
				document.current_state_id = 0;
				document.saved_state_id = 0;
			}
			m_tree_dirty = true;
			m_workspace_structure_dirty = true;
			m_history_warning =
				"Состав рабочей области изменён; история отмены очищена.";
			update_session_metadata();
			remember_active_tree_document();
		}

		void rename_archive_entry(
			std::string_view source_path,
			std::string_view target_path)
		{
			if (source_path.empty() || target_path.empty() ||
				source_path == target_path)
			{
				return;
			}
			const auto extension = archive_extension(source_path);
			if (extension == ".session" || extension == ".htree")
			{
				throw std::invalid_argument(
					"Системные записи рабочей области переименовывать нельзя.");
			}
			if (target_path.find('\\') != std::string_view::npos ||
				target_path.find_last_of('/') !=
					source_path.find_last_of('/') ||
				archive_extension(target_path) != extension)
			{
				throw std::invalid_argument(
					"Укажите новое имя в той же папке и сохраните расширение.");
			}
			if (find_archive_entry(m_session_entries, target_path))
			{
				throw std::filesystem::filesystem_error(
					"Запись с таким именем уже существует",
					std::make_error_code(std::errc::file_exists));
			}
			auto entry = std::find_if(
				m_session_entries.begin(),
				m_session_entries.end(),
				[&](const auto& candidate)
				{
					return lowercase_ascii(candidate.path) ==
						lowercase_ascii(std::string(source_path));
				});
			if (entry == m_session_entries.end())
			{
				throw std::invalid_argument(
					"Запись рабочей области больше недоступна.");
			}
			const std::string old_path(entry->path);
			const std::string new_path(target_path);
			std::filesystem::path current_local_path;
			if (m_current_archive_member_path == old_path)
			{
				const auto document = m_tree_documents.find(old_path);
				if (document != m_tree_documents.end())
				{
					current_local_path = document->second.local_path;
				}
			}
			entry->path = new_path;

			if (const auto document = m_tree_documents.find(old_path);
				document != m_tree_documents.end())
			{
				auto state = document->second;
				const auto separator = new_path.find_last_of('/');
				state.display_name = new_path.substr(
					separator == std::string::npos ? 0 : separator + 1);
				m_tree_documents.erase(document);
				m_tree_documents.emplace(new_path, std::move(state));
			}
			for (auto& tab_path : m_open_tree_tabs)
			{
				if (tab_path == old_path)
				{
					tab_path = new_path;
				}
			}
			if (m_current_archive_member_path == old_path)
			{
				m_current_archive_member_path = new_path;
				m_tree_tab_requested_path = new_path;
				if (current_local_path.empty())
				{
					const auto separator = new_path.find_last_of('/');
					const auto filename = std::string_view(new_path).substr(
						separator == std::string::npos ? 0 : separator + 1);
					const auto dot = filename.find_last_of('.');
					m_current_tree_name = std::string(
						filename.substr(0, dot == std::string_view::npos
							? filename.size()
							: dot));
				}
			}
			if (m_merge_source_archive_path == old_path)
			{
				m_merge_source_archive_path = new_path;
			}
			for (auto& path : m_split_result_member_paths)
			{
				if (path == old_path)
				{
					path = new_path;
				}
			}
			for (auto& pane : m_tree_comparison_panes)
			{
				if (pane.archive_member_path == old_path)
				{
					pane.archive_member_path = new_path;
				}
			}
			mark_workspace_structure_dirty();
			select_archive_entry_in_explorer(new_path);
			m_last_action = std::format(
				"Файл в рабочей области переименован: {}",
				new_path);
		}

		void delete_archive_entry(std::string_view archive_path)
		{
			const auto* found = find_archive_entry(
				m_session_entries,
				archive_path);
			if (!found)
			{
				throw std::invalid_argument(
					"Запись рабочей области больше недоступна.");
			}
			const auto extension = archive_extension(found->path);
			if (extension == ".session" || extension == ".htree")
			{
				throw std::invalid_argument(
					"Системные записи рабочей области удалять нельзя.");
			}
			const std::string path(found->path);
			const bool deleting_current_tree =
				m_current_archive_member_path == path;
			if (extension == ".tree")
			{
				if (m_tree_comparison_mode)
				{
					reset_tree_comparison();
				}
				if (deleting_current_tree)
				{
					clear_current_tree_state(false);
				}
			}
			m_session_entries.erase(
				std::remove_if(
					m_session_entries.begin(),
					m_session_entries.end(),
					[&](const auto& entry)
					{
						return lowercase_ascii(entry.path) ==
							lowercase_ascii(path);
					}),
				m_session_entries.end());
			m_open_tree_tabs.erase(
				std::remove(
					m_open_tree_tabs.begin(),
					m_open_tree_tabs.end(),
					path),
				m_open_tree_tabs.end());
			m_tree_documents.erase(path);
			if (m_merge_source_archive_path == path)
			{
				m_merge_source_archive_path.clear();
			}
			for (auto& split_path : m_split_result_member_paths)
			{
				if (split_path == path)
				{
					split_path.clear();
				}
			}
			m_selected_archive_entry = -1;
			mark_workspace_structure_dirty();
			if (deleting_current_tree)
			{
				const auto remaining_tree = std::find_if(
					m_session_entries.begin(),
					m_session_entries.end(),
					[](const auto& entry)
					{
						return archive_extension(entry.path) == ".tree";
					});
				if (remaining_tree != m_session_entries.end())
				{
					open_archive_tree(*remaining_tree);
					select_archive_entry_in_explorer(
						remaining_tree->path);
				}
				m_tree_dirty = true;
			}
			m_last_action = std::format(
				"Удалена запись из рабочей области: {}",
				path);
		}

		void rename_local_file(
			const std::filesystem::path& source,
			const std::filesystem::path& target)
		{
			if (source.empty() || target.empty())
			{
				throw std::invalid_argument("Имя файла не может быть пустым.");
			}
			if (source.lexically_normal() == target.lexically_normal())
			{
				return;
			}
			if (source.parent_path().lexically_normal() !=
				target.parent_path().lexically_normal())
			{
				throw std::invalid_argument(
					"Переименование не может перемещать файл в другую папку.");
			}
			if (file_extension(source) != file_extension(target))
			{
				throw std::invalid_argument(
					"При переименовании сохраните расширение файла.");
			}
			if (std::filesystem::exists(target))
			{
				throw std::filesystem::filesystem_error(
					"Файл с таким именем уже существует",
					target,
					std::make_error_code(std::errc::file_exists));
			}

			const auto source_identity =
				local_tree_document_identity(source);
			std::filesystem::rename(source, target);
			for (auto& [archive_path, document] : m_tree_documents)
			{
				(void)archive_path;
				if (!document.local_path.empty() &&
					local_tree_document_identity(document.local_path) ==
						source_identity)
				{
					document.local_path = target;
					document.display_name = path_to_utf8(
						target.filename());
				}
			}
			if (!m_current_tree_path.empty() &&
				local_tree_document_identity(m_current_tree_path) ==
					source_identity)
			{
				m_current_tree_path = target;
				m_current_tree_name = path_to_utf8(target.stem());
			}
			if (!m_session_path.empty() &&
				local_tree_document_identity(m_session_path) ==
					source_identity)
			{
				m_session_path = target;
			}
			m_selected_local_path = target;
			m_last_action = std::format(
				"Файл переименован: {}",
				path_to_utf8(target.filename()));
			refresh_explorer();
		}

		void delete_local_file(const std::filesystem::path& path)
		{
			if (path.empty())
			{
				throw std::invalid_argument("Не выбран файл для удаления.");
			}
			const auto identity = local_tree_document_identity(path);
			if (!std::filesystem::remove(path))
			{
				throw std::filesystem::filesystem_error(
					"Не удалось удалить файл",
					path,
					std::make_error_code(std::errc::no_such_file_or_directory));
			}
			for (auto& [archive_path, document] : m_tree_documents)
			{
				(void)archive_path;
				if (!document.local_path.empty() &&
					local_tree_document_identity(document.local_path) == identity)
				{
					document.local_path.clear();
				}
			}
			if (!m_current_tree_path.empty() &&
				local_tree_document_identity(m_current_tree_path) == identity)
			{
				m_current_tree_path.clear();
			}
			if (!m_session_path.empty() &&
				local_tree_document_identity(m_session_path) == identity)
			{
				m_session_path.clear();
				m_workspace_structure_dirty = true;
				m_tree_dirty = true;
			}
			m_selected_local_path.reset();
			m_last_action = std::format(
				"Файл удалён: {}",
				path_to_utf8(path.filename()));
			refresh_explorer();
		}

		void set_archive_entry(std::string path, std::string contents)
		{
			const auto key = lowercase_ascii(path);
			const auto existing = std::find_if(
				m_session_entries.begin(),
				m_session_entries.end(),
				[&](const auto& entry)
				{
					return lowercase_ascii(entry.path) == key;
				});
			if (existing == m_session_entries.end())
			{
				m_session_entries.push_back({
					std::move(path),
					std::move(contents)
				});
			}
			else
			{
				existing->contents = std::move(contents);
			}
		}

		[[nodiscard]] static const tree_practicum::archive_entry*
			find_archive_entry(
				const std::vector<tree_practicum::archive_entry>& entries,
				std::string_view path)
		{
			const auto key = lowercase_ascii(std::string(path));
			const auto found = std::find_if(
				entries.begin(),
				entries.end(),
				[&](const auto& entry)
				{
					return lowercase_ascii(entry.path) == key;
				});
			return found == entries.end()
				? nullptr
				: std::addressof(*found);
		}

		[[nodiscard]] static session_manifest parse_session_manifest(
			const std::vector<tree_practicum::archive_entry>& entries)
		{
			const tree_practicum::archive_entry* manifest_entry = nullptr;
			for (const auto& entry : entries)
			{
				if (archive_extension(entry.path) != ".session")
				{
					continue;
				}
				if (manifest_entry)
				{
					throw std::invalid_argument(
						"Workspace contains more than one .session manifest");
				}
				manifest_entry = std::addressof(entry);
			}
			if (!manifest_entry)
			{
				return {};
			}
			if (manifest_entry->contents.size() > 1024u * 1024u)
			{
				throw std::length_error(
					"Workspace manifest exceeds the supported size limit");
			}

			session_manifest result;
			bool first_line = true;
			bool has_name = false;
			bool has_current_tree = false;
			std::size_t offset = 0;
			while (offset <= manifest_entry->contents.size())
			{
				const auto line_end =
					manifest_entry->contents.find('\n', offset);
				auto line = std::string_view(manifest_entry->contents).substr(
					offset,
					line_end == std::string::npos
						? std::string::npos
						: line_end - offset);
				if (!line.empty() && line.back() == '\r')
				{
					line.remove_suffix(1);
				}

				if (first_line)
				{
					first_line = false;
					if (line == "MNGL-SESSION 1")
					{
						result.version = 1;
					}
					else if (line == "MNGL-SESSION 2")
					{
						result.version = 2;
					}
					else
					{
						throw std::invalid_argument(
							"Workspace manifest has an unsupported signature");
					}
				}
				else if (!line.empty())
				{
					const auto separator = line.find('=');
					if (separator == std::string_view::npos)
					{
						throw std::invalid_argument(
							"Workspace manifest contains an invalid line");
					}
					const auto key = line.substr(0, separator);
					const auto value = line.substr(separator + 1);
					if (key == "name")
					{
						if (has_name)
						{
							throw std::invalid_argument(
								"Workspace manifest contains duplicate names");
						}
						has_name = true;
						result.name.assign(value);
					}
					else if (key == "current_tree" &&
						result.version == 2)
					{
						if (has_current_tree)
						{
							throw std::invalid_argument(
								"Workspace manifest contains duplicate current trees");
						}
						has_current_tree = true;
						result.current_tree_path.assign(value);
					}
					else if (key == "tree")
					{
						result.tree_paths.emplace_back(value);
					}
					else if (key == "history")
					{
						result.history_paths.emplace_back(value);
					}
					else
					{
						throw std::invalid_argument(
							"Workspace manifest contains an unknown field");
					}
				}

				if (line_end == std::string::npos)
				{
					break;
				}
				offset = line_end + 1;
			}
			if (result.version == 0 || !has_name || result.name.empty())
			{
				throw std::invalid_argument(
					"Workspace manifest is incomplete");
			}
			if (result.version == 2 && !has_current_tree)
			{
				throw std::invalid_argument(
					"Workspace manifest is missing its current tree field");
			}

			std::unordered_set<std::string> listed_trees;
			for (const auto& path : result.tree_paths)
			{
				if (tree_practicum::detail::normalize_archive_path(path) != path ||
					archive_extension(path) != ".tree" ||
					!find_archive_entry(entries, path))
				{
					throw std::invalid_argument(
						"Workspace manifest references a missing or invalid tree");
				}
				if (!listed_trees.insert(lowercase_ascii(path)).second)
				{
					throw std::invalid_argument(
						"Workspace manifest contains a duplicate tree path");
				}
			}
			std::unordered_set<std::string> listed_histories;
			for (const auto& path : result.history_paths)
			{
				if (tree_practicum::detail::normalize_archive_path(path) != path ||
					archive_extension(path) != ".htree" ||
					!find_archive_entry(entries, path))
				{
					throw std::invalid_argument(
						"Workspace manifest references a missing or invalid history");
				}
				if (!listed_histories.insert(lowercase_ascii(path)).second)
				{
					throw std::invalid_argument(
						"Workspace manifest contains a duplicate history path");
				}
			}
			if (!result.current_tree_path.empty())
			{
				if (tree_practicum::detail::normalize_archive_path(
						result.current_tree_path) != result.current_tree_path ||
					archive_extension(result.current_tree_path) != ".tree" ||
					!find_archive_entry(entries, result.current_tree_path))
				{
					throw std::invalid_argument(
						"Workspace manifest references a missing current tree");
				}
				if (!listed_trees.empty() &&
					!listed_trees.contains(
						lowercase_ascii(result.current_tree_path)))
				{
					throw std::invalid_argument(
						"Workspace manifest current tree is not listed");
				}
			}
			return result;
		}

		[[nodiscard]] static const tree_practicum::archive_entry*
			select_history_entry(
				const std::vector<tree_practicum::archive_entry>& entries,
				const session_manifest& manifest)
		{
			if (!manifest.history_paths.empty())
			{
				const auto canonical = std::find_if(
					manifest.history_paths.begin(),
					manifest.history_paths.end(),
					[](const auto& path)
					{
						return lowercase_ascii(path) ==
							"history/history.htree";
					});
				return find_archive_entry(
					entries,
					canonical == manifest.history_paths.end()
						? manifest.history_paths.front()
						: *canonical);
			}

			const auto canonical = find_archive_entry(
				entries,
				"history/history.htree");
			if (canonical &&
				archive_extension(canonical->path) == ".htree")
			{
				return canonical;
			}
			const auto found = std::find_if(
				entries.begin(),
				entries.end(),
				[](const auto& entry)
				{
					return archive_extension(entry.path) == ".htree";
				});
			return found == entries.end()
				? nullptr
				: std::addressof(*found);
		}

		[[nodiscard]] std::string make_session_manifest(
			std::string_view session_name) const
		{
			std::string manifest = "MNGL-SESSION 2\nname=";
			for (const unsigned char character : session_name)
			{
				manifest.push_back(
					character == '\n' || character == '\r' ||
						character < 0x20u || character == 0x7Fu
						? '_'
						: static_cast<char>(character));
			}
			manifest += "\ncurrent_tree=";
			manifest += m_current_archive_member_path;
			manifest.push_back('\n');
			for (const auto& entry : m_session_entries)
			{
				const auto extension = archive_extension(entry.path);
				if (extension == ".tree")
				{
					manifest += "tree=";
					manifest += entry.path;
					manifest.push_back('\n');
				}
				else if (extension == ".htree")
				{
					manifest += "history=";
					manifest += entry.path;
					manifest.push_back('\n');
				}
			}
			return manifest;
		}

		void update_session_metadata(std::string_view session_name)
		{
			auto session_entry = std::find_if(
				m_session_entries.begin(),
				m_session_entries.end(),
				[](const auto& entry)
				{
					return archive_extension(entry.path) == ".session";
				});
			if (session_entry == m_session_entries.end())
			{
				m_session_entries.push_back({ "session.session", {} });
				session_entry = std::prev(m_session_entries.end());
			}
			session_entry->contents =
				make_session_manifest(session_name);
		}

		void update_session_metadata()
		{
			update_session_metadata(m_session_name);
		}

		void update_history_archive_entry()
		{
			if (!m_current_archive_member_path.empty())
			{
				set_archive_entry(
					m_current_archive_member_path,
					m_current_tree_serialization);
			}
			set_archive_entry(
				"history/history.htree",
				serialize_history_document(
					m_history,
					current_interface_state(),
					capture_tree_history_state(),
					m_current_document_state_id,
					m_next_document_state_id));
		}

		void register_tree_document(
			std::string_view archive_member_path,
			const std::filesystem::path& local_path = {},
			bool open_tab = true)
		{
			auto [document, inserted] = m_tree_documents.try_emplace(
				std::string(archive_member_path));
			if (!local_path.empty())
			{
				document->second.local_path = local_path;
				if (document->second.display_name.empty())
				{
					document->second.display_name = path_to_utf8(
						local_path.filename());
				}
			}
			if (open_tab &&
				std::find(
					m_open_tree_tabs.begin(),
					m_open_tree_tabs.end(),
					archive_member_path) == m_open_tree_tabs.end())
			{
				m_open_tree_tabs.emplace_back(archive_member_path);
			}
			(void)inserted;
		}

		void remember_active_tree_document()
		{
			if (m_current_archive_member_path.empty())
			{
				return;
			}
			register_tree_document(
				m_current_archive_member_path,
				m_current_tree_path,
				false);
			auto& document = m_tree_documents.at(
				m_current_archive_member_path);
			document.current_state_id = m_current_document_state_id;
			document.saved_state_id = m_saved_document_state_id;
		}

		[[nodiscard]] bool tree_document_is_dirty(
			std::string_view archive_member_path) const
		{
			const auto found = m_tree_documents.find(
				std::string(archive_member_path));
			return found != m_tree_documents.end() &&
				found->second.current_state_id !=
					found->second.saved_state_id;
		}

		[[nodiscard]] bool any_tree_document_is_dirty() const
		{
			return std::any_of(
				m_tree_documents.begin(),
				m_tree_documents.end(),
				[](const auto& item)
				{
					return item.second.current_state_id !=
						item.second.saved_state_id;
				});
		}

		[[nodiscard]] std::string local_tree_document_identity(
			const std::filesystem::path& path) const
		{
			std::error_code error;
			auto absolute = std::filesystem::absolute(path, error);
			if (error)
			{
				absolute = path;
			}
			return lowercase_ascii(path_to_utf8(absolute.lexically_normal()));
		}

		[[nodiscard]] std::string find_tree_document_for_local_path(
			const std::filesystem::path& path) const
		{
			const auto identity = local_tree_document_identity(path);
			for (const auto& [archive_path, document] : m_tree_documents)
			{
				if (!document.local_path.empty() &&
					local_tree_document_identity(document.local_path) == identity)
				{
					return archive_path;
				}
			}
			return {};
		}

		[[nodiscard]] std::string add_tree_to_session(
			std::string_view tree_name,
			std::string_view serialization)
		{
			const std::string base_name = safe_archive_component(tree_name);
			std::string path = "trees/" + base_name + ".tree";
			const auto path_exists = [&](std::string_view candidate)
			{
				const auto key = lowercase_ascii(std::string(candidate));
				return std::any_of(
					m_session_entries.begin(),
					m_session_entries.end(),
					[&](const auto& entry)
					{
						return lowercase_ascii(entry.path) == key;
					});
			};
			for (std::size_t suffix = 2; path_exists(path); ++suffix)
			{
				path = "trees/" + base_name + " (" +
					std::to_string(suffix) + ").tree";
			}

			set_archive_entry(path, std::string(serialization));
			register_tree_document(path);
			m_tree_documents.at(path).saved_state_id =
				std::numeric_limits<std::uint64_t>::max();
			update_session_metadata();
			return path;
		}

		void create_new_tree()
		{
			sync_comparison_pane_with_active_tree(true);
			if (m_new_tree_type < 0 ||
				static_cast<std::size_t>(m_new_tree_type) >=
					tree_file_stems.size())
			{
				throw std::logic_error(
					"Нельзя создать дерево неизвестного типа.");
			}
			const auto* algorithm =
				search_tree_algorithm_for(m_new_tree_type);
			if (!algorithm)
			{
				throw std::logic_error(
					"Для выбранного типа дерева не найден алгоритм.");
			}

			visual_tree empty_tree;
			empty_tree.root = no_visual_node;
			if (m_new_tree_type == 5)
			{
				empty_tree.b_plus = true;
				empty_tree.fanout =
					b_plus_tree<std::int64_t>::default_max_children;
			}
			const std::string serialization =
				algorithm->serialize(empty_tree);
			remember_active_tree_document();
			auto previous_state = capture_tree_history_state();
			const auto archive_member_path = add_tree_to_session(
				tree_file_stems[static_cast<std::size_t>(m_new_tree_type)],
				serialization);

			const auto separator = archive_member_path.find_last_of('/');
			const auto file_name = std::string_view(archive_member_path).substr(
				separator == std::string::npos ? 0 : separator + 1);
			const auto extension = file_name.find_last_of('.');
			const std::string tree_name(
				file_name.substr(
					0,
					extension == std::string_view::npos
						? file_name.size()
						: extension));
			m_current_tree_path.clear();
			m_current_archive_member_path = archive_member_path;
			m_current_document_state_id = 0;
			m_saved_document_state_id =
				m_tree_documents.at(archive_member_path).saved_state_id;
			m_merge_source_archive_path.clear();
			m_selected_local_path.reset();
			m_split_result_member_paths = {};
			m_split_result_key_kind.reset();
			m_split_pivot_label.clear();
			create_window_tree(tree_name, serialization);
			m_current_tree_path.clear();
			m_current_archive_member_path = archive_member_path;
			update_session_metadata();

			const auto entry = std::find_if(
				m_session_entries.begin(),
				m_session_entries.end(),
				[&](const auto& candidate)
				{
					return candidate.path == archive_member_path;
				});
			if (entry == m_session_entries.end())
			{
				throw std::logic_error(
					"Созданное дерево отсутствует в рабочей области.");
			}
			const auto entry_index = static_cast<std::size_t>(
				std::distance(m_session_entries.begin(), entry));
			m_selected_archive_entry =
				entry_index <= static_cast<std::size_t>(
					std::numeric_limits<int>::max())
				? static_cast<int>(entry_index)
				: -1;

			auto archive_delta =
				std::make_shared<archive_entry_history_delta>();
			archive_delta->added_entries.push_back(*entry);
			record_tree_history_change(
				std::move(previous_state),
				std::move(archive_delta));
			m_last_action = std::format(
				"Создан новый файл дерева «{}» типа {}. Добавьте ключ через поле «Ключ»; Ctrl+S сохранит файл.",
				tree_name,
				algorithm->name());
			logger.success(
				"Created empty ",
				algorithm->name(),
				" tree '",
				tree_name,
				"' in the current workspace.");
		}

		void begin_new_workspace()
		{
			request_workspace_action(
				workspace_action_kind::new_workspace);
		}

		void request_workspace_action(
			workspace_action_kind kind,
			const std::filesystem::path& path = {})
		{
			if (kind == workspace_action_kind::none)
			{
				throw std::invalid_argument(
					"Workspace action cannot be empty");
			}
			if (m_pending_workspace_action.kind !=
				workspace_action_kind::none)
			{
				throw std::logic_error(
					"Another workspace action is already waiting");
			}

			m_pending_workspace_action = { kind, path };
			if (m_tree_dirty || any_tree_document_is_dirty())
			{
				m_unsaved_changes_dialog_open = true;
				m_unsaved_changes_dialog_requested = true;
				m_last_action =
					"Сохраните изменения или выберите, что делать с ними.";
				return;
			}
			execute_pending_workspace_action();
		}

		void execute_pending_workspace_action(
			bool discard_unsaved_changes = false)
		{
			auto action = std::move(m_pending_workspace_action);
			m_pending_workspace_action = {};
			m_unsaved_changes_dialog_open = false;
			m_unsaved_changes_dialog_requested = false;
			m_waiting_for_save_before_workspace_action = false;

			if (discard_unsaved_changes &&
				action.kind == workspace_action_kind::load_history &&
				!m_session_path.empty())
			{
				if (!perform_io([&]
				{
					open_session_archive(m_session_path);
				}))
				{
					return;
				}
			}

			switch (action.kind)
			{
			case workspace_action_kind::new_workspace:
				reset_workspace();
				break;
			case workspace_action_kind::open_file:
				(void)perform_io([&]
				{
					open_file_now(action.path);
				});
				break;
			case workspace_action_kind::load_history:
				(void)perform_io([&]
				{
					const auto* entry = find_archive_entry(
						m_session_entries,
						action.archive_member_path);
					if (!entry ||
						archive_extension(entry->path) != ".htree")
					{
						throw std::invalid_argument(
							"Выбранный файл истории больше недоступен.");
					}
					load_history(entry->path, entry->contents);
				});
				break;
			case workspace_action_kind::none:
				break;
			}
		}

		void request_history_restore(std::string_view archive_member_path)
		{
			const auto* entry = find_archive_entry(
				m_session_entries,
				archive_member_path);
			if (!entry || archive_extension(entry->path) != ".htree")
			{
				throw std::invalid_argument(
					"Выбранный файл истории больше недоступен.");
			}
			if (m_pending_workspace_action.kind !=
				workspace_action_kind::none)
			{
				throw std::logic_error(
					"Another workspace action is already waiting");
			}

			m_pending_workspace_action.kind =
				workspace_action_kind::load_history;
			m_pending_workspace_action.archive_member_path =
				entry->path;
			if (m_tree_dirty || any_tree_document_is_dirty())
			{
				m_unsaved_changes_dialog_open = true;
				m_unsaved_changes_dialog_requested = true;
				m_last_action =
					"Восстановление истории заменит текущее состояние дерева.";
				return;
			}
			execute_pending_workspace_action();
		}

		void reset_workspace()
		{
			cancel_tree_animation();
			reset_tree_comparison();
			invalidate_tree_preview_cache();
			clear_tree_find_result();
			reset_history();
			m_key_input_edit_start.reset();
			m_random_count_edit_start.reset();
			m_session_entries.clear();
			m_tree_documents.clear();
			m_open_tree_tabs.clear();
			m_tree_tab_requested_path.clear();
			m_session_path.clear();
			m_session_name = "Новая рабочая область";
			m_selected_workspace = 0;
			m_selected_local_path.reset();
			m_tree_operation_error.clear();
			m_tree_dirty = false;
			m_workspace_structure_dirty = false;
			m_show_demo_trees = false;
			m_current_tree_path.clear();
			m_current_archive_member_path.clear();
			m_merge_source_archive_path.clear();
			m_split_result_member_paths = {};
			m_split_result_key_kind.reset();
			m_split_pivot_label.clear();
			m_current_tree_name.clear();
			m_current_tree_serialization.clear();
			m_loaded_visual_tree.reset();
			m_loaded_search_tree_key_kind.reset();
			m_loaded_search_tree_valid = false;
			m_loaded_tree_index = -1;
			m_tree_canvas_tree_index = -1;
			m_tree_canvas_view_initialized = false;
			m_tree_canvas_fit_requested = true;
			m_tree_parse_error.clear();
			m_selected_archive_entry = -1;
			update_session_metadata();
			m_last_action = "Создана пустая рабочая область.";
		}

		void open_tree_file(const std::filesystem::path& path)
		{
			const auto existing_member =
				find_tree_document_for_local_path(path);
			if (!existing_member.empty())
			{
				const auto* entry = find_archive_entry(
					m_session_entries,
					existing_member);
				if (!entry)
				{
					throw std::logic_error(
						"Открытый файл дерева отсутствует в рабочей области.");
				}
				open_archive_tree(*entry);
			}
			else
			{
				auto contents = tree_practicum::read_tree_file(path);
				remember_active_tree_document();
				const auto tree_name = path_to_utf8(path.stem());
				const std::string name = tree_name.empty()
					? "tree"
					: tree_name;
				const auto member_path = add_tree_to_session(
					name,
					contents);
				register_tree_document(member_path, path);
				m_current_archive_member_path = member_path;
				m_current_tree_path = path;
				m_current_tree_name = name;
				m_current_tree_serialization = std::move(contents);
				m_tree_tab_requested_path = member_path;
				create_window_tree(
					m_current_tree_name,
					m_current_tree_serialization);
				m_current_document_state_id = 0;
				m_saved_document_state_id = 0;
				m_tree_documents.at(member_path).current_state_id = 0;
				m_tree_documents.at(member_path).saved_state_id = 0;
				m_tree_dirty = false;
				update_session_metadata();
			}
			navigate_to_directory(path.has_parent_path()
				? path.parent_path()
				: std::filesystem::current_path());
		}

		void open_session_archive(const std::filesystem::path& path)
		{
			auto entries = tree_practicum::read_tar_archive(path);
			const auto manifest = parse_session_manifest(entries);
			std::optional<history_document> history;
			const auto* history_entry =
				select_history_entry(entries, manifest);
			const bool has_history_entry = history_entry != nullptr;
			const bool empty_history_entry =
				history_entry && history_entry->contents.empty();
			if (history_entry && !history_entry->contents.empty())
			{
				history = deserialize_history_document(
					history_entry->contents);
				resolve_history_document_tree(*history, entries);
				if (manifest.version == 2 &&
					history->current_tree.current_archive_member_path !=
						manifest.current_tree_path)
				{
					throw std::invalid_argument(
						"Workspace manifest and undo history refer to different current trees");
				}
			}

			cancel_tree_animation();
			clear_tree_find_result();
			reset_history();
			m_key_input_edit_start.reset();
			m_random_count_edit_start.reset();
			reset_tree_comparison();
			m_session_entries = std::move(entries);
			m_workspace_structure_dirty = false;
			m_tree_documents.clear();
			m_open_tree_tabs.clear();
			m_tree_tab_requested_path.clear();
			for (const auto& entry : m_session_entries)
			{
				if (archive_extension(entry.path) == ".tree")
				{
					register_tree_document(entry.path);
				}
			}
			m_session_path = path;
			m_session_name = manifest.name.empty()
				? path_to_utf8(path.stem())
				: manifest.name;
			m_tree_operation_error.clear();
			m_tree_dirty = false;
			m_show_demo_trees = false;
			clear_current_tree_state(false);

			if (history)
			{
				apply_history_document(std::move(*history));
				m_last_action = std::format(
					"Открыта рабочая область «{}»: восстановлено дерево и история (отмена {}, повтор {}).",
					m_session_name,
					m_history.undo_size(),
					m_history.redo_size());
			}
			else if (!manifest.current_tree_path.empty())
			{
				const auto* current_tree = find_archive_entry(
					m_session_entries,
					manifest.current_tree_path);
				if (!current_tree)
				{
					throw std::invalid_argument(
						"Workspace manifest current tree is missing");
				}
				open_archive_tree(*current_tree);
				m_history_warning =
					"В архиве нет сохранённой истории undo/redo.";
				m_last_action = std::format(
					"Открыта рабочая область «{}» и дерево «{}» без истории undo/redo.",
					m_session_name,
					m_current_tree_name);
			}
			else
			{
				m_history_warning = history_entry
					? "Файл истории пуст; в нём нет состояний для восстановления."
					: "В архиве нет сохранённой истории undo/redo.";
				m_last_action = std::format(
					"Открыт TAR-проект: {} записей. Выберите .tree в проводнике, чтобы открыть дерево.",
					m_session_entries.size());
			}

			if (m_current_archive_member_path.empty() &&
				!m_open_tree_tabs.empty())
			{
				const auto* first_tree = find_archive_entry(
					m_session_entries,
					m_open_tree_tabs.front());
				if (first_tree)
				{
					open_archive_tree(*first_tree);
					m_last_action = std::format(
						"Открыта рабочая область «{}»: открыто файлов .tree — {}.",
						m_session_name,
						m_open_tree_tabs.size());
				}
			}

			m_tree_dirty = false;
			m_saved_document_state_id =
				m_current_document_state_id;
			remember_active_tree_document();
			if (empty_history_entry)
			{
				m_history_warning =
					"Файл истории пуст; в нём нет состояний для восстановления.";
			}
			else if (!has_history_entry)
			{
				m_history_warning =
					"В архиве нет сохранённой истории undo/redo.";
			}
			navigate_to_directory(path.has_parent_path()
				? path.parent_path()
				: std::filesystem::current_path());
		}

		void open_file(const std::filesystem::path& path)
		{
			if (path.empty())
			{
				throw std::invalid_argument("Путь к файлу не может быть пустым.");
			}
			const auto extension = file_extension(path);
			if (extension != ".tree" &&
				extension != ".stree" &&
				extension != ".tar")
			{
				throw std::invalid_argument(
					"Поддерживаются только файлы .tree, .stree и .tar.");
			}
			if (extension == ".tree")
			{
				(void)perform_io([&]
				{
					open_tree_file(path);
				});
				return;
			}
			request_workspace_action(
				workspace_action_kind::open_file,
				path);
		}

		void open_file_now(const std::filesystem::path& path)
		{
			const auto extension = file_extension(path);
			if (extension == ".tree")
			{
				open_tree_file(path);
			}
			else if (extension == ".stree" || extension == ".tar")
			{
				open_session_archive(path);
			}
			else
			{
				throw std::invalid_argument(
					"Поддерживаются только файлы .tree, .stree и .tar.");
			}
		}

		void import_tree_file(const std::filesystem::path& path)
		{
			open_tree_file(path);
		}

		void create_window_tree(
			std::string_view tree_name,
			std::string_view serialization)
		{
			cancel_tree_animation();
			invalidate_tree_preview_cache();
			clear_tree_find_result();
			m_current_tree_name = tree_name;
			m_current_tree_serialization = serialization;
			m_loaded_visual_tree.reset();
			m_loaded_search_tree_key_kind.reset();
			m_loaded_search_tree_valid = false;
			m_tree_operation_error.clear();
			m_loaded_tree_index = -1;
			m_tree_parse_error.clear();
			m_tree_canvas_tree_index = -1;
			m_tree_canvas_view_initialized = false;
			m_tree_canvas_fit_requested = true;
			if (serialization.starts_with("bst{"))
			{
				m_active_tree = 0;
			}
			else if (serialization.starts_with("avl{"))
			{
				m_active_tree = 1;
			}
			else if (serialization.starts_with("rb_tree{"))
			{
				m_active_tree = 2;
			}
			else if (serialization.starts_with("splay{"))
			{
				m_active_tree = 3;
			}
			else if (serialization.starts_with("treap{"))
			{
				m_active_tree = 4;
			}
			else if (serialization.starts_with("b_plus_tree["))
			{
				m_active_tree = 5;
			}

			try
			{
				std::size_t parsed_tree_index = 0;
				auto parsed_tree = parse_tree_serialization(
					m_current_tree_serialization,
					parsed_tree_index);
				if (const auto* algorithm =
					search_tree_algorithm_for(
						static_cast<int>(parsed_tree_index)))
				{
					try
					{
						m_loaded_search_tree_key_kind =
							infer_search_tree_key_kind(parsed_tree);
						algorithm->validate(
							parsed_tree,
							m_loaded_search_tree_key_kind.value_or(
								search_tree_key_kind::text));
						m_loaded_search_tree_valid = true;
					}
					catch (const std::exception& error)
					{
						m_tree_operation_error = error.what();
					}
				}
				m_active_tree = static_cast<int>(parsed_tree_index);
				m_loaded_tree_index = m_active_tree;
				m_loaded_visual_tree = std::move(parsed_tree);
				m_last_action = std::format(
					"Загружено дерево «{}»: {} узлов.",
					tree_name,
					m_loaded_visual_tree->nodes.size());
				logger.success(
					"Loaded tree '",
					tree_name,
					"' with ",
					m_loaded_visual_tree->nodes.size(),
					" nodes.");
			}
			catch (const tree_serialization_error& error)
			{
				m_tree_parse_error = error.what();
				m_last_action = std::format(
					"Не удалось визуализировать «{}»: {}",
					tree_name,
					m_tree_parse_error);
				logger.error("Unable to visualize tree '", tree_name, "': ", error.what());
			}
		}

		[[nodiscard]] bool can_insert_into_selected_search_tree() const noexcept
		{
			const bool numeric_key_type =
				!m_loaded_search_tree_key_kind.has_value() ||
				*m_loaded_search_tree_key_kind ==
					search_tree_key_kind::signed_integer ||
				*m_loaded_search_tree_key_kind ==
					search_tree_key_kind::floating_point;
			return !m_current_tree_name.empty() &&
				m_loaded_visual_tree.has_value() &&
				m_loaded_search_tree_valid &&
				m_loaded_tree_index == m_active_tree &&
				numeric_key_type &&
				search_tree_algorithm_for(m_active_tree) != nullptr;
		}

		[[nodiscard]] const tree_practicum::archive_entry*
			selected_merge_source() const
		{
			if (m_merge_source_archive_path.empty() ||
				m_merge_source_archive_path == m_current_archive_member_path)
			{
				return nullptr;
			}
			const auto found = std::find_if(
				m_session_entries.begin(),
				m_session_entries.end(),
				[&](const auto& entry)
				{
					return entry.path == m_merge_source_archive_path &&
						archive_extension(entry.path) == ".tree";
				});
			return found == m_session_entries.end()
				? nullptr
				: std::addressof(*found);
		}

		[[nodiscard]] bool can_merge_selected_search_tree() const
		{
			return can_insert_into_selected_search_tree() &&
				selected_merge_source() != nullptr;
		}

		void draw_workspace_tree_selector()
		{
			const char* preview = m_current_archive_member_path.empty()
				? "Нет открытого дерева"
				: m_current_archive_member_path.c_str();
			ImGui::SetNextItemWidth(320.0f);
			if (!ImGui::BeginCombo("Дерево рабочей области", preview))
			{
				return;
			}

			bool found_tree = false;
			for (std::size_t i = 0; i < m_session_entries.size(); ++i)
			{
				const auto& entry = m_session_entries[i];
				if (archive_extension(entry.path) != ".tree")
				{
					continue;
				}

				found_tree = true;
				const bool selected =
					entry.path == m_current_archive_member_path;
				ImGui::PushID(entry.path.c_str());
				const bool clicked = ImGui::Selectable(
					entry.path.c_str(),
					selected);
				ImGui::PopID();

				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
				if (clicked &&
					!selected &&
					perform_io([&]
					{
						open_archive_entry(i);
					}))
				{
					m_selected_archive_entry =
						i <= static_cast<std::size_t>(
							std::numeric_limits<int>::max())
						? static_cast<int>(i)
						: -1;
					m_selected_local_path.reset();
				}
			}
			if (!found_tree)
			{
				ImGui::TextDisabled(
					"В рабочей области пока нет деревьев.");
			}
			ImGui::EndCombo();
		}

		[[nodiscard]] std::size_t workspace_tree_count() const
		{
			return static_cast<std::size_t>(std::count_if(
				m_session_entries.begin(),
				m_session_entries.end(),
				[](const auto& entry)
				{
					return archive_extension(entry.path) == ".tree";
				}));
		}

		void select_archive_entry_in_explorer(std::string_view path)
		{
			for (std::size_t i = 0; i < m_session_entries.size(); ++i)
			{
				if (m_session_entries[i].path != path)
				{
					continue;
				}
				m_selected_archive_entry =
					i <= static_cast<std::size_t>(
						std::numeric_limits<int>::max())
					? static_cast<int>(i)
					: -1;
				m_selected_local_path.reset();
				return;
			}
		}

		void reset_tree_comparison()
		{
			m_tree_comparison_mode = false;
			m_active_comparison_pane = 0;
			for (auto& pane : m_tree_comparison_panes)
			{
				pane.archive_member_path.clear();
				pane.tree.reset();
				pane.tree_index = 0;
				pane.canvas.context_menu_key.reset();
				pane.canvas.context_menu_node.reset();
				pane.canvas.tree_index = -1;
				pane.canvas.zoom = 1.0f;
				pane.canvas.pan = {};
				pane.canvas.view_initialized = false;
				pane.canvas.fit_requested = true;
			}
		}

		void sync_comparison_pane_with_active_tree(
			bool snapshot_active_tree = false)
		{
			if (!m_tree_comparison_mode ||
				m_current_archive_member_path.empty())
			{
				return;
			}

			for (std::size_t i = 0;
				i < m_tree_comparison_panes.size();
				++i)
			{
				auto& pane = m_tree_comparison_panes[i];
				if (pane.archive_member_path !=
					m_current_archive_member_path)
				{
					continue;
				}

				m_active_comparison_pane = static_cast<int>(i);
				if (m_selected_archive_entry < 0 ||
					static_cast<std::size_t>(
						m_selected_archive_entry) >=
						m_session_entries.size() ||
					m_session_entries[
						static_cast<std::size_t>(
							m_selected_archive_entry)].path !=
						m_current_archive_member_path)
				{
					select_archive_entry_in_explorer(
						m_current_archive_member_path);
				}
				if (snapshot_active_tree && m_loaded_visual_tree)
				{
					pane.tree = *m_loaded_visual_tree;
					pane.tree_index =
						static_cast<std::size_t>(m_active_tree);
				}
				return;
			}

			if (!m_loaded_visual_tree)
			{
				return;
			}

			auto& pane = m_tree_comparison_panes[
				static_cast<std::size_t>(std::clamp(
					m_active_comparison_pane,
					0,
					static_cast<int>(
						m_tree_comparison_panes.size() - 1)))];
			pane.archive_member_path = m_current_archive_member_path;
			pane.tree = *m_loaded_visual_tree;
			pane.tree_index = static_cast<std::size_t>(m_active_tree);
			pane.canvas.tree_index = -1;
			pane.canvas.context_menu_key.reset();
			pane.canvas.context_menu_node.reset();
			pane.canvas.view_initialized = false;
			pane.canvas.fit_requested = true;
			select_archive_entry_in_explorer(
				m_current_archive_member_path);
			invalidate_tree_preview_cache();
		}

		void begin_tree_comparison()
		{
			const tree_practicum::archive_entry* primary = nullptr;
			for (const auto& entry : m_session_entries)
			{
				if (entry.path == m_current_archive_member_path &&
					archive_extension(entry.path) == ".tree")
				{
					primary = std::addressof(entry);
					break;
				}
			}
			if (!primary)
			{
				const auto found = std::find_if(
					m_session_entries.begin(),
					m_session_entries.end(),
					[](const auto& entry)
					{
						return archive_extension(entry.path) == ".tree";
					});
				if (found != m_session_entries.end())
				{
					primary = std::addressof(*found);
				}
			}

			if (!primary)
			{
				throw std::logic_error(
					"Добавьте два дерева .tree в рабочую область для сравнения.");
			}

			const auto secondary = std::find_if(
				m_session_entries.begin(),
				m_session_entries.end(),
				[&](const auto& entry)
				{
					return archive_extension(entry.path) == ".tree" &&
						entry.path != primary->path;
				});
			if (secondary == m_session_entries.end())
			{
				throw std::logic_error(
					"Добавьте второе дерево .tree в рабочую область для сравнения.");
			}

			std::size_t primary_tree_index = 0;
			auto primary_tree = parse_tree_serialization(
				primary->contents,
				primary_tree_index);
			std::size_t secondary_tree_index = 0;
			auto secondary_tree = parse_tree_serialization(
				secondary->contents,
				secondary_tree_index);
			if (primary_tree_index >= tree_names.size() ||
				secondary_tree_index >= tree_names.size())
			{
				throw std::logic_error(
					"У рабочего дерева неизвестный тип.");
			}

			const std::string primary_path = primary->path;
			const std::string secondary_path = secondary->path;
			if (m_current_archive_member_path != primary_path)
			{
				open_archive_tree(*primary);
			}

			reset_tree_comparison();
			auto& primary_pane = m_tree_comparison_panes[0];
			primary_pane.archive_member_path = primary_path;
			primary_pane.tree = std::move(primary_tree);
			primary_pane.tree_index = primary_tree_index;
			primary_pane.canvas.fit_requested = true;

			auto& secondary_pane = m_tree_comparison_panes[1];
			secondary_pane.archive_member_path = secondary_path;
			secondary_pane.tree = std::move(secondary_tree);
			secondary_pane.tree_index = secondary_tree_index;
			secondary_pane.canvas.fit_requested = true;

			m_tree_comparison_mode = true;
			m_active_comparison_pane = 0;
			select_archive_entry_in_explorer(primary_path);
			invalidate_tree_preview_cache();
		}

		void select_comparison_tree(
			std::size_t pane_index,
			std::string_view archive_path)
		{
			if (pane_index >= m_tree_comparison_panes.size())
			{
				throw std::out_of_range(
					"Индекс панели дерева вне диапазона.");
			}
			const auto* entry = find_archive_entry(
				m_session_entries,
				archive_path);
			if (!entry || archive_extension(entry->path) != ".tree")
			{
				throw std::invalid_argument(
					"Выбранная запись дерева больше недоступна.");
			}

			const std::string selected_path = entry->path;
			const std::size_t other_pane_index = 1 - pane_index;
			if (m_tree_comparison_panes[other_pane_index]
					.archive_member_path == selected_path)
			{
				throw std::invalid_argument(
					"Одно дерево нельзя открыть в обеих панелях.");
			}

			auto& pane = m_tree_comparison_panes[pane_index];
			const bool tree_changed =
				pane.archive_member_path != selected_path;
			std::optional<visual_tree> parsed_tree;
			std::size_t parsed_tree_index = pane.tree_index;
			if (tree_changed)
			{
				parsed_tree = parse_tree_serialization(
					entry->contents,
					parsed_tree_index);
				if (parsed_tree_index >= tree_names.size())
				{
					throw std::logic_error(
						"У рабочего дерева неизвестный тип.");
				}
			}

			if (m_current_archive_member_path != selected_path)
			{
				open_archive_tree(*entry);
			}

			if (tree_changed)
			{
				pane.archive_member_path = selected_path;
				pane.tree = std::move(parsed_tree);
				pane.tree_index = parsed_tree_index;
				pane.canvas.tree_index = -1;
				pane.canvas.context_menu_key.reset();
				pane.canvas.context_menu_node.reset();
				pane.canvas.view_initialized = false;
				pane.canvas.fit_requested = true;
				invalidate_tree_preview_cache();
			}

			m_active_comparison_pane =
				static_cast<int>(pane_index);
			select_archive_entry_in_explorer(selected_path);
		}

		void focus_comparison_pane(std::size_t pane_index)
		{
			if (pane_index >= m_tree_comparison_panes.size())
			{
				throw std::out_of_range(
					"Индекс панели дерева вне диапазона.");
			}
			const auto path =
				m_tree_comparison_panes[pane_index].archive_member_path;
			if (path.empty())
			{
				throw std::logic_error(
					"В этой панели не выбрано дерево.");
			}
			select_comparison_tree(pane_index, path);
		}

		void draw_comparison_tree_selector(std::size_t pane_index)
		{
			const auto& pane = m_tree_comparison_panes[pane_index];
			const char* preview = pane.archive_member_path.empty()
				? "Выберите дерево"
				: pane.archive_member_path.c_str();
			const std::string combo_label = std::format(
				"Дерево##ComparisonPane{}",
				pane_index);
			ImGui::SetNextItemWidth(-1.0f);
			if (!ImGui::BeginCombo(combo_label.c_str(), preview))
			{
				return;
			}

			bool found_tree = false;
			for (std::size_t i = 0; i < m_session_entries.size(); ++i)
			{
				const auto& entry = m_session_entries[i];
				if (archive_extension(entry.path) != ".tree" ||
					entry.path ==
						m_tree_comparison_panes[1 - pane_index]
							.archive_member_path)
				{
					continue;
				}

				found_tree = true;
				const bool selected =
					entry.path == pane.archive_member_path;
				ImGui::PushID(entry.path.c_str());
				const bool clicked = ImGui::Selectable(
					entry.path.c_str(),
					selected);
				ImGui::PopID();
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
				if (clicked)
				{
					(void)perform_io([&]
					{
						select_comparison_tree(
							pane_index,
							entry.path);
					});
				}
			}
			if (!found_tree)
			{
				ImGui::TextDisabled(
					"Нет доступных деревьев для этой панели.");
			}
			ImGui::EndCombo();
		}

		void draw_comparison_tree_panels()
		{
			sync_comparison_pane_with_active_tree();
			const ImVec2 available = ImGui::GetContentRegionAvail();
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const float pane_width =
				std::max(1.0f, (available.x - spacing) * 0.5f);

			for (std::size_t i = 0;
				i < m_tree_comparison_panes.size();
				++i)
			{
				if (i != 0)
				{
					ImGui::SameLine();
				}

				const char* pane_id = i == 0
					? "TreeComparisonPaneLeft"
					: "TreeComparisonPaneRight";
				if (ImGui::BeginChild(
					pane_id,
					ImVec2(pane_width, 0.0f),
					ImGuiChildFlags_Borders))
				{
					auto& pane = m_tree_comparison_panes[i];
					if (m_active_comparison_pane ==
						static_cast<int>(i))
					{
						ImGui::TextColored(
							{ 0.45f, 0.82f, 1.0f, 1.0f },
							"Активная панель · операции применяются сюда");
					}
					else
					{
						ImGui::TextDisabled("Нажмите на дерево, чтобы выбрать панель");
					}
					draw_comparison_tree_selector(i);
					if (pane.tree)
					{
						const visual_tree* tree_to_draw =
							std::addressof(*pane.tree);
						if (pane.archive_member_path ==
								m_current_archive_member_path &&
							m_loaded_visual_tree)
						{
							tree_to_draw =
								std::addressof(*m_loaded_visual_tree);
						}
						bool canvas_clicked = false;
						draw_tree_preview(
							pane.tree_index,
							&pane.canvas,
							tree_to_draw,
							pane.archive_member_path,
							i,
							i == 0
								? "ComparisonTreeCanvasLeft"
								: "ComparisonTreeCanvasRight",
							&canvas_clicked);
						if (canvas_clicked)
						{
							(void)perform_io([&]
							{
								focus_comparison_pane(i);
							});
						}
					}
					else
					{
						ImGui::TextDisabled(
							"Выберите дерево для отображения.");
					}
				}
				ImGui::EndChild();
			}
		}

		void draw_merge_source_selector()
		{
			const auto* selected = selected_merge_source();
			const char* preview = selected
				? selected->path.c_str()
				: "Выберите дерево-источник";
			ImGui::SetNextItemWidth(220.0f);
			if (ImGui::BeginCombo("Источник##MergeTree", preview))
			{
				bool found_any = false;
				for (std::size_t i = 0; i < m_session_entries.size(); ++i)
				{
					const auto& entry = m_session_entries[i];
					if (archive_extension(entry.path) != ".tree" ||
						entry.path == m_current_archive_member_path)
					{
						continue;
					}
					found_any = true;
					ImGui::PushID(static_cast<int>(i));
					if (ImGui::Selectable(
						entry.path.c_str(),
						entry.path == m_merge_source_archive_path))
					{
						m_merge_source_archive_path = entry.path;
					}
					ImGui::PopID();
				}
				if (!found_any)
				{
					ImGui::TextDisabled(
						"Добавьте ещё один .tree в рабочую область.");
				}
				ImGui::EndCombo();
			}
		}

		void store_search_tree_update(
			visual_tree updated_tree,
			std::string serialization,
			search_tree_key_kind key_kind)
		{
			auto previous_state = capture_tree_history_state();
			clear_tree_find_result();
			if (m_current_archive_member_path.empty())
			{
				throw std::logic_error(
					"У открытого дерева отсутствует запись рабочей области.");
			}
			set_archive_entry(
				m_current_archive_member_path,
				serialization);
			update_session_metadata();
			m_current_tree_serialization = std::move(serialization);
			m_loaded_visual_tree = std::move(updated_tree);
			m_loaded_search_tree_key_kind = key_kind;
			m_loaded_search_tree_valid = true;
			m_loaded_tree_index = m_active_tree;
			record_tree_history_change(std::move(previous_state));
		}

		void perform_search_tree_find()
		{
			clear_tree_find_result();
			if (!can_insert_into_selected_search_tree())
			{
				m_last_action =
					"Поиск доступен для загруженного дерева с ключами int64_t или double.";
				return;
			}

			try
			{
				commit_pending_input_edits();
				cancel_tree_animation();
				m_tree_operation_error.clear();
				auto key = parse_numeric_search_tree_key_input(
					std::string_view(m_key_input.data()),
					m_loaded_search_tree_key_kind);
				const auto key_kind =
					m_loaded_search_tree_key_kind.value_or(
						search_tree_key_kind_for_value(key.value));
				const std::string key_text = full_visual_value(key.value);
				const auto* algorithm =
					search_tree_algorithm_for(m_active_tree);
				if (!algorithm)
				{
					throw std::logic_error(
						"Для выбранного дерева не найден алгоритм поиска.");
				}

				auto updated_tree = *m_loaded_visual_tree;
				auto result = algorithm->find(
					updated_tree,
					key,
					key_kind);
				if (result.changed)
				{
					auto serialization =
						algorithm->serialize(updated_tree);
					store_search_tree_update(
						std::move(updated_tree),
						std::move(serialization),
						key_kind);
				}

				m_tree_find_path = std::move(result.path);
				m_tree_find_tree_index = m_active_tree;
				m_tree_find_result_valid = true;
				m_tree_find_found = result.found;
				m_tree_find_status = result.found
					? std::format(
						"Найден ключ «{}» в {}: проверено {} узлов, путь подсвечен.",
						key_text,
						algorithm->name(),
						result.visited_nodes)
					: std::format(
						"Ключ «{}» не найден в {}: проверено {} узлов, путь поиска подсвечен.",
						key_text,
						algorithm->name(),
						result.visited_nodes);
				if (result.changed)
				{
					m_tree_find_status += " Splay перестроил дерево.";
				}
				m_last_action = m_tree_find_status;
				if (result.found)
				{
					logger.success(
						"Found key ",
						key_text,
						" in ",
						algorithm->name(),
						" after visiting ",
						result.visited_nodes,
						" nodes.");
				}
				else
				{
					logger.info(
						"Key ",
						key_text,
						" was not found in ",
						algorithm->name(),
						" after visiting ",
						result.visited_nodes,
						" nodes.");
				}
			}
			catch (const std::exception& error)
			{
				clear_tree_find_result();
				m_tree_operation_error = error.what();
				m_last_action = std::format(
					"Поиск не выполнен: {}",
					error.what());
				logger.error("Search-tree lookup failed: ", error.what());
			}
		}

		void perform_search_tree_insert()
		{
			clear_tree_find_result();
			if (!can_insert_into_selected_search_tree())
			{
				m_last_action =
					"Операция недоступна: выберите корректное числовое дерево с ключами int64_t или double.";
				return;
			}

			try
			{
				commit_pending_input_edits();
				cancel_tree_animation();
				m_tree_operation_error.clear();
				auto key = parse_numeric_search_tree_key_input(
					std::string_view(m_key_input.data()),
					m_loaded_search_tree_key_kind);
				const auto effective_key_kind =
					m_loaded_search_tree_key_kind.value_or(
						search_tree_key_kind_for_value(key.value));
				const auto inserted_value = full_visual_value(key.value);
				const auto* algorithm =
					search_tree_algorithm_for(m_active_tree);
				if (!algorithm)
				{
					throw std::logic_error(
						"Для выбранного дерева нет алгоритма вставки.");
				}
				auto previous_state = capture_tree_history_state();
				auto updated_tree = *m_loaded_visual_tree;
				const bool tree_is_small_enough_to_animate =
					updated_tree.nodes.size() <
						maximum_animated_tree_nodes;
				const bool capture_splay_animation =
					tree_is_small_enough_to_animate &&
					algorithm->supports_animation();
				std::optional<visual_tree> animation_start;
				if (tree_is_small_enough_to_animate)
				{
					animation_start = updated_tree;
				}
				const visual_key animation_key = key;
				const std::size_t animation_cache_index =
					m_tree_comparison_mode
					? static_cast<std::size_t>(std::clamp(
						m_active_comparison_pane,
						0,
						static_cast<int>(
							m_tree_preview_caches.size() - 1)))
					: 0;
				const auto pre_operation_geometry =
					m_tree_preview_caches[animation_cache_index].geometry;
				const bool has_pre_operation_layout =
					updated_tree.nodes.empty() ||
					(tree_is_small_enough_to_animate &&
						pre_operation_geometry &&
						pre_operation_geometry->revision ==
							m_tree_preview_revision &&
						pre_operation_geometry->source_tree_signature ==
							tree_animation_tree_signature(updated_tree) &&
						pre_operation_geometry->tree_index ==
							m_active_tree &&
						pre_operation_geometry->current_tree_loaded &&
						!pre_operation_geometry->showing_animation &&
						pre_operation_geometry->layout.nodes.size() ==
							updated_tree.nodes.size());
				auto operation = algorithm->insert(
					updated_tree,
					std::move(key),
					effective_key_kind,
					capture_splay_animation);
				if (!operation.changed)
				{
					m_last_action = std::format(
						"Ключ «{}» уже существует в {}.",
						inserted_value,
						algorithm->name());
					logger.warning(
						algorithm->name(),
						" insert skipped because the key already exists: ",
						inserted_value);
					return;
				}

				const bool has_step_animation =
					algorithm->supports_animation() &&
					operation.animation_events_complete &&
					!operation.events.empty();
				const bool capture_layout_transition =
					animation_start.has_value() &&
					!has_step_animation &&
					has_pre_operation_layout;
				std::vector<glm::dvec2> transition_start_centers;
				glm::dvec2 transition_start_layout_size{};
				if (capture_layout_transition)
				{
					if (animation_start->nodes.empty())
					{
						const double invalid_coordinate =
							std::numeric_limits<double>::quiet_NaN();
						transition_start_centers.assign(
							updated_tree.nodes.size(),
							glm::dvec2{
								invalid_coordinate,
								invalid_coordinate
							});
					}
					else
					{
						transition_start_centers =
							make_tree_layout_animation_start_centers(
								*animation_start,
								pre_operation_geometry->layout,
								updated_tree);
						transition_start_layout_size =
							pre_operation_geometry->layout.size;
					}
				}

				auto serialization = algorithm->serialize(updated_tree);
				if (!m_current_archive_member_path.empty())
				{
					set_archive_entry(
						m_current_archive_member_path,
						serialization);
					update_session_metadata();
				}
				m_current_tree_serialization = std::move(serialization);
				m_loaded_visual_tree = std::move(updated_tree);
				m_loaded_search_tree_key_kind = effective_key_kind;
				m_loaded_search_tree_valid = true;
				m_loaded_tree_index = m_active_tree;
				record_tree_history_change(std::move(previous_state));
				if (animation_start.has_value() && has_step_animation)
				{
					try
					{
						publish_tree_animation_events(operation.events);
						auto received_events =
							take_pending_tree_animation_events();
						if (received_events.size() !=
							operation.events.size())
						{
							throw std::logic_error(
								"Event queue size does not match the Splay operation.");
						}
						tree_animation_state animation;
						animation.tree = std::move(animation_start.value());
						animation.events = std::move(received_events);
						animation.tree_index = m_active_tree;
						m_tree_animation = std::move(animation);
					}
					catch (const std::exception& error)
					{
						cancel_tree_animation();
						m_tree_operation_error = std::format(
							"Анимацию Splay не удалось запустить: {}",
							error.what());
						logger.error(
							"Unable to start Splay animation: ",
							error.what());
					}
				}
				else if (capture_layout_transition &&
					transition_start_centers.size() ==
						m_loaded_visual_tree->nodes.size())
				{
					tree_animation_state animation;
					animation.mode =
						tree_animation_mode::layout_transition;
					animation.tree = *m_loaded_visual_tree;
					animation.transition_start_centers =
						std::move(transition_start_centers);
					animation.transition_start_layout_size =
						transition_start_layout_size;
					animation.emphasized_node =
						find_animation_key_node(
							animation.tree,
							animation_key);
					animation.tree_index = m_active_tree;
					m_tree_animation = std::move(animation);
				}
				if (operation.inserted)
				{
					m_last_action = std::format(
						"Ключ «{}» вставлен в {}. Сохраните изменения через Ctrl+S.",
						inserted_value,
						algorithm->name());
					logger.success(
						"Inserted key into ",
						algorithm->name(),
						": ",
						inserted_value);
				}
				else
				{
					m_last_action = std::format(
						"Ключ «{}» уже был в дереве; он перемещён в корень Splay. Сохраните изменения через Ctrl+S.",
						inserted_value);
					logger.info(
						"Duplicate key accessed and splayed to root: ",
						inserted_value);
				}
				if (!tree_is_small_enough_to_animate)
				{
					m_last_action +=
						" Анимация отключена для деревьев от 512 узлов.";
				}
			}
			catch (const std::exception& error)
			{
				m_tree_operation_error = error.what();
				m_last_action = std::format(
					"Вставка не выполнена: {}",
					error.what());
				logger.error("Search-tree insertion failed: ", error.what());
			}
		}

		void perform_search_tree_random_insert()
		{
			clear_tree_find_result();
			if (!can_insert_into_selected_search_tree())
			{
				m_last_action =
					"Операция недоступна: выберите корректное числовое дерево с ключами int64_t или double.";
				return;
			}

			try
			{
				commit_pending_input_edits();
				cancel_tree_animation();
				m_tree_operation_error.clear();
				if (m_random_count == 0)
				{
					m_last_action =
						"Количество случайных ключей должно быть больше нуля.";
					return;
				}

				auto key_kind = m_loaded_search_tree_key_kind;
				if (!key_kind)
				{
					const auto type_probe =
						parse_numeric_search_tree_key_input(
							std::string_view(m_key_input.data()),
							std::nullopt);
					key_kind = search_tree_key_kind_for_value(
						type_probe.value);
				}
				const auto* algorithm =
					search_tree_algorithm_for(m_active_tree);
				if (!algorithm)
				{
					throw std::logic_error(
						"Для выбранного дерева нет алгоритма вставки.");
				}

				std::mt19937_64 random_engine{
					std::random_device{}()
				};
				auto previous_state = capture_tree_history_state();
				auto updated_tree = *m_loaded_visual_tree;
				auto operation = algorithm->insert_many(
					updated_tree,
					m_random_count,
					*key_kind,
					random_engine);
				if (operation.inserted_count != m_random_count)
				{
					throw std::logic_error(
						"Массовая вставка не добавила запрошенное число уникальных ключей.");
				}
				algorithm->validate(updated_tree, *key_kind);
				auto serialization = operation.serialization
					? std::move(*operation.serialization)
					: algorithm->serialize(updated_tree);
				if (!m_current_archive_member_path.empty())
				{
					set_archive_entry(
						m_current_archive_member_path,
						serialization);
					update_session_metadata();
				}
				m_current_tree_serialization = std::move(serialization);
				m_loaded_visual_tree = std::move(updated_tree);
				m_loaded_search_tree_key_kind = *key_kind;
				m_loaded_search_tree_valid = true;
				m_loaded_tree_index = m_active_tree;
				record_tree_history_change(std::move(previous_state));
				m_last_action = std::format(
					"Добавлено {} случайных уникальных ключей в {} без анимации. Сохраните изменения через Ctrl+S.",
					operation.inserted_count,
					algorithm->name());
				logger.success(
					"Inserted ",
					operation.inserted_count,
					" random unique keys into ",
					algorithm->name(),
					" without animation.");
			}
			catch (const std::exception& error)
			{
				m_tree_operation_error = error.what();
				m_last_action = std::format(
					"Массовая вставка не выполнена: {}",
					error.what());
				logger.error(
					"Random bulk insertion failed: ",
					error.what());
			}
		}

		void perform_search_tree_erase(
			std::optional<visual_key> hovered_key = std::nullopt)
		{
			clear_tree_find_result();
			if (!can_insert_into_selected_search_tree())
			{
				m_last_action =
					"Удаление недоступно: выберите корректное дерево с ключами int64_t или double.";
				return;
			}

			try
			{
				commit_pending_input_edits();
				cancel_tree_animation();
				m_tree_operation_error.clear();
				auto key = hovered_key
					? std::move(*hovered_key)
					: parse_numeric_search_tree_key_input(
						std::string_view(m_key_input.data()),
						m_loaded_search_tree_key_kind);
				const auto key_kind = m_loaded_search_tree_key_kind.value_or(
					search_tree_key_kind_for_value(key.value));
				const std::string key_text = full_visual_value(key.value);
				const auto* algorithm =
					search_tree_algorithm_for(m_active_tree);
				if (!algorithm)
				{
					throw std::logic_error(
						"Для выбранного дерева нет алгоритма удаления.");
				}

				auto updated_tree = *m_loaded_visual_tree;
				const auto result = algorithm->erase(
					updated_tree,
					key,
					key_kind);
				if (!result.changed)
				{
					m_last_action = std::format(
						"Ключ «{}» не найден в {}.",
						key_text,
						algorithm->name());
					logger.warning(
						algorithm->name(),
						" erase skipped because the key was not found: ",
						key_text);
					return;
				}

				auto serialization = algorithm->serialize(updated_tree);
				store_search_tree_update(
					std::move(updated_tree),
					std::move(serialization),
					key_kind);
				m_last_action = result.erased
					? std::format(
						"Ключ «{}» удалён из {}. Сохраните изменения через Ctrl+S.",
						key_text,
						algorithm->name())
					: std::format(
						"Ключ «{}» не найден; Splay перестроил путь поиска. Сохраните изменения через Ctrl+S.",
						key_text);
				if (result.erased)
				{
					logger.success(
						"Erased key from ",
						algorithm->name(),
						": ",
						key_text);
				}
				else
				{
					logger.info(
						"Splay adjusted after an unsuccessful erase: ",
						key_text);
				}
			}
			catch (const std::exception& error)
			{
				m_tree_operation_error = error.what();
				m_last_action = std::format(
					"Удаление не выполнено: {}",
					error.what());
				logger.error("Search-tree erasure failed: ", error.what());
			}
		}

		void perform_search_tree_merge()
		{
			clear_tree_find_result();
			if (!can_merge_selected_search_tree())
			{
				m_last_action =
					"Слияние недоступно: выберите числовое дерево и отдельную запись .tree-источника.";
				return;
			}

			try
			{
				commit_pending_input_edits();
				cancel_tree_animation();
				m_tree_operation_error.clear();
				const auto* source_entry = selected_merge_source();
				if (!source_entry)
				{
					throw std::invalid_argument(
						"Дерево-источник больше не доступно в рабочей области.");
				}
				const std::string source_path = source_entry->path;

				std::size_t source_tree_index = 0;
				auto source_tree = parse_tree_serialization(
					source_entry->contents,
					source_tree_index);
				if (source_tree_index != static_cast<std::size_t>(m_active_tree))
				{
					throw std::invalid_argument(
						"Для merge выберите дерево того же типа, что и текущее.");
				}

				const auto source_key_kind =
					infer_search_tree_key_kind(source_tree);
				if (source_key_kind &&
					*source_key_kind != search_tree_key_kind::signed_integer &&
					*source_key_kind != search_tree_key_kind::floating_point)
				{
					throw std::invalid_argument(
						"Merge поддерживает только деревья с ключами int64_t или double.");
				}
				if (m_loaded_search_tree_key_kind &&
					source_key_kind &&
					*m_loaded_search_tree_key_kind != *source_key_kind)
				{
					throw std::invalid_argument(
						"Тип ключей дерева-источника должен совпадать с текущим деревом.");
				}
				const auto key_kind = m_loaded_search_tree_key_kind.value_or(
					source_key_kind.value_or(
						search_tree_key_kind::signed_integer));
				const auto* algorithm =
					search_tree_algorithm_for(m_active_tree);
				if (!algorithm)
				{
					throw std::logic_error(
						"Для выбранного дерева нет алгоритма merge.");
				}
				algorithm->validate(source_tree, key_kind);

				auto updated_tree = *m_loaded_visual_tree;
				const bool changed = algorithm->merge(
					updated_tree,
					source_tree,
					key_kind);
				if (!changed)
				{
					m_last_action =
						"Слияние не изменило дерево: источник пуст.";
					logger.warning(
						"Merge skipped because the source tree is empty: ",
						source_entry->path);
					return;
				}

				auto serialization = algorithm->serialize(updated_tree);
				store_search_tree_update(
					std::move(updated_tree),
					std::move(serialization),
					key_kind);
				m_last_action = std::format(
					"Дерево объединено с «{}». Источник сохранён отдельно; нажмите Ctrl+S для сохранения.",
					source_path);
				logger.success(
					"Merged ",
					algorithm->name(),
					" with source tree ",
					source_path);
			}
			catch (const std::exception& error)
			{
				m_tree_operation_error = error.what();
				m_last_action = std::format(
					"Слияние не выполнено: {}",
					error.what());
				logger.error("Search-tree merge failed: ", error.what());
			}
		}

		void perform_search_tree_split(
			std::optional<visual_key> hovered_key = std::nullopt)
		{
			clear_tree_find_result();
			if (!can_insert_into_selected_search_tree())
			{
				m_last_action =
					"Split недоступен: выберите корректное дерево с ключами int64_t или double.";
				return;
			}

			try
			{
				commit_pending_input_edits();
				cancel_tree_animation();
				m_tree_operation_error.clear();
				auto pivot = hovered_key
					? std::move(*hovered_key)
					: parse_numeric_search_tree_key_input(
						std::string_view(m_key_input.data()),
						m_loaded_search_tree_key_kind);
				const auto key_kind = m_loaded_search_tree_key_kind.value_or(
					search_tree_key_kind_for_value(pivot.value));
				const std::string pivot_text = full_visual_value(pivot.value);
				const auto* algorithm =
					search_tree_algorithm_for(m_active_tree);
				if (!algorithm)
				{
					throw std::logic_error(
						"Для выбранного дерева нет алгоритма split.");
				}

				auto [left_tree, right_tree] = algorithm->split(
					*m_loaded_visual_tree,
					pivot,
					key_kind);
				auto left_serialization = algorithm->serialize(left_tree);
				auto right_serialization = algorithm->serialize(right_tree);
				const std::string source_name = m_current_tree_name.empty()
					? std::string("tree")
					: m_current_tree_name;
				auto previous_state = capture_tree_history_state();
				const auto left_path = add_tree_to_session(
					source_name + " split left",
					left_serialization);
				const auto right_path = add_tree_to_session(
					source_name + " split right",
					right_serialization);
				m_split_result_member_paths = { left_path, right_path };
				m_split_result_key_kind = key_kind;
				m_split_pivot_label = pivot_text;

				const auto left_entry = std::find_if(
					m_session_entries.begin(),
					m_session_entries.end(),
					[&](const auto& entry)
					{
						return entry.path == left_path;
					});
				if (left_entry == m_session_entries.end())
				{
					throw std::logic_error(
						"Не удалось открыть левый результат split.");
				}
				open_archive_entry(static_cast<std::size_t>(
					std::distance(m_session_entries.begin(), left_entry)));
				if (m_loaded_visual_tree &&
					m_loaded_visual_tree->nodes.empty())
				{
					m_loaded_search_tree_key_kind = key_kind;
				}
				m_tree_dirty = true;
				auto split_delta =
					std::make_shared<archive_entry_history_delta>();
				for (const auto& path : { left_path, right_path })
				{
					const auto entry = std::find_if(
						m_session_entries.begin(),
						m_session_entries.end(),
						[&](const auto& candidate)
						{
							return candidate.path == path;
						});
					if (entry == m_session_entries.end())
					{
						throw std::logic_error(
							"Не удалось сохранить результат split в истории.");
					}
					split_delta->added_entries.push_back(*entry);
				}
				record_tree_history_change(
					std::move(previous_state),
					std::move(split_delta));
				m_last_action = std::format(
					"Split по {} завершён: слева ключи < {}, справа >= {}. Результаты сохранены во вкладках; нажмите Ctrl+S.",
					algorithm->name(),
					pivot_text,
					pivot_text);
				logger.success(
					"Split ",
					algorithm->name(),
					" at ",
					pivot_text,
					" into ",
					left_path,
					" and ",
					right_path);
			}
			catch (const std::exception& error)
			{
				m_tree_operation_error = error.what();
				m_last_action = std::format(
					"Split не выполнен: {}",
					error.what());
				logger.error("Search-tree split failed: ", error.what());
			}
		}

		void invoke_tree_tool(
			tree_tool_operation operation,
			std::optional<visual_key> selected_key = std::nullopt,
			std::optional<std::size_t> comparison_pane = std::nullopt)
		{
			if (selected_key &&
				comparison_pane &&
				m_tree_comparison_mode &&
				!perform_io([&]
				{
					focus_comparison_pane(*comparison_pane);
				}))
			{
				return;
			}

			const bool targeted_operation =
				operation == tree_tool_operation::erase ||
				operation == tree_tool_operation::split;
			if (selected_key &&
				targeted_operation &&
				m_execute_hovered_key_tools_immediately)
			{
				m_active_tree_tool.reset();
				if (operation == tree_tool_operation::erase)
				{
					perform_search_tree_erase(std::move(selected_key));
				}
				else
				{
					perform_search_tree_split(std::move(selected_key));
				}
				return;
			}

			commit_pending_input_edits();
			if (selected_key)
			{
				set_path_buffer(
					m_key_input,
					full_visual_value(selected_key->value));
			}
			m_active_tree_tool = operation;
		}

		void perform_tree_tool(tree_tool_operation operation)
		{
			switch (operation)
			{
			case tree_tool_operation::find:
				perform_search_tree_find();
				break;
			case tree_tool_operation::insert:
				perform_search_tree_insert();
				break;
			case tree_tool_operation::random_insert:
				perform_search_tree_random_insert();
				break;
			case tree_tool_operation::erase:
				perform_search_tree_erase();
				break;
			case tree_tool_operation::merge:
				perform_search_tree_merge();
				break;
			case tree_tool_operation::split:
				perform_search_tree_split();
				break;
			}
		}

		void load_history(
			std::string_view entry_path,
			std::string_view contents)
		{
			if (contents.empty())
			{
				m_history.clear();
				m_history_warning =
					"Файл истории пуст; в нём нет состояний для восстановления.";
				m_last_action = std::format(
					"История «{}» пуста.",
					entry_path);
				return;
			}

			auto document = deserialize_history_document(contents);
			resolve_history_document_tree(document, m_session_entries);
			apply_history_document(std::move(document));
			m_last_action = std::format(
				"Восстановлена история «{}»: отмена {}, повтор {}.",
				entry_path,
				m_history.undo_size(),
				m_history.redo_size());
			logger.success(
				"Restored undo history from '",
				entry_path,
				"' with ",
				m_history.undo_size(),
				" undo and ",
				m_history.redo_size(),
				" redo states.");
		}

		static void resolve_history_document_tree(
			history_document& document,
			const std::vector<tree_practicum::archive_entry>& entries)
		{
			auto& current_tree = document.current_tree;
			if (current_tree.archive_entry_delta ||
				current_tree.archive_entries_present)
			{
				throw std::invalid_argument(
					"Undo history current state contains an invalid archive-entry delta");
			}
			if (current_tree.current_archive_member_path.empty())
			{
				if (!current_tree.current_tree_serialization.empty())
				{
					throw std::invalid_argument(
						"Undo history current tree is missing its archive member");
				}
				if (document.current_tree_checksum != 0)
				{
					throw std::invalid_argument(
						"Undo history contains a checksum without a current tree");
				}
				return;
			}

			const auto* entry = find_archive_entry(
				entries,
				current_tree.current_archive_member_path);
			if (!entry || archive_extension(entry->path) != ".tree")
			{
				throw std::invalid_argument(
					"Undo history references a tree missing from the workspace");
			}
			if (history_string_checksum(entry->contents) !=
				document.current_tree_checksum)
			{
				throw std::invalid_argument(
					"Undo history does not match the current tree in the workspace");
			}
			current_tree.current_archive_member_path = entry->path;
			current_tree.current_tree_serialization = entry->contents;
			if (current_tree.current_tree_name.empty())
			{
				const auto separator = entry->path.find_last_of('/');
				const auto file_name = std::string_view(entry->path).substr(
					separator == std::string::npos ? 0 : separator + 1);
				const auto dot = file_name.find_last_of('.');
				current_tree.current_tree_name = std::string(
					dot == std::string_view::npos
						? file_name
						: file_name.substr(0, dot));
			}
			validate_tree_history_snapshot(current_tree);
		}

		void apply_history_document(history_document document)
		{
			cancel_tree_animation();
			clear_tree_find_result();
			m_key_input_edit_start.reset();
			m_random_count_edit_start.reset();
			m_current_document_state_id =
				document.current_document_state_id;
			m_saved_document_state_id =
				document.current_document_state_id;
			m_next_document_state_id =
				document.next_document_state_id;

			auto current_tree = std::move(document.current_tree);
			if (!current_tree.current_archive_member_path.empty())
			{
				restore_tree_history_state(current_tree);
			}
			else
			{
				m_active_tree = current_tree.active_tree;
				clear_current_tree_state();
			}
			restore_interface_state(document.current_interface);
			m_history = std::move(document.history);
			m_current_document_state_id =
				document.current_document_state_id;
			m_saved_document_state_id =
				document.current_document_state_id;
			m_next_document_state_id =
				document.next_document_state_id;
			m_tree_dirty = false;
			m_history_warning.clear();
			remember_active_tree_document();
		}

		void show_session_manifest(std::string_view entry_path)
		{
			const auto manifest = parse_session_manifest(m_session_entries);
			if (manifest.version == 0)
			{
				m_last_action =
					"В архиве нет манифеста рабочей области.";
				return;
			}
			m_last_action = std::format(
				"Манифест «{}»: деревьев {}, файлов истории {}. "
				"Рабочая область восстанавливается при открытии архива.",
				manifest.name.empty() ? entry_path : manifest.name,
				manifest.tree_paths.size(),
				manifest.history_paths.size());
		}

		void open_archive_tree(
			const tree_practicum::archive_entry& entry)
		{
			if (archive_extension(entry.path) != ".tree")
			{
				throw std::invalid_argument(
					"Workspace entry is not a .tree file");
			}

			if (m_current_archive_member_path != entry.path)
			{
				remember_active_tree_document();
			}
			sync_comparison_pane_with_active_tree(true);
			register_tree_document(entry.path);
			const auto& document = m_tree_documents.at(entry.path);
			m_current_tree_path = document.local_path;
			m_current_archive_member_path = entry.path;
			m_tree_tab_requested_path = entry.path;
			const auto separator = entry.path.find_last_of('/');
			const auto file_name = std::string_view(entry.path).substr(
				separator == std::string::npos ? 0 : separator + 1);
			const auto dot = file_name.find_last_of('.');
			const auto tree_name_length = dot == std::string_view::npos
				? file_name.size()
				: dot;
			const std::string tree_name = document.local_path.empty()
				? std::string(file_name.substr(0, tree_name_length))
				: path_to_utf8(document.local_path.stem());
			create_window_tree(
				tree_name,
				entry.contents);
			m_current_document_state_id =
				document.current_state_id;
			m_saved_document_state_id =
				document.saved_state_id;
			m_tree_dirty = m_current_document_state_id !=
				m_saved_document_state_id;
			update_session_metadata();
		}

		void clear_current_tree_state(bool update_metadata = true)
		{
			cancel_tree_animation();
			invalidate_tree_preview_cache();
			clear_tree_find_result();
			m_tree_canvas_context_menu_key.reset();
			m_tree_canvas_context_menu_node.reset();
			m_current_tree_name.clear();
			m_current_tree_serialization.clear();
			m_current_tree_path.clear();
			m_current_archive_member_path.clear();
			m_merge_source_archive_path.clear();
			m_split_result_member_paths = {};
			m_split_result_key_kind.reset();
			m_split_pivot_label.clear();
			m_loaded_visual_tree.reset();
			m_loaded_search_tree_key_kind.reset();
			m_loaded_search_tree_valid = false;
			m_loaded_tree_index = -1;
			m_tree_operation_error.clear();
			m_tree_parse_error.clear();
			m_tree_canvas_tree_index = -1;
			m_tree_canvas_view_initialized = false;
			m_tree_canvas_fit_requested = true;
			m_selected_archive_entry = -1;
			if (update_metadata)
			{
				update_session_metadata();
			}
		}

		void open_archive_entry(std::size_t index)
		{
			if (index >= m_session_entries.size())
			{
				throw std::out_of_range("Выбранная запись архива больше недоступна.");
			}

			clear_tree_find_result();
			const auto& entry = m_session_entries[index];
			const auto extension = archive_extension(entry.path);
			if (extension == ".tree")
			{
				open_archive_tree(entry);
			}
			else if (extension == ".htree")
			{
				request_history_restore(entry.path);
			}
			else if (extension == ".session")
			{
				show_session_manifest(entry.path);
			}
			else
			{
				m_last_action = std::format(
					"Выбран файл «{}» в виртуальной рабочей области.",
					entry.path);
			}
		}

		void save_tree_file(std::filesystem::path path)
		{
			if (m_current_tree_name.empty())
			{
				throw std::logic_error("Нет открытой сериализации дерева для сохранения.");
			}
			if (path.empty())
			{
				throw std::invalid_argument("Путь для сохранения дерева не может быть пустым.");
			}

			auto extension = file_extension(path);
			if (extension.empty())
			{
				path += ".tree";
			}
			else if (extension != ".tree")
			{
				throw std::invalid_argument("Для сохранения дерева используйте расширение .tree.");
			}

			const auto target_name = path_to_utf8(path.stem());
			const auto original_archive_path =
				m_current_archive_member_path;
			tree_practicum::write_tree_file(path, m_current_tree_serialization);
			if (!m_current_archive_member_path.empty() &&
				target_name == m_current_tree_name)
			{
				set_archive_entry(
					m_current_archive_member_path,
					m_current_tree_serialization);
				update_session_metadata();
			}
			else
			{
				m_current_archive_member_path = add_tree_to_session(
					target_name,
					m_current_tree_serialization);
				if (!original_archive_path.empty())
				{
					const auto original =
						m_tree_documents.find(original_archive_path);
					if (original != m_tree_documents.end())
					{
						original->second.current_state_id =
							original->second.saved_state_id;
					}
				}
			}
			m_current_tree_path = path;
			m_current_tree_name = target_name;
			register_tree_document(
				m_current_archive_member_path,
				path);
			auto& document = m_tree_documents.at(
				m_current_archive_member_path);
			document.current_state_id = m_current_document_state_id;
			document.saved_state_id = m_current_document_state_id;
			m_saved_document_state_id = m_current_document_state_id;
			m_tree_dirty = false;
			m_tree_tab_requested_path = m_current_archive_member_path;
			update_session_metadata();
			m_last_action = std::format(
				"Текущий файл сохранён: {}",
				path_to_utf8(path));
			if (any_tree_document_is_dirty())
			{
				m_last_action +=
					" В других вкладках остались изменения.";
			}
			logger.success("Tree saved: ", path_to_utf8(path));
			refresh_explorer();
		}

		void save_session_archive(std::filesystem::path path)
		{
			if (path.empty())
			{
				throw std::invalid_argument(
					"Путь для сохранения рабочей области не может быть пустым.");
			}
			auto extension = file_extension(path);
			if (extension.empty())
			{
				path += ".stree";
				extension = ".stree";
			}
			if (extension != ".stree" && extension != ".tar")
			{
				throw std::invalid_argument(
					"Для сохранения рабочей области используйте .stree или .tar.");
			}

			remember_active_tree_document();
			const auto target_session_name = path_to_utf8(path.stem());
			update_history_archive_entry();
			update_session_metadata(target_session_name);
			tree_practicum::write_tar_archive(path, m_session_entries);
			m_session_name = target_session_name;
			m_session_path = path;
			m_saved_document_state_id = m_current_document_state_id;
			for (auto& [archive_path, document] : m_tree_documents)
			{
				const auto* entry = find_archive_entry(
					m_session_entries,
					archive_path);
				if (entry)
				{
					document.saved_state_id =
						document.current_state_id;
				}
			}
			m_tree_dirty = m_current_document_state_id !=
				m_saved_document_state_id;
			m_workspace_structure_dirty = false;
			m_last_action = std::format(
				"Рабочая область сохранена в несжатый TAR: {}",
				path_to_utf8(path));
			logger.success("Workspace saved as uncompressed TAR: ", path_to_utf8(path));
			refresh_explorer();
		}

		[[nodiscard]] bool has_multiple_tree_entries() const
		{
			std::size_t tree_count = 0;
			for (const auto& entry : m_session_entries)
			{
				if (archive_extension(entry.path) == ".tree" &&
					++tree_count > 1)
				{
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] bool save_current()
		{
			if (!m_session_path.empty())
			{
				return perform_io([&]
				{
					save_session_archive(m_session_path);
				});
			}
			else if (m_workspace_structure_dirty)
			{
				request_file_dialog(
					file_dialog_mode::save_session,
					m_explorer_directory / "workspace.stree");
				return false;
			}
			else if (!m_current_tree_path.empty())
			{
				return perform_io([&]
				{
					save_tree_file(m_current_tree_path);
				});
			}
			else
			{
				const auto default_file = m_current_tree_name.empty()
					? m_explorer_directory / "workspace.stree"
					: m_explorer_directory /
						path_from_utf8(m_current_tree_name + ".tree");
				request_file_dialog(
					m_current_tree_name.empty()
						? file_dialog_mode::save_session
						: file_dialog_mode::save_tree,
					default_file);
				return false;
			}
		}

		void navigate_to_directory(const std::filesystem::path& directory)
		{
			std::error_code error;
			auto absolute = std::filesystem::absolute(directory, error);
			if (error)
			{
				throw std::filesystem::filesystem_error(
					"Cannot resolve explorer directory",
					directory,
					error);
			}
			if (!std::filesystem::is_directory(absolute, error) || error)
			{
				throw std::filesystem::filesystem_error(
					"Explorer path is not a readable directory",
					absolute,
					error ? error : std::make_error_code(std::errc::not_a_directory));
			}

			m_explorer_directory = absolute.lexically_normal();
			set_path_buffer(m_explorer_path, path_to_utf8(m_explorer_directory));
			m_selected_local_path.reset();
			refresh_explorer();
		}

		void refresh_explorer()
		{
			m_explorer_entries.clear();
			m_explorer_error.clear();

			std::error_code error;
			std::filesystem::directory_iterator iterator(
				m_explorer_directory,
				error);
			if (error)
			{
				m_explorer_error = "Не удалось прочитать папку: " + error.message();
				return;
			}

			const std::filesystem::directory_iterator end;
			while (iterator != end)
			{
				const auto entry = *iterator;
				std::error_code status_error;
				const bool is_directory = entry.is_directory(status_error);
				if (status_error)
				{
					m_explorer_error =
						"Не удалось прочитать тип элемента: " + status_error.message();
				}
				else if (is_directory ||
					file_extension(entry.path()) == ".tree" ||
					file_extension(entry.path()) == ".stree" ||
					file_extension(entry.path()) == ".tar")
				{
					m_explorer_entries.push_back({
						entry.path(),
						is_directory
					});
				}

				iterator.increment(error);
				if (error)
				{
					m_explorer_error =
						"Ошибка при чтении папки: " + error.message();
					break;
				}
			}

			std::sort(
				m_explorer_entries.begin(),
				m_explorer_entries.end(),
				[](const explorer_entry& left, const explorer_entry& right)
				{
					if (left.directory != right.directory)
					{
						return left.directory;
					}
					return lowercase_ascii(
						path_to_utf8(left.path.filename())) <
						lowercase_ascii(path_to_utf8(right.path.filename()));
				});
		}

		void draw_explorer(
			mngl::imgui_context::frame_scope& frame,
			ImVec2 position,
			ImVec2 size)
		{
			set_initial_window_rect(position, size);
			auto panel = frame.window("Проводник###TreePracticum.Explorer");
			if (!panel)
			{
				return;
			}

			ImGui::TextDisabled("РАБОЧАЯ ОБЛАСТЬ · ФАЙЛЫ НА ДИСКЕ");
			ImGui::TextWrapped(
				"%s",
				path_to_utf8(m_explorer_directory).c_str());
			if (ImGui::Button("Вверх"))
			{
				(void)perform_io([&]
				{
					navigate_to_directory(m_explorer_directory.parent_path());
				});
			}
			ImGui::SameLine();
			if (ImGui::Button("Обновить"))
			{
				refresh_explorer();
			}
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputTextWithHint(
				"##ExplorerPath",
				"Путь к папке · Enter для перехода",
				m_explorer_path.data(),
				m_explorer_path.size(),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				(void)perform_io([&]
				{
					navigate_to_directory(path_from_utf8(m_explorer_path.data()));
				});
			}
			ImGui::SetNextItemWidth(-1.0f);
			(void)ImGui::InputTextWithHint(
				"##ExplorerSearch",
				"Найти .tree, .stree или .tar...",
				m_explorer_filter.data(),
				m_explorer_filter.size());

			if (ImGui::Button("+ Новая область"))
			{
				begin_new_workspace();
			}
			ImGui::SameLine();
			if (ImGui::Button("Открыть..."))
			{
				request_file_dialog(file_dialog_mode::open);
			}
			ImGui::Separator();

			const std::string_view query{ m_explorer_filter.data() };
			if (!m_explorer_error.empty())
			{
				ImGui::TextWrapped(
					"%s",
					m_explorer_error.c_str());
			}
			if (!m_file_error.empty())
			{
				ImGui::TextColored(
					{ 1.0f, 0.42f, 0.38f, 1.0f },
					"%s",
					m_file_error.c_str());
			}

			ImGui::TextDisabled("ПАПКА");
			enum class explorer_context_action
			{
				open,
				rename,
				remove,
				save_as
			};
			std::optional<std::pair<
				explorer_context_action,
				std::filesystem::path>> local_context_action;
			enum class archive_context_action
			{
				open,
				rename,
				remove,
				save_as
			};
			std::optional<std::pair<
				archive_context_action,
				std::string>> archive_context_action_request;
			std::optional<explorer_entry> activated_entry;
			if (ImGui::BeginChild(
				"ExplorerDiskFiles",
				{ 0.0f, 230.0f },
				ImGuiChildFlags_Borders))
			{
				bool found_local_file = false;
				for (std::size_t i = 0; i < m_explorer_entries.size(); ++i)
				{
					const auto& entry = m_explorer_entries[i];
					const auto file_name = path_to_utf8(entry.path.filename());
					if (!contains_text(file_name, query))
					{
						continue;
					}
					found_local_file = true;
					const auto entry_path = entry.path;
					std::string label = entry.directory ? "[DIR]  " : "[FILE] ";
					label += file_name;

					ImGui::PushID(static_cast<int>(i));
					const bool selected =
						m_selected_local_path &&
						*m_selected_local_path == entry.path;
					const bool clicked = ImGui::Selectable(label.c_str(), selected);
					if (clicked)
					{
						m_selected_local_path = entry.path;
						m_selected_archive_entry = -1;
					}
					if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
					{
						m_selected_local_path = entry.path;
						m_selected_archive_entry = -1;
					}
					if (ImGui::BeginPopupContextItem(
						"ExplorerDiskFileContext"))
					{
						if (ImGui::MenuItem("Открыть"))
						{
							local_context_action = {
								explorer_context_action::open,
								entry_path
							};
						}
						if (!entry.directory)
						{
							ImGui::Separator();
							if (ImGui::MenuItem("Переименовать..."))
							{
								local_context_action = {
									explorer_context_action::rename,
									entry_path
								};
							}
							if (ImGui::MenuItem("Удалить..."))
							{
								local_context_action = {
									explorer_context_action::remove,
									entry_path
								};
							}
							if (ImGui::MenuItem("Сохранить как..."))
							{
								local_context_action = {
									explorer_context_action::save_as,
									entry_path
								};
							}
						}
						ImGui::EndPopup();
					}
					const bool double_clicked = clicked &&
						ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
					ImGui::PopID();

					if (double_clicked)
					{
						activated_entry = explorer_entry{
							entry_path,
							entry.directory
						};
						break;
					}
				}
				if (!found_local_file)
				{
					ImGui::TextDisabled(
						query.empty()
							? "В этой папке нет .tree, .stree или .tar."
							: "Совпадений нет.");
				}
			}
			ImGui::EndChild();
			if (activated_entry)
			{
				(void)perform_io([&]
				{
					if (activated_entry->directory)
					{
						navigate_to_directory(activated_entry->path);
					}
					else
					{
						open_file(activated_entry->path);
					}
				});
			}
			if (local_context_action)
			{
				const auto [action, path] = *local_context_action;
				switch (action)
				{
				case explorer_context_action::open:
					(void)perform_io([&]
					{
						std::error_code error;
						const bool is_directory =
							std::filesystem::is_directory(path, error);
						if (error)
						{
							throw std::filesystem::filesystem_error(
								"Cannot inspect selected explorer path",
								path,
								error);
						}
						if (is_directory)
						{
							navigate_to_directory(path);
						}
						else
						{
							open_file(path);
						}
					});
					break;
				case explorer_context_action::rename:
					request_local_file_rename(path);
					break;
				case explorer_context_action::remove:
					request_local_file_delete(path);
					break;
				case explorer_context_action::save_as:
					begin_local_file_save_as(path);
					break;
				}
			}

			if (m_selected_local_path)
			{
				const auto selected_path = *m_selected_local_path;
				if (ImGui::Button("Открыть выбранное"))
				{
					(void)perform_io([&]
					{
						std::error_code error;
						const bool is_directory =
							std::filesystem::is_directory(selected_path, error);
						if (error)
						{
							throw std::filesystem::filesystem_error(
								"Cannot inspect selected explorer path",
								selected_path,
								error);
						}
						if (is_directory)
						{
							navigate_to_directory(selected_path);
						}
						else
						{
							open_file(selected_path);
						}
					});
				}
				const auto extension = file_extension(selected_path);
				const bool selected_is_directory = std::any_of(
					m_explorer_entries.begin(),
					m_explorer_entries.end(),
					[&](const auto& entry)
					{
						return entry.path == selected_path && entry.directory;
					});
				if (!selected_is_directory && extension == ".tree")
				{
					ImGui::SameLine();
					if (ImGui::Button("Открыть во вкладке"))
					{
						(void)perform_io([&]
						{
							import_tree_file(selected_path);
						});
					}
				}
			}

			ImGui::Separator();
			ImGui::TextDisabled("ВИРТУАЛЬНАЯ РАБОЧАЯ ОБЛАСТЬ · TAR");
			if (ImGui::BeginChild(
				"ExplorerArchiveFiles",
				{ 0.0f, 0.0f },
				ImGuiChildFlags_Borders))
			{
				if (m_session_entries.empty())
				{
					ImGui::TextDisabled("В архиве пока нет файлов.");
				}
				else
				{
					for (std::size_t i = 0; i < m_session_entries.size(); ++i)
					{
						const auto& entry = m_session_entries[i];
						if (!contains_text(entry.path, query))
						{
							continue;
						}

						const auto extension = archive_extension(entry.path);
						const char* marker = extension == ".tree"
							? "[TREE] "
							: extension == ".htree"
								? "[HIST] "
								: extension == ".session"
									? "[SESSION] "
									: "[FILE] ";
						std::string label = marker;
						label += entry.path;
						ImGui::PushID(static_cast<int>(i));
						const bool clicked = ImGui::Selectable(
							label.c_str(),
							m_selected_archive_entry == static_cast<int>(i));
						if (clicked)
						{
							m_selected_archive_entry = static_cast<int>(i);
							m_selected_local_path.reset();
						}
						if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
						{
							m_selected_archive_entry = static_cast<int>(i);
							m_selected_local_path.reset();
						}
						if (ImGui::BeginPopupContextItem(
							"ExplorerArchiveFileContext"))
						{
							if (ImGui::MenuItem("Открыть"))
							{
								archive_context_action_request = {
									archive_context_action::open,
									entry.path
								};
							}
							ImGui::Separator();
							const bool protected_entry =
								extension == ".session" ||
								extension == ".htree";
							ImGui::BeginDisabled(protected_entry);
							if (ImGui::MenuItem("Переименовать..."))
							{
								archive_context_action_request = {
									archive_context_action::rename,
									entry.path
								};
							}
							if (ImGui::MenuItem("Удалить..."))
							{
								archive_context_action_request = {
									archive_context_action::remove,
									entry.path
								};
							}
							ImGui::EndDisabled();
							if (ImGui::MenuItem("Сохранить как..."))
							{
								archive_context_action_request = {
									archive_context_action::save_as,
									entry.path
								};
							}
							ImGui::EndPopup();
						}
						const bool double_clicked = clicked &&
							ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
						ImGui::PopID();

						if (double_clicked)
						{
							(void)perform_io([&]
							{
								open_archive_entry(i);
							});
						}
					}
				}
			}
			ImGui::EndChild();
			if (archive_context_action_request)
			{
				const auto [action, archive_path] =
					*archive_context_action_request;
				switch (action)
				{
				case archive_context_action::open:
					(void)perform_io([&]
					{
						const auto found = std::find_if(
							m_session_entries.begin(),
							m_session_entries.end(),
							[&](const auto& entry)
							{
								return entry.path == archive_path;
							});
						if (found == m_session_entries.end())
						{
							throw std::invalid_argument(
								"Запись рабочей области больше недоступна.");
						}
						open_archive_entry(static_cast<std::size_t>(
							std::distance(m_session_entries.begin(), found)));
					});
					break;
				case archive_context_action::rename:
					request_archive_entry_rename(archive_path);
					break;
				case archive_context_action::remove:
					request_archive_entry_delete(archive_path);
					break;
				case archive_context_action::save_as:
					begin_archive_entry_save_as(archive_path);
					break;
				}
			}

			if (m_selected_archive_entry >= 0 &&
				static_cast<std::size_t>(m_selected_archive_entry) <
					m_session_entries.size())
			{
				if (ImGui::Button("Открыть запись"))
				{
					(void)perform_io([&]
					{
						open_archive_entry(
							static_cast<std::size_t>(m_selected_archive_entry));
					});
				}
			}
		}

		void confirm_file_dialog()
		{
			const auto path = path_from_utf8(m_file_dialog_path.data());
			try
			{
				bool saved = false;
				switch (m_file_dialog_mode)
				{
				case file_dialog_mode::open:
					open_file(path);
					break;
				case file_dialog_mode::load_tree:
					if (file_extension(path) != ".tree")
					{
						throw std::invalid_argument(
							"Выберите файл с расширением .tree.");
					}
					open_file(path);
					break;
				case file_dialog_mode::save_tree:
					save_tree_file(path);
					saved = true;
					break;
				case file_dialog_mode::save_session:
					save_session_archive(path);
					saved = true;
					break;
				case file_dialog_mode::rename_local_file:
					rename_local_file(m_file_dialog_source_path, path);
					m_file_dialog_source_path.clear();
					saved = true;
					break;
				case file_dialog_mode::rename_archive_entry:
					rename_archive_entry(
						m_file_dialog_archive_source_path,
						std::string_view(m_file_dialog_path.data()));
					m_file_dialog_archive_source_path.clear();
					saved = true;
					break;
				case file_dialog_mode::save_copy:
					if (!m_file_dialog_copy_from_memory &&
						m_file_dialog_source_path.empty())
					{
						throw std::logic_error(
							"Не выбран исходный файл для копирования.");
					}
					{
						auto target = path;
						if (file_extension(target).empty())
						{
							target += m_file_dialog_copy_from_memory
								? path_from_utf8(m_file_dialog_required_extension)
								: m_file_dialog_source_path.extension();
						}
						const auto required_extension =
							m_file_dialog_required_extension.empty()
							? file_extension(m_file_dialog_source_path)
							: m_file_dialog_required_extension;
						if (required_extension != file_extension(target))
						{
							throw std::invalid_argument(
								"Сохраните копию с исходным расширением файла.");
						}
						if (m_file_dialog_copy_from_memory)
						{
							if (std::filesystem::exists(target))
							{
								throw std::filesystem::filesystem_error(
									"Файл с таким именем уже существует",
									target,
									std::make_error_code(std::errc::file_exists));
							}
							std::ofstream output(
								target,
								std::ios::binary | std::ios::trunc);
							if (!output)
							{
								throw std::filesystem::filesystem_error(
									"Не удалось создать файл",
									target,
									std::make_error_code(std::errc::io_error));
							}
							output.write(
								m_file_dialog_copy_contents.data(),
								static_cast<std::streamsize>(
									m_file_dialog_copy_contents.size()));
							if (!output)
							{
								throw std::filesystem::filesystem_error(
									"Не удалось записать файл",
									target,
									std::make_error_code(std::errc::io_error));
							}
						}
						else
						{
							std::filesystem::copy_file(
								m_file_dialog_source_path,
								target);
						}
						m_file_dialog_source_path.clear();
						m_file_dialog_copy_contents.clear();
						m_file_dialog_copy_from_memory = false;
						m_file_dialog_required_extension.clear();
						m_selected_local_path = target;
						m_last_action = std::format(
							"Копия сохранена: {}",
							path_to_utf8(target));
						refresh_explorer();
					}
					saved = true;
					break;
				}
				m_file_dialog_error.clear();
				m_file_dialog_open = false;
				ImGui::CloseCurrentPopup();
				if (saved &&
					m_waiting_for_save_before_workspace_action)
				{
					execute_pending_workspace_action();
				}
			}
			catch (const std::exception& error)
			{
				m_file_dialog_error = error.what();
				m_last_action = std::format(
					"Ошибка файловой операции: {}",
					error.what());
			}
		}

		void draw_file_dialog()
		{
			constexpr const char* popup_name =
				"Путь к файлу###TreePracticum.FileDialog";
			if (m_file_dialog_requested)
			{
				ImGui::OpenPopup(popup_name);
				m_file_dialog_requested = false;
			}

			if (!ImGui::BeginPopupModal(
				popup_name,
				&m_file_dialog_open,
				ImGuiWindowFlags_AlwaysAutoResize))
			{
				if (!m_file_dialog_open)
				{
					m_file_dialog_source_path.clear();
					m_file_dialog_archive_source_path.clear();
					m_file_dialog_copy_contents.clear();
					m_file_dialog_copy_from_memory = false;
					m_file_dialog_required_extension.clear();
				}
				if (!m_file_dialog_open &&
					m_waiting_for_save_before_workspace_action)
				{
					m_waiting_for_save_before_workspace_action = false;
					m_pending_workspace_action = {};
				}
				return;
			}

			const char* hint = m_file_dialog_mode == file_dialog_mode::open
				? "Путь к .tree, .stree или .tar"
				: m_file_dialog_mode == file_dialog_mode::load_tree
					? "Путь к файлу .tree"
					: m_file_dialog_mode == file_dialog_mode::save_tree
						? "Путь для сохранения .tree"
						: m_file_dialog_mode == file_dialog_mode::save_session
							? "Путь для сохранения .stree"
							: m_file_dialog_mode == file_dialog_mode::rename_local_file
								? "Новое имя файла в этой же папке"
								: m_file_dialog_mode == file_dialog_mode::rename_archive_entry
									? "Новое имя записи в той же папке архива"
								: "Путь для сохранения копии";
			ImGui::SetNextItemWidth(460.0f);
			const bool enter_pressed = ImGui::InputTextWithHint(
				"##TreePracticumFilePath",
				hint,
				m_file_dialog_path.data(),
				m_file_dialog_path.size(),
				ImGuiInputTextFlags_EnterReturnsTrue);
			if (enter_pressed)
			{
				confirm_file_dialog();
			}
			if (!m_file_dialog_error.empty())
			{
				ImGui::TextColored(
					{ 1.0f, 0.42f, 0.38f, 1.0f },
					"%s",
					m_file_dialog_error.c_str());
			}
			if (ImGui::Button(
				m_file_dialog_mode == file_dialog_mode::open ||
					m_file_dialog_mode == file_dialog_mode::load_tree
					? "Загрузить"
					: m_file_dialog_mode == file_dialog_mode::rename_local_file
						? "Переименовать"
						: m_file_dialog_mode == file_dialog_mode::rename_archive_entry
							? "Переименовать"
						: m_file_dialog_mode == file_dialog_mode::save_copy
							? "Сохранить копию"
							: "Сохранить"))
			{
				confirm_file_dialog();
			}
			ImGui::SameLine();
			if (ImGui::Button("Отмена"))
			{
				m_file_dialog_open = false;
				if (m_waiting_for_save_before_workspace_action)
				{
					m_waiting_for_save_before_workspace_action = false;
					m_pending_workspace_action = {};
				}
				m_file_dialog_source_path.clear();
				m_file_dialog_archive_source_path.clear();
				m_file_dialog_copy_contents.clear();
				m_file_dialog_copy_from_memory = false;
				m_file_dialog_required_extension.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		void draw_file_delete_dialog()
		{
			constexpr const char* popup_name =
				"Удаление файла###TreePracticum.DeleteLocalFile";
			if (m_file_delete_dialog_requested)
			{
				ImGui::OpenPopup(popup_name);
				m_file_delete_dialog_requested = false;
			}
			if (!ImGui::BeginPopupModal(
				popup_name,
				&m_file_delete_dialog_open,
				ImGuiWindowFlags_AlwaysAutoResize))
			{
				return;
			}

			if (m_pending_file_delete_path)
			{
				const std::string path = path_to_utf8(
					*m_pending_file_delete_path);
				ImGui::TextWrapped("Удалить файл с диска?\n%s", path.c_str());
			}
			else if (!m_pending_archive_delete_path.empty())
			{
				ImGui::TextWrapped(
					"Удалить запись из рабочей области?\n%s",
					m_pending_archive_delete_path.c_str());
			}
			if (!m_file_error.empty())
			{
				ImGui::TextColored(
					{ 1.0f, 0.42f, 0.38f, 1.0f },
					"%s",
					m_file_error.c_str());
			}
			if (ImGui::Button("Удалить") &&
				(m_pending_file_delete_path ||
					!m_pending_archive_delete_path.empty()))
			{
				const bool deleted = m_pending_delete_is_archive_entry
					? perform_io([&]
					{
						delete_archive_entry(m_pending_archive_delete_path);
					})
					: perform_io([&]
				{
					delete_local_file(*m_pending_file_delete_path);
					});
				if (deleted)
				{
					m_pending_file_delete_path.reset();
					m_pending_archive_delete_path.clear();
					m_pending_delete_is_archive_entry = false;
					m_file_delete_dialog_open = false;
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Отмена"))
			{
				m_pending_file_delete_path.reset();
				m_pending_archive_delete_path.clear();
				m_pending_delete_is_archive_entry = false;
				m_file_delete_dialog_open = false;
				m_file_error.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		void draw_unsaved_changes_dialog()
		{
			constexpr const char* popup_name =
				"Несохранённые изменения###TreePracticum.UnsavedChanges";
			if (m_unsaved_changes_dialog_requested)
			{
				ImGui::OpenPopup(popup_name);
				m_unsaved_changes_dialog_requested = false;
			}

			if (!ImGui::BeginPopupModal(
				popup_name,
				&m_unsaved_changes_dialog_open,
				ImGuiWindowFlags_AlwaysAutoResize))
			{
				if (!m_unsaved_changes_dialog_open &&
					m_pending_workspace_action.kind !=
						workspace_action_kind::none &&
					!m_waiting_for_save_before_workspace_action)
				{
					m_pending_workspace_action = {};
				}
				return;
			}

			ImGui::TextWrapped(
				"В одной или нескольких вкладках есть несохранённые изменения. "
				"Сохраните рабочую область перед продолжением?");
			if (!m_file_error.empty())
			{
				ImGui::TextColored(
					{ 1.0f, 0.42f, 0.38f, 1.0f },
					"%s",
					m_file_error.c_str());
			}

			if (ImGui::Button("Сохранить"))
			{
				m_file_error.clear();
				m_waiting_for_save_before_workspace_action = true;
				const bool saved = !m_session_path.empty()
					? save_current()
					: false;
				if (m_session_path.empty())
				{
					request_file_dialog(
						file_dialog_mode::save_session,
						m_explorer_directory / "workspace.stree");
				}
				if (saved)
				{
					ImGui::CloseCurrentPopup();
					execute_pending_workspace_action();
				}
				else if (m_file_dialog_open)
				{
					m_unsaved_changes_dialog_open = false;
					ImGui::CloseCurrentPopup();
				}
				else
				{
					m_waiting_for_save_before_workspace_action = false;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Не сохранять"))
			{
				ImGui::CloseCurrentPopup();
				execute_pending_workspace_action(true);
			}
			ImGui::SameLine();
			if (ImGui::Button("Отмена"))
			{
				ImGui::CloseCurrentPopup();
				m_pending_workspace_action = {};
				m_unsaved_changes_dialog_open = false;
			}
			ImGui::EndPopup();
			if (!m_unsaved_changes_dialog_open &&
				m_pending_workspace_action.kind !=
					workspace_action_kind::none &&
				!m_waiting_for_save_before_workspace_action)
			{
				m_pending_workspace_action = {};
			}
		}

		void draw_split_result_tabs()
		{
			if (m_split_result_member_paths[0].empty() &&
				m_split_result_member_paths[1].empty())
			{
				return;
			}

			if (!ImGui::BeginTabBar("SplitResultTabs"))
			{
				return;
			}

			const bool current_is_split_result = std::any_of(
				m_split_result_member_paths.begin(),
				m_split_result_member_paths.end(),
				[&](const auto& path)
				{
					return !path.empty() &&
						path == m_current_archive_member_path;
				});
			if (!current_is_split_result &&
				ImGui::BeginTabItem(
					"Другое дерево###SplitResultOther",
					nullptr,
					ImGuiTabItemFlags_SetSelected))
			{
				ImGui::TextDisabled(
					"%s",
					m_current_tree_name.empty()
						? "Текущее дерево не открыто."
						: "Открытое дерево не является результатом последнего split.");
				ImGui::EndTabItem();
			}

			for (std::size_t i = 0; i < m_split_result_member_paths.size(); ++i)
			{
				const auto& path = m_split_result_member_paths[i];
				if (path.empty())
				{
					continue;
				}
				const auto entry = std::find_if(
					m_session_entries.begin(),
					m_session_entries.end(),
					[&](const auto& candidate)
					{
						return candidate.path == path;
					});
				if (entry == m_session_entries.end())
				{
					continue;
				}

				const auto label = std::format(
					"{} {}###SplitResult{}",
					i == 0 ? "Ключи <" : "Ключи >=",
					m_split_pivot_label,
					i);
				const auto flags = path == m_current_archive_member_path
					? ImGuiTabItemFlags_SetSelected
					: ImGuiTabItemFlags_None;
				if (ImGui::BeginTabItem(label.c_str(), nullptr, flags))
				{
					const bool tab_clicked = ImGui::IsItemClicked();
					if (tab_clicked)
					{
						open_archive_entry(static_cast<std::size_t>(
							std::distance(m_session_entries.begin(), entry)));
						if (m_loaded_visual_tree &&
							m_loaded_visual_tree->nodes.empty())
						{
							m_loaded_search_tree_key_kind =
								m_split_result_key_kind;
						}
					}
					ImGui::TextDisabled(
						"%s",
						path == m_current_archive_member_path
							? path.c_str()
							: "Нажмите вкладку, чтобы открыть результат.");
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}

		void draw_tools(
			mngl::imgui_context::frame_scope& frame,
			ImVec2 viewport_position,
			ImVec2 viewport_size)
		{
			const float panel_width = std::min(
				std::clamp(viewport_size.x * 0.34f, 360.0f, 520.0f),
				std::max(240.0f, viewport_size.x - 32.0f));
			const float panel_height = std::min(
				440.0f,
				std::max(180.0f, viewport_size.y - 32.0f));
			const float min_panel_x = viewport_position.x + 16.0f;
			const float min_panel_y = viewport_position.y + 16.0f;
			const float max_panel_x = std::max(
				min_panel_x,
				viewport_position.x + viewport_size.x - panel_width - 16.0f);
			const float max_panel_y = std::max(
				min_panel_y,
				viewport_position.y + viewport_size.y - panel_height - 16.0f);
			const float preferred_panel_x = viewport_position.x + std::max(
				16.0f,
				viewport_size.x * 0.27f);
			const float preferred_panel_y = viewport_position.y + std::max(
				16.0f,
				viewport_size.y * 0.075f);
			const ImVec2 panel_position{
				std::clamp(preferred_panel_x, min_panel_x, max_panel_x),
				std::clamp(preferred_panel_y, min_panel_y, max_panel_y)
			};
			set_initial_window_rect(
				panel_position,
				{ panel_width, panel_height });

			ImGui::PushStyleColor(
				ImGuiCol_WindowBg,
				{ 0.055f, 0.075f, 0.10f, 0.98f });
			ImGui::PushStyleColor(
				ImGuiCol_Border,
				{ 0.20f, 0.52f, 0.70f, 0.95f });
			ImGui::PushStyleColor(
				ImGuiCol_TitleBg,
				{ 0.07f, 0.15f, 0.21f, 1.0f });
			ImGui::PushStyleColor(
				ImGuiCol_TitleBgActive,
				{ 0.08f, 0.24f, 0.33f, 1.0f });
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 14.0f, 12.0f });
			struct style_guard
			{
				~style_guard()
				{
					ImGui::PopStyleVar(2);
					ImGui::PopStyleColor(4);
				}
			} styles;

			auto panel = frame.window(
				"Операции с деревом###TreePracticum.Operations",
				ImGuiWindowFlags_NoDocking);
			if (!panel)
			{
				commit_pending_input_edits();
				return;
			}

			ImGui::PushStyleColor(
				ImGuiCol_Text,
				{ 0.40f, 0.82f, 0.98f, 1.0f });
			ImGui::TextUnformatted("ОПЕРАЦИИ С ДЕРЕВОМ");
			ImGui::PopStyleColor();
			ImGui::Text("Рабочая область: %s", m_session_name.c_str());
			ImGui::TextDisabled(
				(m_tree_dirty || any_tree_document_is_dirty())
					? "Есть несохранённые изменения · Ctrl+S"
					: m_loaded_visual_tree.has_value() &&
						m_loaded_tree_index == m_active_tree
					? can_insert_into_selected_search_tree()
						? "Загруженное дерево · операции int64_t/double доступны"
					: "Операции доступны только для int64_t и double"
						: m_current_tree_name.empty() &&
							!m_show_demo_trees
						? "Пустая рабочая область · создайте дерево или откройте .tree"
						: "Демо-отрисовка · откройте .tree для операций");
			if (!m_tree_parse_error.empty())
			{
				ImGui::TextColored(
					{ 1.0f, 0.42f, 0.38f, 1.0f },
					"%s",
					m_tree_parse_error.c_str());
			}
			if (!m_tree_operation_error.empty())
			{
				ImGui::TextColored(
					{ 1.0f, 0.42f, 0.38f, 1.0f },
					"%s",
					m_tree_operation_error.c_str());
			}

			if (ImGui::Button("Новая рабочая область"))
			{
				begin_new_workspace();
			}
			ImGui::SameLine();
			if (ImGui::Button("Новое дерево"))
			{
				(void)perform_io([&]
				{
					create_new_tree();
				});
			}
			ImGui::SameLine();
			if (ImGui::Button("Открыть .tree..."))
			{
				request_tree_file_picker();
			}
			ImGui::SameLine();
			ImGui::TextDisabled("или перетащите файл в это окно");

			ImGui::NewLine();
			if (m_tree_comparison_mode)
			{
				ImGui::TextDisabled(
					"Два дерева рядом · операции применяются к активной панели");
			}
			else
			{
				const std::string current_file_label =
					m_current_tree_path.empty()
					? m_current_tree_name
					: path_to_utf8(m_current_tree_path);
				ImGui::TextDisabled(
					"Файл: %s",
					current_file_label.c_str());
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(
				!m_tree_comparison_mode &&
				workspace_tree_count() < 2);
			const bool comparison_mode_clicked = ImGui::Button(
				m_tree_comparison_mode
					? "Одинарный вид"
					: "Два дерева рядом");
			ImGui::EndDisabled();
			if (comparison_mode_clicked)
			{
				if (m_tree_comparison_mode)
				{
					m_tree_comparison_mode = false;
					m_tree_canvas_fit_requested = true;
				}
				else
				{
					(void)perform_io([&]
					{
						begin_tree_comparison();
					});
				}
			}
			if (!m_tree_comparison_mode &&
				workspace_tree_count() < 2)
			{
				ImGui::SameLine();
				ImGui::TextDisabled(
					"Добавьте ещё один .tree для сравнения.");
			}
			ImGui::NewLine();

			const bool can_operate =
				can_insert_into_selected_search_tree();
			ImGui::PushStyleColor(
				ImGuiCol_Button,
				{ 0.10f, 0.29f, 0.39f, 1.0f });
			ImGui::PushStyleColor(
				ImGuiCol_ButtonHovered,
				{ 0.13f, 0.40f, 0.53f, 1.0f });
			ImGui::PushStyleColor(
				ImGuiCol_ButtonActive,
				{ 0.08f, 0.23f, 0.32f, 1.0f });
			ImGui::BeginDisabled(!can_operate);
			if (ImGui::BeginTable(
				"TreeOperations",
				2,
				ImGuiTableFlags_SizingStretchSame))
			{
				const ImVec2 operation_button_size{
					-std::numeric_limits<float>::min(),
					0.0f
				};
				ImGui::TableNextColumn();
				if (ImGui::Button("Найти [F]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::find);
				}
				ImGui::TableNextColumn();
				if (ImGui::Button("Вставить [A / Ins]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::insert);
				}
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				if (ImGui::Button("Удалить [D / Del]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::erase);
				}
				ImGui::TableNextColumn();
				if (ImGui::Button("Split [C]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::split);
				}
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				if (ImGui::Button("Случайная вставка [R]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::random_insert);
				}
				ImGui::TableNextColumn();
				if (ImGui::Button("Объединить [M]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::merge);
				}
				ImGui::EndTable();
			}
			ImGui::EndDisabled();
			ImGui::PopStyleColor(3);
			ImGui::TextDisabled(
				"Параметры операций открываются в отдельном окне.");

			if (m_tree_find_result_valid &&
				m_tree_find_tree_index == m_active_tree)
			{
				ImGui::PushStyleColor(
					ImGuiCol_Text,
					m_tree_find_found
						? ImVec4{ 0.43f, 0.86f, 0.58f, 1.0f }
						: ImVec4{ 1.0f, 0.70f, 0.38f, 1.0f });
				ImGui::TextWrapped("%s", m_tree_find_status.c_str());
				ImGui::PopStyleColor();
			}

			ImGui::Separator();
			if (ImGui::Button("Вписать дерево"))
			{
				if (m_tree_comparison_mode)
				{
					m_tree_comparison_panes[
						static_cast<std::size_t>(std::clamp(
							m_active_comparison_pane,
							0,
							static_cast<int>(
								m_tree_comparison_panes.size() - 1))) ]
						.canvas.fit_requested = true;
				}
				else
				{
					m_tree_canvas_fit_requested = true;
				}
			}
			ImGui::SameLine();
			ImGui::TextDisabled("Колесо: масштаб · средняя кнопка: сдвиг");
			ImGui::Separator();
		}

		void draw_tool_parameters(
			mngl::imgui_context::frame_scope& frame)
		{
			if (!m_active_tree_tool)
			{
				return;
			}

			if (const ImGuiViewport* viewport = ImGui::GetMainViewport())
			{
				const ImVec2 size{
					std::min(520.0f, viewport->WorkSize.x * 0.5f),
					std::min(300.0f, viewport->WorkSize.y * 0.5f)
				};
				set_initial_window_rect(
					{
						viewport->WorkPos.x +
							(viewport->WorkSize.x - size.x) * 0.5f,
						viewport->WorkPos.y +
							(viewport->WorkSize.y - size.y) * 0.5f
					},
					size);
			}

			auto panel = frame.window(
				"Параметры инструмента###TreePracticum.ToolParameters");
			if (!panel)
			{
				commit_pending_input_edits();
				return;
			}

			const tree_tool_operation operation = *m_active_tree_tool;
			const bool can_operate =
				can_insert_into_selected_search_tree();
			const char* operation_name = "";
			const char* action_label = "Выполнить";
			bool submit_on_enter = false;
			switch (operation)
			{
			case tree_tool_operation::find:
				operation_name = "Найти ключ";
				action_label = "Найти";
				break;
			case tree_tool_operation::insert:
				operation_name = "Вставить ключ";
				action_label = "Вставить";
				break;
			case tree_tool_operation::random_insert:
				operation_name = "Случайная вставка";
				action_label = "Добавить";
				break;
			case tree_tool_operation::erase:
				operation_name = "Удалить ключ";
				action_label = "Удалить";
				break;
			case tree_tool_operation::merge:
				operation_name = "Объединить деревья";
				action_label = "Объединить";
				break;
			case tree_tool_operation::split:
				operation_name = "Разделить дерево";
				action_label = "Разделить";
				break;
			}
			ImGui::TextColored(
				{ 0.53f, 0.76f, 0.96f, 1.0f },
				"%s",
				operation_name);
			ImGui::Separator();

			const auto draw_key_parameter = [&](const char* label)
			{
				ImGui::SetNextItemWidth(300.0f);
				const auto key_state_before_widget =
					current_interface_state();
				const bool submitted = ImGui::InputTextWithHint(
					label,
					"ключ int64_t или double",
					m_key_input.data(),
					m_key_input.size(),
					ImGuiInputTextFlags_EnterReturnsTrue);
				track_input_edit(
					m_key_input_edit_start,
					key_state_before_widget,
					ImGui::IsItemActivated(),
					ImGui::IsItemDeactivatedAfterEdit(),
					ImGui::IsItemDeactivated());
				return submitted;
			};

			switch (operation)
			{
			case tree_tool_operation::find:
				submit_on_enter = draw_key_parameter("Ключ");
				break;
			case tree_tool_operation::insert:
				submit_on_enter = draw_key_parameter("Новый ключ");
				break;
			case tree_tool_operation::erase:
				submit_on_enter = draw_key_parameter("Ключ для удаления");
				break;
			case tree_tool_operation::split:
				submit_on_enter = draw_key_parameter("Опорный ключ");
				break;
			case tree_tool_operation::random_insert:
			{
				ImGui::SetNextItemWidth(180.0f);
				const auto random_count_state_before_widget =
					current_interface_state();
				ImU64 random_count_input =
					static_cast<ImU64>(m_random_count);
				(void)ImGui::InputScalar(
					"Количество ключей",
					ImGuiDataType_U64,
					&random_count_input);
				m_random_count =
					static_cast<std::uint64_t>(random_count_input);
				track_input_edit(
					m_random_count_edit_start,
					random_count_state_before_widget,
					ImGui::IsItemActivated(),
					ImGui::IsItemDeactivatedAfterEdit(),
					ImGui::IsItemDeactivated());
				break;
			}
			case tree_tool_operation::merge:
				draw_merge_source_selector();
				break;
			}

			if (!m_tree_operation_error.empty())
			{
				ImGui::TextColored(
					{ 1.0f, 0.42f, 0.38f, 1.0f },
					"%s",
					m_tree_operation_error.c_str());
			}
			if (!can_operate)
			{
				ImGui::TextDisabled(
					"Откройте дерево с ключами int64_t или double.");
			}

			const bool can_apply = operation == tree_tool_operation::merge
				? can_merge_selected_search_tree()
				: can_operate;
			ImGui::BeginDisabled(!can_apply);
			const bool apply_clicked = ImGui::Button(action_label);
			ImGui::EndDisabled();
			ImGui::SameLine();
			const bool cancel_clicked = ImGui::Button("Отмена");
			if (apply_clicked || (submit_on_enter && can_apply))
			{
				commit_pending_input_edits();
				m_tree_operation_error.clear();
				perform_tree_tool(operation);
				if (m_tree_operation_error.empty())
				{
					m_active_tree_tool.reset();
				}
			}
			else if (cancel_clicked)
			{
				commit_pending_input_edits();
				m_active_tree_tool.reset();
			}
		}

		void draw_workspace(
			mngl::imgui_context::frame_scope& frame,
			ImVec2 position,
			ImVec2 size)
		{
			set_initial_window_rect(position, size);
			auto panel = frame.window("Дерево###TreePracticum.Workspace");
			if (!panel)
			{
				return;
			}
			draw_tree_file_tabs();
			if (m_tree_comparison_mode)
			{
				draw_comparison_tree_panels();
			}
			else if (m_current_tree_name.empty())
			{
				ImGui::TextDisabled(
					"Откройте файл .tree или создайте дерево, чтобы начать работу.");
			}
			else
			{
				draw_tree_preview(
					static_cast<std::size_t>(m_active_tree));
			}
		}

		void draw_tree_file_tabs()
		{
			if (m_open_tree_tabs.empty())
			{
				return;
			}

			std::vector<std::string> closed_tabs;
			if (ImGui::BeginTabBar(
				"TreeFileTabs",
				ImGuiTabBarFlags_Reorderable |
					ImGuiTabBarFlags_AutoSelectNewTabs))
			{
				for (const auto& archive_path : m_open_tree_tabs)
				{
					const auto* entry = find_archive_entry(
						m_session_entries,
						archive_path);
					if (!entry || archive_extension(entry->path) != ".tree")
					{
						continue;
					}

					const auto separator = entry->path.find_last_of('/');
					const auto archive_file_name = std::string_view(entry->path).substr(
						separator == std::string::npos ? 0 : separator + 1);
					std::string file_name(archive_file_name);
					const auto document = m_tree_documents.find(entry->path);
					if (document != m_tree_documents.end() &&
						!document->second.display_name.empty())
					{
						file_name = document->second.display_name;
					}
				else if (document != m_tree_documents.end() &&
					!document->second.local_path.empty())
					{
						file_name = path_to_utf8(
							document->second.local_path.filename());
					}
					const bool dirty = tree_document_is_dirty(entry->path);
					const std::string label = std::format(
						"{}{}###TreeFile.{}",
						file_name,
						dirty ? " *" : "",
						entry->path);
					bool tab_open = true;
					const auto flags = entry->path ==
						m_tree_tab_requested_path
						? ImGuiTabItemFlags_SetSelected
						: ImGuiTabItemFlags_None;
					if (ImGui::BeginTabItem(
						label.c_str(),
						&tab_open,
						flags))
					{
						if (m_current_archive_member_path != entry->path)
						{
							open_archive_tree(*entry);
							m_tree_comparison_mode = false;
							m_selected_archive_entry = -1;
						}
						if (m_tree_tab_requested_path == entry->path)
						{
							m_tree_tab_requested_path.clear();
						}
						ImGui::EndTabItem();
					}
					if (!tab_open)
					{
						closed_tabs.push_back(entry->path);
					}
				}
				ImGui::EndTabBar();
			}

			if (closed_tabs.empty())
			{
				return;
			}

			const bool current_tab_closed = std::find(
				closed_tabs.begin(),
				closed_tabs.end(),
				m_current_archive_member_path) != closed_tabs.end();
			m_open_tree_tabs.erase(
				std::remove_if(
					m_open_tree_tabs.begin(),
					m_open_tree_tabs.end(),
					[&](const auto& path)
					{
						return std::find(
							closed_tabs.begin(),
							closed_tabs.end(),
							path) != closed_tabs.end();
					}),
				m_open_tree_tabs.end());
			m_last_action =
				"Вкладка закрыта. Файл остаётся в рабочей области; несохранённые изменения доступны при повторном открытии.";

			if (!current_tab_closed)
			{
				return;
			}
			if (!m_open_tree_tabs.empty())
			{
				if (const auto* next = find_archive_entry(
					m_session_entries,
					m_open_tree_tabs.back()))
				{
					open_archive_tree(*next);
				}
			}
			else
			{
				remember_active_tree_document();
				clear_current_tree_state();
				m_current_document_state_id = 0;
				m_saved_document_state_id = 0;
				m_tree_dirty = false;
				m_tree_tab_requested_path.clear();
			}
		}

		[[nodiscard]] static visual_key make_text_key(
			std::string_view value,
			std::string_view detail = {})
		{
			return {
				visual_key::value_type{
					std::in_place_type<std::string>,
					value },
				std::string(detail),
				false
			};
		}

		[[nodiscard]] static visual_key make_double_key(
			double value,
			std::string_view detail = {})
		{
			return {
				visual_key::value_type{
					std::in_place_type<double>,
					value },
				std::string(detail),
				false
			};
		}

		[[nodiscard]] static visual_key make_integer_key(
			std::int64_t value,
			std::string_view detail = {})
		{
			return {
				visual_key::value_type{
					std::in_place_type<std::int64_t>,
					value },
				std::string(detail),
				false
			};
		}

		[[nodiscard]] static visual_key make_big_integer_key(
			std::string_view decimal,
			std::string_view detail = {})
		{
			return {
				visual_key::value_type{
					std::in_place_type<visual_key::big_integer_text>,
					visual_key::big_integer_text{
						std::string(decimal) } },
				std::string(detail),
				false
			};
		}

		[[nodiscard]] static std::string full_visual_value(
			const visual_key::value_type& value)
		{
			return std::visit(
				[](const auto& item) -> std::string
				{
					using value_type = std::remove_cvref_t<decltype(item)>;
					if constexpr (std::is_same_v<value_type, std::string>)
					{
						return item;
					}
					else if constexpr (
						std::is_same_v<value_type, visual_key::big_integer_text>)
					{
						return std::string(item.decimal);
					}
					else
					{
						std::array<char, 128> buffer{};
						const auto conversion = [&]
						{
							if constexpr (std::is_same_v<value_type, double>)
							{
								return std::to_chars(
									buffer.data(),
									buffer.data() + buffer.size(),
									item,
									std::chars_format::general);
							}
							else
							{
								return std::to_chars(
									buffer.data(),
									buffer.data() + buffer.size(),
									item);
							}
						}();
						if (conversion.ec != std::errc{})
						{
							throw std::runtime_error(
								"Unable to format a tree key for display");
						}
						return {
							buffer.data(),
							static_cast<std::size_t>(conversion.ptr - buffer.data())
						};
					}
				},
				value);
		}

		[[nodiscard]] static float visual_text_width(
			std::string_view text) noexcept
		{
			return ImGui::CalcTextSize(
				text.data(),
				text.data() + text.size()).x;
		}

		[[nodiscard]] static std::size_t next_utf8_boundary(
			std::string_view text,
			std::size_t offset) noexcept
		{
			offset = std::min(offset + 1, text.size());
			while (offset < text.size() &&
				(static_cast<unsigned char>(text[offset]) & 0xC0u) == 0x80u)
			{
				++offset;
			}
			return offset;
		}

		[[nodiscard]] static std::string clip_text_to_width(
			std::string_view text,
			float maximum_width)
		{
			constexpr std::string_view ellipsis = "...";
			constexpr std::size_t maximum_codepoints_to_scan = 256;
			if (text.size() <= maximum_codepoints_to_scan * 4 &&
				visual_text_width(text) <= maximum_width)
			{
				return std::string(text);
			}
			if (visual_text_width(ellipsis) > maximum_width)
			{
				return {};
			}

			std::size_t visible_end = 0;
			std::size_t scanned_codepoints = 0;
			while (visible_end < text.size() &&
				scanned_codepoints < maximum_codepoints_to_scan)
			{
				const std::size_t next =
					next_utf8_boundary(text, visible_end);
				if (visual_text_width(text.substr(0, next)) +
					visual_text_width(ellipsis) > maximum_width)
				{
					break;
				}
				visible_end = next;
				++scanned_codepoints;
			}

			if (visible_end == text.size())
			{
				return std::string(text);
			}
			std::string result(text.substr(0, visible_end));
			result += ellipsis;
			return result;
		}

		[[nodiscard]] static std::string format_integer_scientific(
			std::string_view decimal,
			float maximum_width)
		{
			bool negative = decimal.starts_with('-');
			if (negative)
			{
				decimal.remove_prefix(1);
			}
			while (decimal.size() > 1 && decimal.front() == '0')
			{
				decimal.remove_prefix(1);
			}
			if (decimal.empty() ||
				std::all_of(decimal.begin(), decimal.end(), [](char value)
				{
					return value == '0';
				}))
			{
				return "0";
			}

			const std::string exponent =
				std::to_string(decimal.size() - 1);
			const std::size_t max_significant_digits =
				std::min<std::size_t>(7, decimal.size());
			for (std::size_t significant_digits = max_significant_digits;
				significant_digits > 0;
				--significant_digits)
			{
				std::string candidate;
				if (negative)
				{
					candidate.push_back('-');
				}
				candidate.push_back(decimal.front());
				if (significant_digits > 1)
				{
					candidate.push_back('.');
					candidate.append(
						decimal.substr(1, significant_digits - 1));
				}
				candidate.push_back('e');
				candidate += exponent;
				if (visual_text_width(candidate) <= maximum_width)
				{
					return candidate;
				}
			}

			std::string minimal;
			if (negative)
			{
				minimal.push_back('-');
			}
			minimal.push_back(decimal.front());
			minimal.push_back('e');
			minimal += exponent;
			return minimal;
		}

		[[nodiscard]] static std::string trim_scientific_zeros(
			std::string value)
		{
			auto exponent = value.find_first_of("eE");
			if (exponent == std::string::npos)
			{
				return value;
			}
			while (exponent > 0 && value[exponent - 1] == '0')
			{
				value.erase(exponent - 1, 1);
				--exponent;
			}
			if (exponent > 0 && value[exponent - 1] == '.')
			{
				value.erase(exponent - 1, 1);
			}
			const auto sign = value.find_first_of("eE") + 1;
			if (sign < value.size() && value[sign] == '+')
			{
				value.erase(sign, 1);
			}
			return value;
		}

		[[nodiscard]] static std::string format_double_scientific(
			double value,
			float maximum_width)
		{
			std::string smallest;
			for (int precision = 6; precision >= 0; --precision)
			{
				std::array<char, 128> buffer{};
				const auto conversion = std::to_chars(
					buffer.data(),
					buffer.data() + buffer.size(),
					value,
					std::chars_format::scientific,
					precision);
				if (conversion.ec != std::errc{})
				{
					throw std::runtime_error(
						"Unable to format a floating-point tree key");
				}
				auto candidate = trim_scientific_zeros({
					buffer.data(),
					static_cast<std::size_t>(conversion.ptr - buffer.data())
				});
				smallest = candidate;
				if (visual_text_width(candidate) <= maximum_width)
				{
					return candidate;
				}
			}
			return smallest;
		}

		[[nodiscard]] static std::string format_visual_value(
			const visual_key::value_type& value,
			float maximum_width)
		{
			if (const auto* text = std::get_if<std::string>(&value))
			{
				return clip_text_to_width(*text, maximum_width);
			}

			if (const auto* big_integer =
				std::get_if<visual_key::big_integer_text>(&value))
			{
				if (big_integer->decimal.size() <= 32 &&
					visual_text_width(big_integer->decimal) <= maximum_width)
				{
					return std::string(big_integer->decimal);
				}
				return format_integer_scientific(
					big_integer->decimal,
					maximum_width);
			}

			const auto full_value = full_visual_value(value);
			if (visual_text_width(full_value) <= maximum_width)
			{
				return full_value;
			}
			if (const auto* floating = std::get_if<double>(&value))
			{
				return format_double_scientific(*floating, maximum_width);
			}
			if (const auto* integer = std::get_if<std::int64_t>(&value))
			{
				return format_integer_scientific(
					std::to_string(*integer),
					maximum_width);
			}
			return full_value;
		}

		[[nodiscard]] static std::string_view visual_value_type(
			const visual_key::value_type& value) noexcept
		{
			if (std::holds_alternative<std::string>(value))
			{
				return "string";
			}
			if (std::holds_alternative<double>(value))
			{
				return "double";
			}
			if (std::holds_alternative<std::int64_t>(value))
			{
				return "int64_t";
			}
			return "boost::cpp_int";
		}

		struct tree_serialization_parser
		{
			static constexpr std::size_t maximum_node_count = 1'000'000;
			static constexpr std::size_t maximum_depth = 1'024;

			std::string_view source;
			std::size_t position = 0;
			std::size_t tree_index = 0;
			bool has_metadata = false;
			visual_tree result;

			explicit tree_serialization_parser(std::string_view text)
				: source(text)
			{
			}

			[[nodiscard]] visual_tree parse(std::size_t& parsed_tree_index)
			{
				skip_whitespace();
				if (consume_prefix("b_plus_tree[") ||
					consume_prefix("b\\_plus\\_tree["))
				{
					tree_index = 5;
					result.b_plus = true;
					parse_fanout();
					expect_character('{');
					result.root = parse_b_plus_node(0);
					expect_character('}');
				}
				else if (consume_prefix("bst{"))
				{
					tree_index = 0;
					result.root = parse_binary_node(0);
					expect_character('}');
				}
				else if (consume_prefix("avl{"))
				{
					tree_index = 1;
					has_metadata = true;
					result.root = parse_binary_node(0);
					expect_character('}');
				}
				else if (consume_prefix("rb_tree{") ||
					consume_prefix("rb\\_tree{"))
				{
					tree_index = 2;
					has_metadata = true;
					result.root = parse_binary_node(0);
					expect_character('}');
				}
				else if (consume_prefix("splay{"))
				{
					tree_index = 3;
					result.root = parse_binary_node(0);
					expect_character('}');
				}
				else if (consume_prefix("treap{"))
				{
					tree_index = 4;
					has_metadata = true;
					result.root = parse_binary_node(0);
					expect_character('}');
				}
				else
				{
					fail("неизвестный тип дерева");
				}

				skip_whitespace();
				if (position != source.size())
				{
					fail("лишние символы после дерева");
				}

				infer_key_types();
				if (result.b_plus && result.root != no_visual_node)
				{
					link_b_plus_leaves();
				}
				parsed_tree_index = tree_index;
				return std::move(result);
			}

		private:
			[[noreturn]] void fail(std::string_view message) const
			{
				throw tree_serialization_error(std::format(
					"Ошибка сериализации .tree на байте {}: {}",
					position,
					message));
			}

			void skip_whitespace() noexcept
			{
				while (position < source.size())
				{
					const char value = source[position];
					if (value != ' ' && value != '\t' &&
						value != '\r' && value != '\n')
					{
						break;
					}
					++position;
				}
			}

			[[nodiscard]] bool consume_prefix(std::string_view value) noexcept
			{
				if (!source.substr(position).starts_with(value))
				{
					return false;
				}
				position += value.size();
				return true;
			}

			[[nodiscard]] bool consume_character(char value) noexcept
			{
				if (position >= source.size() || source[position] != value)
				{
					return false;
				}
				++position;
				return true;
			}

			void expect_character(char value)
			{
				skip_whitespace();
				if (!consume_character(value))
				{
					fail(std::format("ожидался символ '{}'", value));
				}
			}

			void expect_double_comma()
			{
				skip_whitespace();
				if (!consume_prefix(",,"))
				{
					fail("ожидался разделитель ',,'");
				}
			}

			[[nodiscard]] std::string read_quoted_string()
			{
				if (!consume_character('"'))
				{
					fail("ожидалась строка в кавычках");
				}

				std::string value;
				while (position < source.size())
				{
					const char current = source[position++];
					if (current == '"')
					{
						return value;
					}
					if (current != '\\')
					{
						value.push_back(current);
						continue;
					}
					if (position >= source.size())
					{
						fail("незавершённая escape-последовательность");
					}

					const char escaped = source[position++];
					switch (escaped)
					{
					case '"':
					case '\\':
						value.push_back(escaped);
						break;
					case 'n':
						value.push_back('\n');
						break;
					case 'r':
						value.push_back('\r');
						break;
					case 't':
						value.push_back('\t');
						break;
					case '0':
						value.push_back('\0');
						break;
					default:
						value.push_back('\\');
						value.push_back(escaped);
						break;
					}
				}
				fail("строка не закрыта кавычкой");
			}

			[[nodiscard]] std::string read_binary_key(bool stop_at_pipe)
			{
				if (position < source.size() && source[position] == '"')
				{
					return read_quoted_string();
				}

				const std::size_t start = position;
				while (position < source.size())
				{
					if (source.substr(position).starts_with(",,") ||
						(stop_at_pipe && source[position] == '|'))
					{
						break;
					}
					++position;
				}
				return std::string(source.substr(start, position - start));
			}

			[[nodiscard]] std::string read_until_double_comma()
			{
				const std::size_t start = position;
				while (position < source.size() &&
					!source.substr(position).starts_with(",,"))
				{
					++position;
				}
				if (position == source.size())
				{
					fail("метаданные не завершены разделителем ',,'");
				}
				std::string value(source.substr(start, position - start));
				position += 2;
				return value;
			}

			[[nodiscard]] visual_key parse_binary_key()
			{
				skip_whitespace();
				const bool quoted =
					position < source.size() && source[position] == '"';
				std::string value = read_binary_key(has_metadata);
				std::string detail;
				if (has_metadata && consume_character('|'))
				{
					detail = read_until_double_comma();
				}
				else
				{
					expect_double_comma();
				}

				return {
					visual_key::value_type{
						std::in_place_type<std::string>,
						std::move(value) },
					std::move(detail),
					!quoted
				};
			}

			[[nodiscard]] std::size_t append_node(visual_node node)
			{
				if (result.nodes.size() >= maximum_node_count)
				{
					fail("превышен лимит в 1 000 000 узлов");
				}
				const std::size_t index = result.nodes.size();
				result.nodes.push_back(std::move(node));
				return index;
			}

			[[nodiscard]] std::size_t parse_binary_node(std::size_t depth)
			{
				skip_whitespace();
				if (consume_character('#'))
				{
					return no_visual_node;
				}
				if (depth >= maximum_depth)
				{
					fail("превышена максимальная глубина дерева");
				}
				expect_character('(');

				visual_node node;
				node.keys.push_back(parse_binary_key());
				const std::string detail = node.keys.front().detail;
				const std::size_t index = append_node(std::move(node));

				const std::size_t left = parse_binary_node(depth + 1);
				expect_character(',');
				const std::size_t right = parse_binary_node(depth + 1);
				expect_character(')');

				auto& stored = result.nodes[index];
				stored.binary_children = { left, right };
				if (left != no_visual_node)
				{
					stored.children.push_back(left);
				}
				if (right != no_visual_node)
				{
					stored.children.push_back(right);
				}
				stored.leaf =
					left == no_visual_node && right == no_visual_node;
				stored.red = tree_index == 2 && detail == "0";
				return index;
			}

			void parse_fanout()
			{
				if (!consume_prefix("fanout="))
				{
					fail("в заголовке B+ дерева отсутствует fanout");
				}
				const std::size_t start = position;
				while (position < source.size() &&
					source[position] >= '0' &&
					source[position] <= '9')
				{
					++position;
				}
				if (start == position)
				{
					fail("некорректное значение fanout");
				}
				const auto conversion = std::from_chars(
					source.data() + start,
					source.data() + position,
					result.fanout);
				if (conversion.ec != std::errc{} ||
					result.fanout < 3 ||
					result.fanout > 1'024)
				{
					fail("fanout должен быть от 3 до 1024");
				}
				expect_character(']');
			}

			[[nodiscard]] std::string read_b_plus_key()
			{
				if (position < source.size() && source[position] == '"')
				{
					return read_quoted_string();
				}

				const std::size_t start = position;
				while (position < source.size() &&
					source[position] != ',' &&
					source[position] != '}')
				{
					++position;
				}
				return std::string(source.substr(start, position - start));
			}

			[[nodiscard]] std::size_t parse_b_plus_node(std::size_t depth)
			{
				skip_whitespace();
				if (consume_character('#'))
				{
					return no_visual_node;
				}
				if (depth >= maximum_depth)
				{
					fail("превышена максимальная глубина B+ дерева");
				}
				if (position >= source.size() ||
					(source[position] != 'I' && source[position] != 'L'))
				{
					fail("ожидался узел I или L в B+ дереве");
				}
				const bool leaf = source[position++] == 'L';
				expect_character('{');

				std::vector<visual_key> keys;
				skip_whitespace();
				if (!consume_character('}'))
				{
					while (true)
					{
						skip_whitespace();
						const bool quoted =
							position < source.size() && source[position] == '"';
						keys.push_back({
							visual_key::value_type{
								std::in_place_type<std::string>,
								read_b_plus_key() },
							{},
							!quoted
						});
						skip_whitespace();
						if (consume_character('}'))
						{
							break;
						}
						expect_character(',');
					}
				}

				visual_node node;
				node.keys = std::move(keys);
				node.leaf = leaf;
				const std::size_t index = append_node(std::move(node));
				if (leaf)
				{
					return index;
				}

				expect_character('[');
				std::vector<std::size_t> children;
				skip_whitespace();
				if (!consume_character(']'))
				{
					while (true)
					{
						children.push_back(parse_b_plus_node(depth + 1));
						skip_whitespace();
						if (consume_character(']'))
						{
							break;
						}
						expect_character(',');
					}
				}
				if (children.size() != result.nodes[index].keys.size() + 1)
				{
					fail("число потомков B+ узла должно быть на один больше числа ключей");
				}
				if (children.size() > result.fanout)
				{
					fail("число потомков B+ узла превышает fanout");
				}
				result.nodes[index].children = std::move(children);
				return index;
			}

			[[nodiscard]] static bool is_integer_token(
				std::string_view value) noexcept
			{
				if (value.empty())
				{
					return false;
				}
				if (value.front() == '-' || value.front() == '+')
				{
					value.remove_prefix(1);
				}
				return !value.empty() &&
					std::all_of(value.begin(), value.end(), [](char digit)
					{
						return digit >= '0' && digit <= '9';
					});
			}

			[[nodiscard]] static bool parse_integer_token(
				std::string_view text,
				std::int64_t& value) noexcept
			{
				if (text.starts_with('+'))
				{
					text.remove_prefix(1);
				}
				const auto conversion = std::from_chars(
					text.data(),
					text.data() + text.size(),
					value);
				return conversion.ec == std::errc{} &&
					conversion.ptr == text.data() + text.size();
			}

			[[nodiscard]] static bool parse_floating_token(
				std::string_view text,
				double& value) noexcept
			{
				if (text.starts_with('+'))
				{
					text.remove_prefix(1);
				}
				const auto conversion = std::from_chars(
					text.data(),
					text.data() + text.size(),
					value,
					std::chars_format::general);
				return conversion.ec == std::errc{} &&
					conversion.ptr == text.data() + text.size();
			}

			void infer_key_types()
			{
				bool has_text = false;
				bool has_floating_point = false;
				bool has_big_integer = false;
				for (const auto& node : result.nodes)
				{
					for (const auto& key : node.keys)
					{
						if (!key.unquoted_serialized_value)
						{
							has_text = true;
							continue;
						}
						const auto& raw = std::get<std::string>(key.value);
						if (is_integer_token(raw))
						{
							std::int64_t integer_value = 0;
							if (!parse_integer_token(raw, integer_value))
							{
								has_big_integer = true;
							}
						}
						else
						{
							double floating_value = 0.0;
							if (parse_floating_token(raw, floating_value))
							{
								has_floating_point = true;
							}
							else
							{
								has_text = true;
							}
						}
					}
				}

				if (has_text)
				{
					return;
				}
				if (has_floating_point && has_big_integer)
				{
					fail("нельзя смешивать double и целые за пределами int64_t");
				}

				for (auto& node : result.nodes)
				{
					for (auto& key : node.keys)
					{
						auto& raw = std::get<std::string>(key.value);
						if (has_floating_point)
						{
							double value = 0.0;
							if (!parse_floating_token(raw, value))
							{
								fail("некорректное значение double");
							}
							key.value = value;
						}
						else if (has_big_integer)
						{
							key.value = visual_key::big_integer_text{
								std::move(raw) };
						}
						else
						{
							std::int64_t value = 0;
							if (!parse_integer_token(raw, value))
							{
								fail("некорректное значение int64_t");
							}
							key.value = value;
						}
						key.unquoted_serialized_value = false;
					}
				}
			}

			void link_b_plus_leaves()
			{
				std::vector<std::size_t> pending{ result.root };
				std::vector<std::size_t> leaves;
				while (!pending.empty())
				{
					const std::size_t index = pending.back();
					pending.pop_back();
					if (index == no_visual_node)
					{
						continue;
					}
					const auto& node = result.nodes[index];
					if (node.leaf)
					{
						leaves.push_back(index);
						continue;
					}
					for (auto child = node.children.rbegin();
						child != node.children.rend();
						++child)
					{
						pending.push_back(*child);
					}
				}
				for (std::size_t i = 0; i + 1 < leaves.size(); ++i)
				{
					result.nodes[leaves[i]].next_leaf = leaves[i + 1];
				}
			}
		};

		[[nodiscard]] static visual_tree parse_tree_serialization(
			std::string_view serialization,
			std::size_t& tree_index)
		{
			return tree_serialization_parser{ serialization }.parse(tree_index);
		}

		[[nodiscard]] static bool is_integer_literal(
			std::string_view value) noexcept
		{
			if (value.empty())
			{
				return false;
			}
			if (value.front() == '+' || value.front() == '-')
			{
				value.remove_prefix(1);
			}
			return !value.empty() &&
				std::all_of(value.begin(), value.end(), [](char character)
				{
					return character >= '0' && character <= '9';
				});
		}

		[[nodiscard]] static bool parse_integer_literal(
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
			const auto conversion = std::from_chars(
				text.data(),
				text.data() + text.size(),
				value);
			return conversion.ec == std::errc{} &&
				conversion.ptr == text.data() + text.size();
		}

		[[nodiscard]] static bool parse_finite_double_literal(
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
			const auto conversion = std::from_chars(
				text.data(),
				text.data() + text.size(),
				value,
				std::chars_format::general);
			return conversion.ec == std::errc{} &&
				conversion.ptr == text.data() + text.size() &&
				std::isfinite(value);
		}

		[[nodiscard]] static std::string normalize_integer_literal(
			std::string_view value)
		{
			if (!is_integer_literal(value))
			{
				throw std::invalid_argument(
					"Для целочисленного дерева требуется целое число.");
			}

			bool negative = value.front() == '-';
			if (value.front() == '-' || value.front() == '+')
			{
				value.remove_prefix(1);
			}
			const std::size_t first_nonzero = value.find_first_not_of('0');
			if (first_nonzero == std::string_view::npos)
			{
				return "0";
			}
			value.remove_prefix(first_nonzero);

			std::string normalized;
			normalized.reserve(value.size() + (negative ? 1 : 0));
			if (negative)
			{
				normalized.push_back('-');
			}
			normalized.append(value);
			return normalized;
		}

		[[nodiscard]] static search_tree_key_kind classify_unquoted_key(
			std::string_view value)
		{
			if (is_integer_literal(value))
			{
				std::int64_t integer = 0;
				return parse_integer_literal(value, integer)
					? search_tree_key_kind::signed_integer
					: search_tree_key_kind::big_integer;
			}
			double floating_point = 0.0;
			return parse_finite_double_literal(value, floating_point)
				? search_tree_key_kind::floating_point
				: search_tree_key_kind::text;
		}

		[[nodiscard]] static search_tree_key_kind search_tree_key_kind_for_value(
			const visual_key::value_type& value)
		{
			if (std::holds_alternative<std::string>(value))
			{
				return search_tree_key_kind::text;
			}
			if (std::holds_alternative<double>(value))
			{
				return search_tree_key_kind::floating_point;
			}
			if (std::holds_alternative<std::int64_t>(value))
			{
				return search_tree_key_kind::signed_integer;
			}
			return search_tree_key_kind::big_integer;
		}

		[[nodiscard]] static search_tree_key_kind classify_search_tree_key(
			const visual_key& key)
		{
			if (!std::holds_alternative<std::string>(key.value))
			{
				const auto kind = search_tree_key_kind_for_value(key.value);
				if (kind == search_tree_key_kind::floating_point &&
					!std::isfinite(std::get<double>(key.value)))
				{
					throw std::invalid_argument(
						"Дерево поиска с NaN или бесконечностью нельзя изменять.");
				}
				return kind;
			}

			if (!key.unquoted_serialized_value)
			{
				return search_tree_key_kind::text;
			}
			return classify_unquoted_key(
				std::get<std::string>(key.value));
		}

		[[nodiscard]] static std::optional<search_tree_key_kind>
			infer_search_tree_key_kind(
			const visual_tree& tree)
		{
			std::optional<search_tree_key_kind> inferred_kind;
			for (const auto& node : tree.nodes)
			{
				if (!tree.b_plus && node.keys.size() != 1)
				{
					throw std::invalid_argument(
						"Узел дерева поиска должен содержать ровно один ключ.");
				}
				for (const auto& key : node.keys)
				{
					const auto node_kind = classify_search_tree_key(key);
					if (inferred_kind && *inferred_kind != node_kind)
					{
						throw std::invalid_argument(
							"В дереве поиска обнаружены смешанные типы ключей.");
					}
					inferred_kind = node_kind;
				}
			}
			return inferred_kind;
		}

		[[nodiscard]] static std::optional<std::string>
			decode_quoted_insert_value(std::string_view value)
		{
			if (!value.starts_with('"'))
			{
				return std::nullopt;
			}
			if (value.size() < 2 || value.back() != '"')
			{
				throw std::invalid_argument(
					"Строковый ключ должен быть заключён в кавычки.");
			}

			std::string decoded;
			for (std::size_t position = 1; position + 1 < value.size(); ++position)
			{
				const char current = value[position];
				if (current == '"')
				{
					throw std::invalid_argument(
						"Кавычку внутри строки нужно экранировать как \\\".");
				}
				if (current != '\\')
				{
					decoded.push_back(current);
					continue;
				}
				if (++position + 1 >= value.size())
				{
					throw std::invalid_argument(
						"Незавершённая escape-последовательность в ключе.");
				}
				switch (value[position])
				{
				case '"':
				case '\\':
					decoded.push_back(value[position]);
					break;
				case 'n':
					decoded.push_back('\n');
					break;
				case 'r':
					decoded.push_back('\r');
					break;
				case 't':
					decoded.push_back('\t');
					break;
				case '0':
					decoded.push_back('\0');
					break;
				default:
					decoded.push_back('\\');
					decoded.push_back(value[position]);
					break;
				}
			}
			return decoded;
		}

		[[nodiscard]] static visual_key parse_search_tree_key_input(
			std::string_view input,
			std::optional<search_tree_key_kind> target_kind)
		{
			auto decoded = decode_quoted_insert_value(input);
			const std::string_view value = decoded
				? std::string_view(*decoded)
				: input;

			if (!target_kind)
			{
				if (decoded)
				{
					return make_text_key(value);
				}
				if (is_integer_literal(value))
				{
					std::int64_t integer = 0;
					if (parse_integer_literal(value, integer))
					{
						return make_integer_key(integer);
					}
					return make_big_integer_key(
						normalize_integer_literal(value));
				}
				double floating_point = 0.0;
				if (parse_finite_double_literal(value, floating_point))
				{
					return make_double_key(floating_point);
				}
				return make_text_key(value);
			}

			switch (*target_kind)
			{
			case search_tree_key_kind::text:
				return make_text_key(value);
			case search_tree_key_kind::floating_point:
			{
				double floating_point = 0.0;
				if (!parse_finite_double_literal(value, floating_point))
				{
					throw std::invalid_argument(
						"Ключ нельзя преобразовать в конечное значение double.");
				}
				return make_double_key(floating_point);
			}
			case search_tree_key_kind::signed_integer:
			{
				std::int64_t integer = 0;
				if (!parse_integer_literal(value, integer))
				{
					throw std::invalid_argument(
						"Для дерева поиска с int64_t нужен целый ключ в диапазоне int64_t.");
				}
				return make_integer_key(integer);
			}
			case search_tree_key_kind::big_integer:
				return make_big_integer_key(
					normalize_integer_literal(value));
			}
			throw std::logic_error("Unknown search-tree key type.");
		}

		[[nodiscard]] static visual_key parse_numeric_search_tree_key_input(
			std::string_view input,
			std::optional<search_tree_key_kind> target_kind)
		{
			auto key = parse_search_tree_key_input(input, target_kind);
			const auto kind = search_tree_key_kind_for_value(key.value);
			if (kind != search_tree_key_kind::signed_integer &&
				kind != search_tree_key_kind::floating_point)
			{
				throw std::invalid_argument(
					"Операции практикума поддерживают только int64_t и конечный double.");
			}
			return key;
		}

		[[nodiscard]] static int compare_big_integer_values(
			std::string_view left,
			std::string_view right)
		{
			const auto normalized_left = normalize_integer_literal(left);
			const auto normalized_right = normalize_integer_literal(right);
			const bool left_negative = normalized_left.starts_with('-');
			const bool right_negative = normalized_right.starts_with('-');
			if (left_negative != right_negative)
			{
				return left_negative ? -1 : 1;
			}

			const std::string_view left_digits = left_negative
				? std::string_view(normalized_left).substr(1)
				: std::string_view(normalized_left);
			const std::string_view right_digits = right_negative
				? std::string_view(normalized_right).substr(1)
				: std::string_view(normalized_right);
			int comparison = 0;
			if (left_digits.size() != right_digits.size())
			{
				comparison = left_digits.size() < right_digits.size() ? -1 : 1;
			}
			else
			{
				const int lexical = left_digits.compare(right_digits);
				comparison = lexical < 0 ? -1 : lexical > 0 ? 1 : 0;
			}
			return left_negative ? -comparison : comparison;
		}

		[[nodiscard]] static int compare_search_tree_keys(
			const visual_key::value_type& left,
			const visual_key::value_type& right,
			search_tree_key_kind kind)
		{
			switch (kind)
			{
			case search_tree_key_kind::text:
			{
				const int comparison = std::get<std::string>(left).compare(
					std::get<std::string>(right));
				return comparison < 0 ? -1 : comparison > 0 ? 1 : 0;
			}
			case search_tree_key_kind::floating_point:
			{
				const double left_value = std::get<double>(left);
				const double right_value = std::get<double>(right);
				if (!std::isfinite(left_value) || !std::isfinite(right_value))
				{
					throw std::invalid_argument(
						"Дерево поиска с NaN или бесконечностью нельзя изменять.");
				}
				return left_value < right_value
					? -1
					: left_value > right_value ? 1 : 0;
			}
			case search_tree_key_kind::signed_integer:
			{
				const auto left_value = std::get<std::int64_t>(left);
				const auto right_value = std::get<std::int64_t>(right);
				return left_value < right_value
					? -1
					: left_value > right_value ? 1 : 0;
			}
			case search_tree_key_kind::big_integer:
				return compare_big_integer_values(
					std::get<visual_key::big_integer_text>(left).decimal,
					std::get<visual_key::big_integer_text>(right).decimal);
			}
			throw std::logic_error("Unknown search-tree key type.");
		}

		static void validate_search_tree_structure(
			const visual_tree& tree,
			search_tree_key_kind kind)
		{
			if (tree.nodes.empty())
			{
				if (tree.root != no_visual_node)
				{
					throw std::invalid_argument(
						"Пустое дерево поиска содержит неверный индекс корня.");
				}
				return;
			}
			if (tree.root >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Корень дерева поиска ссылается на отсутствующий узел.");
			}

			struct validation_frame
			{
				std::size_t node_index;
				std::size_t lower_bound;
				std::size_t upper_bound;
				std::size_t depth;
			};

			std::vector<validation_frame> pending{ {
				tree.root,
				no_visual_node,
				no_visual_node,
				0
			} };
			std::vector<unsigned char> visited(tree.nodes.size(), 0);
			std::size_t visited_count = 0;
			while (!pending.empty())
			{
				const auto frame = pending.back();
				pending.pop_back();
				if (frame.node_index >= tree.nodes.size() ||
					frame.depth >= tree_serialization_parser::maximum_depth ||
					visited[frame.node_index] != 0)
				{
					throw std::invalid_argument(
						"Дерево поиска содержит неверную ссылку, цикл или превышает допустимую глубину.");
				}

				visited[frame.node_index] = 1;
				++visited_count;
				const auto& node = tree.nodes[frame.node_index];
				if (node.keys.size() != 1)
				{
					throw std::invalid_argument(
						"Узел дерева поиска должен содержать ровно один ключ.");
				}
				const auto& key = node.keys.front().value;
				if (frame.lower_bound != no_visual_node &&
					compare_search_tree_keys(
						key,
						tree.nodes[frame.lower_bound].keys.front().value,
						kind) <= 0)
				{
					throw std::invalid_argument(
						"Нарушен порядок ключей в дереве поиска.");
				}
				if (frame.upper_bound != no_visual_node &&
					compare_search_tree_keys(
						key,
						tree.nodes[frame.upper_bound].keys.front().value,
						kind) >= 0)
				{
					throw std::invalid_argument(
						"Нарушен порядок ключей в дереве поиска.");
				}

				const auto left = node.binary_children[0];
				const auto right = node.binary_children[1];
				if (right != no_visual_node)
				{
					pending.push_back({
						right,
						frame.node_index,
						frame.upper_bound,
						frame.depth + 1
					});
				}
				if (left != no_visual_node)
				{
					pending.push_back({
						left,
						frame.lower_bound,
						frame.node_index,
						frame.depth + 1
					});
				}
			}
			if (visited_count != tree.nodes.size())
			{
				throw std::invalid_argument(
					"Дерево поиска содержит узлы, не достижимые из корня.");
			}
		}

		[[nodiscard]] static std::size_t avl_height(
			const visual_tree& tree,
			std::size_t node_index)
		{
			if (node_index == no_visual_node)
			{
				return 0;
			}
			if (node_index >= tree.nodes.size() ||
				tree.nodes[node_index].keys.size() != 1)
			{
				throw std::invalid_argument(
					"Узел AVL содержит неверный индекс или число ключей.");
			}

			const auto& detail = tree.nodes[node_index].keys.front().detail;
			std::size_t height = 0;
			const auto conversion = std::from_chars(
				detail.data(),
				detail.data() + detail.size(),
				height);
			if (detail.empty() ||
				conversion.ec != std::errc{} ||
				conversion.ptr != detail.data() + detail.size() ||
				height == 0)
			{
				throw std::invalid_argument(
					"Высота узла AVL должна быть положительным целым числом.");
			}
			return height;
		}

		static void validate_avl_structure(const visual_tree& tree)
		{
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"Невозможно проверить AVL как B+ дерево.");
			}
			if (tree.nodes.empty())
			{
				if (tree.root != no_visual_node)
				{
					throw std::invalid_argument(
						"Пустое AVL содержит неверный индекс корня.");
				}
				return;
			}
			if (tree.root >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Корень AVL ссылается на отсутствующий узел.");
			}

			struct validation_frame
			{
				std::size_t node_index;
				bool expanded;
			};

			std::vector<validation_frame> pending{ { tree.root, false } };
			std::vector<unsigned char> state(tree.nodes.size(), 0);
			std::vector<std::size_t> heights(tree.nodes.size(), 0);
			std::size_t visited_count = 0;
			while (!pending.empty())
			{
				const auto frame = pending.back();
				pending.pop_back();
				if (frame.node_index >= tree.nodes.size())
				{
					throw std::invalid_argument(
						"AVL содержит ссылку на отсутствующий узел.");
				}

				const auto& node = tree.nodes[frame.node_index];
				if (!frame.expanded)
				{
					if (state[frame.node_index] != 0)
					{
						throw std::invalid_argument(
							"AVL содержит цикл или повторную ссылку на узел.");
					}
					if (node.keys.size() != 1)
					{
						throw std::invalid_argument(
							"Узел AVL должен содержать ровно один ключ.");
					}
					state[frame.node_index] = 1;
					++visited_count;
					pending.push_back({ frame.node_index, true });
					if (node.binary_children[1] != no_visual_node)
					{
						pending.push_back({
							node.binary_children[1],
							false
						});
					}
					if (node.binary_children[0] != no_visual_node)
					{
						pending.push_back({
							node.binary_children[0],
							false
						});
					}
					continue;
				}

				if (state[frame.node_index] != 1)
				{
					throw std::invalid_argument(
						"AVL содержит некорректную структуру узлов.");
				}
				const auto left = node.binary_children[0];
				const auto right = node.binary_children[1];
				const std::size_t left_height = left == no_visual_node
					? 0
					: heights[left];
				const std::size_t right_height = right == no_visual_node
					? 0
					: heights[right];
				const std::size_t expected_height =
					std::max(left_height, right_height) + 1;
				const std::size_t stored_height =
					avl_height(tree, frame.node_index);
				if (stored_height != expected_height)
				{
					throw std::invalid_argument(
						"Сохранённая высота узла AVL не соответствует структуре.");
				}
				const std::size_t height_difference =
					left_height > right_height
					? left_height - right_height
					: right_height - left_height;
				if (height_difference > 1)
				{
					throw std::invalid_argument(
						"AVL нарушает условие балансировки.");
				}

				heights[frame.node_index] = expected_height;
				state[frame.node_index] = 2;
			}

			if (visited_count != tree.nodes.size())
			{
				throw std::invalid_argument(
					"AVL содержит узлы, не достижимые из корня.");
			}
		}

		[[nodiscard]] static bool red_black_node_is_red(
			const visual_tree& tree,
			std::size_t node_index)
		{
			if (node_index == no_visual_node)
			{
				return false;
			}
			if (node_index >= tree.nodes.size() ||
				tree.nodes[node_index].keys.size() != 1)
			{
				throw std::invalid_argument(
					"Узел красно-чёрного дерева содержит неверный индекс или число ключей.");
			}

			const auto& detail = tree.nodes[node_index].keys.front().detail;
			if (detail == "0")
			{
				return true;
			}
			if (detail == "1")
			{
				return false;
			}
			throw std::invalid_argument(
				"Цвет узла красно-чёрного дерева должен быть 0 (красный) или 1 (чёрный).");
		}

		static void set_red_black_node_color(
			visual_tree& tree,
			std::size_t node_index,
			bool red)
		{
			if (node_index >= tree.nodes.size() ||
				tree.nodes[node_index].keys.size() != 1)
			{
				throw std::invalid_argument(
					"Невозможно изменить цвет некорректного узла красно-чёрного дерева.");
			}
			auto& node = tree.nodes[node_index];
			node.keys.front().detail = red ? "0" : "1";
			node.red = red;
		}

		static void validate_red_black_structure(const visual_tree& tree)
		{
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"Невозможно проверить красно-чёрное дерево как B+ дерево.");
			}
			if (tree.nodes.empty())
			{
				if (tree.root != no_visual_node)
				{
					throw std::invalid_argument(
						"Пустое красно-чёрное дерево содержит неверный индекс корня.");
				}
				return;
			}
			if (tree.root >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Корень красно-чёрного дерева ссылается на отсутствующий узел.");
			}
			if (red_black_node_is_red(tree, tree.root))
			{
				throw std::invalid_argument(
					"Корень красно-чёрного дерева должен быть чёрным.");
			}

			struct validation_frame
			{
				std::size_t node_index;
				std::size_t black_count;
				std::size_t depth;
				bool parent_red;
			};

			std::vector<validation_frame> pending{ {
				tree.root,
				0,
				0,
				false
			} };
			std::vector<unsigned char> visited(tree.nodes.size(), 0);
			visited[tree.root] = 1;
			std::optional<std::size_t> expected_black_height;
			std::size_t visited_count = 0;
			while (!pending.empty())
			{
				const auto frame = pending.back();
				pending.pop_back();
				if (frame.node_index >= tree.nodes.size() ||
					frame.depth >= tree_serialization_parser::maximum_depth)
				{
					throw std::invalid_argument(
						"Красно-чёрное дерево содержит неверную ссылку или превышает допустимую глубину.");
				}

				const auto& node = tree.nodes[frame.node_index];
				if (node.keys.size() != 1)
				{
					throw std::invalid_argument(
						"Узел красно-чёрного дерева должен содержать ровно один ключ.");
				}
				const bool red =
					red_black_node_is_red(tree, frame.node_index);
				if (frame.parent_red && red)
				{
					throw std::invalid_argument(
						"Красный узел красно-чёрного дерева не может иметь красного родителя.");
				}
				++visited_count;
				const std::size_t black_count =
					frame.black_count + (red ? 0 : 1);
				for (std::size_t side = 0; side < 2; ++side)
				{
					const auto child = node.binary_children[side];
					if (child == no_visual_node)
					{
						if (expected_black_height.has_value() &&
							expected_black_height.value() != black_count)
						{
							throw std::invalid_argument(
								"Пути от корня красно-чёрного дерева имеют разную чёрную высоту.");
						}
						expected_black_height = black_count;
						continue;
					}
					if (child >= tree.nodes.size() || visited[child] != 0)
					{
						throw std::invalid_argument(
							"Красно-чёрное дерево содержит цикл или повторную ссылку на узел.");
					}
					visited[child] = 1;
					pending.push_back({
						child,
						black_count,
						frame.depth + 1,
						red
					});
				}
			}

			if (visited_count != tree.nodes.size())
			{
				throw std::invalid_argument(
					"Красно-чёрное дерево содержит узлы, не достижимые из корня.");
			}
		}

		[[nodiscard]] static std::uint64_t treap_priority(
			const visual_tree& tree,
			std::size_t node_index)
		{
			if (node_index >= tree.nodes.size() ||
				tree.nodes[node_index].keys.size() != 1)
			{
				throw std::invalid_argument(
					"Узел Treap содержит неверный индекс или число ключей.");
			}

			const auto& detail = tree.nodes[node_index].keys.front().detail;
			std::uint64_t priority = 0;
			const auto conversion = std::from_chars(
				detail.data(),
				detail.data() + detail.size(),
				priority);
			if (detail.empty() ||
				conversion.ec != std::errc{} ||
				conversion.ptr != detail.data() + detail.size())
			{
				throw std::invalid_argument(
					"Приоритет узла Treap должен быть числом uint64_t.");
			}
			return priority;
		}

		static void validate_treap_structure(const visual_tree& tree)
		{
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"Невозможно проверить Treap как B+ дерево.");
			}
			for (std::size_t i = 0; i < tree.nodes.size(); ++i)
			{
				const auto parent_priority = treap_priority(tree, i);
				for (const auto child : tree.nodes[i].binary_children)
				{
					if (child != no_visual_node &&
						parent_priority > treap_priority(tree, child))
					{
						throw std::invalid_argument(
							"Treap нарушает min-heap порядок приоритетов.");
					}
				}
			}
		}

		static void validate_b_plus_tree_structure(
			const visual_tree& tree,
			search_tree_key_kind kind)
		{
			if (!tree.b_plus || tree.fanout < 3 || tree.fanout > 1'024)
			{
				throw std::invalid_argument(
					"B+ дерево должно иметь fanout от 3 до 1024.");
			}
			if (tree.nodes.empty())
			{
				if (tree.root != no_visual_node)
				{
					throw std::invalid_argument(
						"Пустое B+ дерево содержит неверный индекс корня.");
				}
				return;
			}
			if (tree.root >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Корень B+ дерева ссылается на отсутствующий узел.");
			}

			struct key_reference
			{
				std::size_t node_index;
				std::size_t key_index;
			};
			struct subtree_summary
			{
				key_reference minimum{};
				key_reference maximum{};
				std::size_t leaf_depth = 0;
			};
			struct validation_frame
			{
				std::size_t node_index;
				std::size_t depth;
				bool is_root;
				bool expanded;
			};

			const auto key_value = [&](key_reference reference)
				-> const visual_key::value_type&
			{
				return tree.nodes[reference.node_index]
					.keys[reference.key_index].value;
			};

			std::vector<unsigned char> state(tree.nodes.size(), 0);
			std::vector<subtree_summary> summaries(tree.nodes.size());
			std::vector<validation_frame> pending{
				{ tree.root, 0, true, false }
			};
			std::size_t visited_count = 0;
			while (!pending.empty())
			{
				const auto frame = pending.back();
				pending.pop_back();
				if (frame.node_index >= tree.nodes.size() ||
					frame.depth >= tree_serialization_parser::maximum_depth)
				{
					throw std::invalid_argument(
						"B+ дерево содержит неверную ссылку или превышает допустимую глубину.");
				}

				const auto& node = tree.nodes[frame.node_index];
				if (!frame.expanded)
				{
					if (state[frame.node_index] != 0)
					{
						throw std::invalid_argument(
							"B+ дерево содержит цикл или повторную ссылку на узел.");
					}
					state[frame.node_index] = 1;
					++visited_count;

					if (node.leaf)
					{
						const std::size_t minimum_keys = frame.is_root
							? 1
							: tree.fanout / 2;
						if (!node.children.empty() ||
							node.keys.size() < minimum_keys ||
							node.keys.size() >= tree.fanout)
						{
							throw std::invalid_argument(
								"B+ лист нарушает ограничение количества ключей.");
						}
						for (std::size_t i = 1; i < node.keys.size(); ++i)
						{
							if (compare_search_tree_keys(
								node.keys[i - 1].value,
								node.keys[i].value,
								kind) >= 0)
							{
								throw std::invalid_argument(
									"Ключи B+ листа должны идти строго по возрастанию.");
							}
						}
						summaries[frame.node_index] = {
							{ frame.node_index, 0 },
							{ frame.node_index, node.keys.size() - 1 },
							frame.depth
						};
						state[frame.node_index] = 2;
						continue;
					}

					const std::size_t child_count = node.children.size();
					const std::size_t minimum_children = frame.is_root
						? 2
						: tree.fanout / 2 + tree.fanout % 2;
					if (child_count != node.keys.size() + 1 ||
						child_count < minimum_children ||
						child_count > tree.fanout)
					{
						throw std::invalid_argument(
							"B+ внутренний узел нарушает ограничение числа потомков.");
					}
					for (std::size_t i = 1; i < node.keys.size(); ++i)
					{
						if (compare_search_tree_keys(
							node.keys[i - 1].value,
							node.keys[i].value,
							kind) >= 0)
						{
							throw std::invalid_argument(
								"Ключи-разделители B+ должны идти строго по возрастанию.");
						}
					}

					pending.push_back({
						frame.node_index,
						frame.depth,
						frame.is_root,
						true
					});
					for (auto child = node.children.rbegin();
						child != node.children.rend();
						++child)
					{
						if (*child == no_visual_node)
						{
							throw std::invalid_argument(
								"B+ внутренний узел не может иметь пустого потомка.");
						}
						pending.push_back({
							*child,
							frame.depth + 1,
							false,
							false
						});
					}
					continue;
				}

				const auto& children = node.children;
				const auto& first_summary = summaries[children.front()];
				const auto& last_summary = summaries[children.back()];
				for (std::size_t i = 0; i < children.size(); ++i)
				{
					const auto& child_summary = summaries[children[i]];
					if (child_summary.leaf_depth != first_summary.leaf_depth)
					{
						throw std::invalid_argument(
							"Все листья B+ дерева должны иметь одинаковую глубину.");
					}
					if (i == 0)
					{
						continue;
					}

					const auto& separator = node.keys[i - 1].value;
					if (compare_search_tree_keys(
						key_value(child_summary.minimum),
						separator,
						kind) != 0 ||
						compare_search_tree_keys(
							key_value(summaries[children[i - 1]].maximum),
							separator,
							kind) >= 0)
					{
						throw std::invalid_argument(
							"Разделитель B+ должен совпадать с минимумом правого поддерева и быть больше левого.");
					}
				}
				summaries[frame.node_index] = {
					first_summary.minimum,
					last_summary.maximum,
					first_summary.leaf_depth
				};
				state[frame.node_index] = 2;
			}

			if (visited_count != tree.nodes.size())
			{
				throw std::invalid_argument(
					"B+ дерево содержит узлы, недостижимые из корня.");
			}
		}

		static void synchronize_binary_children(visual_node& node)
		{
			node.children.clear();
			for (const auto child : node.binary_children)
			{
				if (child != no_visual_node)
				{
					node.children.push_back(child);
				}
			}
			node.leaf = node.children.empty();
		}

		static void set_binary_child(
			visual_tree& tree,
			std::size_t parent,
			std::size_t side,
			std::size_t child)
		{
			tree.nodes[parent].binary_children[side] = child;
			synchronize_binary_children(tree.nodes[parent]);
		}

		[[nodiscard]] static std::vector<std::size_t>
			build_binary_parent_index(const visual_tree& tree)
		{
			std::vector<std::size_t> parents(
				tree.nodes.size(),
				no_visual_node);
			if (tree.nodes.empty())
			{
				if (tree.root != no_visual_node)
				{
					throw std::invalid_argument(
						"Пустое дерево содержит неверный индекс корня.");
				}
				return parents;
			}
			if (tree.root >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Корень дерева ссылается на отсутствующий узел.");
			}

			std::vector<unsigned char> discovered(tree.nodes.size(), 0);
			std::vector<std::size_t> pending{ tree.root };
			discovered[tree.root] = 1;
			std::size_t visited_count = 0;
			while (!pending.empty())
			{
				const auto node_index = pending.back();
				pending.pop_back();
				++visited_count;
				for (const auto child : tree.nodes[node_index].binary_children)
				{
					if (child == no_visual_node)
					{
						continue;
					}
					if (child >= tree.nodes.size() || discovered[child] != 0)
					{
						throw std::invalid_argument(
							"Дерево содержит неверную ссылку или повторный узел.");
					}
					discovered[child] = 1;
					parents[child] = node_index;
					pending.push_back(child);
				}
			}
			if (visited_count != tree.nodes.size())
			{
				throw std::invalid_argument(
					"Дерево содержит узлы, не достижимые из корня.");
			}
			return parents;
		}

		static void update_avl_node_height(
			visual_tree& tree,
			std::size_t node_index)
		{
			const auto left = tree.nodes[node_index].binary_children[0];
			const auto right = tree.nodes[node_index].binary_children[1];
			const std::size_t height =
				std::max(avl_height(tree, left), avl_height(tree, right)) + 1;
			tree.nodes[node_index].keys.front().detail =
				std::to_string(height);
		}

		[[nodiscard]] static std::ptrdiff_t avl_balance_factor(
			const visual_tree& tree,
			std::size_t node_index)
		{
			const auto left = tree.nodes[node_index].binary_children[0];
			const auto right = tree.nodes[node_index].binary_children[1];
			const std::size_t left_height = avl_height(tree, left);
			const std::size_t right_height = avl_height(tree, right);
			return left_height >= right_height
				? static_cast<std::ptrdiff_t>(left_height - right_height)
				: -static_cast<std::ptrdiff_t>(right_height - left_height);
		}

		[[nodiscard]] static std::size_t rotate_avl_left(
			visual_tree& tree,
			std::size_t root)
		{
			const auto pivot = tree.nodes[root].binary_children[1];
			if (pivot == no_visual_node)
			{
				throw std::logic_error("Левый поворот AVL не имеет правого узла.");
			}
			const auto pivot_left = tree.nodes[pivot].binary_children[0];
			set_binary_child(tree, root, 1, pivot_left);
			set_binary_child(tree, pivot, 0, root);
			update_avl_node_height(tree, root);
			update_avl_node_height(tree, pivot);
			return pivot;
		}

		[[nodiscard]] static std::size_t rotate_avl_right(
			visual_tree& tree,
			std::size_t root)
		{
			const auto pivot = tree.nodes[root].binary_children[0];
			if (pivot == no_visual_node)
			{
				throw std::logic_error("Правый поворот AVL не имеет левого узла.");
			}
			const auto pivot_right = tree.nodes[pivot].binary_children[1];
			set_binary_child(tree, root, 0, pivot_right);
			set_binary_child(tree, pivot, 1, root);
			update_avl_node_height(tree, root);
			update_avl_node_height(tree, pivot);
			return pivot;
		}

		[[nodiscard]] static std::size_t rebalance_avl_subtree(
			visual_tree& tree,
			std::size_t root)
		{
			update_avl_node_height(tree, root);
			const auto balance = avl_balance_factor(tree, root);
			if (balance > 1)
			{
				const auto left = tree.nodes[root].binary_children[0];
				if (avl_balance_factor(tree, left) < 0)
				{
					set_binary_child(
						tree,
						root,
						0,
						rotate_avl_left(tree, left));
					update_avl_node_height(tree, root);
				}
				return rotate_avl_right(tree, root);
			}
			if (balance < -1)
			{
				const auto right = tree.nodes[root].binary_children[1];
				if (avl_balance_factor(tree, right) > 0)
				{
					set_binary_child(
						tree,
						root,
						1,
						rotate_avl_right(tree, right));
					update_avl_node_height(tree, root);
				}
				return rotate_avl_left(tree, root);
			}
			return root;
		}

		struct avl_insert_result
		{
			std::size_t root;
			bool inserted;
		};

		[[nodiscard]] static avl_insert_result insert_avl_subtree(
			visual_tree& tree,
			std::size_t node_index,
			visual_key& key,
			search_tree_key_kind kind,
			std::size_t depth)
		{
			if (depth >= tree_serialization_parser::maximum_depth)
			{
				throw std::length_error(
					"Вставка превысила максимальную глубину AVL.");
			}
			if (node_index == no_visual_node)
			{
				if (tree.nodes.size() >=
					tree_serialization_parser::maximum_node_count)
				{
					throw std::length_error(
						"Достигнут лимит в 1 000 000 узлов.");
				}
				key.detail = "1";
				visual_node inserted_node;
				inserted_node.keys.push_back(std::move(key));
				inserted_node.leaf = true;
				const std::size_t inserted_index = tree.nodes.size();
				tree.nodes.push_back(std::move(inserted_node));
				return { inserted_index, true };
			}
			if (node_index >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Узел AVL ссылается на отсутствующего потомка.");
			}

			std::size_t side = 0;
			std::size_t child = no_visual_node;
			{
				const auto& node = tree.nodes[node_index];
				if (node.keys.size() != 1)
				{
					throw std::invalid_argument(
						"Узел AVL должен содержать ровно один ключ.");
				}
				const int comparison = compare_search_tree_keys(
					key.value,
					node.keys.front().value,
					kind);
				if (comparison == 0)
				{
					return { node_index, false };
				}

				side = comparison < 0 ? 0 : 1;
				child = node.binary_children[side];
			}

			const auto child_result = insert_avl_subtree(
				tree,
				child,
				key,
				kind,
				depth + 1);
			if (!child_result.inserted)
			{
				return { node_index, false };
			}

			set_binary_child(
				tree,
				node_index,
				side,
				child_result.root);
			return {
				rebalance_avl_subtree(tree, node_index),
				true
			};
		}

		[[nodiscard]] static bool insert_avl_key(
			visual_tree& tree,
			visual_key key,
			search_tree_key_kind kind)
		{
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"AVL insertion received a B+ tree.");
			}
			if (tree.nodes.empty() && tree.root != no_visual_node)
			{
				throw std::invalid_argument(
					"Пустое AVL содержит неверный индекс корня.");
			}
			if (!tree.nodes.empty() && tree.root >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Корень AVL ссылается на отсутствующий узел.");
			}

			const auto result = insert_avl_subtree(
				tree,
				tree.root,
				key,
				kind,
				0);
			if (result.inserted)
			{
				tree.root = result.root;
			}
			return result.inserted;
		}

		[[nodiscard]] static std::size_t rotate_red_black_left(
			visual_tree& tree,
			std::vector<std::size_t>& parents,
			std::size_t root)
		{
			const auto pivot = tree.nodes[root].binary_children[1];
			if (pivot == no_visual_node)
			{
				throw std::logic_error(
					"Левый поворот красно-чёрного дерева не имеет правого узла.");
			}

			const auto old_parent = parents[root];
			std::size_t parent_side = 2;
			if (old_parent == no_visual_node)
			{
				if (tree.root != root)
				{
					throw std::logic_error(
						"Корень поворота красно-чёрного дерева потерял родителя.");
				}
			}
			else if (tree.nodes[old_parent].binary_children[0] == root)
			{
				parent_side = 0;
			}
			else if (tree.nodes[old_parent].binary_children[1] == root)
			{
				parent_side = 1;
			}
			else
			{
				throw std::logic_error(
					"Родитель узла поворота красно-чёрного дерева не ссылается на него.");
			}

			const auto pivot_left = tree.nodes[pivot].binary_children[0];
			set_binary_child(tree, root, 1, pivot_left);
			if (pivot_left != no_visual_node)
			{
				parents[pivot_left] = root;
			}
			set_binary_child(tree, pivot, 0, root);
			parents[root] = pivot;
			if (old_parent == no_visual_node)
			{
				tree.root = pivot;
			}
			else
			{
				set_binary_child(tree, old_parent, parent_side, pivot);
			}
			parents[pivot] = old_parent;
			return pivot;
		}

		[[nodiscard]] static std::size_t rotate_red_black_right(
			visual_tree& tree,
			std::vector<std::size_t>& parents,
			std::size_t root)
		{
			const auto pivot = tree.nodes[root].binary_children[0];
			if (pivot == no_visual_node)
			{
				throw std::logic_error(
					"Правый поворот красно-чёрного дерева не имеет левого узла.");
			}

			const auto old_parent = parents[root];
			std::size_t parent_side = 2;
			if (old_parent == no_visual_node)
			{
				if (tree.root != root)
				{
					throw std::logic_error(
						"Корень поворота красно-чёрного дерева потерял родителя.");
				}
			}
			else if (tree.nodes[old_parent].binary_children[0] == root)
			{
				parent_side = 0;
			}
			else if (tree.nodes[old_parent].binary_children[1] == root)
			{
				parent_side = 1;
			}
			else
			{
				throw std::logic_error(
					"Родитель узла поворота красно-чёрного дерева не ссылается на него.");
			}

			const auto pivot_right = tree.nodes[pivot].binary_children[1];
			set_binary_child(tree, root, 0, pivot_right);
			if (pivot_right != no_visual_node)
			{
				parents[pivot_right] = root;
			}
			set_binary_child(tree, pivot, 1, root);
			parents[root] = pivot;
			if (old_parent == no_visual_node)
			{
				tree.root = pivot;
			}
			else
			{
				set_binary_child(tree, old_parent, parent_side, pivot);
			}
			parents[pivot] = old_parent;
			return pivot;
		}

		static void fix_red_black_tree_after_insert(
			visual_tree& tree,
			std::vector<std::size_t>& parents,
			std::size_t node_index)
		{
			while (node_index != tree.root)
			{
				const auto parent = parents[node_index];
				if (parent == no_visual_node ||
					!red_black_node_is_red(tree, parent))
				{
					break;
				}

				const auto grandparent = parents[parent];
				if (grandparent == no_visual_node)
				{
					throw std::logic_error(
						"Красный родитель красно-чёрного дерева не имеет деда.");
				}
				const auto grandparent_left =
					tree.nodes[grandparent].binary_children[0];
				if (grandparent_left == parent)
				{
					const auto uncle =
						tree.nodes[grandparent].binary_children[1];
					if (red_black_node_is_red(tree, uncle))
					{
						set_red_black_node_color(tree, parent, false);
						set_red_black_node_color(tree, uncle, false);
						set_red_black_node_color(tree, grandparent, true);
						node_index = grandparent;
						continue;
					}

					if (tree.nodes[parent].binary_children[1] == node_index)
					{
						node_index = parent;
						(void)rotate_red_black_left(
							tree,
							parents,
							node_index);
					}
					const auto adjusted_parent = parents[node_index];
					if (adjusted_parent == no_visual_node)
					{
						throw std::logic_error(
							"Не удалось найти родителя при балансировке красно-чёрного дерева.");
					}
					const auto adjusted_grandparent =
						parents[adjusted_parent];
					if (adjusted_grandparent == no_visual_node)
					{
						throw std::logic_error(
							"Не удалось найти деда при балансировке красно-чёрного дерева.");
					}
					set_red_black_node_color(
						tree,
						adjusted_parent,
						false);
					set_red_black_node_color(
						tree,
						adjusted_grandparent,
						true);
					(void)rotate_red_black_right(
						tree,
						parents,
						adjusted_grandparent);
				}
				else if (tree.nodes[grandparent].binary_children[1] == parent)
				{
					const auto uncle =
						tree.nodes[grandparent].binary_children[0];
					if (red_black_node_is_red(tree, uncle))
					{
						set_red_black_node_color(tree, parent, false);
						set_red_black_node_color(tree, uncle, false);
						set_red_black_node_color(tree, grandparent, true);
						node_index = grandparent;
						continue;
					}

					if (tree.nodes[parent].binary_children[0] == node_index)
					{
						node_index = parent;
						(void)rotate_red_black_right(
							tree,
							parents,
							node_index);
					}
					const auto adjusted_parent = parents[node_index];
					if (adjusted_parent == no_visual_node)
					{
						throw std::logic_error(
							"Не удалось найти родителя при балансировке красно-чёрного дерева.");
					}
					const auto adjusted_grandparent =
						parents[adjusted_parent];
					if (adjusted_grandparent == no_visual_node)
					{
						throw std::logic_error(
							"Не удалось найти деда при балансировке красно-чёрного дерева.");
					}
					set_red_black_node_color(
						tree,
						adjusted_parent,
						false);
					set_red_black_node_color(
						tree,
						adjusted_grandparent,
						true);
					(void)rotate_red_black_left(
						tree,
						parents,
						adjusted_grandparent);
				}
				else
				{
					throw std::logic_error(
						"Красный родитель не является потомком своего деда.");
				}
			}
			set_red_black_node_color(tree, tree.root, false);
		}

		[[nodiscard]] static bool insert_red_black_key(
			visual_tree& tree,
			visual_key key,
			search_tree_key_kind kind)
		{
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"Red-black insertion received a B+ tree.");
			}
			if (tree.nodes.empty())
			{
				if (tree.root != no_visual_node)
				{
					throw std::invalid_argument(
						"Пустое красно-чёрное дерево содержит неверный индекс корня.");
				}
				key.detail = "1";
				visual_node root;
				root.keys.push_back(std::move(key));
				root.leaf = true;
				root.red = false;
				tree.nodes.push_back(std::move(root));
				tree.root = 0;
				return true;
			}
			if (tree.root >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Корень красно-чёрного дерева ссылается на отсутствующий узел.");
			}

			auto parents = build_binary_parent_index(tree);
			std::size_t current = tree.root;
			std::size_t depth = 0;
			std::size_t parent = no_visual_node;
			std::size_t side = 0;
			while (true)
			{
				if (current >= tree.nodes.size())
				{
					throw std::invalid_argument(
						"Красно-чёрное дерево ссылается на отсутствующий потомок.");
				}
				const auto& node = tree.nodes[current];
				if (node.keys.size() != 1)
				{
					throw std::invalid_argument(
						"Узел красно-чёрного дерева должен содержать ровно один ключ.");
				}
				const int comparison = compare_search_tree_keys(
					key.value,
					node.keys.front().value,
					kind);
				if (comparison == 0)
				{
					return false;
				}

				side = comparison < 0 ? 0 : 1;
				const auto child = node.binary_children[side];
				if (child == no_visual_node)
				{
					if (depth + 1 >=
						tree_serialization_parser::maximum_depth)
					{
						throw std::length_error(
							"Вставка превысила максимальную глубину красно-чёрного дерева.");
					}
					if (tree.nodes.size() >=
						tree_serialization_parser::maximum_node_count)
					{
						throw std::length_error(
							"Достигнут лимит в 1 000 000 узлов.");
					}
					parent = current;
					break;
				}
				current = child;
				++depth;
			}

			const std::size_t inserted_index = tree.nodes.size();
			key.detail = "0";
			visual_node inserted_node;
			inserted_node.keys.push_back(std::move(key));
			inserted_node.leaf = true;
			inserted_node.red = true;
			tree.nodes.push_back(std::move(inserted_node));
			parents.push_back(parent);
			set_binary_child(tree, parent, side, inserted_index);
			fix_red_black_tree_after_insert(
				tree,
				parents,
				inserted_index);
			return true;
		}

		static void record_splay_animation_event(
			std::vector<tree_visual_event>& events,
			bool& capture_available,
			tree_visual_event step)
		{
			if (!capture_available)
			{
				return;
			}
			if (events.size() >= maximum_tree_animation_steps)
			{
				events.clear();
				capture_available = false;
				return;
			}
			events.push_back(std::move(step));
		}

		static void rotate_splay_node_up(
			visual_tree& tree,
			std::size_t parent,
			std::size_t child,
			std::size_t grandparent,
			std::vector<tree_visual_event>* events = nullptr,
			bool* capture_available = nullptr)
		{
			std::size_t parent_side = 2;
			if (tree.nodes[parent].binary_children[0] == child)
			{
				parent_side = 0;
			}
			else if (tree.nodes[parent].binary_children[1] == child)
			{
				parent_side = 1;
			}
			else
			{
				throw std::logic_error(
					"Splay-поворот получил узел, не являющийся потомком родителя.");
			}

			std::size_t grandparent_side = 2;
			if (grandparent == no_visual_node)
			{
				if (tree.root != parent)
				{
					throw std::logic_error(
						"Родитель корневого Splay-поворота не является корнем дерева.");
				}
			}
			else if (tree.nodes[grandparent].binary_children[0] == parent)
			{
				grandparent_side = 0;
			}
			else if (tree.nodes[grandparent].binary_children[1] == parent)
			{
				grandparent_side = 1;
			}
			else
			{
				throw std::logic_error(
					"Дед узла Splay-поворота не ссылается на родителя.");
			}

			const auto moved_subtree =
				tree.nodes[child].binary_children[1 - parent_side];
			set_binary_child(
				tree,
				parent,
				parent_side,
				moved_subtree);
			set_binary_child(
				tree,
				child,
				1 - parent_side,
				parent);
			if (grandparent == no_visual_node)
			{
				tree.root = child;
			}
			else
			{
				set_binary_child(
					tree,
					grandparent,
					grandparent_side,
					child);
			}
			if (events && capture_available && *capture_available)
			{
				record_splay_animation_event(
					*events,
					*capture_available,
					tree_visual_event{
						tree_visual_event::kind::rotate_node_up,
						child,
						parent,
						grandparent
					});
			}
		}

		static void splay_node_to_root(
			visual_tree& tree,
			std::vector<std::size_t>& ancestors,
			std::size_t node_index,
			std::vector<tree_visual_event>* events = nullptr,
			bool* capture_available = nullptr)
		{
			while (!ancestors.empty())
			{
				const auto parent = ancestors.back();
				ancestors.pop_back();
				if (ancestors.empty())
				{
					rotate_splay_node_up(
						tree,
						parent,
						node_index,
						no_visual_node,
						events,
						capture_available);
					break;
				}

				const auto grandparent = ancestors.back();
				ancestors.pop_back();
				const auto great_grandparent = ancestors.empty()
					? no_visual_node
					: ancestors.back();

				std::size_t parent_side = 2;
				if (tree.nodes[grandparent].binary_children[0] == parent)
				{
					parent_side = 0;
				}
				else if (tree.nodes[grandparent].binary_children[1] == parent)
				{
					parent_side = 1;
				}
				else
				{
					throw std::logic_error(
						"Splay-путь потерял связь между родителем и дедом.");
				}

				std::size_t node_side = 2;
				if (tree.nodes[parent].binary_children[0] == node_index)
				{
					node_side = 0;
				}
				else if (tree.nodes[parent].binary_children[1] == node_index)
				{
					node_side = 1;
				}
				else
				{
					throw std::logic_error(
						"Splay-путь потерял связь между узлом и родителем.");
				}

				if (parent_side == node_side)
				{
					rotate_splay_node_up(
						tree,
						grandparent,
						parent,
						great_grandparent,
						events,
						capture_available);
					rotate_splay_node_up(
						tree,
						parent,
						node_index,
						great_grandparent,
						events,
						capture_available);
				}
				else
				{
					rotate_splay_node_up(
						tree,
						parent,
						node_index,
						grandparent,
						events,
						capture_available);
					rotate_splay_node_up(
						tree,
						grandparent,
						node_index,
						great_grandparent,
						events,
						capture_available);
				}
			}

			if (tree.root != node_index)
			{
				throw std::logic_error(
					"Splay-операция не подняла целевой узел в корень.");
			}
		}

		[[nodiscard]] static tree_operation_result insert_splay_key(
			visual_tree& tree,
			visual_key key,
			search_tree_key_kind kind,
			bool collect_animation_events)
		{
			tree_operation_result result;
			bool capture_available = collect_animation_events;
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"Splay insertion received a B+ tree.");
			}
			if (tree.nodes.empty())
			{
				if (tree.root != no_visual_node)
				{
					throw std::invalid_argument(
						"Пустое Splay-дерево содержит неверный индекс корня.");
				}
				visual_node root;
				root.keys.push_back(std::move(key));
				root.leaf = true;
				tree.nodes.push_back(std::move(root));
				tree.root = 0;
				result.inserted = true;
				result.changed = true;
				if (collect_animation_events)
				{
					record_splay_animation_event(
						result.events,
						capture_available,
						tree_visual_event{
							tree_visual_event::kind::insert_node,
							0,
							no_visual_node,
							no_visual_node,
							0,
							tree.nodes.front()
						});
				}
				result.animation_events_complete = capture_available;
				return result;
			}
			if (tree.root >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Корень Splay-дерева ссылается на отсутствующий узел.");
			}

			std::vector<std::size_t> ancestors;
			std::size_t current = tree.root;
			std::size_t depth = 0;
			while (true)
			{
				if (current >= tree.nodes.size())
				{
					throw std::invalid_argument(
						"Splay-дерево ссылается на отсутствующего потомка.");
				}
				const auto& node = tree.nodes[current];
				if (node.keys.size() != 1)
				{
					throw std::invalid_argument(
						"Узел Splay-дерева должен содержать ровно один ключ.");
				}
				const int comparison = compare_search_tree_keys(
					key.value,
					node.keys.front().value,
					kind);
				if (comparison == 0)
				{
					const bool changed = current != tree.root;
					splay_node_to_root(
						tree,
						ancestors,
						current,
						collect_animation_events
							? &result.events
							: nullptr,
						&capture_available);
					result.inserted = false;
					result.changed = changed;
					result.animation_events_complete = capture_available;
					return result;
				}

				const std::size_t side = comparison < 0 ? 0 : 1;
				const auto child = node.binary_children[side];
				if (child == no_visual_node)
				{
					if (depth + 1 >=
						tree_serialization_parser::maximum_depth)
					{
						throw std::length_error(
							"Вставка превысила максимальную глубину Splay-дерева.");
					}
					if (tree.nodes.size() >=
						tree_serialization_parser::maximum_node_count)
					{
						throw std::length_error(
							"Достигнут лимит в 1 000 000 узлов.");
					}

					const auto inserted_index = tree.nodes.size();
					visual_node inserted_node;
					inserted_node.keys.push_back(std::move(key));
					inserted_node.leaf = true;
					tree.nodes.push_back(std::move(inserted_node));
					set_binary_child(tree, current, side, inserted_index);
					if (collect_animation_events)
					{
						record_splay_animation_event(
							result.events,
							capture_available,
							tree_visual_event{
								tree_visual_event::kind::insert_node,
								inserted_index,
								current,
								no_visual_node,
								side,
								tree.nodes[inserted_index]
							});
					}
					ancestors.push_back(current);
					splay_node_to_root(
						tree,
						ancestors,
						inserted_index,
						collect_animation_events
							? &result.events
							: nullptr,
						&capture_available);
					result.inserted = true;
					result.changed = true;
					result.animation_events_complete = capture_available;
					return result;
				}
				ancestors.push_back(current);
				current = child;
				++depth;
			}
		}

		[[nodiscard]] static bool insert_bst_key(
			visual_tree& tree,
			visual_key key,
			search_tree_key_kind kind)
		{
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"BST insertion received a B+ tree.");
			}
			if (tree.nodes.empty())
			{
				visual_node root;
				root.keys.push_back(std::move(key));
				root.leaf = true;
				tree.nodes.push_back(std::move(root));
				tree.root = 0;
				return true;
			}
			if (tree.root >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Корень BST ссылается на отсутствующий узел.");
			}

			std::size_t current = tree.root;
			std::size_t depth = 0;
			while (true)
			{
				const auto& node = tree.nodes[current];
				if (node.keys.size() != 1)
				{
					throw std::invalid_argument(
						"Узел BST должен содержать ровно один ключ.");
				}
				const int comparison = compare_search_tree_keys(
					key.value,
					node.keys.front().value,
					kind);
				if (comparison == 0)
				{
					return false;
				}

				const std::size_t side = comparison < 0 ? 0 : 1;
				const std::size_t child = node.binary_children[side];
				if (child == no_visual_node)
				{
					if (depth + 1 >= tree_serialization_parser::maximum_depth)
					{
						throw std::length_error(
							"Вставка превысила максимальную глубину BST.");
					}
					if (tree.nodes.size() >=
						tree_serialization_parser::maximum_node_count)
					{
						throw std::length_error(
							"Достигнут лимит в 1 000 000 узлов.");
					}

					const std::size_t new_node_index = tree.nodes.size();
					auto updated_binary_children = node.binary_children;
					updated_binary_children[side] = new_node_index;
					std::vector<std::size_t> updated_children;
					updated_children.reserve(2);
					if (updated_binary_children[0] != no_visual_node)
					{
						updated_children.push_back(
							updated_binary_children[0]);
					}
					if (updated_binary_children[1] != no_visual_node)
					{
						updated_children.push_back(
							updated_binary_children[1]);
					}

					visual_node inserted_node;
					inserted_node.keys.push_back(std::move(key));
					inserted_node.leaf = true;
					tree.nodes.push_back(std::move(inserted_node));

					auto& parent = tree.nodes[current];
					parent.binary_children = updated_binary_children;
					parent.children = std::move(updated_children);
					parent.leaf = false;
					return true;
				}
				if (child >= tree.nodes.size())
				{
					throw std::invalid_argument(
						"Узел BST ссылается на отсутствующего потомка.");
				}
				current = child;
				++depth;
			}
		}

		static void append_serialized_bst_key(
			std::string& output,
			const visual_key::value_type& value)
		{
			if (const auto* text = std::get_if<std::string>(&value))
			{
				output.push_back('"');
				for (const char character : *text)
				{
					switch (character)
					{
					case '"':
						output += "\\\"";
						break;
					case '\\':
						output += "\\\\";
						break;
					case '\n':
						output += "\\n";
						break;
					case '\r':
						output += "\\r";
						break;
					case '\t':
						output += "\\t";
						break;
					case '\0':
						output += "\\0";
						break;
					default:
						output.push_back(character);
						break;
					}
				}
				output.push_back('"');
				return;
			}

			if (const auto* big_integer =
				std::get_if<visual_key::big_integer_text>(&value))
			{
				output += normalize_integer_literal(big_integer->decimal);
				return;
			}

			std::string serialized_value = full_visual_value(value);
			if (std::holds_alternative<double>(value) &&
				serialized_value.find_first_of(".eE") == std::string::npos)
			{
				serialized_value += ".0";
			}
			output += serialized_value;
		}

		enum class search_tree_serialization_metadata
		{
			none,
			avl_height,
			red_black_color
		};

		static void append_serialized_search_tree_subtree(
			const visual_tree& tree,
			std::size_t node_index,
			std::size_t depth,
			std::string& output,
			search_tree_serialization_metadata metadata)
		{
			if (node_index == no_visual_node)
			{
				output.push_back('#');
				return;
			}
			if (depth >= tree_serialization_parser::maximum_depth ||
				node_index >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Структура дерева поиска превышает допустимую глубину или содержит неверную ссылку.");
			}

			const auto& node = tree.nodes[node_index];
			if (node.keys.size() != 1)
			{
				throw std::invalid_argument(
					"Узел дерева поиска должен содержать ровно один ключ.");
			}
			output.push_back('(');
			append_serialized_bst_key(output, node.keys.front().value);
			if (metadata != search_tree_serialization_metadata::none)
			{
				if (metadata ==
					search_tree_serialization_metadata::avl_height)
				{
					(void)avl_height(tree, node_index);
				}
				else if (metadata ==
					search_tree_serialization_metadata::red_black_color)
				{
					(void)red_black_node_is_red(tree, node_index);
				}
				output.push_back('|');
				output += node.keys.front().detail;
			}
			output += ",,";
			append_serialized_search_tree_subtree(
				tree,
				node.binary_children[0],
				depth + 1,
				output,
				metadata);
			output.push_back(',');
			append_serialized_search_tree_subtree(
				tree,
				node.binary_children[1],
				depth + 1,
				output,
				metadata);
			output.push_back(')');
		}

		[[nodiscard]] static std::string serialize_bst_tree(
			const visual_tree& tree)
		{
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"Невозможно сериализовать B+ дерево как BST.");
			}
			std::string serialization = "bst{";
			append_serialized_search_tree_subtree(
				tree,
				tree.root,
				0,
				serialization,
				search_tree_serialization_metadata::none);
			serialization.push_back('}');
			return serialization;
		}

		[[nodiscard]] static std::string serialize_avl_tree(
			const visual_tree& tree)
		{
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"Невозможно сериализовать B+ дерево как AVL.");
			}
			std::string serialization = "avl{";
			append_serialized_search_tree_subtree(
				tree,
				tree.root,
				0,
				serialization,
				search_tree_serialization_metadata::avl_height);
			serialization.push_back('}');
			return serialization;
		}

		[[nodiscard]] static std::string serialize_red_black_tree(
			const visual_tree& tree)
		{
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"Невозможно сериализовать B+ дерево как красно-чёрное.");
			}
			std::string serialization = "rb_tree{";
			append_serialized_search_tree_subtree(
				tree,
				tree.root,
				0,
				serialization,
				search_tree_serialization_metadata::red_black_color);
			serialization.push_back('}');
			return serialization;
		}

		[[nodiscard]] static std::string serialize_splay_tree(
			const visual_tree& tree)
		{
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"Невозможно сериализовать B+ дерево как Splay.");
			}
			std::string serialization = "splay{";
			append_serialized_search_tree_subtree(
				tree,
				tree.root,
				0,
				serialization,
				search_tree_serialization_metadata::none);
			serialization.push_back('}');
			return serialization;
		}

		static const std::array<visual_tree, 6>& demo_trees()
		{
			static const std::array<visual_tree, 6> trees = []
			{
				std::array<visual_tree, 6> result{};
				constexpr std::array<std::int64_t, 7> integer_values{
					42, 18, 73, 9, 27, 58, 91
				};
				constexpr std::array<double, 7> floating_values{
					42.125, 18.75, 73.5, 9.25, 27.625, 58.875, 91.0
				};
				constexpr std::array<std::string_view, 7> text_values{
					"root", "alpha", "omega", "a-very-long-key-name",
					"beta", "middle", "zeta"
				};
				constexpr std::array<std::string_view, 7> big_integer_values{
					"42000000000000000000",
					"18000000000000000000",
					"73000000000000000000",
					"9000000000000000000",
					"27000000000000000000",
					"58000000000000000000",
					"91000000000000000000"
				};
				constexpr std::array<std::string_view, 7> priorities{
					"p=90", "p=70", "p=80", "p=40", "p=50", "p=60", "p=30"
				};
				constexpr std::array<std::array<int, 2>, 7> binary_children{ {
					{ 1, 2 }, { 3, 4 }, { 5, 6 }, { -1, -1 },
					{ -1, -1 }, { -1, -1 }, { -1, -1 }
				} };

				for (std::size_t tree_index = 0; tree_index < 5; ++tree_index)
				{
					auto& tree = result[tree_index];
					for (std::size_t i = 0; i < integer_values.size(); ++i)
					{
						visual_node node;
						switch (tree_index)
						{
						case 1:
							node.keys.push_back(
								make_double_key(floating_values[i]));
							break;
						case 3:
							node.keys.push_back(
								make_text_key(text_values[i]));
							break;
						case 4:
							node.keys.push_back(
								make_big_integer_key(
									big_integer_values[i],
									priorities[i]));
							break;
						default:
							node.keys.push_back(
								make_integer_key(integer_values[i]));
							break;
						}
						for (std::size_t side = 0; side < 2; ++side)
						{
							const int child = binary_children[i][side];
							if (child >= 0)
							{
								node.binary_children[side] =
									static_cast<std::size_t>(child);
								node.children.push_back(
									static_cast<std::size_t>(child));
							}
						}
						node.leaf = node.children.empty();
						node.red = tree_index == 2 && (i == 1 || i == 2);
						tree.nodes.push_back(std::move(node));
					}
				}

				auto& b_plus = result[5];
				b_plus.b_plus = true;
				visual_node root;
				root.keys = {
					make_big_integer_key("20000000000000000000"),
					make_big_integer_key("40000000000000000000")
				};
				root.children = { 1, 2, 3 };
				b_plus.nodes.push_back(std::move(root));

				visual_node first_leaf;
				first_leaf.keys = {
					make_big_integer_key("5000000000000000000"),
					make_big_integer_key("10000000000000000000")
				};
				first_leaf.leaf = true;
				first_leaf.next_leaf = 2;
				b_plus.nodes.push_back(std::move(first_leaf));

				visual_node second_leaf;
				second_leaf.keys = {
					make_big_integer_key("20000000000000000000"),
					make_big_integer_key("30000000000000000000")
				};
				second_leaf.leaf = true;
				second_leaf.next_leaf = 3;
				b_plus.nodes.push_back(std::move(second_leaf));

				visual_node third_leaf;
				third_leaf.keys = {
					make_big_integer_key("40000000000000000000"),
					make_big_integer_key("50000000000000000000"),
					make_big_integer_key("60000000000000000000")
				};
				third_leaf.leaf = true;
				b_plus.nodes.push_back(std::move(third_leaf));
				return result;
			}();
			return trees;
		}

		[[nodiscard]] static std::size_t count_visual_keys(
			const visual_tree& tree) noexcept
		{
			std::size_t count = 0;
			for (const auto& node : tree.nodes)
			{
				if (!tree.b_plus || node.leaf)
				{
					count += node.keys.size();
				}
			}
			return count;
		}

		float measure_visual_subtree(
			const visual_tree& tree,
			std::size_t node_index,
			std::vector<visual_layout_node>& layout) const
		{
			constexpr float child_gap = 36.0f;
			auto& node_layout = layout[node_index];
			const auto& node = tree.nodes[node_index];
			node_layout.cell_widths.clear();
			node_layout.display_values.clear();
			node_layout.display_details.clear();
			node_layout.value_scales.clear();
			node_layout.detail_scales.clear();

			const float cell_width = node.keys.empty()
				? visual_node_width
				: visual_node_width /
					static_cast<float>(node.keys.size());
			const float inner_width = std::max(1.0f, cell_width - 12.0f);
			for (const auto& key : node.keys)
			{
				node_layout.cell_widths.push_back(cell_width);
				auto display_value =
					format_visual_value(key.value, inner_width);
				const float value_width =
					visual_text_width(display_value);
				node_layout.value_scales.push_back(
					value_width > inner_width && value_width > 0.0f
						? std::max(0.35f, inner_width / value_width)
						: 1.0f);
				node_layout.display_values.push_back(
					std::move(display_value));

				auto display_detail = key.detail.empty()
					? std::string{}
					: clip_text_to_width(key.detail, inner_width);
				const float detail_width =
					visual_text_width(display_detail) * 0.68f;
				node_layout.detail_scales.push_back(
					detail_width > inner_width && detail_width > 0.0f
						? std::max(0.35f, inner_width / detail_width)
						: 1.0f);
				node_layout.display_details.push_back(
					std::move(display_detail));
			}
			node_layout.width = visual_node_width;

			float children_width = 0.0f;
			for (std::size_t i = 0; i < node.children.size(); ++i)
			{
				if (node.children[i] != no_visual_node)
				{
					(void)measure_visual_subtree(
						tree,
						node.children[i],
						layout);
				}
			}
			if (tree.b_plus)
			{
				for (std::size_t i = 0; i < node.children.size(); ++i)
				{
					const auto child = node.children[i];
					children_width += child == no_visual_node
						? visual_node_width
						: layout[child].subtree_width;
					if (i != 0)
					{
						children_width += child_gap;
					}
				}
			}
			else if (!node.children.empty())
			{
				const std::size_t left = node.binary_children[0];
				const std::size_t right = node.binary_children[1];
				const float left_slot = left == no_visual_node
					? visual_node_width
					: layout[left].subtree_width;
				const float right_slot = right == no_visual_node
					? visual_node_width
					: layout[right].subtree_width;
				children_width = left_slot + child_gap + right_slot;
			}
			node_layout.subtree_width = std::max(node_layout.width, children_width);
			return node_layout.subtree_width;
		}

		void place_visual_subtree(
			const visual_tree& tree,
			std::size_t node_index,
			float left,
			std::size_t depth,
			std::vector<visual_layout_node>& layout) const
		{
			constexpr float child_gap = 36.0f;
			auto& node_layout = layout[node_index];
			const auto& node = tree.nodes[node_index];
			node_layout.center_x = left + node_layout.subtree_width * 0.5f;
			node_layout.depth = depth;

			if (!tree.b_plus)
			{
				const std::size_t left_child = node.binary_children[0];
				const std::size_t right_child = node.binary_children[1];
				if (left_child == no_visual_node &&
					right_child == no_visual_node)
				{
					return;
				}

				const float left_slot = left_child == no_visual_node
					? visual_node_width
					: layout[left_child].subtree_width;
				const float right_slot = right_child == no_visual_node
					? visual_node_width
					: layout[right_child].subtree_width;
				const float children_width =
					left_slot + child_gap + right_slot;
				const float children_left =
					left + (node_layout.subtree_width - children_width) * 0.5f;
				if (left_child != no_visual_node)
				{
					place_visual_subtree(
						tree,
						left_child,
						children_left +
							(left_slot -
								layout[left_child].subtree_width) * 0.5f,
						depth + 1,
						layout);
				}
				if (right_child != no_visual_node)
				{
					place_visual_subtree(
						tree,
						right_child,
						children_left + left_slot + child_gap +
							(right_slot -
								layout[right_child].subtree_width) * 0.5f,
						depth + 1,
						layout);
				}
				return;
			}

			float children_width = 0.0f;
			for (std::size_t i = 0; i < node.children.size(); ++i)
			{
				const auto child = node.children[i];
				children_width += child == no_visual_node
					? visual_node_width
					: layout[child].subtree_width;
				if (i != 0)
				{
					children_width += child_gap;
				}
			}
			float child_left =
				left + (node_layout.subtree_width - children_width) * 0.5f;
			for (const auto child : node.children)
			{
				const float child_width = child == no_visual_node
					? visual_node_width
					: layout[child].subtree_width;
				if (child != no_visual_node)
				{
					place_visual_subtree(
						tree,
						child,
						child_left,
						depth + 1,
						layout);
				}
				child_left += child_width + child_gap;
			}
		}

		void invalidate_tree_preview_cache() noexcept
		{
			if (m_tree_preview_revision ==
				std::numeric_limits<std::uint64_t>::max())
			{
				m_tree_preview_revision = 1;
			}
			else
			{
				++m_tree_preview_revision;
			}
			for (auto& cache : m_tree_preview_caches)
			{
				cache.geometry.reset();
				cache.visibility.reset();
				cache.visibility_geometry.reset();
				cache.visibility_bounds = {};
				cache.visibility_request_geometry.reset();
				cache.visibility_request_id = 0;
				cache.visibility_request_bounds = {};
			}
		}

		[[nodiscard]] static std::string tree_animation_key_identity(
			const visual_key& key)
		{
			return std::format(
				"{}:{}",
				key.value.index(),
				full_visual_value(key.value));
		}

		[[nodiscard]] static std::uint64_t tree_animation_tree_signature(
			const visual_tree& tree)
		{
			std::uint64_t hash = 14695981039346656037ull;
			const auto mix_byte = [&hash](unsigned char byte)
			{
				hash ^= byte;
				hash *= 1099511628211ull;
			};
			const auto mix_integer = [&mix_byte](std::uint64_t value)
			{
				for (std::size_t i = 0; i < sizeof(value); ++i)
				{
					mix_byte(static_cast<unsigned char>(value & 0xffu));
					value >>= 8;
				}
			};
			mix_integer(tree.root);
			mix_integer(tree.fanout);
			mix_integer(tree.b_plus ? 1u : 0u);
			mix_integer(tree.nodes.size());
			for (const auto& node : tree.nodes)
			{
				mix_integer(node.keys.size());
				for (const auto& key : node.keys)
				{
					const auto identity = tree_animation_key_identity(key);
					mix_integer(identity.size());
					for (const unsigned char byte : identity)
					{
						mix_byte(byte);
					}
				}
				mix_integer(node.children.size());
				for (const auto child : node.children)
				{
					mix_integer(child);
				}
				for (const auto child : node.binary_children)
				{
					mix_integer(child);
				}
				mix_integer(node.next_leaf);
				mix_integer(node.leaf ? 1u : 0u);
				mix_integer(node.red ? 1u : 0u);
			}
			return hash;
		}

		[[nodiscard]] static std::vector<glm::dvec2>
			make_tree_layout_animation_start_centers(
				const visual_tree& before,
				const mngl::two_d::tree_layout& before_layout,
				const visual_tree& after)
		{
			if (before.nodes.size() != before_layout.nodes.size())
			{
				return {};
			}

			std::unordered_map<std::string, glm::dvec2> old_key_centers;
			for (std::size_t node_index = 0;
				node_index < before.nodes.size();
				++node_index)
			{
				for (const auto& key : before.nodes[node_index].keys)
				{
					old_key_centers.try_emplace(
						tree_animation_key_identity(key),
						before_layout.nodes[node_index].center);
				}
			}

			std::vector<std::size_t> parents(
				after.nodes.size(),
				no_visual_node);
			for (std::size_t parent = 0;
				parent < after.nodes.size();
				++parent)
			{
				const auto assign_parent =
					[&](std::size_t child)
				{
					if (child != no_visual_node &&
						child < parents.size())
					{
						parents[child] = parent;
					}
				};
				if (after.b_plus)
				{
					for (const auto child : after.nodes[parent].children)
					{
						assign_parent(child);
					}
				}
				else
				{
					for (const auto child :
						after.nodes[parent].binary_children)
					{
						assign_parent(child);
					}
				}
			}

			const double invalid_coordinate =
				std::numeric_limits<double>::quiet_NaN();
			std::vector<glm::dvec2> start_centers(
				after.nodes.size(),
				glm::dvec2{ invalid_coordinate, invalid_coordinate });
			for (std::size_t node_index = 0;
				node_index < after.nodes.size();
				++node_index)
			{
				glm::dvec2 matched_center{};
				std::size_t match_count = 0;
				for (const auto& key : after.nodes[node_index].keys)
				{
					const auto found = old_key_centers.find(
						tree_animation_key_identity(key));
					if (found != old_key_centers.end())
					{
						matched_center += found->second;
						++match_count;
					}
				}
				if (match_count != 0)
				{
					start_centers[node_index] =
						matched_center / static_cast<double>(match_count);
					continue;
				}

				std::size_t ancestor = parents[node_index];
				while (ancestor != no_visual_node &&
					ancestor < after.nodes.size())
				{
					for (const auto& key : after.nodes[ancestor].keys)
					{
						const auto found = old_key_centers.find(
							tree_animation_key_identity(key));
						if (found != old_key_centers.end())
						{
							start_centers[node_index] = found->second;
							break;
						}
					}
					if (std::isfinite(start_centers[node_index].x))
					{
						break;
					}
					ancestor = parents[ancestor];
				}

				if (!std::isfinite(start_centers[node_index].x) &&
					before.root < before_layout.nodes.size())
				{
					start_centers[node_index] =
						before_layout.nodes[before.root].center;
				}
			}
			return start_centers;
		}

		[[nodiscard]] static std::optional<std::size_t>
			find_animation_key_node(
				const visual_tree& tree,
				const visual_key& key)
		{
			const auto identity = tree_animation_key_identity(key);
			for (std::size_t pass = 0; pass < (tree.b_plus ? 2u : 1u); ++pass)
			{
				for (std::size_t node_index = 0;
					node_index < tree.nodes.size();
					++node_index)
				{
					if (tree.b_plus &&
						((pass == 0 && !tree.nodes[node_index].leaf) ||
							(pass == 1 && tree.nodes[node_index].leaf)))
					{
						continue;
					}
					if (std::any_of(
						tree.nodes[node_index].keys.begin(),
						tree.nodes[node_index].keys.end(),
						[&](const visual_key& candidate)
						{
							return tree_animation_key_identity(candidate) == identity;
						}))
					{
						return node_index;
					}
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] std::shared_ptr<const tree_preview_geometry>
			get_tree_preview_geometry(
				const visual_tree& tree,
				std::size_t tree_index,
				bool current_tree_loaded,
				bool showing_animation,
				std::size_t cache_index)
		{
			auto& cache = m_tree_preview_caches.at(cache_index);
			const std::size_t animation_step =
				showing_animation && m_tree_animation.has_value()
				? m_tree_animation->next_event
				: 0;
			if (cache.geometry &&
				cache.geometry->revision ==
					m_tree_preview_revision &&
				cache.geometry->tree_index ==
					static_cast<int>(tree_index) &&
				cache.geometry->current_tree_loaded ==
					current_tree_loaded &&
				cache.geometry->showing_animation ==
					showing_animation &&
				cache.geometry->animation_step == animation_step)
			{
				return cache.geometry;
			}

			auto geometry = std::make_shared<tree_preview_geometry>();
			geometry->revision = m_tree_preview_revision;
			geometry->source_tree_signature =
				tree.nodes.size() < maximum_animated_tree_nodes
				? tree_animation_tree_signature(tree)
				: 0;
			geometry->tree_index = static_cast<int>(tree_index);
			geometry->current_tree_loaded = current_tree_loaded;
			geometry->showing_animation = showing_animation;
			geometry->animation_step = animation_step;
			geometry->leaf_link_space = tree.b_plus ? 24.0f : 0.0f;
			geometry->model.root = tree.root;
			geometry->model.nodes.resize(tree.nodes.size());

			for (std::size_t i = 0; i < tree.nodes.size(); ++i)
			{
				const auto& source = tree.nodes[i];
				auto& target = geometry->model.nodes[i];
				target.width = visual_node_width;
				if (tree.b_plus)
				{
					target.style = source.leaf
						? mngl::two_d::tree_node_style::b_plus_leaf
						: mngl::two_d::tree_node_style::b_plus_internal;
					target.annotation = source.leaf ? "leaf" : "index";
					target.children = source.children;
				}
				else if (tree_index == 2)
				{
					target.style = source.red
						? mngl::two_d::tree_node_style::red
						: mngl::two_d::tree_node_style::black;
					target.annotation = source.red ? "red" : "black";
				}

				if (!tree.b_plus)
				{
					const std::size_t left = source.binary_children[0];
					const std::size_t right = source.binary_children[1];
					if (left != no_visual_node || right != no_visual_node)
					{
						target.children = { left, right };
					}
					else
					{
						target.children = source.children;
					}
				}
				target.next_leaf = source.next_leaf;

				const float cell_width = source.keys.empty()
					? visual_node_width
					: visual_node_width /
						static_cast<float>(source.keys.size());
				const float inner_width = std::max(1.0f, cell_width - 12.0f);
				if (!source.keys.empty())
				{
					target.cell_widths.assign(
						source.keys.size(),
						cell_width);
					target.cell_widths.back() =
						visual_node_width -
						cell_width * static_cast<float>(
							source.keys.size() - 1);
				}
				target.key_metadata.resize(source.keys.size());
				for (std::size_t key_index = 0;
					key_index < source.keys.size();
					++key_index)
				{
					const auto& key = source.keys[key_index];
					target.keys.push_back(
						format_visual_value(key.value, inner_width));
					if (key.detail.empty() ||
						(tree_index == 2 && !tree.b_plus))
					{
						continue;
					}

					std::string metadata = key.detail;
					if (tree_index == 1 && !metadata.starts_with("h="))
					{
						metadata.insert(0, "h=");
					}
					else if (tree_index == 4 &&
						!metadata.starts_with("p="))
					{
						metadata.insert(0, "p=");
					}
					target.key_metadata[key_index] =
						clip_text_to_width(metadata, inner_width);
				}
			}

			auto& options = geometry->draw_options;
			options.node_size = {
				visual_node_width,
				visual_node_height
			};
			options.horizontal_gap = 36.0f;
			options.vertical_gap = 42.0f;
			options.edge_width = 2.0f;
			options.corner_radius = 7.0f;
			options.border_width = 1.5f;
			options.value_text_height = 16.0f;
			options.metadata_text_height = 10.5f;
			options.show_leaf_links = tree.b_plus;
			const auto rgb = [](float red, float green, float blue)
			{
				return mngl::color{
					red / 255.0f,
					green / 255.0f,
					blue / 255.0f,
					1.0f
				};
			};
			options.edge_color = rgb(114.0f, 128.0f, 148.0f);
			options.node_border = rgb(111.0f, 174.0f, 215.0f);
			options.node_fill = tree_index == 3
				? rgb(53.0f, 49.0f, 94.0f)
				: tree_index == 4
					? rgb(62.0f, 48.0f, 87.0f)
					: rgb(35.0f, 57.0f, 77.0f);
			options.red_node_fill = rgb(147.0f, 47.0f, 57.0f);
			options.black_node_fill = rgb(31.0f, 36.0f, 46.0f);
			options.b_plus_leaf_fill = rgb(34.0f, 83.0f, 76.0f);
			options.b_plus_internal_fill = rgb(39.0f, 57.0f, 91.0f);
			options.value_color = rgb(240.0f, 245.0f, 250.0f);
			options.metadata_color = rgb(220.0f, 202.0f, 255.0f);
			options.highlight_border = rgb(255.0f, 223.0f, 128.0f);
			options.leaf_link_color = rgb(74.0f, 210.0f, 190.0f);
			geometry->layout = mngl::two_d::measure_tree_layout(
				geometry->model,
				options);
			geometry->tree_width =
				geometry->layout.size.x + 32.0f * 2.0f;
			geometry->tree_height =
				geometry->layout.size.y +
				48.0f * 2.0f +
				geometry->leaf_link_space;

			cache.geometry = geometry;
			cache.visibility.reset();
			cache.visibility_geometry.reset();
			cache.visibility_bounds = {};
			return geometry;
		}

		[[nodiscard]] static bool rectangle_contains(
			const mngl::two_d::tree_draw_area& outer,
			const mngl::two_d::tree_draw_area& inner) noexcept
		{
			const double outer_left = outer.pos.x;
			const double outer_bottom = outer.pos.y;
			const double outer_right = outer_left + outer.size.x;
			const double outer_top = outer_bottom + outer.size.y;
			const double inner_left = inner.pos.x;
			const double inner_bottom = inner.pos.y;
			const double inner_right = inner_left + inner.size.x;
			const double inner_top = inner_bottom + inner.size.y;
			return inner_left >= outer_left &&
				inner_bottom >= outer_bottom &&
				inner_right <= outer_right &&
				inner_top <= outer_top;
		}

		[[nodiscard]] std::shared_ptr<const mngl::two_d::tree_draw_visibility>
			get_tree_preview_visibility(
				std::size_t cache_index,
				const std::shared_ptr<const tree_preview_geometry>& geometry,
				const mngl::two_d::tree_draw_area& area,
				const mngl::two_d::tree_draw_area& viewport,
				const mngl::two_d::tree_draw_options& draw_options)
		{
			auto& cache = m_tree_preview_caches.at(cache_index);
			const float arrow_size = std::max(
				draw_options.node_size.y * 0.08f,
				draw_options.edge_width * 2.5f);
			const double visibility_padding = std::max(
				draw_options.border_width * 0.5f,
				draw_options.edge_width * 0.5f + arrow_size);
			const glm::dvec2 viewport_padding(visibility_padding);
			const mngl::two_d::tree_draw_area padded_viewport{
				viewport.pos - viewport_padding,
				viewport.size + viewport_padding * 2.0
			};
			const glm::dvec2 overscan{
				std::max(
					viewport.size.x * 0.25f,
					visibility_padding),
				std::max(
					viewport.size.y * 0.25f,
					visibility_padding)
			};
			const mngl::two_d::tree_draw_area cull_view{
				viewport.pos - overscan,
				viewport.size + overscan * 2.0
			};
			const auto visibility_bounds_are_usable =
				[&](const mngl::two_d::tree_draw_area& bounds)
			{
				constexpr double maximum_size_ratio = 1.25;
				return rectangle_contains(bounds, padded_viewport) &&
					static_cast<double>(bounds.size.x) <=
						static_cast<double>(cull_view.size.x) *
							maximum_size_ratio &&
					static_cast<double>(bounds.size.y) <=
						static_cast<double>(cull_view.size.y) *
							maximum_size_ratio;
			};
			auto completed = cache.culler.take_completed_result();
			if (completed &&
				completed->request_id ==
					cache.visibility_request_id &&
				cache.visibility_request_geometry &&
				completed->source.get() ==
					cache.visibility_request_geometry.get())
			{
				const auto requested_geometry =
					cache.visibility_request_geometry;
				const auto requested_bounds =
					cache.visibility_request_bounds;
				cache.visibility_request_geometry.reset();
				cache.visibility_request_id = 0;
				cache.visibility_request_bounds = {};

				if (requested_geometry == geometry)
				{
					if (completed->error)
					{
						std::rethrow_exception(completed->error);
					}

					const bool cached_visibility_is_usable =
						cache.visibility &&
						cache.visibility_geometry == geometry &&
						visibility_bounds_are_usable(
							cache.visibility_bounds);
					if (!cached_visibility_is_usable &&
						visibility_bounds_are_usable(requested_bounds))
					{
						cache.visibility =
							std::make_shared<const
								mngl::two_d::tree_draw_visibility>(
									std::move(completed->visibility));
						cache.visibility_geometry = geometry;
						cache.visibility_bounds =
							requested_bounds;
					}
				}
			}

			const bool has_visibility_for_geometry =
				cache.visibility &&
				cache.visibility_geometry == geometry;
			if (has_visibility_for_geometry &&
				visibility_bounds_are_usable(
					cache.visibility_bounds))
			{
				return cache.visibility;
			}

			if (!has_visibility_for_geometry)
			{
				cache.visibility_request_geometry.reset();
				cache.visibility_request_id = 0;
				cache.visibility_request_bounds = {};
				auto visibility = cache.culler.cull(
					geometry->model,
					geometry->layout,
					area,
					cull_view,
					draw_options);
				cache.visibility =
					std::make_shared<const
						mngl::two_d::tree_draw_visibility>(
							std::move(visibility));
				cache.visibility_geometry = geometry;
				cache.visibility_bounds = cull_view;
				return cache.visibility;
			}

			const bool pending_request_covers_view =
				cache.visibility_request_geometry == geometry &&
				cache.visibility_request_id != 0 &&
				visibility_bounds_are_usable(
					cache.visibility_request_bounds);
			if (!pending_request_covers_view)
			{
				cache.visibility_request_id =
					cache.culler.request_async(
						geometry,
						geometry->model,
						geometry->layout,
						area,
						cull_view,
						draw_options);
				cache.visibility_request_geometry = geometry;
				cache.visibility_request_bounds = cull_view;
			}
			static const auto empty_visibility =
				std::make_shared<const
					mngl::two_d::tree_draw_visibility>();
			return empty_visibility;
		}

		void draw_tree_preview(
			std::size_t tree_index,
			tree_canvas_view_state* canvas_state = nullptr,
			const visual_tree* tree_override = nullptr,
			std::string_view archive_path = {},
			std::size_t cache_index = 0,
			const char* child_id = "TreePreview",
			bool* canvas_clicked = nullptr)
		{
			if (canvas_clicked)
			{
				*canvas_clicked = false;
			}
			auto& canvas_texture = canvas_state
				? canvas_state->texture
				: m_tree_canvas_texture;
			auto& canvas_tree_index = canvas_state
				? canvas_state->tree_index
				: m_tree_canvas_tree_index;
			auto& canvas_zoom = canvas_state
				? canvas_state->zoom
				: m_tree_canvas_zoom;
			auto& canvas_pan = canvas_state
				? canvas_state->pan
				: m_tree_canvas_pan;
			auto& canvas_view_initialized = canvas_state
				? canvas_state->view_initialized
				: m_tree_canvas_view_initialized;
			auto& canvas_fit_requested = canvas_state
				? canvas_state->fit_requested
				: m_tree_canvas_fit_requested;
			auto& context_menu_key = canvas_state
				? canvas_state->context_menu_key
				: m_tree_canvas_context_menu_key;
			auto& context_menu_node = canvas_state
				? canvas_state->context_menu_node
				: m_tree_canvas_context_menu_node;
			if (!ImGui::BeginChild(
				child_id,
				ImVec2(0.0f, 0.0f),
				ImGuiChildFlags_Borders))
			{
				ImGui::EndChild();
				return;
			}

			if (!tree_override &&
				m_current_tree_name.empty() &&
				!m_show_demo_trees)
			{
				ImGui::TextDisabled(
					"Рабочая область пуста. Создайте новое дерево или загрузите .tree.");
				ImGui::EndChild();
				return;
			}

			const bool current_tree_loaded = tree_override
				? true
				: m_loaded_visual_tree.has_value() &&
					m_loaded_tree_index == static_cast<int>(tree_index);
			const bool preview_tree_is_active =
				!tree_override ||
				archive_path == m_current_archive_member_path;
			if (preview_tree_is_active)
			{
				advance_tree_animation(
					static_cast<int>(tree_index),
					ImGui::GetIO().DeltaTime);
			}
			const bool showing_animation =
				preview_tree_is_active &&
				current_tree_loaded &&
				m_tree_animation.has_value() &&
				m_tree_animation->tree_index ==
					static_cast<int>(tree_index);
			const auto& tree = showing_animation
				? m_tree_animation->tree
				: tree_override
					? *tree_override
				: current_tree_loaded
					? *m_loaded_visual_tree
					: demo_trees()[tree_index];
			if (tree.nodes.empty())
			{
				ImGui::TextDisabled(
					current_tree_loaded
						? "Загруженное дерево пустое."
						: "В демо-дереве пока нет узлов.");
				const ImVec2 empty_canvas_size =
					ImGui::GetContentRegionAvail();
				if (empty_canvas_size.x > 0.0f &&
					empty_canvas_size.y > 0.0f)
				{
					ImGui::InvisibleButton(
						"EmptyTreeCanvas",
						empty_canvas_size,
						ImGuiButtonFlags_MouseButtonLeft);
					if (canvas_clicked)
					{
						*canvas_clicked = ImGui::IsItemClicked(
							ImGuiMouseButton_Left);
					}
				}
				ImGui::EndChild();
				return;
			}

			if (tree.b_plus)
			{
				ImGui::TextDisabled(
					"B+: синие узлы — индексы, зелёные — листья, стрелки — next.");
			}
			if (showing_animation)
			{
				if (m_tree_animation->mode ==
					tree_animation_mode::layout_transition)
				{
					const float progress = std::clamp(
						m_tree_animation->elapsed_seconds /
							tree_layout_animation_seconds,
						0.0f,
						1.0f);
					ImGui::TextDisabled(
						"Перестройка раскладки: %.0f%%.",
						static_cast<double>(progress) * 100.0);
				}
				else
				{
					ImGui::TextDisabled(
						"Вставка Splay: шаг %zu из %zu.",
						m_tree_animation->next_event,
						m_tree_animation->events.size());
				}
			}

			const auto geometry = get_tree_preview_geometry(
				tree,
				tree_index,
				current_tree_loaded,
				showing_animation,
				cache_index);
			const auto& render_model = geometry->model;
			mngl::two_d::tree_layout animated_layout;
			const mngl::two_d::tree_layout* layout_to_draw =
				std::addressof(geometry->layout);
			const bool interpolating_layout =
				showing_animation &&
				m_tree_animation->mode ==
					tree_animation_mode::layout_transition &&
				m_tree_animation->transition_start_centers.size() ==
					geometry->layout.nodes.size();
			if (interpolating_layout)
			{
				animated_layout = geometry->layout;
				const double progress = std::clamp(
					static_cast<double>(
						m_tree_animation->elapsed_seconds) /
						static_cast<double>(
							tree_layout_animation_seconds),
					0.0,
					1.0);
				const double eased_progress =
					progress * progress * (3.0 - 2.0 * progress);
				const glm::dvec2 layout_shift =
					(geometry->layout.size -
						m_tree_animation->transition_start_layout_size) * 0.5;
				for (std::size_t i = 0;
					i < animated_layout.nodes.size();
					++i)
				{
					glm::dvec2 start =
						m_tree_animation->transition_start_centers[i];
					if (!std::isfinite(start.x) || !std::isfinite(start.y))
					{
						start = animated_layout.nodes[i].center +
							glm::dvec2{
								0.0,
							-static_cast<double>(visual_node_height)
							};
					}
					else
					{
						start += layout_shift;
					}
					const glm::dvec2 target =
						geometry->layout.nodes[i].center;
					animated_layout.nodes[i].center = start +
						(target - start) * eased_progress;
				}
				layout_to_draw = std::addressof(animated_layout);
			}
			const auto& layout = *layout_to_draw;
			const double tree_width = geometry->tree_width;
			const double tree_height = geometry->tree_height;
			const float leaf_link_space = geometry->leaf_link_space;
			auto render_options = geometry->draw_options;
			constexpr float horizontal_margin = 32.0f;
			constexpr float vertical_margin = 48.0f;

			const std::string_view banner = current_tree_loaded
				? "LOADED TREE"
				: m_tree_parse_error.empty()
					? "DEMO DATA"
					: "PARSE ERROR";
			const ImVec2 canvas_min = ImGui::GetCursorScreenPos();
			const ImVec2 canvas_size = ImGui::GetContentRegionAvail();
			if (canvas_size.x > 0.0f && canvas_size.y > 0.0f)
			{
				(void)ImGui::InvisibleButton(
					"TreeCanvas",
					canvas_size,
					ImGuiButtonFlags_MouseButtonLeft |
						ImGuiButtonFlags_MouseButtonMiddle |
						ImGuiButtonFlags_MouseButtonRight);
				const bool canvas_hovered = ImGui::IsItemHovered();
				const bool canvas_active = ImGui::IsItemActive();
				if (canvas_clicked)
				{
					*canvas_clicked =
						ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
						ImGui::IsItemClicked(ImGuiMouseButton_Middle);
				}
				const ImGuiIO& io = ImGui::GetIO();
				const float scale_x =
					std::isfinite(io.DisplayFramebufferScale.x) &&
						io.DisplayFramebufferScale.x > 0.0f
					? io.DisplayFramebufferScale.x
					: 1.0f;
				const float scale_y =
					std::isfinite(io.DisplayFramebufferScale.y) &&
						io.DisplayFramebufferScale.y > 0.0f
					? io.DisplayFramebufferScale.y
					: 1.0f;
				const double minimum_framebuffer_scale =
					std::min(scale_x, scale_y);
				const auto minimum_safe_zoom =
					[&](const glm::dvec2& pan, double maximum_zoom)
				{
					const double safe_world =
						static_cast<double>(
							std::numeric_limits<float>::max()) /
						64.0;
					const double room_x =
						safe_world -
						static_cast<double>(tree_width) * 0.5;
					const double room_y =
						safe_world -
						static_cast<double>(tree_height) * 0.5;
					if (!std::isfinite(room_x) ||
						!std::isfinite(room_y) ||
						room_x <= 0.0 ||
						room_y <= 0.0 ||
						!std::isfinite(pan.x) ||
						!std::isfinite(pan.y))
					{
						throw std::length_error(
							"Tree canvas exceeds the supported coordinate range");
					}
					const double zoom_for_view = std::max(
						(static_cast<double>(canvas_size.x) * 0.5 +
							std::abs(static_cast<double>(pan.x))) /
							room_x,
						(static_cast<double>(canvas_size.y) * 0.5 +
							std::abs(static_cast<double>(pan.y))) /
							room_y);
					const double zoom_for_edges =
						4.0 / (safe_world * minimum_framebuffer_scale);
					const double zoom = std::max(
						zoom_for_view,
						zoom_for_edges);
					if (!std::isfinite(zoom) ||
						zoom <= 0.0 ||
						zoom > maximum_zoom)
					{
						throw std::length_error(
							"Tree canvas zoom exceeds the supported numeric range");
					}
					return zoom;
				};
				const int current_tree_index = static_cast<int>(tree_index);
				if (canvas_tree_index != current_tree_index)
				{
					canvas_tree_index = current_tree_index;
					canvas_view_initialized = false;
				}

				if (!canvas_view_initialized ||
					canvas_fit_requested)
				{
					const float available_width =
						std::max(1.0f, canvas_size.x - 32.0f);
					const float available_height =
						std::max(1.0f, canvas_size.y - 32.0f);
					const double minimum_zoom =
						minimum_safe_zoom({}, 2.0f);
					canvas_zoom = std::clamp(
						std::min(
							available_width / tree_width,
							available_height / tree_height),
						minimum_zoom,
						2.0);
					canvas_pan = {};
					canvas_fit_requested = false;
					canvas_view_initialized = true;
				}

				const ImVec2 canvas_center{
					canvas_min.x + canvas_size.x * 0.5f,
					canvas_min.y + canvas_size.y * 0.5f
				};
				if (canvas_hovered && io.MouseWheel != 0.0f)
				{
					const double previous_zoom = canvas_zoom;
					const double minimum_zoom =
						minimum_safe_zoom(canvas_pan, 3.0f);
					const double requested_zoom =
						static_cast<double>(previous_zoom) *
						std::pow(1.15, static_cast<double>(io.MouseWheel));
					const double next_zoom = std::clamp(
						requested_zoom,
						minimum_zoom,
						3.0);
					const double screen_offset_x =
						(static_cast<double>(io.MousePos.x) -
							canvas_center.x - canvas_pan.x) /
						previous_zoom;
					const double screen_offset_y =
						(static_cast<double>(io.MousePos.y) -
							canvas_center.y - canvas_pan.y) /
						previous_zoom;
					canvas_zoom = next_zoom;
					canvas_pan.x =
						static_cast<double>(io.MousePos.x) -
						canvas_center.x - screen_offset_x * next_zoom;
					canvas_pan.y =
						static_cast<double>(io.MousePos.y) -
						canvas_center.y - screen_offset_y * next_zoom;
				}
				if (canvas_active &&
					ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
				{
					canvas_pan.x += io.MouseDelta.x;
					canvas_pan.y += io.MouseDelta.y;
				}
				canvas_zoom = std::max(
					canvas_zoom,
					minimum_safe_zoom(canvas_pan, 3.0f));

				const double screen_scale =
					static_cast<double>(canvas_zoom) *
					minimum_framebuffer_scale;
				const double edge_width = std::max(
					static_cast<double>(
						geometry->draw_options.edge_width),
					1.0 / screen_scale);
				if (!std::isfinite(screen_scale) ||
					screen_scale <= 0.0 ||
					!std::isfinite(edge_width) ||
					edge_width >
						std::numeric_limits<float>::max())
				{
					throw std::length_error(
						"Tree canvas render scale exceeds the supported range");
				}
				render_options.edge_width =
					static_cast<float>(edge_width);
				render_options.show_node_text =
					static_cast<double>(
						render_options.value_text_height) *
						screen_scale >=
					4.0;

				const auto target_dimension = [](float logical, float scale)
				{
					const double dimension = std::round(
						static_cast<double>(logical) * scale);
					if (!std::isfinite(dimension) ||
						dimension > std::numeric_limits<std::uint32_t>::max())
					{
						throw std::length_error(
							"Tree canvas exceeds the supported texture size");
					}
					return static_cast<std::uint32_t>(
						std::max(1.0, dimension));
				};
				const glm::uvec2 target_size{
					target_dimension(canvas_size.x, scale_x),
					target_dimension(canvas_size.y, scale_y)
				};
				if (!ensure_tree_canvas_texture(
					canvas_texture,
					target_size))
				{
					ImGui::EndChild();
					return;
				}

				const glm::dvec2 view_size{
					canvas_size.x / canvas_zoom,
					canvas_size.y / canvas_zoom
				};
				const glm::dvec2 view_center{
					tree_width * 0.5 -
						canvas_pan.x / canvas_zoom,
					tree_height * 0.5 +
						canvas_pan.y / canvas_zoom
				};
				// Project only camera-relative coordinates. Absolute world positions
				// never enter the FP32 matrix or per-instance GPU data.
				glm::mat4 canvas_projection(1.0f);
				canvas_projection[0][0] = static_cast<float>(2.0 / view_size.x);
				canvas_projection[1][1] = static_cast<float>(2.0 / view_size.y);
				const mngl::two_d::tree_draw_area visible_area{
					view_center - view_size * 0.5, view_size
				};
				const mngl::two_d::tree_draw_area tree_area{
					horizontal_margin,
					vertical_margin + leaf_link_space,
					layout.size.x,
					layout.size.y
				};
				const auto visibility = get_tree_preview_visibility(
					cache_index,
					geometry,
					tree_area,
					visible_area,
					render_options);
				render_options.coordinate_origin = view_center;
				render_options.clip_bounds = visible_area;

				int hovered_node = -1;
				int hovered_key_index = -1;
				if (canvas_hovered)
				{
					const std::size_t candidate_count =
						interpolating_layout
						? layout.nodes.size()
						: visibility->nodes.size();
					for (std::size_t candidate = 0;
						candidate < candidate_count;
						++candidate)
					{
						const std::size_t i = interpolating_layout
							? candidate
							: visibility->nodes[candidate];
						const auto& node_layout = layout.nodes[i];
						const double content_x =
							horizontal_margin + node_layout.center.x;
						const double content_y =
							vertical_margin + leaf_link_space +
							node_layout.center.y;
						const ImVec2 center{
							static_cast<float>(canvas_center.x +
								(content_x - view_center.x) * canvas_zoom),
							static_cast<float>(canvas_center.y -
								(content_y - view_center.y) * canvas_zoom)
						};
						const double width =
							node_layout.size.x * canvas_zoom;
						const double height =
							node_layout.size.y * canvas_zoom;
						if (io.MousePos.x >= center.x - width * 0.5f &&
							io.MousePos.x <= center.x + width * 0.5f &&
							io.MousePos.y >= center.y - height * 0.5f &&
							io.MousePos.y <= center.y + height * 0.5f)
						{
							hovered_node = static_cast<int>(i);
							const auto& node = tree.nodes[i];
							if (!node.keys.empty())
							{
								const double cursor_x_in_node =
									(static_cast<double>(io.MousePos.x) -
										(static_cast<double>(center.x) -
											static_cast<double>(width) * 0.5)) /
									static_cast<double>(canvas_zoom);
								double cell_right = 0.0;
								for (std::size_t key_index = 0;
									key_index < node.keys.size();
									++key_index)
								{
									const float cell_width =
										key_index <
											node_layout.cell_widths.size()
										? node_layout.cell_widths[key_index]
										: node_layout.size.x /
											static_cast<float>(
												node.keys.size());
									cell_right += cell_width;
									if (cursor_x_in_node <= cell_right ||
										key_index + 1 == node.keys.size())
									{
										hovered_key_index =
											static_cast<int>(key_index);
										break;
									}
								}
							}
							break;
						}
					}
				}
				const visual_key* hovered_key = nullptr;
				if (hovered_node >= 0 && hovered_key_index >= 0)
				{
					hovered_key = std::addressof(
						tree.nodes[
							static_cast<std::size_t>(hovered_node)]
							.keys[
								static_cast<std::size_t>(
									hovered_key_index)]);
				}
				const bool hovered_key_contextable =
					hovered_key &&
					current_tree_loaded &&
					!showing_animation &&
					search_tree_algorithm_for(
						static_cast<int>(tree_index)) != nullptr &&
					(std::holds_alternative<double>(hovered_key->value) ||
						std::holds_alternative<std::int64_t>(
							hovered_key->value));
				if (hovered_key_contextable)
				{
					m_hovered_tree_key = *hovered_key;
					if (m_tree_comparison_mode && canvas_state)
					{
						m_hovered_tree_comparison_pane = cache_index;
					}
				}
				if (hovered_node >= 0 &&
					ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					context_menu_node =
						static_cast<std::size_t>(hovered_node);
					context_menu_key = hovered_key_contextable
						? std::optional<visual_key>{ *hovered_key }
						: std::nullopt;
					ImGui::OpenPopup("TreeNodeTools");
				}
				std::vector<std::size_t> highlighted_nodes;
				if (current_tree_loaded &&
					preview_tree_is_active &&
					m_tree_find_result_valid &&
					m_tree_find_tree_index ==
						static_cast<int>(tree_index))
				{
					for (const std::size_t node_index : m_tree_find_path)
					{
						if (node_index < render_model.nodes.size())
						{
							highlighted_nodes.push_back(node_index);
						}
					}
				}
				if (showing_animation &&
					m_tree_animation->active_event.has_value())
				{
					const auto& event =
						m_tree_animation->active_event.value();
					if (event.node_index < render_model.nodes.size())
					{
						highlighted_nodes.push_back(event.node_index);
					}
					if (event.parent_index < render_model.nodes.size())
					{
						highlighted_nodes.push_back(event.parent_index);
					}
				}
				else if (interpolating_layout &&
					m_tree_animation->emphasized_node.has_value() &&
					m_tree_animation->emphasized_node.value() <
						render_model.nodes.size())
				{
					highlighted_nodes.push_back(
						m_tree_animation->emphasized_node.value());
				}
				if (hovered_node >= 0)
				{
					highlighted_nodes.push_back(
						static_cast<std::size_t>(hovered_node));
				}
				std::sort(
					highlighted_nodes.begin(),
					highlighted_nodes.end());
				highlighted_nodes.erase(
					std::unique(
						highlighted_nodes.begin(),
						highlighted_nodes.end()),
					highlighted_nodes.end());
				render_options.visibility = interpolating_layout
					? nullptr
					: visibility.get();
				render_options.highlighted_nodes = highlighted_nodes;

				const mngl::color background{
					23.0f / 255.0f,
					27.0f / 255.0f,
					34.0f / 255.0f,
					1.0f
				};
				auto canvas_frame = canvas_texture->frame(background);
				canvas_frame.draw(
					[&](mngl::two_d::Frame2D& frame)
					{
						frame.set_view_projection(canvas_projection);
						const auto& visible = visible_area;
						const auto local_coordinate = [](double value)
						{
							return mngl::two_d::detail::checked_tree_coordinate(value);
						};
						const double grid_spacing = std::max(
							32.0,
							32.0 / screen_scale);
						const float grid_width =
							static_cast<float>(0.7 / screen_scale);
						const mngl::color grid_color{
							1.0f, 1.0f, 1.0f, 0.035f
						};
						const double grid_end_x =
							static_cast<double>(visible.pos.x) +
							visible.size.x;
						for (double x = std::floor(
								static_cast<double>(visible.pos.x) /
								grid_spacing) * grid_spacing;
							x < grid_end_x;)
						{
							frame.draw(mngl::two_d::rectangle_shape(
								mngl::rectangle{
									local_coordinate(x - view_center.x),
									local_coordinate(-view_size.y * 0.5),
									grid_width,
									local_coordinate(view_size.y)
								},
								grid_color));
							const double next_x = x + grid_spacing;
							if (next_x <= x)
							{
								break;
							}
							x = next_x;
						}
						const double grid_end_y =
							static_cast<double>(visible.pos.y) +
							visible.size.y;
						for (double y = std::floor(
								static_cast<double>(visible.pos.y) /
								grid_spacing) * grid_spacing;
							y < grid_end_y;)
						{
							frame.draw(mngl::two_d::rectangle_shape(
								mngl::rectangle{
									local_coordinate(-view_size.x * 0.5),
									local_coordinate(y - view_center.y),
									local_coordinate(view_size.x),
									grid_width
								},
								grid_color));
							const double next_y = y + grid_spacing;
							if (next_y <= y)
							{
								break;
							}
							y = next_y;
						}

						mngl::two_d::draw_tree_model_parallel(
							frame,
							m_tree_font,
							render_model,
							layout,
							tree_area,
							render_options);
					});
				canvas_frame.display();

				const ImVec2 canvas_max{
					canvas_min.x + canvas_size.x,
					canvas_min.y + canvas_size.y
				};
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				draw_list->PushClipRect(canvas_min, canvas_max, true);
				const auto& gpu_texture =
					canvas_texture->raw_gpu_texture();
				draw_list->AddImage(
					ImTextureRef(
						reinterpret_cast<ImTextureID>(gpu_texture.raw())),
					canvas_min,
					canvas_max);
				draw_list->AddText(
					{ canvas_min.x + 12.0f, canvas_min.y + 10.0f },
					IM_COL32(160, 173, 190, 255),
					banner.data(),
					banner.data() + banner.size());
				if (!interpolating_layout &&
					visibility->nodes.empty() &&
					m_tree_preview_caches.at(cache_index)
						.visibility_request_id != 0)
				{
					const char* const culling_status =
						"Пересчёт видимых элементов...";
					const ImVec2 status_size =
						ImGui::CalcTextSize(culling_status);
					draw_list->AddText(
						{
							canvas_min.x +
								(canvas_size.x - status_size.x) * 0.5f,
							canvas_min.y +
								(canvas_size.y - status_size.y) * 0.5f
						},
						IM_COL32(190, 198, 210, 220),
						culling_status);
				}
				draw_list->PopClipRect();

				if (hovered_node >= 0)
				{
					const auto& node =
						tree.nodes[static_cast<std::size_t>(hovered_node)];
					ImGui::BeginTooltip();
					ImGui::TextDisabled(
						tree.b_plus && !node.leaf
							? "Внутренний узел B+ — ключи-разделители"
							: tree.b_plus
								? "Лист B+"
								: current_tree_loaded
									? "Узел загруженного дерева"
									: "Узел демонстрационного дерева");
					for (const auto& key : node.keys)
					{
						const std::string full_value =
							full_visual_value(key.value);
						ImGui::TextDisabled(
							"%.*s",
							static_cast<int>(
								visual_value_type(key.value).size()),
							visual_value_type(key.value).data());
						ImGui::TextWrapped("%s", full_value.c_str());
						if (!key.detail.empty())
						{
							ImGui::TextDisabled(
								"%.*s",
								static_cast<int>(key.detail.size()),
								key.detail.data());
						}
					}
					if (hovered_key_contextable)
					{
						ImGui::TextDisabled(
							"Backspace / D / Del — Erase · C — Split · ПКМ — действия с ключом и узлом");
					}
					ImGui::Text("Дочерних узлов: %zu", node.children.size());
					if (node.next_leaf != no_visual_node)
					{
						ImGui::Text(
							"Следующий лист: %zu",
							node.next_leaf + 1);
					}
					ImGui::EndTooltip();
				}

				std::optional<tree_tool_operation> context_menu_action;
				bool center_context_node = false;
				bool fit_context_tree = false;
				if (ImGui::BeginPopup("TreeNodeTools"))
				{
					if (context_menu_node &&
						*context_menu_node < tree.nodes.size())
					{
						ImGui::TextDisabled(
							"Узел %zu",
							*context_menu_node + 1);
						if (context_menu_key)
						{
							const std::string key_text =
								full_visual_value(context_menu_key->value);
							ImGui::TextDisabled(
								"Ключ: %s",
								key_text.c_str());
						}
						ImGui::Separator();
						center_context_node = ImGui::MenuItem(
							"Центрировать узел");
						fit_context_tree = ImGui::MenuItem(
							"Вписать всё дерево");
						if (context_menu_key)
						{
							ImGui::Separator();
							if (ImGui::MenuItem("Найти ключ"))
							{
								context_menu_action =
									tree_tool_operation::find;
							}
							if (ImGui::MenuItem("Удалить ключ [D / Del]"))
							{
								context_menu_action =
									tree_tool_operation::erase;
							}
							if (ImGui::MenuItem("Split по ключу [C]"))
							{
								context_menu_action =
									tree_tool_operation::split;
							}
						}
						if (can_insert_into_selected_search_tree() &&
							ImGui::MenuItem("Вставить ключ..."))
						{
							context_menu_action =
								tree_tool_operation::insert;
						}
					}
					ImGui::EndPopup();
				}
				else if (!ImGui::IsPopupOpen("TreeNodeTools"))
				{
					context_menu_key.reset();
					context_menu_node.reset();
				}
				if (center_context_node &&
					context_menu_node &&
					*context_menu_node < layout.nodes.size())
				{
					const auto& node_layout =
						layout.nodes[*context_menu_node];
					const double target_x =
						horizontal_margin + node_layout.center.x;
					const double target_y = vertical_margin +
						leaf_link_space + node_layout.center.y;
					canvas_pan.x =
						(tree_width * 0.5 - target_x) * canvas_zoom;
					canvas_pan.y =
						(target_y - tree_height * 0.5) * canvas_zoom;
					canvas_view_initialized = true;
					canvas_fit_requested = false;
				}
				if (fit_context_tree)
				{
					canvas_fit_requested = true;
				}

				if (context_menu_action)
				{
					auto key = context_menu_key;
					context_menu_key.reset();
					const auto comparison_pane =
						m_tree_comparison_mode && canvas_state
						? std::optional<std::size_t>{ cache_index }
						: std::nullopt;
					invoke_tree_tool(
						*context_menu_action,
						std::move(key),
						comparison_pane);
				}
			}
			ImGui::EndChild();
		}

		[[nodiscard]] bool ensure_tree_canvas_texture(
			std::optional<mngl::texture>& canvas_texture,
			glm::uvec2 size)
		{
			if (size.x == 0 || size.y == 0)
			{
				return false;
			}

			const auto& device = m_render_window.gpu_device();
			const auto format =
				device.swapchain_texture_format(m_render_window);
			if (format == mngl::gpu::texture_format::invalid)
			{
				return false;
			}
			const auto* device_handle = device.raw();
			if (canvas_texture && canvas_texture->valid())
			{
				const bool compatible =
					canvas_texture->can_render_to() &&
					canvas_texture->format() == format &&
					canvas_texture->raw_gpu_texture().device_raw() ==
						device_handle;
				const glm::uvec2 current_size =
					canvas_texture->size();
				if (compatible &&
					current_size.x == size.x &&
					current_size.y == size.y)
				{
					return true;
				}
			}

			auto replacement = m_render_window.create_texture(size, format);
			if (!replacement.valid() ||
				!replacement.can_render_to() ||
				replacement.format() != format ||
				replacement.raw_gpu_texture().device_raw() != device_handle ||
				device.swapchain_texture_format(m_render_window) != format)
			{
				return false;
			}
			canvas_texture.emplace(std::move(replacement));
			return true;
		}

		void draw_functions(
			mngl::imgui_context::frame_scope& frame,
			ImVec2 position,
			ImVec2 size)
		{
			set_initial_window_rect(position, size);
			auto panel = frame.window("Поиск функций###TreePracticum.Functions");
			if (!panel)
			{
				return;
			}

			ImGui::TextDisabled("КАТАЛОГ ОПЕРАЦИЙ");
			if (m_focus_function_search)
			{
				ImGui::SetKeyboardFocusHere();
				m_focus_function_search = false;
			}
			ImGui::SetNextItemWidth(-1.0f);
			(void)ImGui::InputTextWithHint(
				"##FunctionSearch",
				"find, insert, mass insert, random test...",
				m_function_filter.data(),
				m_function_filter.size());

			const char* tree_filter_name = m_tree_filter < 0
				? "Все деревья"
				: tree_names[static_cast<std::size_t>(m_tree_filter)].data();
			if (ImGui::BeginCombo("Дерево##FunctionFilter", tree_filter_name))
			{
				if (ImGui::Selectable("Все деревья", m_tree_filter < 0))
				{
					m_tree_filter = -1;
				}
				for (std::size_t i = 0; i < tree_names.size(); ++i)
				{
					if (ImGui::Selectable(
						tree_names[i].data(),
						m_tree_filter == static_cast<int>(i)))
					{
						m_tree_filter = static_cast<int>(i);
					}
				}
				ImGui::EndCombo();
			}

			const std::string_view query{ m_function_filter.data() };
			const ImVec2 available = ImGui::GetContentRegionAvail();
			const float list_height = std::max(110.0f, available.y * 0.58f);
			bool found_any = false;
			if (ImGui::BeginChild(
				"FunctionResults",
				{ 0.0f, list_height },
				ImGuiChildFlags_Borders))
			{
				for (std::size_t i = 0; i < functions.size(); ++i)
				{
					const auto& function = functions[i];
					if (m_tree_filter >= 0 &&
						function.tree_index != static_cast<std::size_t>(m_tree_filter))
					{
						continue;
					}
					if (!function_matches_query(function, query))
					{
						continue;
					}

					found_any = true;
					ImGui::PushID(static_cast<int>(i));
					const float name_width = std::max(
						90.0f,
						ImGui::GetContentRegionAvail().x * 0.48f);
					if (ImGui::Selectable(
						function.name.data(),
						m_selected_function == static_cast<int>(i),
						ImGuiSelectableFlags_None,
						{ name_width, ImGui::GetTextLineHeight() }))
					{
						m_selected_function = static_cast<int>(i);
					}
					ImGui::SameLine();
					ImGui::TextDisabled(
						"%.*s",
						static_cast<int>(tree_names[function.tree_index].size()),
						tree_names[function.tree_index].data());
					ImGui::TextDisabled(
						"%.*s",
						static_cast<int>(function.signature.size()),
						function.signature.data());
					ImGui::PopID();
				}
				if (!found_any)
				{
					ImGui::TextDisabled("Функции не найдены.");
				}
			}
			ImGui::EndChild();

			const auto& selected = functions[
				static_cast<std::size_t>(m_selected_function)];
			const bool selected_matches =
				(m_tree_filter < 0 ||
					selected.tree_index == static_cast<std::size_t>(m_tree_filter)) &&
				function_matches_query(selected, query);
			ImGui::Separator();
			if (!selected_matches)
			{
				ImGui::TextDisabled("Выберите функцию из списка результатов.");
				return;
			}
			ImGui::TextColored(
				{ 0.53f, 0.76f, 0.96f, 1.0f },
				"%.*s",
				static_cast<int>(selected.signature.size()),
				selected.signature.data());
			ImGui::TextWrapped(
				"%.*s",
				static_cast<int>(selected.description.size()),
				selected.description.data());

			const bool implemented_tree_operation =
				selected.name == "find" ||
				selected.name == "insert" ||
				selected.name == "insert_random" ||
				selected.name == "erase" ||
				selected.name == "merge" ||
				selected.name == "split";
			if (implemented_tree_operation)
			{
				const bool selected_tree_is_open =
					selected.tree_index == static_cast<std::size_t>(m_active_tree) &&
					m_loaded_visual_tree.has_value() &&
					m_loaded_tree_index == m_active_tree;
				if (!selected_tree_is_open)
				{
					if (ImGui::Button("Перейти к выбранному типу дерева"))
					{
						select_tree(static_cast<int>(selected.tree_index));
						m_last_action =
							"Выберите сериализацию дерева этого типа в проводнике.";
					}
					ImGui::SameLine();
					ImGui::TextDisabled(
						"Сначала откройте дерево типа %.*s.",
						static_cast<int>(tree_names[
							selected.tree_index].size()),
						tree_names[selected.tree_index].data());
					return;
				}

				const bool can_operate =
					can_insert_into_selected_search_tree();
				ImGui::BeginDisabled(
					!can_operate);
				const char* action_label =
					selected.name == "find"
					? "Найти ключ"
					: selected.name == "insert"
					? "Вставить [A / Ins]"
					: selected.name == "insert_random"
					? "Добавить случайные [R]"
					: selected.name == "erase"
					? "Удалить [D / Del]"
					: selected.name == "merge"
					? "Объединить [M]"
					: "Разделить [C]";
				const bool action_clicked = ImGui::Button(action_label);
				ImGui::EndDisabled();
				if (action_clicked)
				{
					if (selected.name == "find")
					{
						invoke_tree_tool(tree_tool_operation::find);
					}
					else if (selected.name == "insert")
					{
						invoke_tree_tool(tree_tool_operation::insert);
					}
					else if (selected.name == "insert_random")
					{
						invoke_tree_tool(
							tree_tool_operation::random_insert);
					}
					else if (selected.name == "erase")
					{
						invoke_tree_tool(tree_tool_operation::erase);
					}
					else if (selected.name == "merge")
					{
						invoke_tree_tool(tree_tool_operation::merge);
					}
					else
					{
						invoke_tree_tool(tree_tool_operation::split);
					}
				}
				if (selected.name == "merge" &&
					!can_merge_selected_search_tree())
				{
					ImGui::SameLine();
					ImGui::TextDisabled(
						"Укажите источник в окне параметров.");
				}
				else if (!can_operate)
				{
					ImGui::SameLine();
					ImGui::TextDisabled(
						"Операция доступна для корректного дерева с ключами int64_t или double.");
				}
				return;
			}

			if (ImGui::Button("Выбрать тип нового дерева"))
			{
				select_tree(static_cast<int>(selected.tree_index));
				m_last_action = "Выбран тип для следующего нового файла.";
				ImGui::SameLine();
				ImGui::TextDisabled(
					"операция пока не подключена");
			}
		}

		void draw_information(
			mngl::imgui_context::frame_scope& frame,
			ImVec2 position,
			ImVec2 size)
		{
			set_initial_window_rect(position, size);
			auto panel = frame.window("Информация###TreePracticum.Information");
			if (!panel)
			{
				return;
			}

			if (ImGui::BeginTabBar("InformationTabs"))
			{
				if (ImGui::BeginTabItem("Сведения"))
				{
					ImGui::Text(
						"Рабочая область: %s",
						m_session_name.c_str());
					ImGui::SameLine();
					ImGui::Text("Дерево: %s", tree_names[
						static_cast<std::size_t>(m_active_tree)].data());
					const bool empty_workspace =
						m_current_tree_name.empty() &&
						!m_show_demo_trees;
					const bool current_tree_loaded =
						m_loaded_visual_tree.has_value() &&
						m_loaded_tree_index == m_active_tree;
					if (empty_workspace)
					{
						ImGui::TextDisabled(
							"Рабочая область пуста. Создайте дерево или загрузите .tree.");
					}
					else
					{
						const auto& displayed_tree = current_tree_loaded
							? *m_loaded_visual_tree
							: demo_trees()[
								static_cast<std::size_t>(m_active_tree)];
						ImGui::Text(
							"%s: %zu узлов · ключей: %zu",
							current_tree_loaded ? "Загружено" : "Демо",
							displayed_tree.nodes.size(),
							count_visual_keys(displayed_tree));
						if (displayed_tree.b_plus)
						{
							ImGui::TextDisabled(
								"B+: внутренние узлы содержат разделители, листья связаны next-ссылками.");
						}
					}
					ImGui::Separator();
					if (!m_session_path.empty())
					{
						ImGui::TextWrapped(
							"Сессия: %s · несжатый TAR · файлов: %zu",
							path_to_utf8(m_session_path).c_str(),
							m_session_entries.size());
					}
					else if (!m_current_tree_path.empty())
					{
						ImGui::TextWrapped(
							"Файл дерева: %s",
							path_to_utf8(m_current_tree_path).c_str());
					}
					if (!m_current_tree_name.empty())
					{
						ImGui::Text(
							"Сериализация: %s · %zu байт",
							m_current_tree_name.c_str(),
							m_current_tree_serialization.size());
						ImGui::TextDisabled("Предпросмотр .tree:");
						if (ImGui::BeginChild(
							"TreeSerializationPreview",
							{ 0.0f, 62.0f },
							ImGuiChildFlags_Borders))
						{
							const auto preview_size = std::min<std::size_t>(
								m_current_tree_serialization.size(),
								2048);
							ImGui::TextWrapped(
								"%.*s",
								static_cast<int>(preview_size),
								m_current_tree_serialization.data());
						}
						ImGui::EndChild();
						if (m_current_tree_serialization.size() > 2048)
						{
							ImGui::TextDisabled("Предпросмотр ограничен 2048 байтами.");
						}
					}
					ImGui::TextDisabled("Последнее действие:");
					ImGui::TextWrapped("%s", m_last_action.c_str());
					if (!m_file_error.empty())
					{
						ImGui::TextColored(
							{ 1.0f, 0.42f, 0.38f, 1.0f },
							"%s",
							m_file_error.c_str());
					}
					if (!m_tree_parse_error.empty())
					{
						ImGui::TextColored(
							{ 1.0f, 0.42f, 0.38f, 1.0f },
							"Ошибка визуализации .tree: %s",
							m_tree_parse_error.c_str());
					}
					else
					{
						ImGui::TextDisabled(
							empty_workspace
								? "Предпросмотр дерева отсутствует."
								: current_tree_loaded
									? "Показано загруженное дерево."
									: "Показана демонстрационная структура.");
					}
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("История"))
				{
					ImGui::TextDisabled(
						"Ctrl+Z отменяет последнее действие, Ctrl+Shift+Z повторяет его.");
					ImGui::Text(
						"Отмена: %zu · повтор: %zu · лимит: %zu состояний",
						m_history.undo_size(),
						m_history.redo_size(),
						history_stack_type::history_limit);
					ImGui::Text(
						"Память истории: %.1f / %.0f МиБ",
						static_cast<double>(m_history.history_bytes()) /
							(1024.0 * 1024.0),
						static_cast<double>(
							history_stack_type::memory_limit_bytes) /
							(1024.0 * 1024.0));
					ImGui::TextDisabled(
						"История включает операции над деревьями и изменения полей; хранится только в памяти.");
					if (!m_history_warning.empty())
					{
						ImGui::TextColored(
							{ 1.0f, 0.72f, 0.32f, 1.0f },
							"%s",
							m_history_warning.c_str());
					}
					const auto history_count = std::count_if(
						m_session_entries.begin(),
						m_session_entries.end(),
						[](const auto& entry)
						{
							return archive_extension(entry.path) == ".htree";
						});
					ImGui::Text(
						"Файлов истории в рабочей области: %zu",
						static_cast<std::size_t>(history_count));
					ImGui::Text(
						"Последнее действие: %.*s",
						static_cast<int>(m_last_action.size()),
						m_last_action.data());
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Журнал"))
				{
					if (ImGui::SmallButton("Очистить"))
					{
						logger.clear_entries();
					}
					const auto entries = logger.snapshot();
					ImGui::SameLine();
					ImGui::TextDisabled("%zu сообщений", entries.size());
					ImGui::Separator();

					if (ImGui::BeginChild(
						"LogEntries",
						{ 0.0f, 0.0f },
						ImGuiChildFlags_Borders))
					{
						const bool follow_tail =
							ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
						if (entries.empty())
						{
							ImGui::TextDisabled("Журнал пока пуст.");
						}
						else
						{
							for (const auto& entry : entries)
							{
								draw_log_entry(entry);
							}
							if (follow_tail)
							{
								ImGui::SetScrollHereY(1.0f);
							}
						}
					}
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
		}

		static void draw_log_entry(const Logger::entry& entry)
		{
			ImGui::TextDisabled("%s", entry.timestamp.c_str());
			ImGui::SameLine();
			const auto level = Logger::level_name(entry.level);
			const auto rgba = Logger::level_color(entry.level).rgba_glsl();
			ImGui::TextColored(
				{ rgba.r, rgba.g, rgba.b, rgba.a },
				"[%.*s]",
				static_cast<int>(level.size()),
				level.data());
			if (!entry.message.empty())
			{
				ImGui::SameLine();
				ImGui::TextWrapped("%s", entry.message.c_str());
			}
		}

		template<class Value>
		[[nodiscard]] static Value native_key_value(const visual_key& key)
		{
			if (const auto* value = std::get_if<Value>(&key.value))
			{
				return *value;
			}
			throw std::invalid_argument(
				"Ключ операции не соответствует типу значений дерева.");
		}

		template<class Value>
		[[nodiscard]] static tree_find_result find_visual_search_tree_typed(
			const visual_tree& tree,
			const visual_key& key)
		{
			tree_find_result result;
			if (tree.nodes.empty())
			{
				return result;
			}
			if (tree.root >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Корень дерева ссылается на отсутствующий узел.");
			}

			const Value query = native_key_value<Value>(key);
			const auto compare_with_query = [query](const visual_key& stored)
			{
				const Value value = native_key_value<Value>(stored);
				if (value < query)
				{
					return -1;
				}
				if (query < value)
				{
					return 1;
				}
				return 0;
			};

			std::size_t current = tree.root;
			while (current != no_visual_node)
			{
				if (current >= tree.nodes.size() ||
					result.path.size() >= tree.nodes.size())
				{
					throw std::invalid_argument(
						"Дерево содержит неверную или циклическую ссылку.");
				}
				result.path.push_back(current);
				result.visited_nodes = result.path.size();

				const auto& node = tree.nodes[current];
				if (tree.b_plus)
				{
					if (node.leaf)
					{
						for (const auto& stored : node.keys)
						{
							const int comparison =
								compare_with_query(stored);
							if (comparison == 0)
							{
								result.found = true;
								return result;
							}
							if (comparison > 0)
							{
								break;
							}
						}
						return result;
					}

					std::size_t child_index = 0;
					while (child_index < node.keys.size() &&
						compare_with_query(node.keys[child_index]) <= 0)
					{
						++child_index;
					}
					if (child_index >= node.children.size() ||
						node.children[child_index] == no_visual_node)
					{
						throw std::invalid_argument(
							"B+ внутренний узел содержит неверную ссылку на потомка.");
					}
					current = node.children[child_index];
					continue;
				}

				if (node.keys.size() != 1)
				{
					throw std::invalid_argument(
						"Узел бинарного дерева должен содержать ровно один ключ.");
				}
				const int comparison =
					compare_with_query(node.keys.front());
				if (comparison == 0)
				{
					result.found = true;
					return result;
				}
				const std::size_t side = comparison > 0 ? 0 : 1;
				current = node.binary_children[side];
			}
			return result;
		}

		[[nodiscard]] static tree_find_result find_visual_search_tree(
			const visual_tree& tree,
			const visual_key& key,
			search_tree_key_kind kind)
		{
			if (kind == search_tree_key_kind::signed_integer)
			{
				return find_visual_search_tree_typed<std::int64_t>(
					tree,
					key);
			}
			if (kind == search_tree_key_kind::floating_point)
			{
				return find_visual_search_tree_typed<double>(tree, key);
			}
			throw std::invalid_argument(
				"Поиск поддерживает только int64_t и конечный double.");
		}

		[[nodiscard]] static visual_key make_random_search_tree_key(
			search_tree_key_kind kind,
			std::mt19937_64& random_engine)
		{
			if (kind == search_tree_key_kind::signed_integer)
			{
				return make_integer_key(std::bit_cast<std::int64_t>(
					static_cast<std::uint64_t>(random_engine())));
			}
			if (kind == search_tree_key_kind::floating_point)
			{
				const double unit_value =
					std::generate_canonical<
						double,
						std::numeric_limits<double>::digits>(
							random_engine);
				return make_double_key(unit_value * 2.0 - 1.0);
			}
			throw std::invalid_argument(
				"Случайная вставка поддерживает только int64_t и конечный double.");
		}

		class tree_operation_algorithm
		{
		public:
			virtual ~tree_operation_algorithm() = default;
			[[nodiscard]] virtual std::size_t tree_index() const noexcept = 0;
			[[nodiscard]] virtual std::string_view name() const noexcept = 0;
			virtual void validate(
				const visual_tree& tree,
				search_tree_key_kind kind) const = 0;
			[[nodiscard]] virtual tree_find_result find(
				visual_tree& tree,
				const visual_key& key,
				search_tree_key_kind kind) const
			{
				return find_visual_search_tree(tree, key, kind);
			}
			[[nodiscard]] virtual tree_operation_result insert(
				visual_tree& tree,
				visual_key key,
				search_tree_key_kind kind,
				bool collect_animation_events) const = 0;
			[[nodiscard]] virtual tree_bulk_insert_result insert_many(
				visual_tree& tree,
				std::uint64_t requested_count,
				search_tree_key_kind kind,
				std::mt19937_64& random_engine) const
			{
				tree_bulk_insert_result result;
				while (result.inserted_count < requested_count)
				{
					auto operation = insert(
						tree,
						make_random_search_tree_key(
							kind,
							random_engine),
						kind,
						false);
					if (operation.inserted)
					{
						++result.inserted_count;
					}
				}
				return result;
			}
			[[nodiscard]] virtual std::string serialize(
				const visual_tree& tree) const = 0;
			[[nodiscard]] virtual tree_erase_result erase(
				visual_tree& tree,
				const visual_key& key,
				search_tree_key_kind kind) const
			{
				const auto result = erase_native_search_tree(
					tree_index(),
					tree,
					key,
					kind);
				if (result.changed)
				{
					tree = parse_native_operation_tree(
						result.serialization,
						tree_index(),
						kind);
				}
				return {
					result.operation_succeeded,
					result.changed
				};
			}
			[[nodiscard]] virtual bool merge(
				visual_tree& left,
				const visual_tree& right,
				search_tree_key_kind kind) const
			{
				const auto result = merge_native_search_trees(
					tree_index(),
					left,
					right,
					kind);
				if (result.changed)
				{
					left = parse_native_operation_tree(
						result.serialization,
						tree_index(),
						kind);
				}
				return result.changed;
			}
			[[nodiscard]] virtual std::pair<visual_tree, visual_tree> split(
				const visual_tree& tree,
				const visual_key& pivot,
				search_tree_key_kind kind) const
			{
				auto serializations = split_native_search_tree(
					tree_index(),
					tree,
					pivot,
					kind);
				return {
					parse_native_operation_tree(
						serializations.first,
						tree_index(),
						kind),
					parse_native_operation_tree(
						serializations.second,
						tree_index(),
						kind)
				};
			}
			[[nodiscard]] virtual bool supports_animation() const noexcept
			{
				return false;
			}
		};

		class bst_tree_algorithm final : public tree_operation_algorithm
		{
		public:
			[[nodiscard]] std::size_t tree_index() const noexcept override
			{
				return 0;
			}

			[[nodiscard]] std::string_view name() const noexcept override
			{
				return "BST";
			}

			void validate(
				const visual_tree& tree,
				search_tree_key_kind kind) const override
			{
				validate_search_tree_structure(tree, kind);
			}

			[[nodiscard]] tree_operation_result insert(
				visual_tree& tree,
				visual_key key,
				search_tree_key_kind kind,
				bool) const override
			{
				const bool inserted = insert_bst_key(
					tree,
					std::move(key),
					kind);
				return { inserted, inserted, true, {} };
			}

			[[nodiscard]] std::string serialize(
				const visual_tree& tree) const override
			{
				return serialize_bst_tree(tree);
			}
		};

		class avl_tree_algorithm final : public tree_operation_algorithm
		{
		public:
			[[nodiscard]] std::size_t tree_index() const noexcept override
			{
				return 1;
			}

			[[nodiscard]] std::string_view name() const noexcept override
			{
				return "AVL";
			}

			void validate(
				const visual_tree& tree,
				search_tree_key_kind kind) const override
			{
				validate_search_tree_structure(tree, kind);
				validate_avl_structure(tree);
			}

			[[nodiscard]] tree_operation_result insert(
				visual_tree& tree,
				visual_key key,
				search_tree_key_kind kind,
				bool) const override
			{
				const bool inserted = insert_avl_key(
					tree,
					std::move(key),
					kind);
				return { inserted, inserted, true, {} };
			}

			[[nodiscard]] std::string serialize(
				const visual_tree& tree) const override
			{
				return serialize_avl_tree(tree);
			}
		};

		class red_black_tree_algorithm final
			: public tree_operation_algorithm
		{
		public:
			[[nodiscard]] std::size_t tree_index() const noexcept override
			{
				return 2;
			}

			[[nodiscard]] std::string_view name() const noexcept override
			{
				return "красно-чёрное дерево";
			}

			void validate(
				const visual_tree& tree,
				search_tree_key_kind kind) const override
			{
				validate_search_tree_structure(tree, kind);
				validate_red_black_structure(tree);
			}

			[[nodiscard]] tree_operation_result insert(
				visual_tree& tree,
				visual_key key,
				search_tree_key_kind kind,
				bool) const override
			{
				const bool inserted = insert_red_black_key(
					tree,
					std::move(key),
					kind);
				return { inserted, inserted, true, {} };
			}

			[[nodiscard]] tree_bulk_insert_result insert_many(
				visual_tree& tree,
				std::uint64_t requested_count,
				search_tree_key_kind kind,
				std::mt19937_64& random_engine) const override
			{
				return insert_native_search_tree_many(
					tree,
					requested_count,
					kind,
					tree_index(),
					random_engine);
			}

			[[nodiscard]] std::string serialize(
				const visual_tree& tree) const override
			{
				return serialize_red_black_tree(tree);
			}
		};

		class splay_tree_algorithm final : public tree_operation_algorithm
		{
		public:
			[[nodiscard]] std::size_t tree_index() const noexcept override
			{
				return 3;
			}

			[[nodiscard]] std::string_view name() const noexcept override
			{
				return "Splay";
			}

			void validate(
				const visual_tree& tree,
				search_tree_key_kind kind) const override
			{
				validate_search_tree_structure(tree, kind);
			}

			[[nodiscard]] tree_find_result find(
				visual_tree& tree,
				const visual_key& key,
				search_tree_key_kind kind) const override
			{
				if (kind == search_tree_key_kind::signed_integer)
				{
					return find_splay_search_tree_typed<std::int64_t>(
						tree,
						key);
				}
				if (kind == search_tree_key_kind::floating_point)
				{
					return find_splay_search_tree_typed<double>(
						tree,
						key);
				}
				throw std::invalid_argument(
					"Поиск поддерживает только int64_t и конечный double.");
			}

			[[nodiscard]] tree_operation_result insert(
				visual_tree& tree,
				visual_key key,
				search_tree_key_kind kind,
				bool collect_animation_events) const override
			{
				return insert_splay_key(
					tree,
					std::move(key),
					kind,
					collect_animation_events);
			}

			[[nodiscard]] std::string serialize(
				const visual_tree& tree) const override
			{
				return serialize_splay_tree(tree);
			}

			[[nodiscard]] bool supports_animation() const noexcept override
			{
				return true;
			}
		};

		class treap_tree_algorithm final : public tree_operation_algorithm
		{
		public:
			[[nodiscard]] std::size_t tree_index() const noexcept override
			{
				return 4;
			}

			[[nodiscard]] std::string_view name() const noexcept override
			{
				return "Treap";
			}

			void validate(
				const visual_tree& tree,
				search_tree_key_kind kind) const override
			{
				validate_search_tree_structure(tree, kind);
				validate_treap_structure(tree);
			}

			[[nodiscard]] tree_operation_result insert(
				visual_tree& tree,
				visual_key key,
				search_tree_key_kind kind,
				bool) const override
			{
				return insert_native_search_tree(
					tree,
					std::move(key),
					kind,
					tree_index());
			}

			[[nodiscard]] tree_bulk_insert_result insert_many(
				visual_tree& tree,
				std::uint64_t requested_count,
				search_tree_key_kind kind,
				std::mt19937_64& random_engine) const override
			{
				return insert_native_search_tree_many(
					tree,
					requested_count,
					kind,
					tree_index(),
					random_engine);
			}

			[[nodiscard]] std::string serialize(
				const visual_tree& tree) const override
			{
				return serialize_native_search_tree(
					tree,
					tree_index());
			}
		};

		class b_plus_tree_algorithm final : public tree_operation_algorithm
		{
		public:
			[[nodiscard]] std::size_t tree_index() const noexcept override
			{
				return 5;
			}

			[[nodiscard]] std::string_view name() const noexcept override
			{
				return "B+";
			}

			void validate(
				const visual_tree& tree,
				search_tree_key_kind kind) const override
			{
				validate_b_plus_tree_structure(tree, kind);
			}

			[[nodiscard]] tree_operation_result insert(
				visual_tree& tree,
				visual_key key,
				search_tree_key_kind kind,
				bool) const override
			{
				return insert_native_search_tree(
					tree,
					std::move(key),
					kind,
					tree_index());
			}

			[[nodiscard]] tree_bulk_insert_result insert_many(
				visual_tree& tree,
				std::uint64_t requested_count,
				search_tree_key_kind kind,
				std::mt19937_64& random_engine) const override
			{
				return insert_native_search_tree_many(
					tree,
					requested_count,
					kind,
					tree_index(),
					random_engine);
			}

			[[nodiscard]] std::string serialize(
				const visual_tree& tree) const override
			{
				return serialize_native_search_tree(
					tree,
					tree_index());
			}
		};

		template<class NativeTree, class Value>
		[[nodiscard]] static NativeTree make_native_binary_tree(
			const visual_tree& tree)
		{
			using node_type = typename NativeTree::node_type;
			if (tree.b_plus)
			{
				throw std::invalid_argument(
					"Ожидалось бинарное дерево поиска, получено B+ дерево.");
			}
			if (tree.nodes.empty())
			{
				if (tree.root != no_visual_node)
				{
					throw std::invalid_argument(
						"Пустое дерево содержит неверный индекс корня.");
				}
				return NativeTree::from_validated_root(
					std::unique_ptr<node_type>{});
			}
			if (tree.root >= tree.nodes.size())
			{
				throw std::invalid_argument(
					"Корень дерева ссылается на отсутствующий узел.");
			}

			std::vector<std::unique_ptr<node_type>> nodes(tree.nodes.size());
			std::vector<node_type*> raw_nodes(tree.nodes.size(), nullptr);
			for (std::size_t i = 0; i < tree.nodes.size(); ++i)
			{
				const auto& source = tree.nodes[i];
				if (source.keys.size() != 1)
				{
					throw std::invalid_argument(
						"Узел бинарного дерева должен содержать ровно один ключ.");
				}
				const Value value = native_key_value<Value>(
					source.keys.front());
				if constexpr (std::is_same_v<NativeTree, avl<Value>>)
				{
					nodes[i] = std::make_unique<node_type>(
						value,
						avl_height(tree, i));
				}
				else if constexpr (std::is_same_v<NativeTree, rb_tree<Value>>)
				{
					nodes[i] = std::make_unique<node_type>(
						value,
						red_black_node_is_red(tree, i)
							? rb_color::red
							: rb_color::black);
				}
				else if constexpr (std::is_same_v<NativeTree, treap<Value>>)
				{
					nodes[i] = std::make_unique<node_type>(
						value,
						treap_priority(tree, i));
				}
				else
				{
					nodes[i] = std::make_unique<node_type>(value);
				}
				raw_nodes[i] = nodes[i].get();
			}

			for (std::size_t i = 0; i < tree.nodes.size(); ++i)
			{
				for (std::size_t side = 0; side < 2; ++side)
				{
					const auto child = tree.nodes[i].binary_children[side];
					if (child == no_visual_node)
					{
						continue;
					}
					if (child >= nodes.size() ||
						!nodes[child] ||
						!raw_nodes[i])
					{
						throw std::invalid_argument(
							"Бинарное дерево содержит повторную или неверную ссылку.");
					}
					if (side == 0)
					{
						raw_nodes[i]->set_left(std::move(nodes[child]));
					}
					else
					{
						raw_nodes[i]->set_right(std::move(nodes[child]));
					}
				}
			}

			auto root = std::move(nodes[tree.root]);
			if (!root)
			{
				throw std::invalid_argument(
					"Корень бинарного дерева был использован как потомок.");
			}
			if (std::any_of(nodes.begin(), nodes.end(), [](const auto& node)
				{
					return static_cast<bool>(node);
				}))
			{
				throw std::invalid_argument(
					"Бинарное дерево содержит узлы, недостижимые из корня.");
			}
			return NativeTree::from_validated_root(std::move(root));
		}

		template<class Value, class Visitor>
		static void for_each_b_plus_value(
			const visual_tree& tree,
			Visitor&& visitor)
		{
			if (!tree.b_plus)
			{
				throw std::invalid_argument(
					"Ожидалось B+ дерево.");
			}
			if (tree.nodes.empty())
			{
				return;
			}

			std::size_t leaf_index = tree.root;
			while (leaf_index < tree.nodes.size() &&
				!tree.nodes[leaf_index].leaf)
			{
				const auto& children = tree.nodes[leaf_index].children;
				if (children.empty())
				{
					throw std::invalid_argument(
						"B+ внутренний узел не содержит потомков.");
				}
				leaf_index = children.front();
			}

			std::size_t visited_leaves = 0;
			while (leaf_index != no_visual_node)
			{
				if (leaf_index >= tree.nodes.size() ||
					!tree.nodes[leaf_index].leaf ||
					++visited_leaves > tree.nodes.size())
				{
					throw std::invalid_argument(
						"B+ дерево содержит неверную связь между листьями.");
				}
				const auto& leaf = tree.nodes[leaf_index];
				for (const auto& key : leaf.keys)
				{
					visitor(native_key_value<Value>(key));
				}
				leaf_index = leaf.next_leaf;
			}
		}

		template<class Value>
		[[nodiscard]] static b_plus_tree<Value> make_native_b_plus_tree(
			const visual_tree& tree)
		{
			if (!tree.b_plus || tree.fanout < 3 || tree.fanout > 1'024)
			{
				throw std::invalid_argument(
					"B+ дерево должно иметь fanout от 3 до 1024.");
			}
			b_plus_tree<Value> result(tree.fanout);
			for_each_b_plus_value<Value>(
				tree,
				[&](const Value& value)
				{
					if (!result.insert(value))
					{
						throw std::invalid_argument(
							"B+ дерево содержит повторный ключ.");
					}
				});
			return result;
		}

		[[nodiscard]] static visual_tree parse_native_operation_tree(
			std::string_view serialization,
			std::size_t expected_tree_index,
			search_tree_key_kind expected_key_kind)
		{
			std::size_t parsed_tree_index = 0;
			auto tree = parse_tree_serialization(
				serialization,
				parsed_tree_index);
			if (parsed_tree_index != expected_tree_index)
			{
				throw std::logic_error(
					"Нативная операция изменила тип дерева.");
			}
			const auto key_kind = infer_search_tree_key_kind(tree);
			if (key_kind && *key_kind != expected_key_kind)
			{
				throw std::logic_error(
					"Нативная операция изменила тип ключей.");
			}
			const auto* algorithm = search_tree_algorithm_for(
				static_cast<int>(expected_tree_index));
			if (!algorithm)
			{
				throw std::logic_error(
					"Для результата операции не найден алгоритм дерева.");
			}
			algorithm->validate(
				tree,
				key_kind.value_or(expected_key_kind));
			return tree;
		}

		template<class Value>
		[[nodiscard]] static tree_find_result
			find_splay_search_tree_typed(
				visual_tree& tree,
				const visual_key& key)
		{
			auto result = find_visual_search_tree_typed<Value>(tree, key);
			if (result.path.empty())
			{
				return result;
			}
			const std::size_t visited_nodes = result.visited_nodes;
			const bool found = result.found;
			const auto target = result.path.back();
			if (target == tree.root)
			{
				return result;
			}
			auto ancestors = std::move(result.path);
			ancestors.pop_back();

			splay_node_to_root(tree, ancestors, target);
			result = find_visual_search_tree_typed<Value>(tree, key);
			if (result.found != found)
			{
				throw std::logic_error(
					"Результат поиска изменился после перестройки Splay.");
			}
			result.visited_nodes = visited_nodes;
			result.changed = true;
			return result;
		}

		template<class Value>
		[[nodiscard]] static std::string serialize_native_search_tree_typed(
			const visual_tree& tree,
			std::size_t tree_index)
		{
			switch (tree_index)
			{
			case 0:
				return make_native_binary_tree<bst<Value>, Value>(tree)
					.to_string();
			case 1:
				return make_native_binary_tree<avl<Value>, Value>(tree)
					.to_string();
			case 2:
				return make_native_binary_tree<rb_tree<Value>, Value>(tree)
					.to_string();
			case 3:
				return make_native_binary_tree<splay_tree<Value>, Value>(tree)
					.to_string();
			case 4:
				return make_native_binary_tree<treap<Value>, Value>(tree)
					.to_string();
			case 5:
				return make_native_b_plus_tree<Value>(tree).to_string();
			default:
				throw std::invalid_argument(
					"Неизвестный тип дерева поиска.");
			}
		}

		[[nodiscard]] static std::string serialize_native_search_tree(
			const visual_tree& tree,
			std::size_t tree_index)
		{
			const auto key_kind = infer_search_tree_key_kind(tree);
			if (!key_kind ||
				*key_kind == search_tree_key_kind::signed_integer)
			{
				return serialize_native_search_tree_typed<std::int64_t>(
					tree,
					tree_index);
			}
			if (*key_kind == search_tree_key_kind::floating_point)
			{
				return serialize_native_search_tree_typed<double>(
					tree,
					tree_index);
			}
			throw std::invalid_argument(
				"Операции поддерживают только int64_t и конечный double.");
		}

		template<class Value, class NativeTree>
		[[nodiscard]] static std::uint64_t insert_random_native_values(
			NativeTree& native_tree,
			std::uint64_t requested_count,
			search_tree_key_kind kind,
			std::mt19937_64& random_engine)
		{
			std::uint64_t inserted_count = 0;
			while (inserted_count < requested_count)
			{
				const auto key = make_random_search_tree_key(
					kind,
					random_engine);
				if (native_tree.insert(native_key_value<Value>(key)))
				{
					++inserted_count;
				}
			}
			return inserted_count;
		}

		template<class Value>
		[[nodiscard]] static tree_bulk_insert_result
			insert_native_search_tree_many_typed(
				visual_tree& tree,
				std::uint64_t requested_count,
				search_tree_key_kind kind,
				std::size_t tree_index,
				std::mt19937_64& random_engine)
		{
			if (requested_count == 0)
			{
				return {};
			}

			std::uint64_t inserted_count = 0;
			std::string serialization;
			if (tree_index == 2)
			{
				auto native =
					make_native_binary_tree<rb_tree<Value>, Value>(tree);
				inserted_count = insert_random_native_values<Value>(
					native,
					requested_count,
					kind,
					random_engine);
				serialization = native.to_string();
			}
			else if (tree_index == 4)
			{
				auto native =
					make_native_binary_tree<treap<Value>, Value>(tree);
				inserted_count = insert_random_native_values<Value>(
					native,
					requested_count,
					kind,
					random_engine);
				serialization = native.to_string();
			}
			else if (tree_index == 5)
			{
				auto native = make_native_b_plus_tree<Value>(tree);
				inserted_count = insert_random_native_values<Value>(
					native,
					requested_count,
					kind,
					random_engine);
				serialization = native.to_string();
			}
			else
			{
				throw std::invalid_argument(
					"Нативная массовая вставка не поддерживается для этого алгоритма.");
			}

			tree = parse_native_operation_tree(
				serialization,
				tree_index,
				kind);
			return {
				inserted_count,
				std::optional<std::string>{
					std::move(serialization)
				}
			};
		}

		[[nodiscard]] static tree_bulk_insert_result
			insert_native_search_tree_many(
				visual_tree& tree,
				std::uint64_t requested_count,
				search_tree_key_kind kind,
				std::size_t tree_index,
				std::mt19937_64& random_engine)
		{
			if (kind == search_tree_key_kind::signed_integer)
			{
				return insert_native_search_tree_many_typed<std::int64_t>(
					tree,
					requested_count,
					kind,
					tree_index,
					random_engine);
			}
			if (kind == search_tree_key_kind::floating_point)
			{
				return insert_native_search_tree_many_typed<double>(
					tree,
					requested_count,
					kind,
					tree_index,
					random_engine);
			}
			throw std::invalid_argument(
				"Операции поддерживают только int64_t и конечный double.");
		}

		template<class Value>
		[[nodiscard]] static tree_operation_result insert_native_search_tree_typed(
			visual_tree& tree,
			const visual_key& key,
			search_tree_key_kind kind,
			std::size_t tree_index)
		{
			const Value value = native_key_value<Value>(key);
			bool inserted = false;
			std::string serialization;
			if (tree_index == 4)
			{
				auto native = make_native_binary_tree<treap<Value>, Value>(tree);
				inserted = native.insert(value);
				if (inserted)
				{
					serialization = native.to_string();
				}
			}
			else if (tree_index == 5)
			{
				auto native = make_native_b_plus_tree<Value>(tree);
				inserted = native.insert(value);
				if (inserted)
				{
					serialization = native.to_string();
				}
			}
			else
			{
				throw std::invalid_argument(
					"Нативная вставка не поддерживается для этого алгоритма.");
			}

			if (inserted)
			{
				tree = parse_native_operation_tree(
					serialization,
					tree_index,
					kind);
			}
			tree_operation_result result;
			result.inserted = inserted;
			result.changed = inserted;
			return result;
		}

		[[nodiscard]] static tree_operation_result insert_native_search_tree(
			visual_tree& tree,
			visual_key key,
			search_tree_key_kind kind,
			std::size_t tree_index)
		{
			if (kind == search_tree_key_kind::signed_integer)
			{
				return insert_native_search_tree_typed<std::int64_t>(
					tree,
					key,
					kind,
					tree_index);
			}
			if (kind == search_tree_key_kind::floating_point)
			{
				return insert_native_search_tree_typed<double>(
					tree,
					key,
					kind,
					tree_index);
			}
			throw std::invalid_argument(
				"Операции поддерживают только int64_t и конечный double.");
		}

		template<class Value>
		[[nodiscard]] static tree_native_change_result erase_native_search_tree_typed(
			const visual_tree& tree,
			const visual_key& key,
			search_tree_key_kind kind,
			std::size_t tree_index)
		{
			const Value value = native_key_value<Value>(key);
			bool erased = false;
			bool changed = false;
			std::string serialization;
			switch (tree_index)
			{
			case 0:
			{
				auto native = make_native_binary_tree<bst<Value>, Value>(tree);
				erased = native.erase(value);
				changed = erased;
				if (changed)
				{
					serialization = native.to_string();
				}
				break;
			}
			case 1:
			{
				auto native = make_native_binary_tree<avl<Value>, Value>(tree);
				erased = native.erase(value);
				changed = erased;
				if (changed)
				{
					serialization = native.to_string();
				}
				break;
			}
			case 2:
			{
				auto native = make_native_binary_tree<rb_tree<Value>, Value>(tree);
				erased = native.erase(value);
				changed = erased;
				if (changed)
				{
					serialization = native.to_string();
				}
				break;
			}
			case 3:
			{
				auto native = make_native_binary_tree<splay_tree<Value>, Value>(tree);
				const auto before = native.to_string();
				erased = native.erase(value);
				serialization = native.to_string();
				changed = erased || serialization != before;
				break;
			}
			case 4:
			{
				auto native = make_native_binary_tree<treap<Value>, Value>(tree);
				erased = native.erase(value);
				changed = erased;
				if (changed)
				{
					serialization = native.to_string();
				}
				break;
			}
			case 5:
			{
				auto native = make_native_b_plus_tree<Value>(tree);
				erased = native.erase(value);
				changed = erased;
				if (changed)
				{
					serialization = native.to_string();
				}
				break;
			}
			default:
				throw std::invalid_argument(
					"Неизвестный тип дерева поиска.");
			}
			return { erased, changed, std::move(serialization) };
		}

		[[nodiscard]] static tree_native_change_result erase_native_search_tree(
			std::size_t tree_index,
			const visual_tree& tree,
			const visual_key& key,
			search_tree_key_kind kind)
		{
			if (kind == search_tree_key_kind::signed_integer)
			{
				return erase_native_search_tree_typed<std::int64_t>(
					tree,
					key,
					kind,
					tree_index);
			}
			if (kind == search_tree_key_kind::floating_point)
			{
				return erase_native_search_tree_typed<double>(
					tree,
					key,
					kind,
					tree_index);
			}
			throw std::invalid_argument(
				"Операции поддерживают только int64_t и конечный double.");
		}

		template<class Value>
		[[nodiscard]] static tree_native_change_result merge_native_search_trees_typed(
			std::size_t tree_index,
			const visual_tree& left,
			const visual_tree& right,
			search_tree_key_kind kind)
		{
			if (right.nodes.empty())
			{
				return {};
			}
			switch (tree_index)
			{
			case 0:
			{
				auto native_left =
					make_native_binary_tree<bst<Value>, Value>(left);
				auto native_right =
					make_native_binary_tree<bst<Value>, Value>(right);
				native_left.merge(std::move(native_right));
				return { true, true, native_left.to_string() };
			}
			case 1:
			{
				auto native_left =
					make_native_binary_tree<avl<Value>, Value>(left);
				auto native_right =
					make_native_binary_tree<avl<Value>, Value>(right);
				native_left.merge(std::move(native_right));
				return { true, true, native_left.to_string() };
			}
			case 2:
			{
				auto native_left =
					make_native_binary_tree<rb_tree<Value>, Value>(left);
				auto native_right =
					make_native_binary_tree<rb_tree<Value>, Value>(right);
				native_left.merge(std::move(native_right));
				return { true, true, native_left.to_string() };
			}
			case 3:
			{
				auto native_left =
					make_native_binary_tree<splay_tree<Value>, Value>(left);
				auto native_right =
					make_native_binary_tree<splay_tree<Value>, Value>(right);
				native_left.merge(std::move(native_right));
				return { true, true, native_left.to_string() };
			}
			case 4:
			{
				auto native_left =
					make_native_binary_tree<treap<Value>, Value>(left);
				auto native_right =
					make_native_binary_tree<treap<Value>, Value>(right);
				native_left.merge(std::move(native_right));
				return { true, true, native_left.to_string() };
			}
			case 5:
			{
				std::optional<Value> maximum_left;
				std::optional<Value> minimum_right;
				for_each_b_plus_value<Value>(
					left,
					[&](const Value& value)
					{
						maximum_left = value;
					});
				for_each_b_plus_value<Value>(
					right,
					[&](const Value& value)
					{
						if (!minimum_right)
						{
							minimum_right = value;
						}
					});
				if (maximum_left && minimum_right &&
					!(*maximum_left < *minimum_right))
				{
					throw std::invalid_argument(
						"B+ merge requires every left key to precede every right key");
				}
				auto native_left = make_native_b_plus_tree<Value>(left);
				for_each_b_plus_value<Value>(
					right,
					[&](const Value& value)
					{
						if (!native_left.insert(value))
						{
							throw std::invalid_argument(
								"B+ merge encountered a duplicate key.");
						}
					});
				return { true, true, native_left.to_string() };
			}
			default:
				throw std::invalid_argument(
					"Неизвестный тип дерева поиска.");
			}
		}

		[[nodiscard]] static tree_native_change_result merge_native_search_trees(
			std::size_t tree_index,
			const visual_tree& left,
			const visual_tree& right,
			search_tree_key_kind kind)
		{
			if (kind == search_tree_key_kind::signed_integer)
			{
				return merge_native_search_trees_typed<std::int64_t>(
					tree_index,
					left,
					right,
					kind);
			}
			if (kind == search_tree_key_kind::floating_point)
			{
				return merge_native_search_trees_typed<double>(
					tree_index,
					left,
					right,
					kind);
			}
			throw std::invalid_argument(
				"Операции поддерживают только int64_t и конечный double.");
		}

		template<class Value>
		[[nodiscard]] static std::pair<std::string, std::string>
			split_native_search_tree_typed(
				std::size_t tree_index,
				const visual_tree& tree,
				const visual_key& pivot,
				search_tree_key_kind)
		{
			const Value value = native_key_value<Value>(pivot);
			switch (tree_index)
			{
			case 0:
			{
				auto native = make_native_binary_tree<bst<Value>, Value>(tree);
				auto [left, right] = native.split(value);
				return { left.to_string(), right.to_string() };
			}
			case 1:
			{
				auto native = make_native_binary_tree<avl<Value>, Value>(tree);
				auto [left, right] = native.split(value);
				return { left.to_string(), right.to_string() };
			}
			case 2:
			{
				auto native = make_native_binary_tree<rb_tree<Value>, Value>(tree);
				auto [left, right] = native.split(value);
				return { left.to_string(), right.to_string() };
			}
			case 3:
			{
				auto native =
					make_native_binary_tree<splay_tree<Value>, Value>(tree);
				auto [left, right] = native.split(value);
				return { left.to_string(), right.to_string() };
			}
			case 4:
			{
				auto native = make_native_binary_tree<treap<Value>, Value>(tree);
				auto [left, right] = native.split(value);
				return { left.to_string(), right.to_string() };
			}
			case 5:
			{
				b_plus_tree<Value> left(tree.fanout);
				b_plus_tree<Value> right(tree.fanout);
				for_each_b_plus_value<Value>(
					tree,
					[&](const Value& key)
					{
						auto& output = key < value ? left : right;
						if (!output.insert(key))
						{
							throw std::invalid_argument(
								"B+ split encountered a duplicate key.");
						}
					});
				return { left.to_string(), right.to_string() };
			}
			default:
				throw std::invalid_argument(
					"Неизвестный тип дерева поиска.");
			}
		}

		[[nodiscard]] static std::pair<std::string, std::string>
			split_native_search_tree(
				std::size_t tree_index,
				const visual_tree& tree,
				const visual_key& pivot,
				search_tree_key_kind kind)
		{
			if (kind == search_tree_key_kind::signed_integer)
			{
				return split_native_search_tree_typed<std::int64_t>(
					tree_index,
					tree,
					pivot,
					kind);
			}
			if (kind == search_tree_key_kind::floating_point)
			{
				return split_native_search_tree_typed<double>(
					tree_index,
					tree,
					pivot,
					kind);
			}
			throw std::invalid_argument(
				"Операции поддерживают только int64_t и конечный double.");
		}

		[[nodiscard]] static const tree_operation_algorithm*
			search_tree_algorithm_for(int tree_index) noexcept
		{
			static const bst_tree_algorithm bst;
			static const avl_tree_algorithm avl;
			static const red_black_tree_algorithm red_black;
			static const splay_tree_algorithm splay;
			static const treap_tree_algorithm treap;
			static const b_plus_tree_algorithm b_plus;

			switch (tree_index)
			{
			case 0:
				return &bst;
			case 1:
				return &avl;
			case 2:
				return &red_black;
			case 3:
				return &splay;
			case 4:
				return &treap;
			case 5:
				return &b_plus;
			default:
				return nullptr;
			}
		}

		mngl::render_window& m_render_window;
		mngl::two_d::font& m_tree_font;
		std::optional<mngl::texture> m_tree_canvas_texture;
		std::uint64_t m_tree_animation_source_id =
				next_tree_animation_source_id.fetch_add(1);
		std::shared_ptr<tree_animation_event_queue>
			m_tree_animation_event_queue =
				std::make_shared<tree_animation_event_queue>();
		std::optional<tree_animation_state> m_tree_animation;
		SDL_Window* m_native_window = nullptr;
		std::string m_tree_dialog_default_location;
		std::array<char, 128> m_explorer_filter{};
		std::array<char, 1024> m_explorer_path{};
		std::array<char, 1024> m_file_dialog_path{};
		std::filesystem::path m_file_dialog_source_path;
		std::optional<std::filesystem::path> m_pending_file_delete_path;
		std::string m_file_dialog_archive_source_path;
		std::string m_file_dialog_copy_contents;
		std::string m_pending_archive_delete_path;
		std::string m_file_dialog_required_extension;
		std::array<char, 128> m_function_filter{};
		std::filesystem::path m_explorer_directory;
		std::filesystem::path m_session_path;
		std::filesystem::path m_current_tree_path;
		std::optional<std::filesystem::path> m_selected_local_path;
		std::vector<explorer_entry> m_explorer_entries;
		std::vector<tree_practicum::archive_entry> m_session_entries;
		std::unordered_map<std::string, tree_document_state>
			m_tree_documents;
		std::vector<std::string> m_open_tree_tabs;
		std::string m_tree_tab_requested_path;
		std::string m_session_name;
		std::string m_current_tree_name;
		std::string m_current_tree_serialization;
		std::string m_current_archive_member_path;
		std::string m_merge_source_archive_path;
		std::array<std::string, 2> m_split_result_member_paths;
		std::optional<search_tree_key_kind> m_split_result_key_kind;
		std::string m_split_pivot_label;
		std::optional<visual_tree> m_loaded_visual_tree;
		std::optional<visual_key> m_hovered_tree_key;
		std::optional<std::size_t> m_hovered_tree_comparison_pane;
		std::optional<visual_key> m_tree_canvas_context_menu_key;
		std::optional<std::size_t> m_tree_canvas_context_menu_node;
		std::optional<tree_tool_operation> m_active_tree_tool;
		std::array<tree_comparison_pane, 2> m_tree_comparison_panes;
		std::array<tree_preview_cache_state, 2> m_tree_preview_caches;
		std::optional<search_tree_key_kind> m_loaded_search_tree_key_kind;
		std::vector<std::size_t> m_tree_find_path;
		std::string m_tree_find_status;
		std::string m_tree_parse_error;
		std::string m_tree_operation_error;
		std::string m_explorer_error;
		std::string m_file_error;
		std::string m_file_dialog_error;
		std::array<char, 1024> m_key_input{};
		std::uint64_t m_random_count = 25;
		int m_selected_workspace = 1;
		int m_selected_function = 0;
		int m_selected_archive_entry = -1;
		int m_active_tree = 0;
		int m_new_tree_type = 0;
		int m_tree_find_tree_index = -1;
		int m_loaded_tree_index = -1;
		int m_tree_filter = -1;
		int m_active_comparison_pane = 0;
		int m_tree_canvas_tree_index = -1;
		double m_tree_canvas_zoom = 1.0;
		glm::dvec2 m_tree_canvas_pan{};
		file_dialog_mode m_file_dialog_mode = file_dialog_mode::open;
		pending_workspace_action m_pending_workspace_action;
		bool m_show_explorer = true;
		bool m_show_workspace = true;
		bool m_show_tools = true;
		bool m_show_functions = true;
		bool m_show_information = true;
		bool m_execute_hovered_key_tools_immediately = true;
		bool m_tree_canvas_view_initialized = false;
		bool m_tree_canvas_fit_requested = false;
		bool m_focus_function_search = false;
		bool m_file_dialog_open = false;
		bool m_file_dialog_requested = false;
		bool m_file_delete_dialog_open = false;
		bool m_file_delete_dialog_requested = false;
		bool m_file_dialog_copy_from_memory = false;
		bool m_pending_delete_is_archive_entry = false;
		bool m_unsaved_changes_dialog_open = false;
		bool m_unsaved_changes_dialog_requested = false;
		bool m_waiting_for_save_before_workspace_action = false;
		bool m_tree_dirty = false;
		bool m_workspace_structure_dirty = false;
		bool m_show_demo_trees = false;
		bool m_tree_comparison_mode = false;
		bool m_loaded_search_tree_valid = false;
		bool m_tree_find_result_valid = false;
		bool m_tree_find_found = false;
		std::string m_last_action =
			"Откройте файл .tree, чтобы выполнять операции над деревом.";
		std::string m_history_warning;
		history_stack_type m_history;
		std::optional<interface_state> m_key_input_edit_start;
		std::optional<interface_state> m_random_count_edit_start;
		std::uint64_t m_current_document_state_id = 0;
		std::uint64_t m_saved_document_state_id = 0;
		std::uint64_t m_next_document_state_id = 1;
		std::uint64_t m_tree_preview_revision = 0;
	};
}
