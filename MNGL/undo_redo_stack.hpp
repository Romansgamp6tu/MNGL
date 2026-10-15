#pragma once

#include <concepts>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

template<
	std::equality_comparable State,
	std::size_t HistoryLimit = 256,
	std::size_t MemoryLimitBytes = std::numeric_limits<std::size_t>::max()>
class undo_redo_stack
{
public:
	static_assert(HistoryLimit > 0);

	static constexpr std::size_t history_limit = HistoryLimit;
	static constexpr std::size_t memory_limit_bytes = MemoryLimitBytes;

	enum class record_result
	{
		unchanged,
		recorded,
		state_too_large
	};

	record_result record(State previous, const State& current)
	{
		if (previous == current)
		{
			return record_result::unchanged;
		}
		if (state_bytes(previous) > memory_limit_bytes ||
			state_bytes(current) > memory_limit_bytes)
		{
			clear();
			return record_result::state_too_large;
		}

		redo_stack.clear();
		undo_stack.push_back(std::move(previous));
		trim_history();
		return record_result::recorded;
	}

	std::optional<State> undo(State current)
	{
		if (undo_stack.empty())
		{
			return std::nullopt;
		}
		if (state_bytes(current) > memory_limit_bytes)
		{
			clear();
			return std::nullopt;
		}

		redo_stack.push_back(std::move(current));
		State restored = std::move(undo_stack.back());
		undo_stack.pop_back();
		trim_history();
		return restored;
	}

	std::optional<State> redo(State current)
	{
		if (redo_stack.empty())
		{
			return std::nullopt;
		}
		if (state_bytes(current) > memory_limit_bytes)
		{
			clear();
			return std::nullopt;
		}

		undo_stack.push_back(std::move(current));
		State restored = std::move(redo_stack.back());
		redo_stack.pop_back();
		trim_history();
		return restored;
	}

	void clear()
	{
		undo_stack.clear();
		redo_stack.clear();
	}

	void replace_states(
		std::vector<State> undo_states,
		std::vector<State> redo_states)
	{
		if (redo_states.size() > history_limit ||
			undo_states.size() > history_limit - redo_states.size())
		{
			throw std::length_error("Undo history contains too many states");
		}

		std::size_t bytes = 0;
		const auto validate_size = [&bytes](const auto& states)
		{
			for (const auto& state : states)
			{
				const auto state_size = state_bytes(state);
				if (state_size > memory_limit_bytes - bytes)
				{
					throw std::length_error(
						"Undo history exceeds its memory limit");
				}
				bytes += state_size;
			}
		};
		validate_size(undo_states);
		validate_size(redo_states);

		undo_stack = std::move(undo_states);
		redo_stack = std::move(redo_states);
	}

	[[nodiscard]] bool can_undo() const noexcept
	{
		return !undo_stack.empty();
	}

	[[nodiscard]] bool can_redo() const noexcept
	{
		return !redo_stack.empty();
	}

	[[nodiscard]] std::size_t undo_size() const noexcept
	{
		return undo_stack.size();
	}

	[[nodiscard]] std::size_t redo_size() const noexcept
	{
		return redo_stack.size();
	}

	[[nodiscard]] const std::vector<State>& undo_states() const noexcept
	{
		return undo_stack;
	}

	[[nodiscard]] const std::vector<State>& redo_states() const noexcept
	{
		return redo_stack;
	}

	[[nodiscard]] std::size_t history_bytes() const
	{
		std::size_t bytes = 0;
		for (const auto& state : undo_stack)
		{
			bytes = add_saturated(bytes, state_bytes(state));
		}
		for (const auto& state : redo_stack)
		{
			bytes = add_saturated(bytes, state_bytes(state));
		}
		return bytes;
	}

	[[nodiscard]] const State* undo_target() const noexcept
	{
		return undo_stack.empty()
			? nullptr
			: std::addressof(undo_stack.back());
	}

	[[nodiscard]] const State* redo_target() const noexcept
	{
		return redo_stack.empty()
			? nullptr
			: std::addressof(redo_stack.back());
	}

private:
	[[nodiscard]] static std::size_t state_bytes(const State& state)
	{
		if constexpr (requires
		{
			{ state.history_bytes() } -> std::convertible_to<std::size_t>;
		})
		{
			return state.history_bytes();
		}
		else
		{
			return sizeof(State);
		}
	}

	[[nodiscard]] static std::size_t add_saturated(
		std::size_t left,
		std::size_t right) noexcept
	{
		return right > std::numeric_limits<std::size_t>::max() - left
			? std::numeric_limits<std::size_t>::max()
			: left + right;
	}

	void trim_history()
	{
		while (undo_stack.size() + redo_stack.size() > history_limit ||
			history_bytes() > memory_limit_bytes)
		{
			if (!undo_stack.empty())
			{
				undo_stack.erase(undo_stack.begin());
			}
			else if (!redo_stack.empty())
			{
				redo_stack.erase(redo_stack.begin());
			}
			else
			{
				break;
			}
		}
	}

	std::vector<State> undo_stack;
	std::vector<State> redo_stack;
};
