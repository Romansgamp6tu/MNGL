#pragma once

#include "event.hpp"
#include "imgui_compat.hpp"
#include "logger.h"
#include "mod_command_catalog.hpp"
#include "mod_loader.hpp"
#include "render_window.hpp"
#include "tree_algorithm_api.hpp"
#include "tree_custom_command_api.hpp"
#include "tree_document.hpp"
#include "tree_invariant_api.hpp"
#include "tree_practicum_storage.hpp"
#include "tree_python_bridge.hpp"
#include "tree_renderer.hpp"
#include "undo_redo_stack.hpp"
#include "../tree/bst.hpp"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_filesystem.h>

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

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

		struct owned_function_entry
		{
			std::string algorithm_id;
			std::string name;
			std::string signature;
			std::string description;
			std::vector<mngl::mods::command_catalog::parameter> parameters;
			bool from_shared_catalog = false;
			bool custom = false;
		};

		struct function_entry
		{
			tree_practicum::mods::tree_id algorithm_id;
			std::string_view name;
			std::string_view signature;
			std::string_view description;
			const owned_function_entry* definition = nullptr;
		};

		struct tree_type_descriptor
		{
			tree_practicum::mods::algorithm_info algorithm;
		};

		struct explorer_entry
		{
			std::filesystem::path path;
			bool directory;
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

		[[nodiscard]] static tree_practicum::mods::operation
			tree_operation_capability(tree_tool_operation operation) noexcept
		{
			switch (operation)
			{
			case tree_tool_operation::find:
				return tree_practicum::mods::operation::find;
			case tree_tool_operation::insert:
				return tree_practicum::mods::operation::insert;
			case tree_tool_operation::random_insert:
				return tree_practicum::mods::operation::random_insert;
			case tree_tool_operation::erase:
				return tree_practicum::mods::operation::erase;
			case tree_tool_operation::merge:
				return tree_practicum::mods::operation::merge;
			case tree_tool_operation::split:
				return tree_practicum::mods::operation::split;
			}
			return tree_practicum::mods::operation::find;
		}

		[[nodiscard]] static std::optional<tree_tool_operation>
			tree_tool_for_function(std::string_view function_name) noexcept
		{
			if (function_name == "find") return tree_tool_operation::find;
			if (function_name == "insert") return tree_tool_operation::insert;
			if (function_name == "insert_random")
				return tree_tool_operation::random_insert;
			if (function_name == "erase") return tree_tool_operation::erase;
			if (function_name == "merge") return tree_tool_operation::merge;
			if (function_name == "split") return tree_tool_operation::split;
			return std::nullopt;
		}

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
			tree_practicum::mods::no_tree_node;
		inline static constexpr float visual_node_width = 156.0f;
		inline static constexpr float visual_node_height = 54.0f;
		inline static constexpr std::size_t maximum_animated_tree_nodes = 512;
		inline static constexpr std::size_t maximum_tree_animation_steps = 48;
		inline static constexpr float tree_animation_step_seconds = 0.14f;
		inline static constexpr float tree_layout_animation_seconds = 0.38f;

		using visual_key = tree_practicum::mods::key;
		using visual_node = tree_practicum::mods::node;
		using visual_tree = tree_practicum::mods::tree;

		struct demo_tree_entry
		{
			tree_practicum::mods::tree_id algorithm_id;
			visual_tree value;
		};

		struct tree_preview_geometry
		{
			mngl::two_d::tree_draw_model model;
			mngl::two_d::tree_layout layout;
			mngl::two_d::tree_draw_options draw_options;
			struct statistics
			{
				std::size_t node_count = 0;
				std::size_t key_count = 0;
				std::size_t leaf_count = 0;
				std::size_t height = 0;
				std::size_t maximum_branching = 0;
				std::string key_type = "не задан";
				std::string root_value = "—";
			} properties;
			std::uint64_t revision = 0;
			std::uint64_t source_tree_signature = 0;
			std::size_t animation_step = 0;
			std::string algorithm_id;
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
			std::string algorithm_id;
			double zoom = 1.0;
			glm::dvec2 pan{};
			bool view_initialized = false;
			bool fit_requested = true;
		};

		struct tree_comparison_pane
		{
			std::string archive_member_path;
			std::optional<visual_tree> tree;
			std::string algorithm_id;
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

		struct tree_document_window_state
		{
			tree_canvas_view_state canvas;
			tree_preview_cache_state preview;
			visual_tree tree;
			std::string algorithm_id;
			std::string source_contents;
			std::string parse_error;
			bool parsed = false;
		};

		using tree_visual_event = tree_practicum::mods::visual_event;
		using tree_operation_result = tree_practicum::mods::operation_result;
		using tree_bulk_insert_result = tree_practicum::mods::bulk_insert_result;
		using tree_erase_result = tree_practicum::mods::erase_result;
		using tree_find_result = tree_practicum::mods::find_result;
		using search_tree_key_kind = tree_practicum::mods::key_kind;

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
			step_events,
			layout_transition
		};

		struct tree_animation_state
		{
			tree_animation_mode mode = tree_animation_mode::step_events;
			visual_tree tree;
			std::vector<tree_visual_event> events;
			std::vector<glm::dvec2> transition_start_centers;
			glm::dvec2 transition_start_layout_size{};
			std::optional<std::size_t> emphasized_node;
			std::string algorithm_id;
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
			std::string selected_tree_id =
				std::string(tree_practicum::mods::ids::bst);
			std::array<char, 1024> key_input{};
			std::uint64_t random_count = 0;

			bool operator==(const interface_state&) const = default;

			[[nodiscard]] std::size_t history_bytes() const noexcept
			{
				return sizeof(*this) + selected_tree_id.size();
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
			std::string active_tree_id =
				std::string(tree_practicum::mods::ids::bst);
			std::string loaded_tree_id;
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
				add(active_tree_id.size());
				add(loaded_tree_id.size());
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
		inline static constexpr std::uint32_t history_file_version = 2;
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

		[[nodiscard]] const tree_type_descriptor* available_tree_type_for(
			tree_practicum::mods::tree_id id) const noexcept
		{
			const auto found = std::find_if(
				m_available_tree_types.begin(),
				m_available_tree_types.end(),
				[id](const auto& type) { return type.algorithm.id == id; });
			return found == m_available_tree_types.end()
				? nullptr
				: std::addressof(*found);
		}

		[[nodiscard]] const tree_type_descriptor*
			available_tree_type_for_serialization(
				std::string_view source) const noexcept
		{
			while (!source.empty() &&
				(source.front() == ' ' || source.front() == '\t' ||
					source.front() == '\r' || source.front() == '\n'))
			{
				source.remove_prefix(1);
			}

			const tree_type_descriptor* match = nullptr;
			std::size_t match_prefix_size = 0;
			for (const auto& type : m_available_tree_types)
			{
				const auto& info = type.algorithm;
				const auto consider_prefix = [&](std::string_view prefix)
				{
					if (!prefix.empty() && source.starts_with(prefix) &&
						prefix.size() > match_prefix_size)
					{
						match = std::addressof(type);
						match_prefix_size = prefix.size();
					}
				};
				consider_prefix(info.serialization_prefix);
				for (const auto alias :
					info.legacy_serialization_prefixes)
				{
					consider_prefix(alias);
				}
			}
			return match;
		}

		[[nodiscard]] std::string_view tree_name_for(
			tree_practicum::mods::tree_id id) const noexcept
		{
			const auto* type = available_tree_type_for(id);
			return type ? type->algorithm.display_name : id;
		}

		[[nodiscard]] static bool valid_tree_algorithm_id(
			std::string_view id) noexcept
		{
			if (id.empty())
			{
				return false;
			}
			const auto valid_character = [](char character)
			{
				return (character >= 'a' && character <= 'z') ||
					(character >= '0' && character <= '9') ||
					character == '.' || character == '_' || character == '-';
			};
			return ((id.front() >= 'a' && id.front() <= 'z') ||
				(id.front() >= '0' && id.front() <= '9')) &&
				std::all_of(id.begin(), id.end(), valid_character);
		}

		[[nodiscard]] static bool safe_tree_file_stem(
			std::string_view stem) noexcept
		{
			return !stem.empty() &&
				std::all_of(stem.begin(), stem.end(), [](char character)
				{
					return (character >= 'a' && character <= 'z') ||
						(character >= 'A' && character <= 'Z') ||
						(character >= '0' && character <= '9') ||
						character == '_' || character == '-';
				});
		}

		[[nodiscard]] static search_tree_key_kind default_tree_key_kind(
			const tree_practicum::mods::algorithm_info& info)
		{
			return tree_practicum::mods::default_key_kind_for(info);
		}

		inline static constexpr std::array<workspace_entry, 4> workspace_entries{ {
			{ "Черновик", "Текущая рабочая область", false },
			{ "Демо: семь ключей", "Демонстрационный сценарий", false },
			{ "До split", "Снимок демонстрационного дерева", true },
			{ "После split", "Снимок результата", true }
		} };



		[[nodiscard]] std::vector<function_entry> available_functions() const
		{
			std::vector<function_entry> result;
			for (const auto& function : m_command_functions)
			{
				if (available_tree_type_for(function->algorithm_id))
				{
					result.push_back({
						function->algorithm_id,
						function->name,
						function->signature,
						function->description,
						function.get() });
				}
			}

			constexpr std::array generic_operations{
				std::pair{
					tree_practicum::mods::operation::find,
					function_entry{
						{}, "find", "find(key)",
						"Ищет ключ и возвращает путь к нему." } },
				std::pair{
					tree_practicum::mods::operation::insert,
					function_entry{
						{}, "insert", "insert(key)",
						"Вставляет ключ в дерево." } },
				std::pair{
					tree_practicum::mods::operation::random_insert,
					function_entry{
						{}, "insert_random", "insert_random(count)",
						"Добавляет указанное число случайных ключей." } },
				std::pair{
					tree_practicum::mods::operation::erase,
					function_entry{
						{}, "erase", "erase(key)",
						"Удаляет ключ из дерева." } },
				std::pair{
					tree_practicum::mods::operation::merge,
					function_entry{
						{}, "merge", "merge(source)",
						"Объединяет текущее дерево с источником." } },
				std::pair{
					tree_practicum::mods::operation::split,
					function_entry{
						{}, "split", "split(pivot)",
						"Разделяет дерево по опорному ключу." } }
			};

			for (const auto& type : m_available_tree_types)
			{
				for (const auto& [operation, generic] : generic_operations)
				{
					if (!tree_practicum::mods::supports(
						type.algorithm.supported_operations,
						operation))
					{
						continue;
					}
					const bool already_documented = std::any_of(
						result.begin(), result.end(),
						[&](const auto& function)
						{
							return function.algorithm_id == type.algorithm.id &&
								function.name == generic.name;
						});
					if (!already_documented)
					{
						auto function = generic;
						function.algorithm_id = type.algorithm.id;
						result.push_back(function);
					}
				}
			}
			return result;
		}

	public:
		tree_practicum_ui(
			mngl::render_window& render_window,
			mngl::two_d::font& tree_font,
			SDL_Window* native_window = nullptr)
			: m_mod_loader(m_mod_registry)
			, m_render_window(render_window)
			, m_tree_font(tree_font)
			, m_native_window(
				native_window ? native_window : render_window.raw())
		{
			load_application_settings();
			initialize_tree_mods();
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
			initialize_live_python_bridge();
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
			save_application_settings();
			std::lock_guard lock(tree_animation_queues_mutex);
			tree_animation_event_queues.erase(m_tree_animation_source_id);
			if (!m_live_python_directory.empty())
			{
				std::error_code error;
				std::filesystem::remove_all(
					m_live_python_directory,
					error);
			}
		}

		tree_practicum_ui(const tree_practicum_ui&) = delete;
		tree_practicum_ui& operator=(const tree_practicum_ui&) = delete;
		tree_practicum_ui(tree_practicum_ui&&) = delete;
		tree_practicum_ui& operator=(tree_practicum_ui&&) = delete;

		void draw(mngl::imgui_context::frame_scope& frame)
		{
			// A closed window can still have its texture referenced by this
			// frame's ImGui draw data. Release its GPU resources on the next frame.
			std::erase_if(
				m_tree_document_window_states,
				[this](const auto& item)
				{
					return std::find(
						m_open_tree_tabs.begin(),
						m_open_tree_tabs.end(),
						item.first) == m_open_tree_tabs.end();
				});
			m_hovered_tree_key.reset();
			m_hovered_tree_comparison_pane.reset();
			m_hovered_tree_archive_path.clear();
			process_pending_tree_files();
			draw_menu(frame);
			ImGui::DockSpaceOverViewport();

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			if (!viewport)
			{
				process_live_python_bridge();
				commit_pending_input_edits();
				process_keyboard_shortcuts();
				return;
			}

			const ImVec2 origin = viewport->WorkPos;
			const ImVec2 area = viewport->WorkSize;
			if (area.x <= 0.0f || area.y <= 0.0f)
			{
				process_live_python_bridge();
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
			// Tree-window focus is resolved while drawing the workspace. Process
			// Python requests afterwards so selected_tree() observes this frame's
			// active document instead of the previous frame's selection.
			process_live_python_bridge();
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
			draw_new_tree_dialog(frame);
			draw_file_dialog();
			draw_file_delete_dialog();
			draw_unsaved_changes_dialog();
			process_keyboard_shortcuts();
		}

	private:
		[[nodiscard]] interface_state current_interface_state() const
		{
			return {
				m_selected_workspace,
				m_new_tree_id,
				m_key_input,
				m_random_count
			};
		}

		static void validate_interface_state(const interface_state& state)
		{
			if (state.selected_workspace < 0 ||
				static_cast<std::size_t>(state.selected_workspace) >=
					workspace_entries.size() ||
				!valid_tree_algorithm_id(state.selected_tree_id) ||
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
			writer.write_string(state.selected_tree_id);
			writer.write_bytes(std::string_view(
				state.key_input.data(),
				state.key_input.size()));
			writer.write_unsigned(state.random_count);
		}

		[[nodiscard]] std::string tree_id_from_legacy_index(int index) const
		{
			const auto found = std::find_if(
				m_available_tree_types.begin(),
				m_available_tree_types.end(),
				[index](const auto& type)
				{
					return type.algorithm.legacy_tree_catalog_index == index;
				});
			if (found == m_available_tree_types.end())
			{
				throw std::invalid_argument(
					"Undo history contains an invalid legacy tree index");
			}
			return std::string(found->algorithm.id);
		}

		[[nodiscard]] interface_state read_interface_state(
			history_binary_reader& reader,
			std::uint32_t version)
		{
			interface_state state;
			state.selected_workspace = reader.read_signed();
			state.selected_tree_id = version == 1
				? tree_id_from_legacy_index(reader.read_signed())
				: reader.read_string();
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
			if (!valid_tree_algorithm_id(state.active_tree_id) ||
				(!state.loaded_tree_id.empty() &&
					!valid_tree_algorithm_id(state.loaded_tree_id)) ||
				(state.loaded_search_tree_valid &&
					state.loaded_tree_id.empty()) ||
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
			writer.write_string(state.active_tree_id);
			writer.write_string(state.loaded_tree_id);
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

		[[nodiscard]] tree_history_snapshot
			read_tree_history_snapshot(
				history_binary_reader& reader,
				std::uint32_t version)
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
			if (version == 1)
			{
				state.active_tree_id = tree_id_from_legacy_index(
					reader.read_signed());
				const int loaded_index = reader.read_signed();
				state.loaded_tree_id = loaded_index < 0
					? std::string{}
					: tree_id_from_legacy_index(loaded_index);
			}
			else
			{
				state.active_tree_id = reader.read_string();
				state.loaded_tree_id = reader.read_string();
			}
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

		[[nodiscard]] history_state read_history_state(
			history_binary_reader& reader,
			std::uint32_t version)
		{
			const auto kind = reader.read_unsigned<std::uint8_t>();
			if (kind == 0)
			{
				return history_state{
					read_interface_state(reader, version) };
			}
			if (kind != 1)
			{
				throw std::invalid_argument(
					"Undo history contains an unknown state type");
			}

			tree_history_state state;
			state.is_after = reader.read_bool();
			auto transition = std::make_shared<tree_history_transition>();
			transition->before = read_tree_history_snapshot(reader, version);
			transition->after = read_tree_history_snapshot(reader, version);
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

		[[nodiscard]] history_document deserialize_history_document(
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
			if (version != 1 && version != history_file_version)
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
				read_interface_state(reader, version);
			document.current_tree =
				read_tree_history_snapshot(reader, version);
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
				document.undo_states.push_back(
					read_history_state(reader, version));
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
				document.redo_states.push_back(
					read_history_state(reader, version));
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
						"Индекс нового узла в анимации не совпадает с размером дерева.");
				}
				if (step.parent_index == no_visual_node)
				{
					if (!tree.nodes.empty() ||
						step.node_index != 0 ||
						step.inserted_node.binary_children[0] != no_visual_node ||
						step.inserted_node.binary_children[1] != no_visual_node)
					{
						throw std::logic_error(
							"Некорректный шаг вставки корня в анимации дерева.");
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
					"Некорректный шаг вставки узла в анимации дерева.");
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
					"Анимация дерева содержит ссылку на отсутствующий узел.");
			}
			rotate_binary_node_up(
				tree,
				step.parent_index,
				step.node_index,
				step.grandparent_index);
		}

		[[nodiscard]] float tree_animation_duration(
			float base_duration) const noexcept
		{
			const float speed = std::clamp(
				m_tree_animation_speed,
				0.5f,
				4.0f);
			return base_duration / speed;
		}

		void advance_tree_animation(
			std::string_view algorithm_id,
			float delta_time)
		{
			if (!m_tree_animations_enabled)
			{
				cancel_tree_animation();
				return;
			}
			if (!m_tree_animation.has_value())
			{
				return;
			}
			if (m_tree_animation->algorithm_id != algorithm_id)
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
			const float step_duration = tree_animation_duration(
				tree_animation_step_seconds);
			if (animation.mode ==
				tree_animation_mode::layout_transition)
			{
				if (animation.elapsed_seconds >=
					tree_animation_duration(
						tree_layout_animation_seconds))
				{
					m_tree_animation.reset();
				}
				return;
			}
			if (animation.next_event >= animation.events.size())
			{
				if (animation.elapsed_seconds >= step_duration)
				{
					m_tree_animation.reset();
				}
				return;
			}
			if (animation.elapsed_seconds < step_duration)
			{
				return;
			}

			animation.elapsed_seconds -= step_duration;
			const auto& step = animation.events[animation.next_event++];
			try
			{
				apply_tree_visual_event(animation.tree, step);
				animation.active_event = step;
			}
			catch (const std::exception& error)
			{
				m_tree_operation_error = std::format(
					"Анимация дерева остановлена: {}",
					error.what());
				logger.error(
					"Unable to process a tree animation event: ",
					error.what());
				cancel_tree_animation();
			}
		}

		void restore_interface_state(const interface_state& state)
		{
			m_selected_workspace = state.selected_workspace;
			m_new_tree_id = state.selected_tree_id;
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
			state.active_tree_id = m_active_tree_id;
			state.loaded_tree_id = m_loaded_tree_id;
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
			m_tree_tab_requested_path =
				state.current_archive_member_path;
			m_merge_source_archive_path =
				state.merge_source_archive_path;
			m_split_result_member_paths =
				state.split_result_member_paths;
			m_split_result_key_kind = state.split_result_key_kind;
			m_split_pivot_label = state.split_pivot_label;
			m_active_tree_id = state.active_tree_id;
			m_loaded_tree_id = state.loaded_tree_id;
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
			restore_active_tree_document_view();
			remember_active_tree_document();
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

		void select_tree(std::string_view algorithm_id)
		{
			if (!available_tree_type_for(algorithm_id) ||
				algorithm_id == m_new_tree_id)
			{
				return;
			}
			commit_pending_input_edits();
			const auto previous = current_interface_state();
			m_new_tree_id = algorithm_id;
			record_interface_change(previous);
		}

		void request_new_tree_dialog()
		{
			m_pending_new_tree_id.clear();
			m_new_tree_dialog_open = true;
			m_new_tree_dialog_recenter = true;
		}

		void draw_new_tree_dialog(
			mngl::imgui_context::frame_scope& frame)
		{
			if (!m_new_tree_dialog_open)
			{
				return;
			}

			if (m_new_tree_dialog_recenter)
			{
				if (const ImGuiViewport* viewport = ImGui::GetMainViewport())
				{
					const ImVec2 center{
						viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
						viewport->WorkPos.y + viewport->WorkSize.y * 0.5f
					};
					ImGui::SetNextWindowPos(
						center,
						ImGuiCond_Always,
						{ 0.5f, 0.5f });
				}
				m_new_tree_dialog_recenter = false;
			}

			auto dialog = frame.window(
				"Создать дерево###TreePracticum.NewTreeDialog",
				ImGuiWindowFlags_AlwaysAutoResize |
					ImGuiWindowFlags_NoCollapse |
					ImGuiWindowFlags_NoDocking |
					ImGuiWindowFlags_NoSavedSettings);
			if (!dialog)
			{
				return;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				m_new_tree_dialog_open = false;
				m_pending_new_tree_id.clear();
				return;
			}

			ImGui::TextWrapped(
				"Выберите встроенный алгоритм или одно из загруженных расширений.");

			std::string preview = "Выберите реализацию...";
			for (const auto& type : m_available_tree_types)
			{
				if (type.algorithm.id == m_pending_new_tree_id)
				{
					preview = std::string(type.algorithm.display_name);
					preview += type.algorithm.id ==
						tree_practicum::mods::ids::bst
						? " (встроенный)"
						: " (расширение)";
					break;
				}
			}

			ImGui::SetNextItemWidth(380.0f);
			if (ImGui::BeginCombo("Алгоритм дерева", preview.c_str()))
			{
				for (const auto& type : m_available_tree_types)
				{
					const std::string id(type.algorithm.id);
					std::string label(type.algorithm.display_name);
					label += type.algorithm.id ==
						tree_practicum::mods::ids::bst
						? " (встроенный)"
						: " (расширение)";
					ImGui::PushID(id.c_str());
					const bool selected = id == m_pending_new_tree_id;
					if (ImGui::Selectable(label.c_str(), selected))
					{
						m_pending_new_tree_id = id;
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}

			const bool selected_type_available =
				available_tree_type_for(m_pending_new_tree_id) != nullptr;
			ImGui::BeginDisabled(!selected_type_available);
			if (ImGui::Button("Создать дерево"))
			{
				const std::string selected_id = m_pending_new_tree_id;
				m_new_tree_dialog_open = false;
				m_pending_new_tree_id.clear();
				(void)perform_io([this, selected_id]
				{
					select_tree(selected_id);
					create_new_tree();
				});
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Отмена"))
			{
				m_new_tree_dialog_open = false;
				m_pending_new_tree_id.clear();
			}
		}

		void clear_tree_find_result() noexcept
		{
			m_tree_find_path.clear();
			m_tree_find_status.clear();
			m_tree_find_tree_id.clear();
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
					const auto source_archive_path =
						m_hovered_tree_archive_path;
					m_hovered_tree_key.reset();
					m_hovered_tree_comparison_pane.reset();
					invoke_tree_tool(
						tree_tool_operation::erase,
						std::move(key),
						std::move(comparison_pane),
						source_archive_path);
					m_hovered_tree_archive_path.clear();
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
					const auto source_archive_path =
						m_hovered_tree_archive_path;
					m_hovered_tree_key.reset();
					m_hovered_tree_comparison_pane.reset();
					invoke_tree_tool(
						tree_tool_operation::erase,
						std::move(key),
						std::move(comparison_pane),
						source_archive_path);
					m_hovered_tree_archive_path.clear();
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
					const auto source_archive_path =
						m_hovered_tree_archive_path;
					m_hovered_tree_key.reset();
					m_hovered_tree_comparison_pane.reset();
					invoke_tree_tool(
						tree_tool_operation::split,
						std::move(key),
						std::move(comparison_pane),
						source_archive_path);
					m_hovered_tree_archive_path.clear();
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
					request_new_tree_dialog();
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
				ImGui::TextDisabled("Анимации дерева");
				if (ImGui::MenuItem(
					"Показывать анимации операций",
					nullptr,
					&m_tree_animations_enabled))
				{
					if (!m_tree_animations_enabled)
					{
						cancel_tree_animation();
					}
					save_application_settings();
				}
				if (ImGui::BeginMenu(
					"Скорость анимации",
					m_tree_animations_enabled))
				{
					constexpr std::array speeds{ 0.5f, 1.0f, 2.0f, 4.0f };
					constexpr std::array labels{
						"0,5×",
						"1×",
						"2×",
						"4×"
					};
					for (std::size_t index = 0;
						index < speeds.size();
						++index)
					{
						if (ImGui::MenuItem(
							labels[index],
							nullptr,
							m_tree_animation_speed == speeds[index]))
						{
							m_tree_animation_speed = speeds[index];
							save_application_settings();
						}
					}
					ImGui::EndMenu();
				}

				ImGui::Separator();
				ImGui::TextDisabled("Поведение операций");
				if (ImGui::MenuItem(
					"Сразу выполнять Erase/Split по наведённому ключу",
					nullptr,
					&m_execute_hovered_key_tools_immediately))
				{
					save_application_settings();
				}
				ImGui::TextDisabled(
					m_execute_hovered_key_tools_immediately
						? "Выключите для подтверждения в окне параметров."
						: "Включите для немедленного выполнения.");
				ImGui::Separator();
				if (ImGui::MenuItem("Сбросить настройки по умолчанию"))
				{
					m_tree_animations_enabled = true;
					m_tree_animation_speed = 1.0f;
					m_execute_hovered_key_tools_immediately = true;
					cancel_tree_animation();
					save_application_settings();
				}
				ImGui::TextDisabled("Настройки сохраняются автоматически.");
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Вид"))
			{
				ImGui::MenuItem("Проводник", nullptr, &m_show_explorer);
				ImGui::MenuItem("Документы дерева", nullptr, &m_show_workspace);
				ImGui::MenuItem("Панель операций", nullptr, &m_show_tools);
				ImGui::MenuItem("Поиск функций", nullptr, &m_show_functions);
				ImGui::MenuItem("Информация", nullptr, &m_show_information);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Инструменты"))
			{
				if (ImGui::MenuItem(
					"Найти ключ",
					"F",
					false,
					supports_tool_operation(tree_tool_operation::find)))
				{
					invoke_tree_tool(tree_tool_operation::find);
				}
				if (ImGui::MenuItem(
					"Вставить ключ",
					"A / Ins",
					false,
					supports_tool_operation(tree_tool_operation::insert)))
				{
					invoke_tree_tool(tree_tool_operation::insert);
				}
				if (ImGui::MenuItem(
					"Случайная вставка",
					"R",
					false,
					supports_tool_operation(tree_tool_operation::random_insert)))
				{
					invoke_tree_tool(
						tree_tool_operation::random_insert);
				}
				if (ImGui::MenuItem(
					"Удалить ключ",
					"D / Del",
					false,
					supports_tool_operation(tree_tool_operation::erase)))
				{
					invoke_tree_tool(tree_tool_operation::erase);
				}
				if (ImGui::MenuItem(
					"Разделить дерево",
					"C",
					false,
					supports_tool_operation(tree_tool_operation::split)))
				{
					invoke_tree_tool(tree_tool_operation::split);
				}
				if (ImGui::MenuItem(
					"Объединить с источником",
					"M",
					false,
					supports_tool_operation(tree_tool_operation::merge)))
				{
					invoke_tree_tool(tree_tool_operation::merge);
				}
				if (!can_insert_into_selected_search_tree())
				{
					ImGui::TextDisabled(
						"Для операций выберите корректное дерево с поддерживаемым типом ключей.");
				}
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

		bool function_matches_query(
			const function_entry& function,
			std::string_view query) const noexcept
		{
			if (function_has_alias(function.name, query))
			{
				return true;
			}

			return contains_text(function.name, query) ||
				contains_text(function.signature, query) ||
				contains_text(tree_name_for(function.algorithm_id), query) ||
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

		[[nodiscard]] static std::filesystem::path application_settings_path()
		{
			std::unique_ptr<char, decltype(&SDL_free)> preference_directory(
				SDL_GetPrefPath("MNGL", "MNGL"),
				SDL_free);
			if (!preference_directory)
			{
				return {};
			}
			const auto directory = path_from_utf8(preference_directory.get());
			return directory / "settings.ini";
		}

		void load_application_settings() noexcept
		{
			try
			{
				const auto path = application_settings_path();
				if (path.empty())
				{
					return;
				}
				std::ifstream stream(path);
				std::string line;
				if (!std::getline(stream, line) ||
					line != "MNGL_APP_SETTINGS\t1")
				{
					return;
				}

				while (std::getline(stream, line))
				{
					const auto separator = line.find('=');
					if (separator == std::string::npos)
					{
						continue;
					}
					const auto name = std::string_view(line).substr(0, separator);
					const auto value = std::string_view(line).substr(separator + 1);
					if (name == "tree_animations")
					{
						if (value == "true")
						{
							m_tree_animations_enabled = true;
						}
						else if (value == "false")
						{
							m_tree_animations_enabled = false;
						}
					}
					else if (name == "tree_animation_speed")
					{
						float speed = 0.0f;
						const auto [end, error] = std::from_chars(
							value.data(),
							value.data() + value.size(),
							speed);
						if (error == std::errc{} &&
							end == value.data() + value.size() &&
							std::isfinite(speed) &&
							(speed == 0.5f || speed == 1.0f ||
								speed == 2.0f || speed == 4.0f))
						{
							m_tree_animation_speed = speed;
						}
					}
					else if (name == "execute_hovered_key_tools_immediately")
					{
						if (value == "true")
						{
							m_execute_hovered_key_tools_immediately = true;
						}
						else if (value == "false")
						{
							m_execute_hovered_key_tools_immediately = false;
						}
					}
				}
			}
			catch (const std::exception& error)
			{
				logger.warning(
					"Unable to load application settings: ",
					error.what());
			}
		}

		void save_application_settings() noexcept
		{
			try
			{
				const auto path = application_settings_path();
				if (path.empty())
				{
					return;
				}
				std::ofstream stream(
					path,
					std::ios::trunc);
				if (!stream)
				{
					logger.warning(
						"Unable to save application settings to ",
						path_to_utf8(path));
					return;
				}
				stream << "MNGL_APP_SETTINGS\t1\n"
					<< "tree_animations="
					<< (m_tree_animations_enabled ? "true" : "false") << '\n'
					<< "tree_animation_speed="
					<< std::format("{:.1f}", m_tree_animation_speed) << '\n'
					<< "execute_hovered_key_tools_immediately="
					<< (m_execute_hovered_key_tools_immediately
						? "true"
						: "false") << '\n';
				if (!stream)
				{
					logger.warning(
						"Failed while writing application settings to ",
						path_to_utf8(path));
				}
			}
			catch (const std::exception& error)
			{
				logger.warning(
					"Unable to save application settings: ",
					error.what());
			}
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
				try
				{
					logger.error(
						"Could not queue the selected or dropped tree file.");
				}
				catch (...)
				{
				}
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
				try
				{
					logger.error(
						"Could not queue the native file dialog error.");
				}
				catch (...)
				{
				}
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

		void initialize_live_python_bridge() noexcept
		{
			try
			{
				std::random_device random;
				for (int attempt = 0; attempt < 8; ++attempt)
				{
					const auto random_hex = [&]
					{
						std::string value;
						value.reserve(32);
						for (int word = 0; word < 4; ++word)
						{
							value += std::format("{:08x}", random());
						}
						return value;
					};
					const auto session_id = random_hex();
					const auto token = random_hex();
					const auto directory =
						std::filesystem::temp_directory_path() /
						("mngl-live-" + session_id);
					std::error_code error;
					if (!std::filesystem::create_directory(directory, error))
					{
						continue;
					}
					m_live_python_directory = directory;
					m_live_python_session_id = session_id;
					m_live_python_token = token;
					#if defined(_WIN32)
					const auto process_id = static_cast<std::uint64_t>(
						GetCurrentProcessId());
					#else
					const auto process_id = static_cast<std::uint64_t>(::getpid());
					#endif
					std::ofstream metadata(
						directory / "session.meta",
						std::ios::binary | std::ios::trunc);
					metadata << "protocol=1\n"
						"session_id=" << session_id << "\n"
						"process_id=" << process_id << "\n"
						"token=" << token << "\n";
					metadata.flush();
					if (!metadata)
					{
						throw std::runtime_error(
							"Could not publish the live Python bridge session.");
					}
					logger.info(
						"Live Python bridge session is ready (ID ",
						session_id,
						").");
					return;
				}
				throw std::runtime_error(
					"Could not allocate a unique live Python bridge directory.");
			}
			catch (const std::exception& error)
			{
				if (!m_live_python_directory.empty())
				{
					std::error_code cleanup_error;
					std::filesystem::remove_all(
						m_live_python_directory,
						cleanup_error);
				}
				m_live_python_directory.clear();
				m_live_python_session_id.clear();
				m_live_python_token.clear();
				logger.warning(
					"Live Python bridge is unavailable: ",
					error.what());
			}
		}

		void write_live_python_response(
			const python_bridge::response& response) noexcept
		{
			if (m_live_python_directory.empty() ||
				!python_bridge::valid_request_id(response.request_id))
			{
				return;
			}
			try
			{
				const auto filename = "response-" + response.request_id;
				const auto temporary =
					m_live_python_directory / (filename + ".tmp");
				const auto target =
					m_live_python_directory / (filename + ".ready");
				std::ofstream output(
					temporary,
					std::ios::binary | std::ios::trunc);
				output << python_bridge::encode_response(
					response,
					m_live_python_token);
				output.flush();
				if (!output)
				{
					throw std::runtime_error(
						"Could not write a live Python bridge response.");
				}
				output.close();
				std::error_code error;
				std::filesystem::rename(temporary, target, error);
				if (error)
				{
					std::filesystem::remove(temporary);
					throw std::filesystem::filesystem_error(
						"Could not publish a live Python bridge response",
						temporary,
						target,
						error);
				}
			}
			catch (const std::exception& error)
			{
				logger.error(
					"Unable to return a live Python result: ",
					error.what());
			}
		}

		[[nodiscard]] std::string live_python_tree_inventory(
			std::string_view scope) const
		{
			std::vector<const tree_practicum::archive_entry*> entries;
			if (scope == "tree.selected")
			{
				if (const auto* entry = find_archive_entry(
					m_session_entries,
					m_current_archive_member_path);
					entry && archive_extension(entry->path) == ".tree")
				{
					entries.push_back(entry);
				}
			}
			else if (scope == "trees.opened")
			{
				entries.reserve(m_open_tree_tabs.size());
				for (const auto& path : m_open_tree_tabs)
				{
					if (const auto* entry = find_archive_entry(
						m_session_entries,
						path);
						entry && archive_extension(entry->path) == ".tree")
					{
						entries.push_back(entry);
					}
				}
			}
			else if (scope == "trees.loaded")
			{
				entries.reserve(m_session_entries.size());
				for (const auto& entry : m_session_entries)
				{
					if (archive_extension(entry.path) == ".tree")
					{
						entries.push_back(std::addressof(entry));
					}
				}
			}
			else
			{
				throw std::invalid_argument(
					"Unknown live Python tree inventory scope.");
			}

			std::string result = std::format(
				"tree_count={}\n",
				entries.size());
			const auto append_field = [&result](
				std::string_view name,
				std::string_view value)
			{
				result.append(name);
				result.push_back('=');
				result.append(value);
				result.push_back('\n');
			};
			for (std::size_t index = 0; index < entries.size(); ++index)
			{
				const auto& entry = *entries[index];
				const auto document = m_tree_documents.find(entry.path);
				std::string name;
				if (document != m_tree_documents.end() &&
					!document->second.display_name.empty())
				{
					name = document->second.display_name;
				}
				else if (document != m_tree_documents.end() &&
					!document->second.local_path.empty())
				{
					name = path_to_utf8(
						document->second.local_path.filename());
				}
			else
			{
				const auto separator = entry.path.find_last_of('/');
				const auto filename = std::string_view(entry.path).substr(
					separator == std::string::npos ? 0 : separator + 1);
				const auto extension = filename.find_last_of('.');
				name = std::string(filename.substr(
					0,
					extension == std::string_view::npos
						? filename.size()
						: extension));
			}

				const auto* type = available_tree_type_for_serialization(
					entry.contents);
				const auto prefix = std::format("tree_{}", index);
				append_field(
					prefix + "_reference_hex",
					python_bridge::hex_encode("archive:" + entry.path));
				append_field(
					prefix + "_name_hex",
					python_bridge::hex_encode(name));
				append_field(
					prefix + "_algorithm_hex",
					python_bridge::hex_encode(
						type ? type->algorithm.id : std::string_view{}));
				append_field(
					prefix + "_is_open",
					std::find(
						m_open_tree_tabs.begin(),
						m_open_tree_tabs.end(),
						entry.path) != m_open_tree_tabs.end()
						? "true"
						: "false");
				append_field(
					prefix + "_is_selected",
					entry.path == m_current_archive_member_path
						? "true"
						: "false");
				append_field(
					prefix + "_is_dirty",
					tree_document_is_dirty(entry.path)
						? "true"
						: "false");
			}
			return result;
		}

		[[nodiscard]] std::string find_workspace_tree_document(
			std::string_view tree_reference) const
		{
			std::string member_path;
			if (tree_reference.starts_with("archive:"))
			{
				member_path = std::string(tree_reference.substr(8));
			}
			else
			{
				member_path = find_tree_document_for_local_path(
					path_from_utf8(tree_reference));
			}
			if (member_path.empty())
			{
				throw std::invalid_argument(
					"This tree is not loaded in the current MNGL workspace.");
			}
			const auto* entry = find_archive_entry(
				m_session_entries,
				member_path);
			if (!entry || archive_extension(entry->path) != ".tree")
			{
				throw std::invalid_argument(
					"This workspace item is not a loaded .tree document.");
			}
			return member_path;
		}

		[[nodiscard]] python_bridge::response execute_live_python_request(
			const python_bridge::request& request)
		{
			python_bridge::response response;
			response.request_id = request.request_id;
			try
			{
				const auto& operation = request.operation;
				const auto& tree_reference = request.tree_reference;
				const auto& key = request.key;
				if (operation == "tree.selected" ||
					operation == "trees.opened" ||
					operation == "trees.loaded")
				{
					response.stdout_text =
						live_python_tree_inventory(operation);
					return response;
				}

				const auto archive_path = find_workspace_tree_document(
					tree_reference);
				const auto* entry = find_archive_entry(
					m_session_entries,
					archive_path);
				if (!entry)
				{
					throw std::logic_error(
						"The requested tree is missing from the current workspace.");
				}
				const auto selected_entry = *entry;
				const bool tree_tab_is_open = std::find(
					m_open_tree_tabs.begin(),
					m_open_tree_tabs.end(),
					archive_path) != m_open_tree_tabs.end();
				m_tree_comparison_mode = false;
				if (m_current_archive_member_path != archive_path ||
					!tree_tab_is_open)
				{
					open_archive_tree(selected_entry);
				}
				if (operation == "select")
				{
					m_tree_tab_requested_path = archive_path;
					response.stdout_text =
						"selected=true\nreference_hex=" +
						python_bridge::hex_encode(
							"archive:" + archive_path) + "\n";
					return response;
				}

				if (key.size() >= m_key_input.size())
				{
					throw std::invalid_argument(
						"Tree key text is too long for the MNGL operation input.");
				}
				m_tree_comparison_mode = false;
				if (m_current_archive_member_path != archive_path)
				{
					open_archive_tree(selected_entry);
				}
				m_tree_tab_requested_path = archive_path;
				m_key_input_edit_start.reset();
				set_path_buffer(m_key_input, key);
				const std::string serialization_before =
					m_current_tree_serialization;

				if (operation == "find")
				{
					if (!supports_tool_operation(tree_tool_operation::find))
					{
						throw std::invalid_argument(
							"Find is not supported by the open tree or its key type.");
					}
					perform_search_tree_find();
					if (!m_tree_operation_error.empty())
					{
						throw std::runtime_error(m_tree_operation_error);
					}
					response.stdout_text = std::format(
						"operation=find\nfound={}\nchanged={}\nkey={}\n",
						m_tree_find_found ? "true" : "false",
						m_current_tree_serialization != serialization_before
							? "true"
							: "false",
						key);
				}
				else if (operation == "insert")
				{
					if (!supports_tool_operation(tree_tool_operation::insert))
					{
						throw std::invalid_argument(
							"Insert is not supported by the open tree or its key type.");
					}
					perform_search_tree_insert();
					if (!m_tree_operation_error.empty())
					{
						throw std::runtime_error(m_tree_operation_error);
					}
					response.stdout_text = std::format(
						"operation=insert\nchanged={}\nkey={}\n",
						m_current_tree_serialization != serialization_before
							? "true"
							: "false",
						key);
				}
				else if (operation == "erase")
				{
					if (!supports_tool_operation(tree_tool_operation::erase))
					{
						throw std::invalid_argument(
							"Erase is not supported by the open tree or its key type.");
					}
					perform_search_tree_erase();
					if (!m_tree_operation_error.empty())
					{
						throw std::runtime_error(m_tree_operation_error);
					}
					response.stdout_text = std::format(
						"operation=erase\nchanged={}\nkey={}\n",
						m_current_tree_serialization != serialization_before
							? "true"
							: "false",
						key);
				}
				else
				{
					throw std::invalid_argument(
						"Live Python supports find, insert, and erase operations.");
				}
				logger.success(
					"Live Python command completed: ",
					operation,
					" (request ",
					request.request_id,
					").");
			}
			catch (const std::exception& error)
			{
				response.exit_code = 1;
				response.stderr_text = error.what();
				m_tree_operation_error = error.what();
				m_last_action = std::format(
					"Команда Python не выполнена: {}",
					error.what());
				logger.error(
					"Live Python command failed (request ",
					request.request_id,
					"): ",
					error.what());
			}
			return response;
		}

		void process_live_python_bridge() noexcept
		{
			if (m_live_python_directory.empty())
			{
				return;
			}
			if (m_live_python_response)
			{
				if (m_tree_animation)
				{
					return;
				}
				write_live_python_response(*m_live_python_response);
				m_live_python_response.reset();
				return;
			}

			const auto request_path = m_live_python_directory / "request.pending";
			std::ifstream request_file(request_path, std::ios::binary);
			if (!request_file)
			{
				return;
			}
			const std::string request_contents{
				std::istreambuf_iterator<char>(request_file),
				std::istreambuf_iterator<char>() };
			request_file.close();
			std::error_code remove_error;
			std::filesystem::remove(request_path, remove_error);

			try
			{
				auto request = python_bridge::decode_request(
					request_contents,
					m_live_python_token);
				auto response = execute_live_python_request(request);
				if (!python_bridge::valid_request_id(response.request_id))
				{
					logger.error(
						"Ignored a live Python request without a request ID.");
					return;
				}
				m_live_python_response = std::move(response);
			}
			catch (const std::exception& error)
			{
				logger.warning(
					"Rejected a live Python request: ",
					error.what());
			}
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
					same_local_tree_document_path(m_current_tree_path, path);
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

			if ((extension == ".stree" || extension == ".tar") &&
				!m_session_path.empty() &&
				same_local_tree_document_path(m_session_path, path))
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
				state.workspace_path = new_path;
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
			if (m_tree_tab_requested_path == path)
			{
				m_tree_tab_requested_path.clear();
			}
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
				normalized_local_tree_document_path(source);
			std::filesystem::rename(source, target);
			for (auto& [archive_path, document] : m_tree_documents)
			{
				(void)archive_path;
				if (!document.local_path.empty() &&
					same_local_tree_document_path(
						document.local_path,
						source_identity))
				{
					document.local_path = target;
					document.display_name = path_to_utf8(
						target.filename());
				}
			}
			if (!m_current_tree_path.empty() &&
				same_local_tree_document_path(
					m_current_tree_path,
					source_identity))
			{
				m_current_tree_path = target;
				m_current_tree_name = path_to_utf8(target.stem());
			}
			if (!m_session_path.empty() &&
				same_local_tree_document_path(
					m_session_path,
					source_identity))
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
					same_local_tree_document_path(document.local_path, path))
				{
					document.local_path.clear();
				}
			}
			if (!m_current_tree_path.empty() &&
				same_local_tree_document_path(m_current_tree_path, path))
			{
				m_current_tree_path.clear();
			}
			if (!m_session_path.empty() &&
				same_local_tree_document_path(m_session_path, path))
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
			auto document = m_tree_documents.try_emplace(
				std::string(archive_member_path)).first;
			document->second.workspace_path = archive_member_path;
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
			remember_active_tree_document_view();
		}

		void remember_active_tree_document_view() noexcept
		{
			if (m_current_archive_member_path.empty())
			{
				return;
			}
			const auto found = m_tree_documents.find(
				m_current_archive_member_path);
			if (found == m_tree_documents.end())
			{
				return;
			}
			auto& view = found->second.view;
			if (const auto canvas = m_tree_document_window_states.find(
				m_current_archive_member_path);
				canvas != m_tree_document_window_states.end() && canvas->second)
			{
				const auto& document_canvas = canvas->second->canvas;
				view.zoom = document_canvas.zoom;
				view.pan_x = document_canvas.pan.x;
				view.pan_y = document_canvas.pan.y;
				view.initialized = document_canvas.view_initialized;
				view.fit_requested = document_canvas.fit_requested;
				return;
			}
			view.zoom = m_tree_canvas_zoom;
			view.pan_x = m_tree_canvas_pan.x;
			view.pan_y = m_tree_canvas_pan.y;
			view.initialized = m_tree_canvas_view_initialized;
			view.fit_requested = m_tree_canvas_fit_requested;
		}

		void restore_active_tree_document_view() noexcept
		{
			if (m_current_archive_member_path.empty())
			{
				return;
			}
			const auto found = m_tree_documents.find(
				m_current_archive_member_path);
			if (found == m_tree_documents.end())
			{
				return;
			}
			const auto& view = found->second.view;
			m_tree_canvas_zoom = view.zoom;
			m_tree_canvas_pan = { view.pan_x, view.pan_y };
			m_tree_canvas_view_initialized = view.initialized;
			m_tree_canvas_fit_requested = view.fit_requested;
			if (const auto canvas = m_tree_document_window_states.find(
				m_current_archive_member_path);
				canvas != m_tree_document_window_states.end() && canvas->second)
			{
				canvas->second->canvas.zoom = view.zoom;
				canvas->second->canvas.pan = {
					view.pan_x,
					view.pan_y
				};
				canvas->second->canvas.view_initialized = view.initialized;
				canvas->second->canvas.fit_requested = view.fit_requested;
			}
		}

		void request_fit_active_tree_document() noexcept
		{
			m_tree_canvas_fit_requested = true;
			if (const auto canvas = m_tree_document_window_states.find(
				m_current_archive_member_path);
				canvas != m_tree_document_window_states.end() && canvas->second)
			{
				canvas->second->canvas.fit_requested = true;
			}
		}

		[[nodiscard]] bool tree_document_is_dirty(
			std::string_view archive_member_path) const
		{
			const auto found = m_tree_documents.find(
				std::string(archive_member_path));
			return found != m_tree_documents.end() &&
				found->second.is_dirty();
		}

		[[nodiscard]] bool any_tree_document_is_dirty() const
		{
			return std::any_of(
				m_tree_documents.begin(),
				m_tree_documents.end(),
				[](const auto& item)
				{
					return item.second.is_dirty();
				});
		}

		[[nodiscard]] static std::filesystem::path
			normalized_local_tree_document_path(
			const std::filesystem::path& path)
		{
			std::error_code error;
			auto absolute = std::filesystem::absolute(path, error);
			if (error)
			{
				absolute = path;
			}
			return absolute.lexically_normal();
		}

		[[nodiscard]] static bool same_local_tree_document_path(
			const std::filesystem::path& left,
			const std::filesystem::path& right)
		{
			const auto normalized_left =
				normalized_local_tree_document_path(left);
			const auto normalized_right =
				normalized_local_tree_document_path(right);
			if (normalized_left == normalized_right)
			{
				return true;
			}

			std::error_code error;
			const bool equivalent = std::filesystem::equivalent(
				normalized_left,
				normalized_right,
				error);
			if (!error)
			{
				return equivalent;
			}

#if defined(_WIN32)
			const auto& left_native = normalized_left.native();
			const auto& right_native = normalized_right.native();
			if (!left_native.empty() && !right_native.empty() &&
				left_native.size() <=
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

		[[nodiscard]] std::string find_tree_document_for_local_path(
			const std::filesystem::path& path) const
		{
			for (const auto& [archive_path, document] : m_tree_documents)
			{
				if (!document.local_path.empty() &&
					same_local_tree_document_path(document.local_path, path))
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
			auto& document = m_tree_documents.at(path);
			document.saved_state_id =
				std::numeric_limits<std::uint64_t>::max();
			update_session_metadata();
			return path;
		}

		void create_new_tree()
		{
			sync_comparison_pane_with_active_tree(true);
			const auto* type = available_tree_type_for(m_new_tree_id);
			if (!type)
			{
				throw std::logic_error(
					"Нельзя создать дерево неизвестного типа.");
			}
			const auto* algorithm =
				tree_algorithm_for(m_new_tree_id);
			if (!algorithm)
			{
				throw std::logic_error(
					"Для выбранного типа дерева не найден алгоритм.");
			}

			auto empty_tree = algorithm->create_empty(
				default_tree_key_kind(type->algorithm));
			const std::string serialization =
				algorithm->serialize(empty_tree);
			remember_active_tree_document();
			auto previous_state = capture_tree_history_state();
			const auto archive_member_path = add_tree_to_session(
				type->algorithm.file_stem,
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
			m_tree_tab_requested_path = archive_member_path;
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
			m_tree_document_window_states.clear();
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
			m_tree_tab_requested_path.clear();
			m_merge_source_archive_path.clear();
			m_split_result_member_paths = {};
			m_split_result_key_kind.reset();
			m_split_pivot_label.clear();
			m_current_tree_name.clear();
			m_current_tree_serialization.clear();
			m_loaded_visual_tree.reset();
			m_loaded_search_tree_key_kind.reset();
			m_loaded_search_tree_valid = false;
			m_loaded_tree_id.clear();
			m_tree_canvas_tree_id.clear();
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
				remember_active_tree_document();
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
			m_tree_document_window_states.clear();
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
			m_loaded_tree_id.clear();
			m_tree_parse_error.clear();
			m_tree_canvas_tree_id.clear();
			m_tree_canvas_view_initialized = false;
			m_tree_canvas_fit_requested = true;
			try
			{
				const auto* type = available_tree_type_for_serialization(
					m_current_tree_serialization);
				if (!type)
				{
					throw tree_serialization_error(
						"неизвестный тип дерева");
				}
				const auto* algorithm =
					tree_algorithm_for(type->algorithm.id);
				if (!algorithm)
				{
					throw tree_serialization_error(
						"для этого типа дерева не загружен алгоритм");
				}
				auto document = algorithm->deserialize(
					m_current_tree_serialization);
				if (document.algorithm_id != type->algorithm.id)
				{
					throw tree_serialization_error(
						"Алгоритм вернул документ с другим ID дерева.");
				}
				const auto inferred_key_kind =
					infer_search_tree_key_kind(document.value);
				const auto key_kind = document.keys.value_or(
					inferred_key_kind.value_or(
						default_tree_key_kind(type->algorithm)));
				m_active_tree_id = document.algorithm_id;
				m_loaded_tree_id = m_active_tree_id;
				m_loaded_search_tree_key_kind = key_kind;
				try
				{
					algorithm->validate(document.value, key_kind);
					m_loaded_search_tree_valid = true;
				}
				catch (const std::exception& error)
				{
					m_tree_operation_error = error.what();
				}
				m_loaded_visual_tree = std::move(document.value);
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

		[[nodiscard]] bool has_loaded_supported_tree() const
		{
			const auto* type = available_tree_type_for(m_active_tree_id);
			if (!type || !tree_algorithm_for(m_active_tree_id) ||
				m_current_tree_name.empty() ||
				!m_loaded_visual_tree.has_value() ||
				!m_loaded_search_tree_valid ||
				m_loaded_tree_id != m_active_tree_id)
			{
				return false;
			}

			if (m_loaded_search_tree_key_kind)
			{
				return tree_practicum::mods::supports(
					type->algorithm.supported_key_kinds,
					*m_loaded_search_tree_key_kind);
			}

			return type->algorithm.supported_key_kinds != 0;
		}

		[[nodiscard]] bool supports_current_tree_operation(
			tree_practicum::mods::operation operation) const
		{
			const auto* type = available_tree_type_for(m_active_tree_id);
			return has_loaded_supported_tree() && type &&
				tree_practicum::mods::supports(
					type->algorithm.supported_operations,
					operation);
		}

		[[nodiscard]] bool supports_tool_operation(
			tree_tool_operation operation) const
		{
			return supports_current_tree_operation(
				tree_operation_capability(operation));
		}

		[[nodiscard]] bool can_insert_into_selected_search_tree() const
		{
			if (!has_loaded_supported_tree())
			{
				return false;
			}
			const auto* type = available_tree_type_for(m_active_tree_id);
			return type && type->algorithm.supported_operations != 0;
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
			return supports_current_tree_operation(
				tree_practicum::mods::operation::merge) &&
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
				pane.algorithm_id.clear();
				pane.canvas.context_menu_key.reset();
				pane.canvas.context_menu_node.reset();
				pane.canvas.algorithm_id.clear();
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
					pane.algorithm_id = m_active_tree_id;
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
			pane.algorithm_id = m_active_tree_id;
			pane.canvas.algorithm_id.clear();
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

			auto primary_document = deserialize_tree_document(primary->contents);
			auto secondary_document = deserialize_tree_document(secondary->contents);

			const std::string primary_path = primary->path;
			const std::string secondary_path = secondary->path;
			if (m_current_archive_member_path != primary_path)
			{
				open_archive_tree(*primary);
			}

			reset_tree_comparison();
			auto& primary_pane = m_tree_comparison_panes[0];
			primary_pane.archive_member_path = primary_path;
			primary_pane.tree = std::move(primary_document.value);
			primary_pane.algorithm_id = std::move(primary_document.algorithm_id);
			primary_pane.canvas.fit_requested = true;

			auto& secondary_pane = m_tree_comparison_panes[1];
			secondary_pane.archive_member_path = secondary_path;
			secondary_pane.tree = std::move(secondary_document.value);
			secondary_pane.algorithm_id = std::move(secondary_document.algorithm_id);
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
			std::optional<tree_practicum::mods::document> parsed_document;
			if (tree_changed)
			{
				parsed_document = deserialize_tree_document(entry->contents);
			}

			if (m_current_archive_member_path != selected_path)
			{
				open_archive_tree(*entry);
			}

			if (tree_changed)
			{
				pane.archive_member_path = selected_path;
				pane.algorithm_id = parsed_document->algorithm_id;
				pane.tree = std::move(parsed_document->value);
				pane.canvas.algorithm_id.clear();
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
							pane.algorithm_id,
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
			m_loaded_tree_id = m_active_tree_id;
			record_tree_history_change(std::move(previous_state));
		}

		void perform_search_tree_find()
		{
			clear_tree_find_result();
			if (!supports_tool_operation(tree_tool_operation::find))
			{
				m_last_action =
					"Поиск доступен для загруженного дерева с поддерживаемым типом ключей.";
				return;
			}

			try
			{
				commit_pending_input_edits();
				cancel_tree_animation();
				m_tree_operation_error.clear();
				auto key = parse_search_tree_key_input(
					std::string_view(m_key_input.data()),
					m_loaded_search_tree_key_kind);
				const auto key_kind =
					m_loaded_search_tree_key_kind.value_or(
						search_tree_key_kind_for_value(key.value));
				const std::string key_text = full_visual_value(key.value);
				const auto* algorithm =
					tree_algorithm_for(m_active_tree_id);
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
				m_tree_find_tree_id = m_active_tree_id;
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
					m_tree_find_status += std::format(
						" {} изменил структуру дерева.",
						algorithm->name());
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
			if (!supports_tool_operation(tree_tool_operation::insert))
			{
				m_last_action =
					"Операция недоступна: выберите корректное дерево с поддерживаемым типом ключей.";
				return;
			}

			try
			{
				commit_pending_input_edits();
				cancel_tree_animation();
				m_tree_operation_error.clear();
				auto key = parse_search_tree_key_input(
					std::string_view(m_key_input.data()),
					m_loaded_search_tree_key_kind);
				const auto effective_key_kind =
					m_loaded_search_tree_key_kind.value_or(
						search_tree_key_kind_for_value(key.value));
				const auto inserted_value = full_visual_value(key.value);
				const auto* algorithm =
					tree_algorithm_for(m_active_tree_id);
				if (!algorithm)
				{
					throw std::logic_error(
						"Для выбранного дерева нет алгоритма вставки.");
				}
				auto previous_state = capture_tree_history_state();
				auto updated_tree = *m_loaded_visual_tree;
				const bool tree_is_small_enough_to_animate =
					m_tree_animations_enabled &&
					updated_tree.nodes.size() <
						maximum_animated_tree_nodes;
				const bool collect_animation_events =
					tree_is_small_enough_to_animate &&
					algorithm->supports_animation();
				const auto* algorithm_type =
					available_tree_type_for(m_active_tree_id);
				const bool supports_layout_transition = algorithm_type &&
					tree_practicum::mods::supports(
						algorithm_type->algorithm.animations
							.layout_transition_operations,
						tree_practicum::mods::operation::insert);
				std::optional<visual_tree> animation_start;
				if (tree_is_small_enough_to_animate)
				{
					animation_start = updated_tree;
				}
				const visual_key animation_key = key;
				const auto active_document_window =
					m_tree_document_window_states.find(
						m_current_archive_member_path);
				const auto pre_operation_geometry =
					active_document_window !=
						m_tree_document_window_states.end() &&
					active_document_window->second
					? active_document_window->second->preview.geometry
					: m_tree_preview_caches.front().geometry;
				const bool has_pre_operation_layout =
					updated_tree.nodes.empty() ||
					(tree_is_small_enough_to_animate &&
						pre_operation_geometry &&
						pre_operation_geometry->revision ==
							m_tree_preview_revision &&
						pre_operation_geometry->source_tree_signature ==
							tree_animation_tree_signature(updated_tree) &&
						pre_operation_geometry->algorithm_id ==
							m_active_tree_id &&
						pre_operation_geometry->current_tree_loaded &&
						!pre_operation_geometry->showing_animation &&
						pre_operation_geometry->layout.nodes.size() ==
							updated_tree.nodes.size());
				auto operation = algorithm->insert(
					updated_tree,
					std::move(key),
					effective_key_kind,
					collect_animation_events);
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
					supports_layout_transition &&
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
				m_loaded_tree_id = m_active_tree_id;
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
								"Animation event queue size does not match the tree operation.");
						}
						tree_animation_state animation;
						animation.tree = std::move(animation_start.value());
						animation.events = std::move(received_events);
						animation.algorithm_id = m_active_tree_id;
						m_tree_animation = std::move(animation);
					}
					catch (const std::exception& error)
					{
						cancel_tree_animation();
						m_tree_operation_error = std::format(
							"Анимацию дерева не удалось запустить: {}",
							error.what());
						logger.error(
							"Unable to start tree animation: ",
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
					animation.algorithm_id = m_active_tree_id;
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
						"Ключ «{}» уже был в дереве; структура дерева изменилась. Сохраните изменения через Ctrl+S.",
						inserted_value);
					logger.info(
						"Duplicate key changed the tree structure: ",
						inserted_value);
				}
				if (!m_tree_animations_enabled)
				{
					m_last_action += " Анимации отключены в настройках.";
				}
				else if (!tree_is_small_enough_to_animate)
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
			if (!supports_tool_operation(tree_tool_operation::random_insert))
			{
				m_last_action =
					"Операция недоступна: выберите корректное дерево с поддерживаемым типом ключей.";
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
						parse_search_tree_key_input(
							std::string_view(m_key_input.data()),
							std::nullopt);
					key_kind = search_tree_key_kind_for_value(
						type_probe.value);
				}
				const auto* algorithm =
					tree_algorithm_for(m_active_tree_id);
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
				m_loaded_tree_id = m_active_tree_id;
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
			if (!supports_tool_operation(tree_tool_operation::erase))
			{
				m_last_action =
					"Удаление недоступно: выберите корректное дерево с поддерживаемым типом ключей.";
				return;
			}

			try
			{
				commit_pending_input_edits();
				cancel_tree_animation();
				m_tree_operation_error.clear();
				auto key = hovered_key
					? std::move(*hovered_key)
					: parse_search_tree_key_input(
						std::string_view(m_key_input.data()),
						m_loaded_search_tree_key_kind);
				const auto key_kind = m_loaded_search_tree_key_kind.value_or(
					search_tree_key_kind_for_value(key.value));
				const std::string key_text = full_visual_value(key.value);
				const std::string tree_document_label =
					m_current_archive_member_path.empty()
					? m_current_tree_name
					: m_current_archive_member_path;
				const auto* algorithm =
					tree_algorithm_for(m_active_tree_id);
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
						"Ключ «{}» не найден в {} ({}).",
						key_text,
						algorithm->name(),
						tree_document_label);
					logger.warning(
						algorithm->name(),
						" erase skipped because the key was not found in tree '",
						tree_document_label,
						"': ",
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
						"Ключ «{}» не удалён; структура дерева изменилась. Сохраните изменения через Ctrl+S.",
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
						algorithm->name(),
						" changed tree structure after an unsuccessful erase: ",
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
					"Слияние недоступно: выберите поддерживаемое дерево и отдельную запись .tree-источника.";
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

				auto source_document = deserialize_tree_document(
					source_entry->contents);
				if (source_document.algorithm_id != m_active_tree_id)
				{
					throw std::invalid_argument(
						"Для merge выберите дерево того же типа, что и текущее.");
				}

				auto& source_tree = source_document.value;
				const auto* algorithm =
					tree_algorithm_for(m_active_tree_id);
				const auto* type =
					available_tree_type_for(m_active_tree_id);
				if (!algorithm || !type)
				{
					throw std::logic_error(
						"Для выбранного дерева нет алгоритма merge.");
				}
				auto source_key_kind = source_document.keys;
				if (!source_key_kind)
				{
					source_key_kind = infer_search_tree_key_kind(source_tree);
				}
				const auto key_kind = m_loaded_search_tree_key_kind.value_or(
					source_key_kind.value_or(
						default_tree_key_kind(type->algorithm)));
				if (source_key_kind && *source_key_kind != key_kind)
				{
					throw std::invalid_argument(
						"Тип ключей дерева-источника должен совпадать с текущим деревом.");
				}
				if (!tree_practicum::mods::supports(
					type->algorithm.supported_key_kinds,
					key_kind))
				{
					throw std::invalid_argument(
						"Тип ключей дерева-источника не поддерживается выбранным алгоритмом.");
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
			if (!supports_tool_operation(tree_tool_operation::split))
			{
				m_last_action =
					"Split недоступен: выберите корректное дерево с поддерживаемым типом ключей.";
				return;
			}

			try
			{
				commit_pending_input_edits();
				cancel_tree_animation();
				m_tree_operation_error.clear();
				auto pivot = hovered_key
					? std::move(*hovered_key)
					: parse_search_tree_key_input(
						std::string_view(m_key_input.data()),
						m_loaded_search_tree_key_kind);
				const auto key_kind = m_loaded_search_tree_key_kind.value_or(
					search_tree_key_kind_for_value(pivot.value));
				const std::string pivot_text = full_visual_value(pivot.value);
				const auto* algorithm =
					tree_algorithm_for(m_active_tree_id);
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

		[[nodiscard]] bool activate_tree_document_for_operation(
			std::string_view archive_path)
		{
			if (archive_path.empty() ||
				archive_path == m_current_archive_member_path)
			{
				return true;
			}

			const auto* entry = find_archive_entry(
				m_session_entries,
				archive_path);
			if (!entry)
			{
				m_file_error =
					"Документ дерева больше не доступен в рабочей области.";
				m_last_action = m_file_error;
				return false;
			}

			const tree_practicum::archive_entry entry_copy = *entry;
			return perform_io([&]
			{
				open_archive_tree(entry_copy);
			});
		}

		void invoke_tree_tool(
			tree_tool_operation operation,
			std::optional<visual_key> selected_key = std::nullopt,
			std::optional<std::size_t> comparison_pane = std::nullopt,
			std::string_view source_archive_path = {})
		{
			if (comparison_pane &&
				m_tree_comparison_mode &&
				!perform_io([&]
				{
					focus_comparison_pane(*comparison_pane);
				}))
			{
				return;
			}
			if (!comparison_pane &&
				!activate_tree_document_for_operation(source_archive_path))
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
				m_active_tree_id = current_tree.active_tree_id;
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
			auto& document = m_tree_documents.at(entry.path);
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
			restore_active_tree_document_view();
			remember_active_tree_document();
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
			m_loaded_tree_id.clear();
			m_tree_operation_error.clear();
			m_tree_parse_error.clear();
			m_tree_canvas_tree_id.clear();
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
			std::optional<tree_document_view_state> original_view;
			if (const auto original = m_tree_documents.find(
				original_archive_path);
				original != m_tree_documents.end())
			{
				original_view = original->second.view;
			}
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
				if (original_view)
				{
					m_tree_documents.at(
						m_current_archive_member_path).view = *original_view;
				}
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
			remember_active_tree_document();
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

			ImGui::TextDisabled("ПАПКА НА ДИСКЕ");
			const auto explorer_directory_name =
				path_to_utf8(m_explorer_directory.filename());
			const auto explorer_directory_label = explorer_directory_name.empty()
				? path_to_utf8(m_explorer_directory)
				: explorer_directory_name;
			ImGui::Text("%s", explorer_directory_label.c_str());
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

			const bool show_local_files = ImGui::CollapsingHeader(
				"Файлы на диске",
				ImGuiTreeNodeFlags_DefaultOpen);
			const float local_files_height = std::clamp(
				ImGui::GetContentRegionAvail().y * 0.42f,
				110.0f,
				260.0f);
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
			if (show_local_files && ImGui::BeginChild(
				"ExplorerDiskFiles",
				{ 0.0f, local_files_height },
				ImGuiChildFlags_Borders))
			{
				auto render_directory = [&](auto&& self,
					const std::filesystem::path& directory,
					const std::vector<explorer_entry>* cached_entries) -> void
				{
					std::vector<explorer_entry> discovered_entries;
					std::string read_error;
					if (cached_entries)
					{
						discovered_entries = *cached_entries;
					}
					else
					{
						std::error_code error;
						std::filesystem::directory_iterator iterator(directory, error);
						if (error)
						{
							read_error = error.message();
						}
						else
						{
							const std::filesystem::directory_iterator end;
							while (iterator != end)
							{
								const auto& item = *iterator;
								std::error_code status_error;
								const bool is_directory = item.is_directory(status_error);
								if (status_error)
								{
									read_error = status_error.message();
								}
								else if (is_directory ||
									file_extension(item.path()) == ".tree" ||
									file_extension(item.path()) == ".stree" ||
									file_extension(item.path()) == ".tar")
								{
									discovered_entries.push_back({ item.path(), is_directory });
								}
								iterator.increment(error);
								if (error)
								{
									read_error = error.message();
									break;
								}
							}
						}
					}

					std::sort(
						discovered_entries.begin(),
						discovered_entries.end(),
						[](const explorer_entry& left, const explorer_entry& right)
						{
							if (left.directory != right.directory)
							{
								return left.directory;
							}
							return lowercase_ascii(path_to_utf8(left.path.filename())) <
								lowercase_ascii(path_to_utf8(right.path.filename()));
						});

					if (discovered_entries.empty())
					{
						ImGui::TextDisabled(
							read_error.empty()
								? (query.empty()
									? "Здесь пока нет файлов дерева."
									: "Совпадений нет.")
								: "Не удалось прочитать папку.");
						return;
					}

					bool drew_entry = false;
					for (const auto& entry : discovered_entries)
					{
						const auto file_name = path_to_utf8(entry.path.filename());
						if (!entry.directory && !contains_text(file_name, query))
						{
							continue;
						}
						drew_entry = true;
						const auto entry_path = entry.path;
						const bool selected = m_selected_local_path &&
							*m_selected_local_path == entry_path;
						const auto path_id = path_to_utf8(entry_path);
						ImGui::PushID(path_id.c_str());
						bool clicked = false;
						bool opened = false;
						if (entry.directory)
						{
							if (selected)
							{
								ImGui::PushStyleColor(
									ImGuiCol_Header,
									ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
								ImGui::PushStyleColor(
									ImGuiCol_Text,
									{ 0.55f, 0.76f, 0.98f, 1.0f });
							}
							opened = ImGui::TreeNodeEx(
								"##ExplorerFolder",
								ImGuiTreeNodeFlags_OpenOnArrow |
									ImGuiTreeNodeFlags_OpenOnDoubleClick |
									ImGuiTreeNodeFlags_SpanAvailWidth |
									ImGuiTreeNodeFlags_FramePadding,
								"%s",
								file_name.c_str());
							clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
							if (selected)
							{
								ImGui::PopStyleColor(2);
							}
						}
						else
						{
							const auto extension = file_extension(entry_path);
							const ImVec4 marker_color = extension == ".tree"
								? ImVec4{ 0.43f, 0.72f, 0.98f, 1.0f }
								: extension == ".stree"
									? ImVec4{ 0.75f, 0.60f, 0.96f, 1.0f }
									: ImVec4{ 0.86f, 0.67f, 0.38f, 1.0f };
							ImGui::TextColored(marker_color, "●");
							ImGui::SameLine(0.0f, 5.0f);
							clicked = ImGui::Selectable(file_name.c_str(), selected);
						}
						if (clicked ||
							ImGui::IsItemClicked(ImGuiMouseButton_Right))
						{
							m_selected_local_path = entry_path;
							m_selected_archive_entry = -1;
						}
						if (ImGui::BeginPopupContextItem("ExplorerDiskFileContext"))
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
						if (opened)
						{
							self(self, entry_path, nullptr);
							ImGui::TreePop();
						}
						ImGui::PopID();

						if (double_clicked)
						{
							activated_entry = explorer_entry{
								entry_path,
								entry.directory
							};
							return;
						}
						if (activated_entry)
						{
							return;
						}
					}
					if (!drew_entry)
					{
						ImGui::TextDisabled("Совпадений нет.");
					}
				};
				render_directory(render_directory, m_explorer_directory, &m_explorer_entries);
			}
			if (show_local_files)
			{
				ImGui::EndChild();
			}
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
				std::error_code selected_path_error;
				const bool selected_is_directory =
					std::filesystem::is_directory(selected_path, selected_path_error);
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
			const bool show_archive_files = ImGui::CollapsingHeader(
				"Состав рабочей области",
				ImGuiTreeNodeFlags_DefaultOpen);
			std::optional<std::size_t> activated_archive_entry;
			if (show_archive_files && ImGui::BeginChild(
				"ExplorerArchiveFiles",
				{ 0.0f, 0.0f },
				ImGuiChildFlags_Borders))
			{
				struct archive_explorer_node
				{
					std::string name;
					std::string path;
					std::vector<archive_explorer_node> children;
					std::optional<std::size_t> entry_index;
				};
				std::vector<archive_explorer_node> roots;
				for (std::size_t i = 0; i < m_session_entries.size(); ++i)
				{
					const auto& path = m_session_entries[i].path;
					if (!contains_text(path, query))
					{
						continue;
					}

					auto* siblings = &roots;
					std::size_t component_begin = 0;
					while (component_begin < path.size())
					{
						const auto slash = path.find('/', component_begin);
						const auto component_end = slash == std::string::npos
							? path.size()
							: slash;
						const auto component = path.substr(
							component_begin,
							component_end - component_begin);
						if (component.empty())
						{
							component_begin = component_end + 1;
							continue;
						}

						auto child = std::find_if(
							siblings->begin(),
							siblings->end(),
							[&](const archive_explorer_node& node)
							{
								return node.name == component;
							});
						if (child == siblings->end())
						{
							siblings->push_back({ component, {}, {}, std::nullopt });
							child = std::prev(siblings->end());
						}
						if (child->path.empty())
						{
							child->path = component_begin == 0
								? component
								: path.substr(0, component_end);
						}

						const bool is_last = slash == std::string::npos;
						if (is_last)
						{
							child->entry_index = i;
						}
						else
						{
							siblings = &child->children;
						}
						component_begin = component_end + 1;
					}
				}

				auto sort_nodes = [&](auto&& self,
					std::vector<archive_explorer_node>& nodes) -> void
				{
					std::sort(
						nodes.begin(),
						nodes.end(),
						[](const archive_explorer_node& left,
							const archive_explorer_node& right)
						{
							const bool left_directory = !left.entry_index.has_value();
							const bool right_directory = !right.entry_index.has_value();
							if (left_directory != right_directory)
							{
								return left_directory;
							}
							return lowercase_ascii(left.name) < lowercase_ascii(right.name);
						});
					for (auto& node : nodes)
					{
						self(self, node.children);
					}
				};
				sort_nodes(sort_nodes, roots);

				if (m_session_entries.empty())
				{
					ImGui::TextDisabled("В архиве пока нет файлов.");
				}
				else if (roots.empty())
				{
					ImGui::TextDisabled("Совпадений нет.");
				}
				else
				{
					auto render_archive_nodes = [&](auto&& self,
						std::vector<archive_explorer_node>& nodes) -> void
					{
						for (auto& node : nodes)
						{
							if (!node.entry_index)
							{
								ImGui::PushID(node.path.c_str());
								const bool opened = ImGui::TreeNodeEx(
									"##ExplorerArchiveFolder",
									ImGuiTreeNodeFlags_OpenOnArrow |
										ImGuiTreeNodeFlags_OpenOnDoubleClick |
										ImGuiTreeNodeFlags_SpanAvailWidth |
										ImGuiTreeNodeFlags_FramePadding,
									"%s",
									node.name.c_str());
								if (opened)
								{
									self(self, node.children);
									ImGui::TreePop();
								}
								ImGui::PopID();
								continue;
							}

							const auto index = *node.entry_index;
							const auto& entry = m_session_entries[index];
							const auto extension = archive_extension(entry.path);
							const ImVec4 marker_color = extension == ".tree"
								? ImVec4{ 0.43f, 0.72f, 0.98f, 1.0f }
								: extension == ".htree"
									? ImVec4{ 0.75f, 0.60f, 0.96f, 1.0f }
									: extension == ".session"
										? ImVec4{ 0.86f, 0.67f, 0.38f, 1.0f }
										: ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
							ImGui::PushID(node.path.c_str());
							ImGui::TextColored(marker_color, "●");
							ImGui::SameLine(0.0f, 5.0f);
							const bool clicked = ImGui::Selectable(
								node.name.c_str(),
								m_selected_archive_entry == static_cast<int>(index));
							if (clicked ||
								ImGui::IsItemClicked(ImGuiMouseButton_Right))
							{
								m_selected_archive_entry = static_cast<int>(index);
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
							if (clicked &&
								ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							{
								activated_archive_entry = index;
							}
							ImGui::PopID();
					}
					};
					render_archive_nodes(render_archive_nodes, roots);
			}
			}
			if (show_archive_files)
			{
				ImGui::EndChild();
			}
			if (activated_archive_entry)
			{
				(void)perform_io([&]
				{
					open_archive_entry(*activated_archive_entry);
				});
			}
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
			const float max_panel_width =
				std::max(1.0f, viewport_size.x - 32.0f);
			const float min_panel_width =
				std::min(240.0f, max_panel_width);
			const float panel_width = std::min(
				std::clamp(
					viewport_size.x * 0.34f,
					min_panel_width,
					std::min(520.0f, max_panel_width)),
				max_panel_width);
			const float max_panel_height =
				std::max(1.0f, viewport_size.y - 32.0f);
			const float panel_height = std::min(
				max_panel_height,
				std::min(
					440.0f,
					std::max(180.0f, viewport_size.y - 32.0f)));
			const float min_panel_x = viewport_position.x + 16.0f;
			const float min_panel_y = viewport_position.y + 16.0f;
			const float max_panel_x = std::max(
				min_panel_x,
				viewport_position.x + viewport_size.x - panel_width - 16.0f);
			const float max_panel_y = std::max(
				min_panel_y,
				viewport_position.y + viewport_size.y - panel_height - 16.0f);
			const float functions_width =
				m_show_functions ? viewport_size.x * 0.25f : 0.0f;
			const float workspace_right =
				viewport_position.x + viewport_size.x - functions_width;
			const float preferred_panel_x =
				workspace_right - panel_width - 16.0f;
			const float preferred_panel_y = viewport_position.y + 16.0f;
			const ImVec2 panel_position{
				std::clamp(preferred_panel_x, min_panel_x, max_panel_x),
				std::clamp(preferred_panel_y, min_panel_y, max_panel_y)
			};
			set_initial_window_rect(
				panel_position,
				{ panel_width, panel_height });

			auto panel = frame.window(
				"Операции с деревом###TreePracticum.Operations.v2");
			if (!panel)
			{
				commit_pending_input_edits();
				return;
			}
			ImGui::Text("Рабочая область: %s", m_session_name.c_str());
			ImGui::TextDisabled(
				(m_tree_dirty || any_tree_document_is_dirty())
					? "Есть несохранённые изменения · Ctrl+S"
					: m_loaded_visual_tree.has_value() &&
						m_loaded_tree_id == m_active_tree_id
					? can_insert_into_selected_search_tree()
						? "Для загруженного дерева доступны операции алгоритма"
					: "У алгоритма нет доступных операций для этого дерева"
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
				request_new_tree_dialog();
			}
			ImGui::SameLine();
			if (ImGui::Button("Открыть .tree..."))
			{
				request_tree_file_picker();
			}
			ImGui::SameLine();
			ImGui::TextDisabled("или перетащите файл в это окно");

			ImGui::NewLine();
			const std::string current_file_label =
				m_current_tree_path.empty()
				? m_current_tree_name
				: path_to_utf8(m_current_tree_path);
			ImGui::TextDisabled(
				"Файл: %s",
				current_file_label.c_str());
			ImGui::NewLine();

			ImGui::PushStyleColor(
				ImGuiCol_Button,
				{ 0.10f, 0.29f, 0.39f, 1.0f });
			ImGui::PushStyleColor(
				ImGuiCol_ButtonHovered,
				{ 0.13f, 0.40f, 0.53f, 1.0f });
			ImGui::PushStyleColor(
				ImGuiCol_ButtonActive,
				{ 0.08f, 0.23f, 0.32f, 1.0f });
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
				ImGui::BeginDisabled(!supports_tool_operation(
					tree_tool_operation::find));
				if (ImGui::Button("Найти [F]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::find);
				}
				ImGui::EndDisabled();
				ImGui::TableNextColumn();
				ImGui::BeginDisabled(!supports_tool_operation(
					tree_tool_operation::insert));
				if (ImGui::Button("Вставить [A / Ins]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::insert);
				}
				ImGui::EndDisabled();
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::BeginDisabled(!supports_tool_operation(
					tree_tool_operation::erase));
				if (ImGui::Button("Удалить [D / Del]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::erase);
				}
				ImGui::EndDisabled();
				ImGui::TableNextColumn();
				ImGui::BeginDisabled(!supports_tool_operation(
					tree_tool_operation::split));
				if (ImGui::Button("Split [C]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::split);
				}
				ImGui::EndDisabled();
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::BeginDisabled(!supports_tool_operation(
					tree_tool_operation::random_insert));
				if (ImGui::Button("Случайная вставка [R]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::random_insert);
				}
				ImGui::EndDisabled();
				ImGui::TableNextColumn();
				ImGui::BeginDisabled(!supports_tool_operation(
					tree_tool_operation::merge));
				if (ImGui::Button("Объединить [M]", operation_button_size))
				{
					invoke_tree_tool(tree_tool_operation::merge);
				}
				ImGui::EndDisabled();
				ImGui::EndTable();
			}
			ImGui::PopStyleColor(3);
			ImGui::TextDisabled(
				"Параметры операций открываются в отдельном окне.");

			if (m_tree_find_result_valid &&
				m_tree_find_tree_id == m_active_tree_id)
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
				request_fit_active_tree_document();
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
				supports_tool_operation(operation);
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
					"ключ подходящего типа (строку можно заключить в кавычки)",
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
					"Эта операция недоступна для выбранного алгоритма или типа ключа.");
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
			if (m_open_tree_tabs.empty())
			{
				draw_empty_tree_workspace(frame, position, size);
				return;
			}

			std::vector<std::string> closed_documents;
			const auto open_paths = m_open_tree_tabs;
			for (std::size_t index = 0; index < open_paths.size(); ++index)
			{
				const auto& archive_path = open_paths[index];
				const auto* entry = find_archive_entry(
					m_session_entries,
					archive_path);
				if (!entry || archive_extension(entry->path) != ".tree")
				{
					closed_documents.push_back(archive_path);
					continue;
				}

				auto& document_state = tree_document_window_state_for(*entry);
				const auto document = m_tree_documents.find(archive_path);
				const auto separator = archive_path.find_last_of('/');
				std::string display_name = document != m_tree_documents.end() &&
					!document->second.display_name.empty()
					? document->second.display_name
					: document != m_tree_documents.end() &&
						!document->second.local_path.empty()
						? path_to_utf8(document->second.local_path.filename())
						: archive_path.substr(
							separator == std::string::npos ? 0 : separator + 1);
				const bool dirty = tree_document_is_dirty(archive_path);
				const std::string title = std::format(
					"{}{}###TreeDocument.{}",
					display_name,
					dirty ? " *" : "",
					archive_path);
				const float cascade = static_cast<float>(index % 8) * 24.0f;
				set_initial_window_rect(
					{ position.x + cascade, position.y + cascade },
					{
						std::max(size.x * 0.72f, 420.0f),
						std::max(size.y * 0.72f, 320.0f)
					});
				const bool focus_requested =
					archive_path == m_tree_tab_requested_path;
				if (focus_requested)
				{
					ImGui::SetNextWindowFocus();
				}
				bool window_open = true;
				auto window = frame.window(title, window_open);
				if (focus_requested)
				{
					m_tree_tab_requested_path.clear();
				}
				if (window_open &&
					ImGui::IsWindowFocused(
						ImGuiFocusedFlags_RootAndChildWindows) &&
					document_state.parse_error.empty() &&
					m_current_archive_member_path != archive_path)
				{
					const tree_practicum::archive_entry entry_copy = *entry;
					if (perform_io([&]
					{
						open_archive_tree(entry_copy);
					}))
					{
						m_selected_archive_entry = -1;
					}
				}
				if (window)
				{
					if (!document_state.parse_error.empty())
					{
						ImGui::TextWrapped(
							"Не удалось открыть дерево: %s",
							document_state.parse_error.c_str());
					}
					else
					{
						const visual_tree* const tree_to_draw =
							archive_path == m_current_archive_member_path &&
							m_loaded_visual_tree
							? std::addressof(*m_loaded_visual_tree)
							: std::addressof(document_state.tree);
						draw_tree_preview(
							document_state.algorithm_id,
							&document_state.canvas,
							tree_to_draw,
							archive_path,
							0,
							"TreeDocumentCanvas",
							nullptr,
							&document_state.preview);
					}
				}
				if (!window_open)
				{
					closed_documents.push_back(archive_path);
				}
				sync_tree_document_view(archive_path, document_state);
			}

			if (!closed_documents.empty())
			{
				const bool active_document_closed = std::find(
					closed_documents.begin(),
					closed_documents.end(),
					m_current_archive_member_path) != closed_documents.end();
				std::erase_if(
					m_open_tree_tabs,
					[&](const auto& path)
					{
						return std::find(
							closed_documents.begin(),
							closed_documents.end(),
							path) != closed_documents.end();
					});
				if (std::find(
					closed_documents.begin(),
					closed_documents.end(),
					m_tree_tab_requested_path) != closed_documents.end())
				{
					m_tree_tab_requested_path.clear();
				}
				m_last_action =
					"Окно дерева закрыто. Файл остаётся в рабочей области.";
				if (active_document_closed)
				{
					if (!m_open_tree_tabs.empty())
					{
						if (const auto* next = find_archive_entry(
							m_session_entries,
							m_open_tree_tabs.back()))
						{
							const tree_practicum::archive_entry entry_copy = *next;
							(void)perform_io([&]
							{
								open_archive_tree(entry_copy);
							});
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
			}
		}

		void draw_empty_tree_workspace(
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
				draw_tree_preview(m_active_tree_id);
			}
			remember_active_tree_document_view();
		}

		tree_document_window_state& tree_document_window_state_for(
			const tree_practicum::archive_entry& entry)
		{
			auto& state = m_tree_document_window_states[entry.path];
			if (!state)
			{
				state = std::make_unique<tree_document_window_state>();
				if (const auto document = m_tree_documents.find(entry.path);
					document != m_tree_documents.end())
				{
					state->canvas.zoom = document->second.view.zoom;
					state->canvas.pan = {
						document->second.view.pan_x,
						document->second.view.pan_y
					};
					state->canvas.view_initialized =
						document->second.view.initialized;
					state->canvas.fit_requested =
						document->second.view.fit_requested;
				}
			}
			if (state->source_contents != entry.contents)
			{
				const std::string previous_algorithm_id = state->algorithm_id;
				state->source_contents = entry.contents;
				state->parse_error.clear();
				try
				{
					auto document = deserialize_tree_document(entry.contents);
					state->tree = std::move(document.value);
					state->algorithm_id = std::move(document.algorithm_id);
					state->parsed = true;
				}
				catch (const std::exception& error)
				{
					state->tree = {};
					state->algorithm_id.clear();
					state->parse_error = error.what();
					state->parsed = false;
				}
				if (previous_algorithm_id != state->algorithm_id)
				{
					state->canvas.view_initialized = false;
					state->canvas.fit_requested = true;
				}
				state->canvas.algorithm_id = state->algorithm_id;
				clear_tree_preview_cache_state(state->preview);
			}
			return *state;
		}

		static void clear_tree_preview_cache_state(
			tree_preview_cache_state& cache) noexcept
		{
			cache.geometry.reset();
			cache.visibility.reset();
			cache.visibility_geometry.reset();
			cache.visibility_bounds = {};
			cache.visibility_request_geometry.reset();
			cache.visibility_request_id = 0;
			cache.visibility_request_bounds = {};
		}

		void sync_tree_document_view(
			std::string_view archive_path,
			const tree_document_window_state& state) noexcept
		{
			const auto document = m_tree_documents.find(std::string(archive_path));
			if (document == m_tree_documents.end())
			{
				return;
			}
			auto& view = document->second.view;
			view.zoom = state.canvas.zoom;
			view.pan_x = state.canvas.pan.x;
			view.pan_y = state.canvas.pan.y;
			view.initialized = state.canvas.view_initialized;
			view.fit_requested = state.canvas.fit_requested;
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
		std::string algorithm_id;
			bool has_metadata = false;
			visual_tree result;

			explicit tree_serialization_parser(std::string_view text)
				: source(text)
			{
			}

			[[nodiscard]] visual_tree parse(std::string& parsed_algorithm_id)
			{
				skip_whitespace();
				if (consume_prefix("bst{"))
				{
					algorithm_id = tree_practicum::mods::ids::bst;
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
				parsed_algorithm_id = std::move(algorithm_id);
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

		};

		[[nodiscard]] static visual_tree parse_tree_serialization(
			std::string_view serialization,
			std::string& algorithm_id)
		{
			return tree_serialization_parser{ serialization }.parse(algorithm_id);
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
				if (!tree.multiway && node.keys.size() != 1)
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

		static void rotate_binary_node_up(
			visual_tree& tree,
			std::size_t parent,
			std::size_t child,
			std::size_t grandparent)
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
					"Поворот получил узел, не являющийся потомком родителя.");
			}

			std::size_t grandparent_side = 2;
			if (grandparent == no_visual_node)
			{
				if (tree.root != parent)
				{
					throw std::logic_error(
						"Родитель корневого поворота не является корнем дерева.");
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
					"Дед узла поворота не ссылается на родителя.");
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
		}

		[[nodiscard]] static bool insert_bst_key(
			visual_tree& tree,
			visual_key key,
			search_tree_key_kind kind)
		{
			if (tree.multiway)
			{
				throw std::invalid_argument(
					"BST insertion received a multiway tree.");
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
			none
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
			if (tree.multiway)
			{
				throw std::invalid_argument(
					"Невозможно сериализовать многопутевое дерево как BST.");
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

		static const std::array<demo_tree_entry, 1>& demo_trees()
		{
			static const std::array<demo_tree_entry, 1> trees = []
			{
				std::array<demo_tree_entry, 1> result{{
					{ tree_practicum::mods::ids::bst, {} }
				}};
				constexpr std::array<std::int64_t, 7> integer_values{
					42, 18, 73, 9, 27, 58, 91
				};
				constexpr std::array<std::array<int, 2>, 7> binary_children{ {
					{ 1, 2 }, { 3, 4 }, { 5, 6 }, { -1, -1 },
					{ -1, -1 }, { -1, -1 }, { -1, -1 }
				} };

				auto& tree = result.front().value;
				for (std::size_t i = 0; i < integer_values.size(); ++i)
				{
					visual_node node;
					node.keys.push_back(
						make_integer_key(integer_values[i]));
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
					tree.nodes.push_back(std::move(node));
				}
				return result;
			}();
			return trees;
		}

		[[nodiscard]] const visual_tree& demo_tree_for(
			tree_practicum::mods::tree_id algorithm_id)
		{
			const auto& trees = demo_trees();
			const auto found = std::find_if(
				trees.begin(),
				trees.end(),
				[algorithm_id](const auto& entry)
				{
					return entry.algorithm_id == algorithm_id;
				});
			if (algorithm_id == tree_practicum::mods::ids::bst &&
				found != trees.end() && tree_algorithm_for(algorithm_id))
			{
				return found->value;
			}

			const auto* algorithm = tree_algorithm_for(algorithm_id);
			const auto* type = available_tree_type_for(algorithm_id);
			if (algorithm && type)
			{
				auto [empty, inserted] = m_empty_demo_trees.try_emplace(
					std::string(algorithm_id));
				if (inserted)
				{
					empty->second = algorithm->create_empty(
						default_tree_key_kind(type->algorithm));
				}
				return empty->second;
			}

			static const visual_tree empty_tree{};
			return empty_tree;
		}

		[[nodiscard]] static std::size_t count_visual_keys(
			const visual_tree& tree) noexcept
		{
			std::size_t count = 0;
			for (const auto& node : tree.nodes)
			{
				if (!tree.multiway || node.leaf)
				{
					count += node.keys.size();
				}
			}
			return count;
		}

		[[nodiscard]] static tree_preview_geometry::statistics
			summarize_visual_tree(const visual_tree& tree)
		{
			tree_preview_geometry::statistics properties;
			properties.node_count = tree.nodes.size();
			properties.key_count = count_visual_keys(tree);

			std::string_view observed_key_type;
			bool mixed_key_types = false;
			for (std::size_t node_index = 0;
				node_index < tree.nodes.size();
				++node_index)
			{
				const auto& node = tree.nodes[node_index];
				std::size_t child_count = 0;
				const auto count_child = [&](std::size_t child)
				{
					if (child != no_visual_node &&
						child < tree.nodes.size())
					{
						++child_count;
					}
				};
				if (tree.multiway)
				{
					for (const auto child : node.children)
					{
						count_child(child);
					}
				}
				else
				{
					for (const auto child : node.binary_children)
					{
						count_child(child);
					}
				}
				properties.maximum_branching = std::max(
					properties.maximum_branching,
					child_count);
				if ((tree.multiway && node.leaf) ||
					(!tree.multiway && child_count == 0))
				{
					++properties.leaf_count;
				}

				for (const auto& key : node.keys)
				{
					const auto candidate = visual_value_type(key.value);
					if (observed_key_type.empty())
					{
						observed_key_type = candidate;
					}
					else if (observed_key_type != candidate)
					{
						mixed_key_types = true;
					}
				}
			}

			if (mixed_key_types)
			{
				properties.key_type = "смешанные";
			}
			else if (observed_key_type == "string")
			{
				properties.key_type = "строка";
			}
			else if (observed_key_type == "boost::cpp_int")
			{
			properties.key_type = "целое произвольной точности";
			}
			else if (!observed_key_type.empty())
			{
				properties.key_type = observed_key_type;
			}

			if (tree.root < tree.nodes.size())
			{
				const auto& root = tree.nodes[tree.root];
				if (!root.keys.empty())
				{
					properties.root_value = full_visual_value(
						root.keys.front().value);
				}

				std::vector<bool> visited(tree.nodes.size(), false);
				std::vector<std::pair<std::size_t, std::size_t>> pending{
					{ tree.root, 1 }
				};
				visited[tree.root] = true;
				while (!pending.empty())
				{
					const auto [node_index, depth] = pending.back();
					pending.pop_back();
					properties.height = std::max(
						properties.height,
						depth);

					const auto visit_child = [&](std::size_t child)
					{
						if (child == no_visual_node ||
							child >= tree.nodes.size() ||
							visited[child])
						{
							return;
						}
						visited[child] = true;
						pending.emplace_back(child, depth + 1);
					};
					const auto& node = tree.nodes[node_index];
					if (tree.multiway)
					{
						for (const auto child : node.children)
						{
							visit_child(child);
						}
					}
					else
					{
						for (const auto child : node.binary_children)
						{
							visit_child(child);
						}
					}
				}
			}

			return properties;
		}

		void draw_tree_properties(
			const tree_preview_geometry::statistics& properties,
			tree_practicum::mods::tree_id algorithm_id,
			bool multiway,
			bool current_tree_loaded,
			std::string_view archive_path)
		{
			if (!ImGui::CollapsingHeader(
				"Общие свойства",
				ImGuiTreeNodeFlags_DefaultOpen))
			{
				return;
			}

			const std::string algorithm_name(tree_name_for(algorithm_id));
			const std::string structure_name = multiway
				? "Многопутевое"
				: "Бинарное";
			const std::string node_count = std::to_string(
				properties.node_count);
			const std::string key_count = std::to_string(
				properties.key_count);
			const std::string height = std::to_string(properties.height);
			const std::string leaf_count = std::to_string(
				properties.leaf_count);
			const std::string maximum_branching = std::to_string(
				properties.maximum_branching);
			const float property_width = std::max(
				1.0f,
				(ImGui::GetContentRegionAvail().x - 16.0f) / 3.0f);
			const std::string root_value = clip_text_to_width(
				properties.root_value,
				property_width - 12.0f);

			if (ImGui::BeginTable(
				"TreeGeneralProperties",
				3,
				ImGuiTableFlags_BordersInnerH |
					ImGuiTableFlags_BordersInnerV |
					ImGuiTableFlags_RowBg |
					ImGuiTableFlags_SizingStretchSame |
					ImGuiTableFlags_NoSavedSettings))
			{
				const auto property = [](
					std::string_view label,
					std::string_view value)
				{
					ImGui::TableNextColumn();
					ImGui::TextDisabled(
						"%.*s",
						static_cast<int>(label.size()),
						label.data());
					ImGui::TextWrapped(
						"%.*s",
						static_cast<int>(value.size()),
						value.data());
				};

				ImGui::TableNextRow();
				property("Алгоритм", algorithm_name);
				property("Структура", structure_name);
				property("Тип ключа", properties.key_type);
				ImGui::TableNextRow();
				property("Узлы", node_count);
				property("Ключи", key_count);
				property("Высота", height);
				ImGui::TableNextRow();
				property("Листья", leaf_count);
				property("Макс. потомков", maximum_branching);
				property("Ключ корня", root_value);
				ImGui::EndTable();
			}

			if (current_tree_loaded)
			{
				const bool dirty = archive_path.empty()
					? m_tree_dirty
					: tree_document_is_dirty(archive_path);
				ImGui::TextColored(
					dirty
						? ImVec4{ 1.0f, 0.72f, 0.32f, 1.0f }
						: ImVec4{ 0.43f, 0.86f, 0.58f, 1.0f },
					dirty
						? "● Есть несохранённые изменения"
						: "● Сохранено");
			}
			else
			{
				ImGui::TextDisabled("Демонстрационные данные");
			}
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
			if (tree.multiway)
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

			if (!tree.multiway)
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
			m_hovered_tree_node.reset();
			m_hovered_tree_node_algorithm_id.clear();
			m_invariant_report_cached = false;
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
				clear_tree_preview_cache_state(cache);
			}
			for (auto& [path, state] : m_tree_document_window_states)
			{
				(void)path;
				if (state)
				{
					clear_tree_preview_cache_state(state->preview);
				}
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
			mix_integer(tree.multiway ? 1u : 0u);
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
				mix_integer(node.emphasized ? 1u : 0u);
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
				if (after.multiway)
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
			for (std::size_t pass = 0; pass < (tree.multiway ? 2u : 1u); ++pass)
			{
				for (std::size_t node_index = 0;
					node_index < tree.nodes.size();
					++node_index)
				{
					if (tree.multiway &&
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
				tree_practicum::mods::tree_id algorithm_id,
				bool current_tree_loaded,
				bool showing_animation,
				std::size_t cache_index,
				tree_preview_cache_state* cache_state = nullptr)
		{
			auto& cache = cache_state
				? *cache_state
				: m_tree_preview_caches.at(cache_index);
			const std::size_t animation_step =
				showing_animation && m_tree_animation.has_value()
				? m_tree_animation->next_event
				: 0;
			if (cache.geometry &&
				cache.geometry->revision ==
					m_tree_preview_revision &&
				cache.geometry->algorithm_id == algorithm_id &&
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
			geometry->algorithm_id = algorithm_id;
			geometry->current_tree_loaded = current_tree_loaded;
			geometry->showing_animation = showing_animation;
			geometry->animation_step = animation_step;
			geometry->leaf_link_space = tree.multiway ? 24.0f : 0.0f;
			geometry->model.root = tree.root;
			geometry->model.nodes.resize(tree.nodes.size());
			const auto* type = available_tree_type_for(algorithm_id);
			const auto* visualization = type
				? std::addressof(type->algorithm.visualization)
				: nullptr;
			geometry->properties = summarize_visual_tree(tree);

			for (std::size_t i = 0; i < tree.nodes.size(); ++i)
			{
				const auto& source = tree.nodes[i];
				auto& target = geometry->model.nodes[i];
				target.width = visual_node_width;
				if (tree.multiway)
				{
					target.style = source.leaf
						? mngl::two_d::tree_node_style::multiway_leaf
						: mngl::two_d::tree_node_style::multiway_internal;
					target.annotation = source.leaf ? "leaf" : "index";
					target.children = source.children;
				}
				else if (visualization &&
					visualization->node_variants ==
						tree_practicum::mods::node_variant_mode::emphasis)
				{
					target.style = source.emphasized
						? mngl::two_d::tree_node_style::emphasized
						: mngl::two_d::tree_node_style::neutral;
					const auto label = visualization->node_variant_labels[
						source.emphasized ? 1u : 0u];
					if (!label.empty())
					{
						target.annotation = label;
					}
				}

				if (!tree.multiway)
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
						(visualization &&
							!visualization->show_key_details))
					{
						continue;
					}

					std::string metadata;
					if (visualization)
					{
						metadata.append(
							visualization->key_detail_prefix);
					}
					metadata.append(key.detail);
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
			options.show_leaf_links = tree.multiway;
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
			options.node_fill = rgb(35.0f, 57.0f, 77.0f);
			options.emphasized_node_fill = rgb(147.0f, 47.0f, 57.0f);
			options.neutral_node_fill = rgb(31.0f, 36.0f, 46.0f);
			options.multiway_leaf_fill = rgb(34.0f, 83.0f, 76.0f);
			options.multiway_internal_fill = rgb(39.0f, 57.0f, 91.0f);
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
				const mngl::two_d::tree_draw_options& draw_options,
				tree_preview_cache_state* cache_state = nullptr)
		{
			auto& cache = cache_state
				? *cache_state
				: m_tree_preview_caches.at(cache_index);
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
			tree_practicum::mods::tree_id algorithm_id,
			tree_canvas_view_state* canvas_state = nullptr,
			const visual_tree* tree_override = nullptr,
			std::string_view archive_path = {},
			std::size_t cache_index = 0,
			const char* child_id = "TreePreview",
			bool* canvas_clicked = nullptr,
			tree_preview_cache_state* preview_cache = nullptr)
		{
			if (canvas_clicked)
			{
				*canvas_clicked = false;
			}
			auto& canvas_texture = canvas_state
				? canvas_state->texture
				: m_tree_canvas_texture;
		auto& canvas_algorithm_id = canvas_state
				? canvas_state->algorithm_id
			: m_tree_canvas_tree_id;
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
					m_loaded_tree_id == algorithm_id;
			const bool preview_tree_is_active =
				!tree_override ||
				archive_path == m_current_archive_member_path;
			if (preview_tree_is_active)
			{
				advance_tree_animation(
					algorithm_id,
					ImGui::GetIO().DeltaTime);
			}
			const bool showing_animation =
				preview_tree_is_active &&
				current_tree_loaded &&
				m_tree_animation.has_value() &&
				m_tree_animation->algorithm_id == algorithm_id;
			const auto& tree = showing_animation
				? m_tree_animation->tree
				: tree_override
					? *tree_override
				: current_tree_loaded
					? *m_loaded_visual_tree
					: demo_tree_for(algorithm_id);
			const bool tree_is_empty = tree.nodes.empty();
			if (tree.multiway && !tree_is_empty)
			{
				ImGui::TextDisabled(
					"Внутренние узлы хранят разделители, листья связаны цепочкой.");
			}

			const auto geometry = get_tree_preview_geometry(
				tree,
				algorithm_id,
				current_tree_loaded,
				showing_animation,
				cache_index,
				preview_cache);
			draw_tree_properties(
				geometry->properties,
				algorithm_id,
				tree.multiway,
				current_tree_loaded,
				archive_path);
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
							tree_animation_duration(
								tree_layout_animation_seconds)),
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
				if (canvas_algorithm_id != algorithm_id)
				{
					canvas_algorithm_id = algorithm_id;
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
					std::max(1.0, layout.size.x),
					std::max(1.0, layout.size.y)
				};
				const auto visibility = get_tree_preview_visibility(
					cache_index,
					geometry,
					tree_area,
					visible_area,
					render_options,
					preview_cache);
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
					tree_algorithm_for(algorithm_id) != nullptr &&
					(std::holds_alternative<double>(hovered_key->value) ||
						std::holds_alternative<std::int64_t>(
							hovered_key->value));
				if (hovered_key_contextable)
				{
					m_hovered_tree_key = *hovered_key;
					m_hovered_tree_archive_path = std::string(archive_path);
					if (m_tree_comparison_mode && canvas_state)
					{
						m_hovered_tree_comparison_pane = cache_index;
					}
				}
				if (hovered_node >= 0 && current_tree_loaded &&
					algorithm_id == m_active_tree_id && !showing_animation)
				{
					m_hovered_tree_node =
						static_cast<std::size_t>(hovered_node);
					m_hovered_tree_node_algorithm_id =
						std::string(algorithm_id);
				}
				if (canvas_hovered &&
					ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					context_menu_node = hovered_node >= 0
						? std::optional<std::size_t>{
							static_cast<std::size_t>(hovered_node) }
						: std::nullopt;
					context_menu_key = hovered_node >= 0 &&
						hovered_key_contextable
						? std::optional<visual_key>{ *hovered_key }
						: std::nullopt;
					ImGui::OpenPopup("TreeNodeTools");
				}
				std::vector<std::size_t> highlighted_nodes;
				if (current_tree_loaded &&
					preview_tree_is_active &&
					m_tree_find_result_valid &&
					m_tree_find_tree_id == algorithm_id)
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
				if (showing_animation)
				{
					std::string animation_status;
					if (m_tree_animation->mode ==
						tree_animation_mode::layout_transition)
					{
						const float progress = std::clamp(
							m_tree_animation->elapsed_seconds /
								tree_animation_duration(
									tree_layout_animation_seconds),
							0.0f,
							1.0f);
						animation_status = std::format(
							"Раскладка · {:.0f}%",
							static_cast<double>(progress) * 100.0);
					}
					else
					{
						animation_status = std::format(
							"Анимация · шаг {}/{}",
							m_tree_animation->next_event,
							m_tree_animation->events.size());
					}

					constexpr float horizontal_padding = 8.0f;
					constexpr float vertical_padding = 5.0f;
					const ImVec2 status_text_size =
						ImGui::CalcTextSize(animation_status.c_str());
					const ImVec2 status_size{
						status_text_size.x + horizontal_padding * 2.0f,
						status_text_size.y + vertical_padding * 2.0f
					};
					const ImVec2 status_min{
						std::max(
							canvas_min.x + 8.0f,
							canvas_max.x - status_size.x - 8.0f),
						canvas_min.y + 8.0f
					};
					const ImVec2 status_max{
						status_min.x + status_size.x,
						status_min.y + status_size.y
					};
					ImVec4 status_background = ImGui::GetStyleColorVec4(
						ImGuiCol_WindowBg);
					status_background.w = 0.78f;
					ImVec4 status_border = ImGui::GetStyleColorVec4(
						ImGuiCol_Border);
					status_border.w = 0.35f;
					draw_list->AddRectFilled(
						status_min,
						status_max,
						ImGui::ColorConvertFloat4ToU32(status_background),
						6.0f);
					draw_list->AddRect(
						status_min,
						status_max,
						ImGui::ColorConvertFloat4ToU32(status_border),
						6.0f);
					draw_list->AddText(
						{
							status_min.x + horizontal_padding,
							status_min.y + vertical_padding
						},
						ImGui::ColorConvertFloat4ToU32(
							ImGui::GetStyleColorVec4(ImGuiCol_Text)),
						animation_status.c_str());
				}
				if (tree_is_empty)
				{
					const char* const empty_tree_message = current_tree_loaded
						? "Пустое дерево\nПКМ — действия с деревом"
						: "В демо пока нет узлов";
					const ImVec2 message_size =
						ImGui::CalcTextSize(empty_tree_message);
					draw_list->AddText(
						{
							canvas_min.x +
								(canvas_size.x - message_size.x) * 0.5f,
							canvas_min.y +
								(canvas_size.y - message_size.y) * 0.5f
						},
						IM_COL32(160, 173, 190, 220),
						empty_tree_message);
				}
				if (!interpolating_layout &&
					visibility->nodes.empty() &&
					(preview_cache
						? *preview_cache
						: m_tree_preview_caches.at(cache_index))
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
						tree.multiway && !node.leaf
							? "Внутренний узел хранит ключи-разделители"
							: tree.multiway
								? "Лист многопутевого дерева"
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
					const bool context_node_is_valid = context_menu_node &&
						*context_menu_node < tree.nodes.size();
					if (context_node_is_valid)
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
					}
					else
					{
						ImGui::TextDisabled(
							current_tree_loaded
								? "Действия с деревом"
								: "Демо-дерево");
					}
					fit_context_tree = ImGui::MenuItem(
						"Вписать всё дерево");
					if (context_menu_key)
					{
						ImGui::Separator();
						if (supports_tool_operation(
							tree_tool_operation::find) &&
							ImGui::MenuItem("Найти ключ"))
						{
							context_menu_action =
								tree_tool_operation::find;
						}
						if (supports_tool_operation(
							tree_tool_operation::erase) &&
							ImGui::MenuItem("Удалить ключ [D / Del]"))
						{
							context_menu_action =
								tree_tool_operation::erase;
						}
						if (supports_tool_operation(
							tree_tool_operation::split) &&
							ImGui::MenuItem("Split по ключу [C]"))
						{
							context_menu_action =
								tree_tool_operation::split;
						}
					}
					if (current_tree_loaded &&
						!showing_animation &&
						supports_tool_operation(
							tree_tool_operation::insert) &&
						ImGui::MenuItem("Вставить ключ..."))
					{
						context_menu_action =
							tree_tool_operation::insert;
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
						comparison_pane,
						archive_path);
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
			auto function_entries = available_functions();
			if (m_tree_filter >= static_cast<int>(m_available_tree_types.size()))
			{
				m_tree_filter = -1;
			}
			if (m_selected_function < 0 ||
				m_selected_function >= static_cast<int>(function_entries.size()))
			{
				m_selected_function = 0;
			}

			std::string tree_filter_name_storage = "Все деревья";
			if (m_tree_filter >= 0)
			{
				tree_filter_name_storage = std::string(
					m_available_tree_types[
						static_cast<std::size_t>(m_tree_filter)]
						.algorithm.display_name);
			}
			const char* tree_filter_name = tree_filter_name_storage.c_str();
			if (ImGui::BeginCombo("Дерево##FunctionFilter", tree_filter_name))
			{
				if (ImGui::Selectable("Все деревья", m_tree_filter < 0))
				{
					m_tree_filter = -1;
				}
				for (std::size_t i = 0;
					i < m_available_tree_types.size();
					++i)
				{
					const std::string display_name(
						m_available_tree_types[i].algorithm.display_name);
					if (ImGui::Selectable(
						display_name.c_str(),
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
				for (std::size_t i = 0; i < function_entries.size(); ++i)
				{
					const auto& function = function_entries[i];
					if (m_tree_filter >= 0 &&
						function.algorithm_id != m_available_tree_types[
							static_cast<std::size_t>(m_tree_filter)].algorithm.id)
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
						static_cast<int>(tree_name_for(
							function.algorithm_id).size()),
						tree_name_for(function.algorithm_id).data());
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

			const auto& selected = function_entries[
				static_cast<std::size_t>(m_selected_function)];
			const bool selected_matches =
				(m_tree_filter < 0 ||
					selected.algorithm_id == m_available_tree_types[
						static_cast<std::size_t>(m_tree_filter)].algorithm.id) &&
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

			if (selected.definition && selected.definition->custom)
			{
				const bool selected_tree_is_open =
					selected.algorithm_id == m_active_tree_id &&
					m_loaded_visual_tree.has_value() &&
					m_loaded_tree_id == m_active_tree_id;
				if (!selected_tree_is_open)
				{
					if (ImGui::Button("Перейти к выбранному типу дерева"))
					{
						select_tree(selected.algorithm_id);
						m_last_action =
							"Выберите сериализацию дерева этого типа в проводнике.";
					}
					ImGui::SameLine();
					ImGui::TextDisabled(
						"Сначала откройте дерево типа %.*s.",
						static_cast<int>(tree_name_for(
							selected.algorithm_id).size()),
						tree_name_for(selected.algorithm_id).data());
					return;
				}

				const auto& command = *selected.definition;
				auto& inputs = m_custom_command_inputs[
					command_identity(command.algorithm_id, command.name)];
				inputs.resize(command.parameters.size());
				for (std::size_t i = 0; i < command.parameters.size(); ++i)
				{
					const auto& parameter = command.parameters[i];
					ImGui::PushID(static_cast<int>(i));
					(void)ImGui::InputTextWithHint(
						parameter.label.c_str(),
						parameter.hint.empty()
							? parameter.type.c_str()
							: parameter.hint.c_str(),
						inputs[i].data(),
						inputs[i].size());
					ImGui::PopID();
				}

				const auto* handler = tree_command_handler_for(
					command.algorithm_id);
				const bool can_execute = handler &&
					handler->supports_command(command.name) &&
					m_loaded_search_tree_valid &&
					m_loaded_search_tree_key_kind.has_value();
				ImGui::BeginDisabled(!can_execute);
				const bool execute_clicked = ImGui::Button("Выполнить команду");
				ImGui::EndDisabled();
				if (execute_clicked)
				{
					execute_custom_command(command, inputs);
				}
				if (!m_custom_command_error.empty())
				{
					ImGui::TextColored(
						{ 1.0f, 0.42f, 0.38f, 1.0f },
						"%s",
						m_custom_command_error.c_str());
				}
				if (!handler)
				{
					ImGui::TextDisabled(
						"Обработчик команды мода не зарегистрирован.");
				}
				return;
			}

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
					selected.algorithm_id == m_active_tree_id &&
					m_loaded_visual_tree.has_value() &&
					m_loaded_tree_id == m_active_tree_id;
				if (!selected_tree_is_open)
				{
					if (ImGui::Button("Перейти к выбранному типу дерева"))
					{
						select_tree(selected.algorithm_id);
						m_last_action =
							"Выберите сериализацию дерева этого типа в проводнике.";
					}
					ImGui::SameLine();
					ImGui::TextDisabled(
						"Сначала откройте дерево типа %.*s.",
						static_cast<int>(tree_name_for(
							selected.algorithm_id).size()),
						tree_name_for(selected.algorithm_id).data());
					return;
				}

				const auto selected_operation =
					tree_tool_for_function(selected.name);
				const bool can_operate = selected_operation &&
					supports_tool_operation(*selected_operation);
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
				if (action_clicked && selected_operation)
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
					"Операция недоступна для выбранного типа ключей или алгоритма.");
				}
				return;
			}

			if (ImGui::Button("Выбрать тип нового дерева"))
			{
				select_tree(selected.algorithm_id);
				m_last_action = "Выбран тип для следующего нового файла.";
				ImGui::SameLine();
				ImGui::TextDisabled(
					"операция пока не подключена");
			}
		}

		[[nodiscard]] static std::string command_identity(
			std::string_view algorithm_id,
			std::string_view command_id)
		{
			std::string result(algorithm_id);
			result.push_back('\n');
			result.append(command_id);
			return result;
		}

		void execute_custom_command(
			const owned_function_entry& command,
			const std::vector<std::array<char, 256>>& inputs)
		{
			clear_tree_find_result();
			m_custom_command_error.clear();
			try
			{
				const auto* handler = tree_command_handler_for(
					command.algorithm_id);
				const auto* algorithm = tree_algorithm_for(command.algorithm_id);
				if (!handler || !handler->supports_command(command.name) ||
					!algorithm || !m_loaded_visual_tree ||
					!m_loaded_search_tree_key_kind)
				{
					throw std::logic_error(
						"Команда или открытое дерево больше недоступны.");
				}
				if (inputs.size() != command.parameters.size())
				{
					throw std::logic_error(
						"Число полей команды не совпадает с её описанием.");
				}

				std::vector<mngl::mods::tree::command_argument> arguments;
				arguments.reserve(command.parameters.size());
				for (std::size_t i = 0; i < command.parameters.size(); ++i)
				{
					const auto& parameter = command.parameters[i];
					const std::string_view input(inputs[i].data());
					if (parameter.type == "text" ||
						parameter.type == "bit_string")
					{
						arguments.emplace_back(std::string(input));
					}
					else if (parameter.type == "int64")
					{
						std::int64_t value = 0;
						const auto parsed = std::from_chars(
							input.data(), input.data() + input.size(), value);
						if (input.empty() || parsed.ec != std::errc{} ||
							parsed.ptr != input.data() + input.size())
						{
							throw std::invalid_argument(
								"Параметр «" + parameter.label +
								"» должен быть целым int64_t.");
						}
						arguments.emplace_back(value);
					}
					else if (parameter.type == "double")
					{
						double value = 0.0;
						const auto parsed = std::from_chars(
							input.data(), input.data() + input.size(), value,
							std::chars_format::general);
						if (input.empty() || parsed.ec != std::errc{} ||
							parsed.ptr != input.data() + input.size() ||
							!std::isfinite(value))
						{
							throw std::invalid_argument(
								"Параметр «" + parameter.label +
								"» должен быть конечным числом.");
						}
						arguments.emplace_back(value);
					}
					else if (parameter.type == "boolean")
					{
						if (input == "true" || input == "1")
							arguments.emplace_back(true);
						else if (input == "false" || input == "0")
							arguments.emplace_back(false);
						else
							throw std::invalid_argument(
								"Параметр «" + parameter.label +
								"» должен быть true/false или 1/0.");
					}
					else
					{
						throw std::logic_error(
							"Описание команды содержит неизвестный тип параметра.");
					}
				}

				cancel_tree_animation();
				m_tree_operation_error.clear();
				auto updated_tree = *m_loaded_visual_tree;
				const auto key_kind = *m_loaded_search_tree_key_kind;
				auto result = handler->execute(
					command.name,
					updated_tree,
					key_kind,
					std::span<const mngl::mods::tree::command_argument>(
						arguments));
				if (std::any_of(
					result.highlighted_nodes.begin(),
					result.highlighted_nodes.end(),
					[&](std::size_t index)
					{
						return index >= updated_tree.nodes.size();
					}))
				{
					throw std::invalid_argument(
						"Обработчик вернул индекс узла вне дерева.");
				}
				if (result.changed)
				{
					algorithm->validate(updated_tree, key_kind);
					auto serialization = algorithm->serialize(updated_tree);
					store_search_tree_update(
						std::move(updated_tree),
						std::move(serialization),
						key_kind);
				}
				m_tree_find_path = std::move(result.highlighted_nodes);
				m_tree_find_tree_id = m_active_tree_id;
				m_tree_find_result_valid = true;
				m_tree_find_found = true;
				m_tree_find_status = result.message.empty()
					? "Команда мода выполнена."
					: std::move(result.message);
				m_last_action = m_tree_find_status;
			}
			catch (const std::exception& error)
			{
				m_custom_command_error = error.what();
				m_tree_find_tree_id = m_active_tree_id;
				m_tree_find_result_valid = true;
				m_tree_find_status = "Ошибка команды: " + m_custom_command_error;
				m_last_action = m_tree_find_status;
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
					ImGui::Text(
						"Дерево: %.*s",
						static_cast<int>(tree_name_for(
							m_active_tree_id).size()),
						tree_name_for(m_active_tree_id).data());
					const bool empty_workspace =
						m_current_tree_name.empty() &&
						!m_show_demo_trees;
					const bool current_tree_loaded =
						m_loaded_visual_tree.has_value() &&
						m_loaded_tree_id == m_active_tree_id;
					if (empty_workspace)
					{
						ImGui::TextDisabled(
							"Рабочая область пуста. Создайте дерево или загрузите .tree.");
					}
					else
					{
						const auto& displayed_tree = current_tree_loaded
							? *m_loaded_visual_tree
							: demo_tree_for(m_active_tree_id);
						ImGui::Text(
							"%s: %zu узлов · ключей: %zu",
							current_tree_loaded ? "Загружено" : "Демо",
							displayed_tree.nodes.size(),
							count_visual_keys(displayed_tree));
						if (displayed_tree.multiway)
						{
							ImGui::TextDisabled(
							"Внутренние узлы содержат разделители, листья связаны цепочкой.");
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
				if (ImGui::BeginTabItem("Инварианты и ход"))
				{
					draw_invariants_and_operation();
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

		void draw_invariants_and_operation()
		{
			const bool tree_is_open = m_loaded_visual_tree.has_value() &&
				m_loaded_tree_id == m_active_tree_id &&
				m_loaded_search_tree_valid;
			if (!tree_is_open)
			{
				ImGui::TextDisabled(
					"Откройте корректный файл дерева, чтобы проверить его инварианты.");
				return;
			}

			const auto* type = available_tree_type_for(m_active_tree_id);
			const auto* inspector = tree_invariant_inspector_for(m_active_tree_id);
			if (!type || !inspector)
			{
				ImGui::TextDisabled(
					"Для этого типа дерева мод не предоставил инспектор инвариантов.");
				return;
			}
			const auto key_kind = m_loaded_search_tree_key_kind.value_or(
				default_tree_key_kind(type->algorithm));
			if (!m_invariant_report_cached ||
				m_invariant_report_tree_id != m_active_tree_id ||
				m_invariant_report_revision != m_tree_preview_revision ||
				m_invariant_report_key_kind != key_kind)
			{
				m_invariant_report = {};
				m_invariant_report_error.clear();
				try
				{
					m_invariant_report = inspector->inspect(
						*m_loaded_visual_tree,
						key_kind);
				}
				catch (const std::exception& error)
				{
					m_invariant_report_error = error.what();
				}
				m_invariant_report_tree_id = m_active_tree_id;
				m_invariant_report_revision = m_tree_preview_revision;
				m_invariant_report_key_kind = key_kind;
				m_invariant_report_cached = true;
			}

			ImGui::TextDisabled("ПРОВЕРКА СТРУКТУРЫ");
			if (!m_invariant_report_error.empty())
			{
				ImGui::TextColored(
					{ 1.0f, 0.42f, 0.38f, 1.0f },
					"Проверка завершилась ошибкой: %s",
					m_invariant_report_error.c_str());
			}
			else if (m_invariant_report.checks.empty())
			{
				ImGui::TextDisabled("Инспектор не вернул проверок.");
			}
			else
			{
				for (std::size_t i = 0;
					i < m_invariant_report.checks.size();
					++i)
				{
					const auto& check = m_invariant_report.checks[i];
					ImGui::PushID(static_cast<int>(i));
					ImGui::TextColored(
						check.passed
							? ImVec4{ 0.43f, 0.86f, 0.58f, 1.0f }
							: ImVec4{ 1.0f, 0.42f, 0.38f, 1.0f },
						"%s %s",
						check.passed ? "✓" : "✗",
						check.name.c_str());
					ImGui::TextWrapped("%s", check.detail.c_str());
					ImGui::PopID();
				}
			}

			ImGui::Separator();
			ImGui::TextDisabled("МЕТРИКИ УЗЛА");
			const bool hovered_node_available =
				m_hovered_tree_node.has_value() &&
				m_hovered_tree_node_algorithm_id == m_active_tree_id &&
				*m_hovered_tree_node < m_loaded_visual_tree->nodes.size();
			if (hovered_node_available)
			{
				ImGui::TextDisabled(
					"Последний узел под курсором: %zu",
					*m_hovered_tree_node);
				ImGui::SameLine();
				if (ImGui::SmallButton("Показать##HoveredInvariantNode"))
				{
					m_invariant_selected_node = static_cast<int>(
						std::min<std::size_t>(
							*m_hovered_tree_node,
							static_cast<std::size_t>(
								std::numeric_limits<int>::max())));
				}
			}
			if (m_invariant_selected_node < 0 &&
				!m_loaded_visual_tree->nodes.empty())
			{
				m_invariant_selected_node = 0;
			}
			if (!m_loaded_visual_tree->nodes.empty())
			{
				ImGui::SetNextItemWidth(110.0f);
				(void)ImGui::InputInt(
					"Индекс узла",
					&m_invariant_selected_node);
				m_invariant_selected_node = std::clamp(
					m_invariant_selected_node,
					0,
					static_cast<int>(std::min<std::size_t>(
						m_loaded_visual_tree->nodes.size() - 1,
						static_cast<std::size_t>(
							std::numeric_limits<int>::max()))));
				try
				{
					const auto details = inspector->describe_node(
						*m_loaded_visual_tree,
						key_kind,
						static_cast<std::size_t>(m_invariant_selected_node));
					ImGui::TextWrapped("%s", details.c_str());
				}
				catch (const std::exception& error)
				{
					ImGui::TextColored(
						{ 1.0f, 0.42f, 0.38f, 1.0f },
						"Не удалось прочитать узел: %s",
						error.what());
				}
			}
			else
			{
				ImGui::TextDisabled("Дерево пустое.");
			}

			ImGui::Separator();
			ImGui::TextDisabled("ХОД ОПЕРАЦИИ");
			ImGui::TextWrapped("%s", m_last_action.c_str());
			if (m_tree_animation &&
				m_tree_animation->algorithm_id == m_active_tree_id)
			{
				if (m_tree_animation->mode == tree_animation_mode::step_events)
				{
					ImGui::Text(
						"Шаг анимации: %zu / %zu",
						m_tree_animation->next_event,
						m_tree_animation->events.size());
					if (m_tree_animation->active_event)
					{
						const auto& event =
							*m_tree_animation->active_event;
						const char* action =
							event.action == tree_visual_event::kind::insert_node
							? "Создание узла"
							: "Поворот узла вверх";
						const auto parent_label =
							event.parent_index == no_visual_node
							? std::string("∅")
							: std::to_string(event.parent_index);
						ImGui::Text(
							"%s · узел %zu · родитель %s",
							action,
							event.node_index,
							parent_label.c_str());
					}
				}
				else
				{
					const float progress = std::clamp(
						m_tree_animation->elapsed_seconds /
							tree_animation_duration(
								tree_layout_animation_seconds),
						0.0f,
						1.0f);
					ImGui::ProgressBar(progress, { -1.0f, 0.0f }, "Переход раскладки");
				}
			}
			else if (m_tree_find_result_valid &&
				m_tree_find_tree_id == m_active_tree_id)
			{
				ImGui::Text(
					"Узлы результата: %zu",
					m_tree_find_path.size());
				if (ImGui::BeginChild(
					"InvariantOperationTrace",
					{ 0.0f, 110.0f },
					ImGuiChildFlags_Borders))
				{
					const auto visible_count = std::min<std::size_t>(
						m_tree_find_path.size(),
						static_cast<std::size_t>(
							std::numeric_limits<int>::max()));
					ImGuiListClipper clipper;
					clipper.Begin(static_cast<int>(visible_count));
					while (clipper.Step())
					{
						for (int i = clipper.DisplayStart;
							i < clipper.DisplayEnd;
							++i)
						{
							const auto node_index = m_tree_find_path[
								static_cast<std::size_t>(i)];
							if (node_index >= m_loaded_visual_tree->nodes.size())
							{
								ImGui::Text("%d. узел %zu (вне дерева)", i + 1, node_index);
								continue;
							}
							const auto& node =
								m_loaded_visual_tree->nodes[node_index];
							std::string labels;
							for (std::size_t key_index = 0;
								key_index < node.keys.size();
								++key_index)
							{
								if (key_index != 0) labels += ", ";
								labels += full_visual_value(
									node.keys[key_index].value);
							}
							ImGui::Text(
								"%d. узел %zu · %s",
								i + 1,
								node_index,
								labels.c_str());
						}
					}
				}
				ImGui::EndChild();
			}
			else
			{
				ImGui::TextDisabled(
					"Детальный путь доступен для поиска; текущая операция сообщает только итог.");
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
				if (tree.multiway)
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
					"Внутренний узел содержит неверную ссылку на потомка.");
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

		class tree_operation_algorithm : public tree_practicum::mods::algorithm
		{
		public:
			[[nodiscard]] virtual std::string_view name() const noexcept
			{
				return info().display_name;
			}

			[[nodiscard]] visual_tree create_empty(
				tree_practicum::mods::key_kind) const override
			{
				visual_tree result;
				result.root = no_visual_node;
				return result;
			}

			[[nodiscard]] tree_practicum::mods::document deserialize(
				std::string_view source) const override
			{
				std::string parsed_id;
				auto result = parse_tree_serialization(source, parsed_id);
				if (parsed_id != id())
				{
					throw tree_serialization_error(
						"Tree serialization belongs to a different algorithm");
				}
				const auto keys = infer_search_tree_key_kind(result);
				return {
					std::string(id()),
					std::move(result),
					keys
				};
			}

			void validate(
				const visual_tree& tree,
				search_tree_key_kind kind) const override = 0;

			[[nodiscard]] tree_find_result find(
				visual_tree& tree,
				const visual_key& key,
				search_tree_key_kind kind) const override
			{
				return find_visual_search_tree(tree, key, kind);
			}

			[[nodiscard]] tree_operation_result insert(
				visual_tree& tree,
				visual_key key,
				search_tree_key_kind kind,
				bool collect_animation_events) const override = 0;

			[[nodiscard]] tree_bulk_insert_result insert_many(
				visual_tree& tree,
				std::uint64_t requested_count,
				search_tree_key_kind kind,
				std::mt19937_64& random_engine) const override
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
			[[nodiscard]] tree_erase_result erase(
				visual_tree& tree,
				const visual_key& key,
				search_tree_key_kind kind) const override
			{
				const auto result = erase_native_search_tree(
					id(),
					tree,
					key,
					kind);
				if (result.changed)
				{
					tree = parse_native_operation_tree(
						result.serialization,
						id(),
						kind);
				}
				return {
					result.operation_succeeded,
					result.changed
				};
			}

			[[nodiscard]] bool merge(
				visual_tree& left,
				const visual_tree& right,
				search_tree_key_kind kind) const override
			{
				const auto result = merge_native_search_trees(
					id(),
					left,
					right,
					kind);
				if (result.changed)
				{
					left = parse_native_operation_tree(
						result.serialization,
						id(),
						kind);
				}
				return result.changed;
			}

			[[nodiscard]] std::pair<visual_tree, visual_tree> split(
				const visual_tree& tree,
				const visual_key& pivot,
				search_tree_key_kind kind) const override
			{
				auto serializations = split_native_search_tree(
					id(),
					tree,
					pivot,
					kind);
				return {
					parse_native_operation_tree(
						serializations.first,
						id(),
						kind),
					parse_native_operation_tree(
						serializations.second,
						id(),
						kind)
				};
			}
		};

		class bst_tree_algorithm final : public tree_operation_algorithm
		{
		public:
			[[nodiscard]] const tree_practicum::mods::algorithm_info& info() const noexcept override
			{
				return tree_practicum::mods::builtin_bst_info;
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

		class bst_tree_invariant_inspector final :
			public mngl::mods::tree::invariant_inspector
		{
		public:
			[[nodiscard]] std::string_view algorithm_id() const noexcept override
			{
				return tree_practicum::mods::ids::bst;
			}

			[[nodiscard]] mngl::mods::tree::invariant_report inspect(
				const visual_tree& tree,
				search_tree_key_kind keys) const override
			{
				mngl::mods::tree::invariant_report report;
				try
				{
					validate_search_tree_structure(tree, keys);
					report.checks.push_back({
						"Порядок BST",
						true,
						"Для каждого узла ключи слева меньше, а ключи справа больше.",
					});
					report.checks.push_back({
						"Связность структуры",
						true,
						"Нет циклов и недостижимых узлов; каждый узел содержит один ключ.",
					});
				}
				catch (const std::exception& error)
				{
					report.checks.push_back({
						"Структура BST",
						false,
						std::format("Нарушение: {}", error.what()),
					});
				}
				return report;
			}

			[[nodiscard]] std::string describe_node(
				const visual_tree& tree,
				search_tree_key_kind,
				std::size_t node_index) const override
			{
				if (node_index >= tree.nodes.size())
				{
					throw std::out_of_range("Индекс узла BST вне дерева.");
				}
				const auto& current = tree.nodes[node_index];
				if (current.keys.size() != 1)
				{
					throw std::invalid_argument(
						"Узел BST должен содержать ровно один ключ.");
				}
				const auto child_label = [&](std::size_t child)
				{
					if (child == no_visual_node) return std::string("∅");
					if (child >= tree.nodes.size() ||
						tree.nodes[child].keys.size() != 1)
					{
						throw std::invalid_argument(
							"Дочерний узел BST содержит неверную ссылку.");
					}
					return full_visual_value(
						tree.nodes[child].keys.front().value);
				};
				return std::format(
					"Ключ: {} · левый потомок: {} · правый потомок: {}",
					full_visual_value(current.keys.front().value),
					child_label(current.binary_children[0]),
					child_label(current.binary_children[1]));
			}
		};

		template<class NativeTree, class Value>
		[[nodiscard]] static NativeTree make_native_binary_tree(
			const visual_tree& tree)
		{
			using node_type = typename NativeTree::node_type;
			if (tree.multiway)
			{
				throw std::invalid_argument(
					"Ожидалось бинарное дерево поиска, получено многопутевое дерево.");
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
				nodes[i] = std::make_unique<node_type>(value);
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

		[[nodiscard]] static visual_tree parse_native_operation_tree(
			std::string_view serialization,
			tree_practicum::mods::tree_id expected_algorithm_id,
			search_tree_key_kind expected_key_kind)
		{
			std::string parsed_algorithm_id;
			auto tree = parse_tree_serialization(
				serialization,
				parsed_algorithm_id);
			if (parsed_algorithm_id != expected_algorithm_id)
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
				expected_algorithm_id);
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
		[[nodiscard]] static tree_native_change_result erase_native_search_tree_typed(
			const visual_tree& tree,
			const visual_key& key,
			search_tree_key_kind kind,
			tree_practicum::mods::tree_id algorithm_id)
		{
			const Value value = native_key_value<Value>(key);
			bool erased = false;
			bool changed = false;
			std::string serialization;
			if (algorithm_id == tree_practicum::mods::ids::bst)
			{
				auto native = make_native_binary_tree<bst<Value>, Value>(tree);
				erased = native.erase(value);
				changed = erased;
				if (changed)
				{
					serialization = native.to_string();
				}
			}
			else
			{
				throw std::invalid_argument(
					"Неизвестный ID алгоритма дерева.");
			}
			return { erased, changed, std::move(serialization) };
		}

		[[nodiscard]] static tree_native_change_result erase_native_search_tree(
			tree_practicum::mods::tree_id algorithm_id,
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
					algorithm_id);
			}
			if (kind == search_tree_key_kind::floating_point)
			{
				return erase_native_search_tree_typed<double>(
					tree,
					key,
					kind,
					algorithm_id);
			}
			throw std::invalid_argument(
				"Операции поддерживают только int64_t и конечный double.");
		}

		template<class Value>
		[[nodiscard]] static tree_native_change_result merge_native_search_trees_typed(
			tree_practicum::mods::tree_id algorithm_id,
			const visual_tree& left,
			const visual_tree& right,
			search_tree_key_kind kind)
		{
			if (right.nodes.empty())
			{
				return {};
			}
			if (algorithm_id == tree_practicum::mods::ids::bst)
			{
				auto native_left =
					make_native_binary_tree<bst<Value>, Value>(left);
				auto native_right =
					make_native_binary_tree<bst<Value>, Value>(right);
				native_left.merge(std::move(native_right));
				return { true, true, native_left.to_string() };
			}
			else
			{
				throw std::invalid_argument(
					"Неизвестный ID алгоритма дерева.");
			}
		}

		[[nodiscard]] static tree_native_change_result merge_native_search_trees(
			tree_practicum::mods::tree_id algorithm_id,
			const visual_tree& left,
			const visual_tree& right,
			search_tree_key_kind kind)
		{
			if (kind == search_tree_key_kind::signed_integer)
			{
				return merge_native_search_trees_typed<std::int64_t>(
					algorithm_id,
					left,
					right,
					kind);
			}
			if (kind == search_tree_key_kind::floating_point)
			{
				return merge_native_search_trees_typed<double>(
					algorithm_id,
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
				tree_practicum::mods::tree_id algorithm_id,
				const visual_tree& tree,
				const visual_key& pivot,
				search_tree_key_kind)
		{
			const Value value = native_key_value<Value>(pivot);
			if (algorithm_id == tree_practicum::mods::ids::bst)
			{
				auto native = make_native_binary_tree<bst<Value>, Value>(tree);
				auto [left, right] = native.split(value);
				return { left.to_string(), right.to_string() };
			}
			else
			{
				throw std::invalid_argument(
					"Неизвестный ID алгоритма дерева.");
			}
		}

		[[nodiscard]] static std::pair<std::string, std::string>
			split_native_search_tree(
				tree_practicum::mods::tree_id algorithm_id,
				const visual_tree& tree,
				const visual_key& pivot,
				search_tree_key_kind kind)
		{
			if (kind == search_tree_key_kind::signed_integer)
			{
				return split_native_search_tree_typed<std::int64_t>(
					algorithm_id,
					tree,
					pivot,
					kind);
			}
			if (kind == search_tree_key_kind::floating_point)
			{
				return split_native_search_tree_typed<double>(
					algorithm_id,
					tree,
					pivot,
					kind);
			}
			throw std::invalid_argument(
				"Операции поддерживают только int64_t и конечный double.");
		}

		[[nodiscard]] const tree_practicum::mods::algorithm*
			tree_algorithm_for(
				tree_practicum::mods::tree_id algorithm_id) const
		{
			const auto found = m_tree_algorithms.find(std::string(algorithm_id));
			return found == m_tree_algorithms.end()
				? nullptr
				: found->second.get();
		}

		[[nodiscard]] const mngl::mods::tree::custom_command_handler*
			tree_command_handler_for(std::string_view algorithm_id) const
		{
			const auto found = m_tree_command_handlers.find(
				std::string(algorithm_id));
			return found == m_tree_command_handlers.end()
				? nullptr
				: found->second.get();
		}

		[[nodiscard]] const mngl::mods::tree::invariant_inspector*
			tree_invariant_inspector_for(std::string_view algorithm_id) const
		{
			const auto found = m_tree_invariant_inspectors.find(
				std::string(algorithm_id));
			return found == m_tree_invariant_inspectors.end()
				? nullptr
				: found->second.get();
		}

		void initialize_tree_mods()
		{
			m_mod_registry.apply_declarations();
			auto& algorithms = m_mod_registry.define_class<
				tree_practicum::mods::algorithm>(
					std::string(tree_practicum::mods::algorithm_class_id));
			(void)m_mod_registry.define_class<
				mngl::mods::tree::custom_command_handler>(
					std::string(
						mngl::mods::tree::custom_command_class_id));
			auto& invariant_inspectors = m_mod_registry.define_class<
				mngl::mods::tree::invariant_inspector>(
					std::string(
						mngl::mods::tree::invariant_inspector_class_id));
			algorithms.add_builtin(
				std::string(tree_practicum::mods::ids::bst),
				[] { return std::make_unique<bst_tree_algorithm>(); });
			invariant_inspectors.add_builtin(
				std::string(tree_practicum::mods::ids::bst),
				[] { return std::make_unique<bst_tree_invariant_inspector>(); });

			const char* executable_base = SDL_GetBasePath();
			if (!executable_base)
			{
				logger.warning(
					"Could not locate the executable directory; tree mods were not loaded.");
				refresh_tree_algorithm_catalog();
				refresh_tree_command_catalog();
				refresh_tree_invariant_catalog();
				std::error_code current_path_error;
				const auto current_directory =
					std::filesystem::current_path(current_path_error);
				if (!current_path_error)
				{
					load_command_catalogs(current_directory, {});
				}
				return;
			}
			const std::filesystem::path mods_directory =
				std::filesystem::path(executable_base) / "mods";

			std::error_code directory_error;
			if (!std::filesystem::is_directory(mods_directory, directory_error))
			{
				logger.info(
					"No mods directory found at '",
					path_to_utf8(mods_directory),
					"'; starting with built-in BST only.");
				refresh_tree_algorithm_catalog();
				refresh_tree_command_catalog();
				refresh_tree_invariant_catalog();
				load_command_catalogs(
					std::filesystem::path(executable_base), mods_directory);
				return;
			}

			try
			{
				for (const auto& result : m_mod_loader.load_all(mods_directory))
				{
					if (result)
					{
						logger.success(
							"Loaded mod '", result.mod_id, "': ", result.message);
					}
					else
					{
						logger.error(
							"Rejected mod '",
							result.mod_id.empty() ? "<unknown>" : result.mod_id,
							"': ",
							result.message);
					}
				}
			}
			catch (const std::exception& error)
			{
				logger.error(
					"Failed to scan mods directory '",
					path_to_utf8(mods_directory),
					"': ",
					error.what());
			}
			refresh_tree_algorithm_catalog();
			refresh_tree_command_catalog();
			refresh_tree_invariant_catalog();
			load_command_catalogs(
				std::filesystem::path(executable_base), mods_directory);
		}

		[[nodiscard]] static std::optional<tree_practicum::mods::operation>
			command_operation(std::string_view name) noexcept
		{
			using operation = tree_practicum::mods::operation;
			if (name == "find") return operation::find;
			if (name == "insert") return operation::insert;
			if (name == "insert_random") return operation::random_insert;
			if (name == "erase") return operation::erase;
			if (name == "merge") return operation::merge;
			if (name == "split") return operation::split;
			return std::nullopt;
		}

		void load_command_catalog_file(
			const std::filesystem::path& path,
			bool from_shared_catalog)
		{
			std::error_code file_error;
			const auto size = std::filesystem::file_size(path, file_error);
			if (file_error)
			{
				throw std::runtime_error(
					"cannot inspect catalog file: " + file_error.message());
			}
			if (size > 4 * 1024 * 1024)
			{
				throw std::length_error("command catalog file exceeds 4 MiB");
			}
			std::ifstream input(path, std::ios::binary);
			if (!input)
			{
				throw std::runtime_error("cannot open command catalog file");
			}
			const std::string source{
				std::istreambuf_iterator<char>(input),
				std::istreambuf_iterator<char>() };
			const auto catalog = mngl::mods::command_catalog::parse(source);
			std::size_t added = 0;
			for (const auto& command : catalog.commands)
			{
				if (!valid_tree_algorithm_id(command.algorithm_id))
				{
					logger.warning(
						"Ignoring command with an invalid tree ID in '",
						path_to_utf8(path), "'.");
					continue;
				}
				const auto* type = available_tree_type_for(command.algorithm_id);
				const auto operation = command_operation(command.name);
				const auto* command_handler =
					tree_command_handler_for(command.algorithm_id);
				const bool is_custom_command = !operation && command_handler &&
					command_handler->supports_command(command.name);
				const bool supported_operation = type && operation &&
					command.parameters.empty() &&
					tree_practicum::mods::supports(
						type->algorithm.supported_operations, *operation);
				if (!type || (!supported_operation && !is_custom_command))
				{
					logger.warning(
						"Ignoring unsupported command '", command.name,
						"' for tree '", command.algorithm_id, "'.");
					continue;
				}
				const auto identity = command.algorithm_id + "\n" + command.name;
				const auto existing = m_command_function_indices.find(identity);
				if (existing != m_command_function_indices.end())
				{
					auto& previous = m_command_functions[existing->second];
					if (previous->from_shared_catalog && !from_shared_catalog)
					{
					auto replacement =
						std::make_unique<owned_function_entry>();
						replacement->algorithm_id = command.algorithm_id;
						replacement->name = command.name;
						replacement->signature = command.signature;
						replacement->description = command.description;
						replacement->parameters = command.parameters;
						replacement->from_shared_catalog = false;
						replacement->custom = is_custom_command;
						previous = std::move(replacement);
						++added;
						continue;
					}
					logger.warning(
						"Ignoring duplicate command description '",
						command.name, "' for tree '", command.algorithm_id, "'.");
					continue;
				}
				auto stored = std::make_unique<owned_function_entry>();
				stored->algorithm_id = command.algorithm_id;
				stored->name = command.name;
				stored->signature = command.signature;
				stored->description = command.description;
				stored->parameters = command.parameters;
				stored->from_shared_catalog = from_shared_catalog;
				stored->custom = is_custom_command;
				m_command_function_indices.emplace(
					identity, m_command_functions.size());
				m_command_functions.push_back(std::move(stored));
				++added;
			}
			logger.info(
				"Loaded ", added, " command descriptions from '",
				path_to_utf8(path), "'.");
		}

		void load_command_catalogs(
			const std::filesystem::path& executable_directory,
			const std::filesystem::path& mods_directory)
		{
			m_command_functions.clear();
			m_command_function_indices.clear();
			const auto shared_catalog =
				executable_directory / "data" / "tree_commands.json";
			std::error_code error;
			bool has_shared_catalog =
				std::filesystem::is_regular_file(shared_catalog, error);
			std::vector<std::filesystem::path> package_catalogs;
			try
			{
				if (std::filesystem::is_directory(mods_directory, error))
				{
					for (const auto& package :
						std::filesystem::directory_iterator(mods_directory))
					{
						if (!package.is_directory(error))
						{
							error.clear();
							continue;
						}
						const auto path = package.path() / "commands.json";
						if (std::filesystem::is_regular_file(path, error))
						{
							package_catalogs.push_back(path);
						}
						error.clear();
					}
				}
			}
			catch (const std::exception& scan_error)
			{
				logger.error(
					"Failed to scan mod command catalogs in '",
					path_to_utf8(mods_directory), "': ", scan_error.what());
			}
			std::sort(package_catalogs.begin(), package_catalogs.end());
			if (has_shared_catalog)
			{
				try
				{
					load_command_catalog_file(shared_catalog, true);
				}
				catch (const std::exception& error)
				{
					logger.error(
						"Failed to load command catalog '",
						path_to_utf8(shared_catalog),
						"': ", error.what());
				}
			}
			for (const auto& path : package_catalogs)
			{
				try
				{
					load_command_catalog_file(path, false);
				}
				catch (const std::exception& error)
				{
					logger.error(
						"Failed to load command catalog '", path_to_utf8(path),
						"': ", error.what());
				}
			}
		}

		void refresh_tree_command_catalog()
		{
			m_tree_command_handlers.clear();
			auto& registry = m_mod_registry.classes<
				mngl::mods::tree::custom_command_handler>(
					mngl::mods::tree::custom_command_class_id);
			const auto implementations = registry.implementations();
			m_tree_command_handlers.reserve(implementations.size());
			for (const auto& implementation : implementations)
			{
				auto handler = registry.create(implementation.id);
				if (!handler)
				{
					logger.error(
						"Tree command handler factory returned no object for '",
						implementation.id,
						"'.");
					continue;
				}
				m_tree_command_handlers.emplace(
					implementation.id,
					std::move(handler));
			}
		}

		void refresh_tree_invariant_catalog()
		{
			m_tree_invariant_inspectors.clear();
			auto& registry = m_mod_registry.classes<
				mngl::mods::tree::invariant_inspector>(
					mngl::mods::tree::invariant_inspector_class_id);
			const auto implementations = registry.implementations();
			m_tree_invariant_inspectors.reserve(implementations.size());
			for (const auto& implementation : implementations)
			{
				auto inspector = registry.create(implementation.id);
				if (!inspector)
				{
					logger.error(
						"Tree invariant inspector factory returned no object for '",
						implementation.id,
						"'.");
					continue;
				}
				if (inspector->algorithm_id() != implementation.id)
				{
					logger.error(
						"Tree invariant inspector ID does not match its factory ID: '",
						implementation.id,
						"'.");
					continue;
				}
				m_tree_invariant_inspectors.emplace(
					implementation.id,
					std::move(inspector));
			}
		}

		void refresh_tree_algorithm_catalog()
		{
			m_available_tree_types.clear();
			m_empty_demo_trees.clear();
			m_tree_algorithms.clear();

			auto& registry = m_mod_registry.classes<
				tree_practicum::mods::algorithm>(
					tree_practicum::mods::algorithm_class_id);
			const auto implementations = registry.implementations();
			m_tree_algorithms.reserve(implementations.size());
			m_available_tree_types.reserve(implementations.size());
			for (const auto& implementation : implementations)
			{
				auto algorithm = registry.create(implementation.id);
				if (!algorithm)
				{
					logger.error(
						"Tree algorithm factory returned no object for '",
						implementation.id,
						"'.");
					continue;
				}

				const auto& info = algorithm->info();
				if (info.id != implementation.id ||
					info.display_name.empty() ||
					info.serialization_prefix.empty() ||
					info.supported_key_kinds == 0 ||
					info.legacy_tree_catalog_index < -1 ||
					!safe_tree_file_stem(info.file_stem))
				{
					logger.error(
						"Tree algorithm '",
						implementation.id,
						"' has invalid metadata or an ID that differs from its factory ID.");
					continue;
				}

				const auto duplicate_legacy_index = std::find_if(
					m_available_tree_types.begin(),
					m_available_tree_types.end(),
					[&](const auto& type)
					{
						return info.legacy_tree_catalog_index >= 0 &&
							type.algorithm.legacy_tree_catalog_index ==
								info.legacy_tree_catalog_index;
					});
				if (duplicate_legacy_index != m_available_tree_types.end())
				{
					logger.error(
						"Tree algorithm '", implementation.id,
						"' duplicates a legacy catalog index owned by '",
						duplicate_legacy_index->algorithm.id,
						"'.");
					continue;
				}

				const auto duplicate_prefix = std::find_if(
					m_available_tree_types.begin(),
					m_available_tree_types.end(),
					[&](const auto& type)
					{
						return type.algorithm.serialization_prefix ==
							info.serialization_prefix;
					});
				if (duplicate_prefix != m_available_tree_types.end())
				{
					logger.error(
						"Tree algorithm '",
						implementation.id,
						"' uses a serialization prefix already owned by '",
						duplicate_prefix->algorithm.id,
						"'.");
					continue;
				}

				m_available_tree_types.push_back({ info });
				m_tree_algorithms.emplace(
					implementation.id,
					std::move(algorithm));
			}

			std::sort(
				m_available_tree_types.begin(),
				m_available_tree_types.end(),
				[](const auto& left, const auto& right)
				{
					const bool left_is_bst = left.algorithm.id ==
						tree_practicum::mods::ids::bst;
					const bool right_is_bst = right.algorithm.id ==
						tree_practicum::mods::ids::bst;
					return left_is_bst != right_is_bst
						? left_is_bst
						: left.algorithm.id < right.algorithm.id;
				});

			if (!tree_algorithm_for(tree_practicum::mods::ids::bst))
			{
				throw std::logic_error(
					"The built-in BST tree algorithm was not registered.");
			}
		}

		[[nodiscard]] static const tree_operation_algorithm*
			search_tree_algorithm_for(
				tree_practicum::mods::tree_id algorithm_id) noexcept
		{
			static const bst_tree_algorithm bst;

			if (algorithm_id == bst.id())
				return &bst;
			return nullptr;
		}

		[[nodiscard]] tree_practicum::mods::document
			deserialize_tree_document(std::string_view source)
		{
			const auto* type = available_tree_type_for_serialization(source);
			if (!type)
			{
				throw tree_serialization_error("неизвестный тип дерева");
			}
			const auto* algorithm = tree_algorithm_for(
				type->algorithm.id);
			if (!algorithm)
			{
				throw tree_serialization_error(
					"для этого типа дерева не загружен алгоритм");
			}
			try
			{
				auto document = algorithm->deserialize(source);
				if (document.algorithm_id != type->algorithm.id)
				{
					throw tree_serialization_error(
						"алгоритм вернул документ с другим ID дерева");
				}
				return document;
			}
			catch (const tree_serialization_error&)
			{
				throw;
			}
			catch (const std::exception& error)
			{
				throw tree_serialization_error(error.what());
			}
		}

		mngl::mods::registry m_mod_registry;
		mngl::mods::mod_loader m_mod_loader;
		std::unordered_map<std::string,
			std::unique_ptr<tree_practicum::mods::algorithm>> m_tree_algorithms;
		std::unordered_map<std::string,
			std::unique_ptr<mngl::mods::tree::custom_command_handler>>
			m_tree_command_handlers;
		std::unordered_map<std::string,
			std::unique_ptr<mngl::mods::tree::invariant_inspector>>
			m_tree_invariant_inspectors;
		std::vector<std::unique_ptr<owned_function_entry>>
			m_command_functions;
		std::unordered_map<std::string, std::size_t>
			m_command_function_indices;
		std::unordered_map<std::string,
			std::vector<std::array<char, 256>>> m_custom_command_inputs;
		std::unordered_map<std::string, visual_tree> m_empty_demo_trees;
		std::vector<tree_type_descriptor> m_available_tree_types;
		mngl::render_window& m_render_window;
		mngl::two_d::font& m_tree_font;
		std::optional<mngl::texture> m_tree_canvas_texture;
		std::uint64_t m_tree_animation_source_id =
				next_tree_animation_source_id.fetch_add(1);
		std::shared_ptr<tree_animation_event_queue>
			m_tree_animation_event_queue =
				std::make_shared<tree_animation_event_queue>();
		std::optional<tree_animation_state> m_tree_animation;
		std::filesystem::path m_live_python_directory;
		std::string m_live_python_session_id;
		std::string m_live_python_token;
		std::optional<python_bridge::response> m_live_python_response;
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
		std::unordered_map<std::string, tree_document>
			m_tree_documents;
		std::unordered_map<std::string,
			std::unique_ptr<tree_document_window_state>>
			m_tree_document_window_states;
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
		std::string m_hovered_tree_archive_path;
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
		std::string m_custom_command_error;
		std::string m_invariant_report_error;
		std::string m_invariant_report_tree_id;
		std::string m_explorer_error;
		std::string m_file_error;
		std::string m_file_dialog_error;
		std::array<char, 1024> m_key_input{};
		std::uint64_t m_random_count = 25;
		int m_selected_workspace = 1;
		int m_selected_function = 0;
		int m_selected_archive_entry = -1;
		std::string m_active_tree_id =
			std::string(tree_practicum::mods::ids::bst);
		std::string m_new_tree_id =
			std::string(tree_practicum::mods::ids::bst);
		std::string m_tree_find_tree_id;
		std::string m_loaded_tree_id;
		int m_tree_filter = -1;
		int m_active_comparison_pane = 0;
		std::string m_tree_canvas_tree_id;
		double m_tree_canvas_zoom = 1.0;
		glm::dvec2 m_tree_canvas_pan{};
		file_dialog_mode m_file_dialog_mode = file_dialog_mode::open;
		pending_workspace_action m_pending_workspace_action;
		bool m_show_explorer = true;
		bool m_show_workspace = true;
		bool m_show_tools = true;
		bool m_new_tree_dialog_open = false;
		bool m_new_tree_dialog_recenter = false;
		std::string m_pending_new_tree_id;
		bool m_show_functions = true;
		bool m_show_information = true;
		bool m_execute_hovered_key_tools_immediately = true;
		bool m_tree_animations_enabled = true;
		float m_tree_animation_speed = 1.0f;
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
		std::uint64_t m_invariant_report_revision =
			std::numeric_limits<std::uint64_t>::max();
		std::optional<tree_practicum::mods::key_kind>
			m_invariant_report_key_kind;
		std::optional<std::size_t> m_hovered_tree_node;
		std::string m_hovered_tree_node_algorithm_id;
		int m_invariant_selected_node = -1;
		mngl::mods::tree::invariant_report m_invariant_report;
		bool m_invariant_report_cached = false;
	};
}
