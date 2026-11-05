#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace mngl::console
{
	enum class argument_type
	{
		string,
		integer,
		floating_point,
		flag
	};

	enum class argument_form
	{
		positional,
		flag
	};

	using argument_value = std::variant<std::string, std::int64_t, double, bool>;

	struct argument_spec
	{
		std::string name;
		std::string description;
		argument_type type{argument_type::string};
		argument_form form{argument_form::positional};
		std::string option_name;
		bool required{true};
		std::optional<argument_value> default_value;
	};

	struct command_descriptor
	{
		std::string id;
		std::string description;
		std::vector<argument_spec> arguments;
	};

	struct command_result
	{
		std::string stdout_text;
		int exit_code{};
		std::string stderr_text;
	};

	enum class output_format
	{
		human_readable,
		json
	};

	struct stream_options
	{
		bool stop_on_error{};
		output_format format{output_format::human_readable};
	};

	struct stream_result
	{
		std::size_t commands_processed{};
		std::size_t commands_failed{};
		bool quit_requested{};
		int exit_code{};
	};

	enum class error_code
	{
		invalid_command_id,
		invalid_command_description,
		duplicate_command,
		invalid_argument,
		unknown_command,
		argument_count_mismatch,
		argument_type_mismatch,
		argument_out_of_range
	};

	class registry_error : public std::runtime_error
	{
	public:
		registry_error(error_code code, std::string message, std::string command_id = {})
		    : std::runtime_error(std::move(message)), m_code(code), m_command_id(std::move(command_id))
		{
		}

		[[nodiscard]] error_code code() const noexcept { return m_code; }
		[[nodiscard]] const std::string& command_id() const noexcept { return m_command_id; }

	private:
		error_code m_code;
		std::string m_command_id;
	};

	namespace detail
	{
		template <class T> using argument_t = std::remove_cvref_t<T>;

		template <class T>
		inline constexpr bool is_integer_argument_v =
		    std::integral<argument_t<T>> && !std::same_as<argument_t<T>, bool> &&
		    sizeof(argument_t<T>) <= sizeof(std::int64_t);

		template <class T>
		inline constexpr bool is_supported_argument_v =
		    std::same_as<argument_t<T>, std::string> || is_integer_argument_v<T> ||
		    std::same_as<argument_t<T>, float> || std::same_as<argument_t<T>, double>;

		template <class T> constexpr argument_type get_argument_type()
		{
			using value_t = argument_t<T>;
			if constexpr (std::same_as<value_t, std::string>)
				return argument_type::string;
			else if constexpr (is_integer_argument_v<value_t>)
				return argument_type::integer;
			else
				return argument_type::floating_point;
		}

		inline bool is_valid_command_id(std::string_view id) noexcept
		{
			if (id.empty() || id.front() == '.' || id.front() == '-' || id.back() == '.')
				return false;

			bool previous_was_dot = false;
			for (const unsigned char ch : id)
			{
				const bool valid = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
				                   (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.';
				if (!valid || (ch == '.' && previous_was_dot))
					return false;
				previous_was_dot = ch == '.';
			}
			return true;
		}

		inline bool is_valid_argument_name(std::string_view name) noexcept
		{
			if (name.empty())
				return false;
			for (const unsigned char ch : name)
			{
				const bool valid = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
				                   (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
				if (!valid)
					return false;
			}
			return true;
		}

		template <class T>
		decltype(auto) decode_argument(const argument_value& value, std::string_view command_id,
		                               std::string_view argument_name)
		{
			using value_t = argument_t<T>;
			if constexpr (std::same_as<value_t, std::string>)
			{
				const auto* stored = std::get_if<std::string>(&value);
				if (!stored)
					throw registry_error(error_code::argument_type_mismatch,
					                     "Argument '" + std::string(argument_name) + "' has the wrong type",
					                     std::string(command_id));
				return *stored;
			}
			else if constexpr (is_integer_argument_v<value_t>)
			{
				const auto* stored = std::get_if<std::int64_t>(&value);
				if (!stored)
					throw registry_error(error_code::argument_type_mismatch,
					                     "Argument '" + std::string(argument_name) + "' has the wrong type",
					                     std::string(command_id));
				if (!std::in_range<value_t>(*stored))
					throw registry_error(error_code::argument_out_of_range,
					                     "Argument '" + std::string(argument_name) +
					                         "' is outside the handler type range",
					                     std::string(command_id));
				return static_cast<value_t>(*stored);
			}
			else if constexpr (std::floating_point<value_t>)
			{
				const auto* stored = std::get_if<double>(&value);
				if (!stored)
					throw registry_error(error_code::argument_type_mismatch,
					                     "Argument '" + std::string(argument_name) + "' has the wrong type",
					                     std::string(command_id));
				if constexpr (!std::same_as<value_t, double>)
				{
					constexpr auto max_value = std::numeric_limits<value_t>::max();
					if (*stored < -max_value || *stored > max_value)
						throw registry_error(error_code::argument_out_of_range,
						                     "Argument '" + std::string(argument_name) +
						                         "' is outside the handler type range",
						                     std::string(command_id));
				}
				return static_cast<value_t>(*stored);
			}
			else
			{
				static_assert(std::same_as<value_t, bool>,
				              "Flags are declared with flag(), not argument<T>()");
				const auto* stored = std::get_if<bool>(&value);
				if (!stored)
					throw registry_error(error_code::argument_type_mismatch,
					                     "Argument '" + std::string(argument_name) + "' has the wrong type",
					                     std::string(command_id));
				return *stored;
			}
		}

		inline command_result normalize_result(command_result result)
		{
			return result;
		}

		inline command_result normalize_result(std::string result)
		{
			return {std::move(result), 0};
		}

		inline command_result normalize_result(std::string_view result)
		{
			return {std::string(result), 0};
		}

		inline command_result normalize_result(const char* result)
		{
			return {result ? std::string(result) : std::string{}, 0};
		}

		template <class T>
		inline constexpr bool is_supported_result_v =
		    std::same_as<std::remove_cvref_t<T>, command_result> ||
		    std::same_as<std::remove_cvref_t<T>, std::string> ||
		    std::same_as<std::remove_cvref_t<T>, std::string_view> ||
		    std::same_as<std::remove_cvref_t<T>, const char*> || std::same_as<std::remove_cvref_t<T>, char*>;

		template <class... Args, class F, std::size_t... Indices>
		command_result invoke_typed_handler(F& handler, std::span<const argument_value> values,
		                                    const command_descriptor& descriptor,
		                                    std::index_sequence<Indices...>)
		{
			using result_t = std::invoke_result_t<F&, Args...>;
			if constexpr (std::is_void_v<result_t>)
			{
				if constexpr (sizeof...(Args) == 0)
					std::invoke(handler);
				else
					std::invoke(handler, decode_argument<Args>(values[Indices], descriptor.id,
					                                           descriptor.arguments[Indices].name)...);
				return {};
			}
			else
			{
				if constexpr (sizeof...(Args) == 0)
					return normalize_result(std::invoke(handler));
				else
					return normalize_result(
					    std::invoke(handler, decode_argument<Args>(values[Indices], descriptor.id,
						                                           descriptor.arguments[Indices].name)...));
			}
		}

		template <class... Args, class F> auto make_handler(F&& callback)
		{
			using callback_t = std::decay_t<F>;
			static_assert(std::copy_constructible<callback_t>, "Command handlers must be copyable");
			static_assert(std::invocable<callback_t&, Args...>,
			              "The handler must accept the command's declared argument types");
			using result_t = std::invoke_result_t<callback_t&, Args...>;
			static_assert(std::is_void_v<result_t> || is_supported_result_v<result_t>,
			              "A command handler must return void, std::string, std::string_view, "
			              "a string literal, or command_result");

			return [fn = callback_t(std::forward<F>(callback))](
			           std::span<const argument_value> values,
			           const command_descriptor& descriptor) mutable -> command_result
			{
				if (values.size() != sizeof...(Args))
					throw registry_error(error_code::argument_count_mismatch,
					                     "Command argument count does not match its declaration",
					                     descriptor.id);
				return invoke_typed_handler<Args...>(fn, values, descriptor,
				                                     std::index_sequence_for<Args...>{});
			};
		}
	} // namespace detail

} // namespace mngl::console
