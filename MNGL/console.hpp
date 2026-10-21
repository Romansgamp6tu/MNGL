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
        argument_type type{ argument_type::string };
        argument_form form{ argument_form::positional };
        std::string option_name;
        bool required{ true };
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
        output_format format{ output_format::human_readable };
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
            : std::runtime_error(std::move(message)),
              m_code(code),
              m_command_id(std::move(command_id))
        {}

        [[nodiscard]] error_code code() const noexcept { return m_code; }
        [[nodiscard]] const std::string& command_id() const noexcept { return m_command_id; }

    private:
        error_code m_code;
        std::string m_command_id;
    };

    namespace detail
    {
        template<class T>
        using argument_t = std::remove_cvref_t<T>;

        template<class T>
        inline constexpr bool is_integer_argument_v =
            std::integral<argument_t<T>> &&
            !std::same_as<argument_t<T>, bool> &&
            sizeof(argument_t<T>) <= sizeof(std::int64_t);

        template<class T>
        inline constexpr bool is_supported_argument_v =
            std::same_as<argument_t<T>, std::string> ||
            is_integer_argument_v<T> ||
            std::same_as<argument_t<T>, float> ||
            std::same_as<argument_t<T>, double>;

        template<class T>
        constexpr argument_type get_argument_type()
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
                const bool valid =
                    (ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    ch == '_' || ch == '-' || ch == '.';
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
                const bool valid =
                    (ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    ch == '_' || ch == '-';
                if (!valid)
                    return false;
            }
            return true;
        }

        template<class T>
        decltype(auto) decode_argument(const argument_value& value,
            std::string_view command_id, std::string_view argument_name)
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
                        "Argument '" + std::string(argument_name) + "' is outside the handler type range",
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
                            "Argument '" + std::string(argument_name) + "' is outside the handler type range",
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
            return { std::move(result), 0 };
        }

        inline command_result normalize_result(std::string_view result)
        {
            return { std::string(result), 0 };
        }

        inline command_result normalize_result(const char* result)
        {
            return { result ? std::string(result) : std::string{}, 0 };
        }

        template<class T>
        inline constexpr bool is_supported_result_v =
            std::same_as<std::remove_cvref_t<T>, command_result> ||
            std::same_as<std::remove_cvref_t<T>, std::string> ||
            std::same_as<std::remove_cvref_t<T>, std::string_view> ||
            std::same_as<std::remove_cvref_t<T>, const char*> ||
            std::same_as<std::remove_cvref_t<T>, char*>;

        template<class... Args, class F, std::size_t... Indices>
        command_result invoke_typed_handler(F& handler,
            std::span<const argument_value> values,
            const command_descriptor& descriptor,
            std::index_sequence<Indices...>)
        {
            using result_t = std::invoke_result_t<F&, Args...>;
            if constexpr (std::is_void_v<result_t>)
            {
                if constexpr (sizeof...(Args) == 0)
                    std::invoke(handler);
                else
                    std::invoke(handler,
                        decode_argument<Args>(values[Indices], descriptor.id,
                            descriptor.arguments[Indices].name)...);
                return {};
            }
            else
            {
                if constexpr (sizeof...(Args) == 0)
                    return normalize_result(std::invoke(handler));
                else
                    return normalize_result(std::invoke(handler,
                        decode_argument<Args>(values[Indices], descriptor.id,
                            descriptor.arguments[Indices].name)...));
            }
        }

        template<class... Args, class F>
        auto make_handler(F&& callback)
        {
            using callback_t = std::decay_t<F>;
            static_assert(std::copy_constructible<callback_t>,
                "Command handlers must be copyable");
            static_assert(std::invocable<callback_t&, Args...>,
                "The handler must accept the command's declared argument types");
            using result_t = std::invoke_result_t<callback_t&, Args...>;
            static_assert(std::is_void_v<result_t> || is_supported_result_v<result_t>,
                "A command handler must return void, std::string, std::string_view, "
                "a string literal, or command_result");

            return [fn = callback_t(std::forward<F>(callback))]
                (std::span<const argument_value> values,
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
    }

    class registry;

    template<class... Args>
    class command_builder
    {
    public:
        command_builder(const command_builder&) = delete;
        command_builder& operator=(const command_builder&) = delete;
        command_builder(command_builder&&) noexcept = default;
        command_builder& operator=(command_builder&&) noexcept = default;

        template<class T>
            requires detail::is_supported_argument_v<T>
        [[nodiscard]] auto argument(std::string name, std::string description) &&
        {
            using value_t = detail::argument_t<T>;
            argument_spec spec;
            spec.name = std::move(name);
            spec.description = std::move(description);
            spec.type = detail::get_argument_type<value_t>();
            spec.form = argument_form::positional;
            return std::move(*this).template append<value_t>(std::move(spec));
        }

        [[nodiscard]] auto flag(std::string name, std::string description) &&
        {
            argument_spec spec;
            spec.name = std::move(name);
            spec.description = std::move(description);
            spec.type = argument_type::flag;
            spec.form = argument_form::flag;
            spec.option_name = "--" + spec.name;
            spec.required = false;
            spec.default_value = false;
            return std::move(*this).template append<bool>(std::move(spec));
        }

        template<class F>
        registry& handler(F&& callback);

    private:
        command_builder(registry& owner, command_descriptor descriptor)
            : m_owner(&owner), m_descriptor(std::move(descriptor))
        {}

        template<class T>
        [[nodiscard]] auto append(argument_spec spec) &&
        {
            m_descriptor.arguments.push_back(std::move(spec));
            return command_builder<Args..., T>(*m_owner, std::move(m_descriptor));
        }

        registry* m_owner{};
        command_descriptor m_descriptor;

        template<class...>
        friend class command_builder;
        friend class registry;
    };

    class registry
    {
    public:
        using value_list = std::vector<argument_value>;

        [[nodiscard]] command_builder<> add_command(std::string id,
            std::string description)
        {
            command_descriptor descriptor;
            descriptor.id = std::move(id);
            descriptor.description = std::move(description);
            return command_builder<>(*this, std::move(descriptor));
        }

        [[nodiscard]] const command_descriptor* find(std::string_view id) const
        {
            const auto found = m_commands.find(std::string(id));
            return found == m_commands.end() ? nullptr : &found->second.descriptor;
        }

        [[nodiscard]] std::vector<command_descriptor> commands() const
        {
            std::vector<command_descriptor> result;
            result.reserve(m_commands.size());
            for (const auto& item : m_commands)
                result.push_back(item.second.descriptor);
            std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
            {
                return left.id < right.id;
            });
            return result;
        }

        [[nodiscard]] command_result invoke(std::string_view id,
            std::span<const argument_value> values) const
        {
            const auto found = m_commands.find(std::string(id));
            if (found == m_commands.end())
                throw registry_error(error_code::unknown_command,
                    "Unknown command '" + std::string(id) + "'", std::string(id));

            const auto& entry = found->second;
            if (values.size() != entry.descriptor.arguments.size())
                throw registry_error(error_code::argument_count_mismatch,
                    "Command argument count does not match its declaration",
                    entry.descriptor.id);

            for (std::size_t i = 0; i < values.size(); ++i)
            {
                const auto& spec = entry.descriptor.arguments[i];
                const bool matches =
                    (spec.type == argument_type::string && std::holds_alternative<std::string>(values[i])) ||
                    (spec.type == argument_type::integer && std::holds_alternative<std::int64_t>(values[i])) ||
                    (spec.type == argument_type::floating_point && std::holds_alternative<double>(values[i])) ||
                    (spec.type == argument_type::flag && std::holds_alternative<bool>(values[i]));
                if (!matches)
                    throw registry_error(error_code::argument_type_mismatch,
                        "Argument '" + spec.name + "' has the wrong type",
                        entry.descriptor.id);
            }

            return entry.handler(values, entry.descriptor);
        }

    private:
        using erased_handler = std::function<command_result(
            std::span<const argument_value>, const command_descriptor&)>;

        struct command_entry
        {
            command_descriptor descriptor;
            erased_handler handler;
        };

        void register_command(command_descriptor descriptor, erased_handler handler)
        {
            if (!detail::is_valid_command_id(descriptor.id))
                throw registry_error(error_code::invalid_command_id,
                    "Command IDs must contain letters, digits, '_', '-' or '.', "
                    "must not start with '-' or contain empty dot-separated segments", descriptor.id);
            if (descriptor.description.empty())
                throw registry_error(error_code::invalid_command_description,
                    "Command description must not be empty", descriptor.id);

            std::vector<std::string> names;
            std::vector<std::string> options;
            for (const auto& arg : descriptor.arguments)
            {
                if (!detail::is_valid_argument_name(arg.name) || arg.description.empty())
                    throw registry_error(error_code::invalid_argument,
                        "Argument names must be non-empty and use letters, digits, '_', or '-'; "
                        "argument descriptions must not be empty", descriptor.id);
                if (std::find(names.begin(), names.end(), arg.name) != names.end())
                    throw registry_error(error_code::invalid_argument,
                        "Duplicate argument name '" + arg.name + "'", descriptor.id);
                names.push_back(arg.name);

                if (arg.form == argument_form::flag)
                {
                    if (arg.name.front() == '-' || arg.name.back() == '-')
                        throw registry_error(error_code::invalid_argument,
                            "Flag names must not start or end with '-'", descriptor.id);
                    if (arg.name == "help")
                        throw registry_error(error_code::invalid_argument,
                            "The '--help' flag is reserved by the console dispatcher", descriptor.id);
                    if (arg.type != argument_type::flag || arg.option_name != "--" + arg.name)
                        throw registry_error(error_code::invalid_argument,
                            "Flag metadata is inconsistent", descriptor.id);
                    if (std::find(options.begin(), options.end(), arg.option_name) != options.end())
                        throw registry_error(error_code::invalid_argument,
                            "Duplicate flag '" + arg.option_name + "'", descriptor.id);
                    options.push_back(arg.option_name);
                }
                else if (arg.type == argument_type::flag || !arg.option_name.empty())
                {
                    throw registry_error(error_code::invalid_argument,
                        "Positional arguments cannot be declared as flags", descriptor.id);
                }
            }

            const auto command_id = descriptor.id;
            const auto insert_result = m_commands.emplace(command_id,
                command_entry{ std::move(descriptor), std::move(handler) });
            if (!insert_result.second)
                throw registry_error(error_code::duplicate_command,
                    "Command '" + command_id + "' is already registered", command_id);
        }

        std::unordered_map<std::string, command_entry> m_commands;

        template<class...>
        friend class command_builder;
    };

    template<class... Args>
    template<class F>
    registry& command_builder<Args...>::handler(F&& callback)
    {
        if (!m_owner)
            throw std::logic_error("Command builder has already been consumed");
        auto* owner = m_owner;
        auto erased = detail::make_handler<Args...>(std::forward<F>(callback));
        owner->register_command(std::move(m_descriptor), std::move(erased));
        m_owner = nullptr;
        return *owner;
    }

    class dispatcher
    {
    public:
        explicit dispatcher(const registry& commands) noexcept
            : m_commands(&commands)
        {}

        template<class Argv>
        int run_process(int argc, Argv argv, std::istream& input,
            std::ostream& output, std::ostream& error,
            stream_options options = {}) const
        {
            if (argc < 0)
            {
                const auto result = usage_error("Process argument count cannot be negative");
                write_result(result, output, error, options.format);
                return result.exit_code;
            }
            if (argc > 0 && argv == nullptr)
            {
                const auto result = usage_error("Process argument array is null");
                write_result(result, output, error, options.format);
                return result.exit_code;
            }

            std::vector<std::string_view> tokens;
            if (argc > 1)
                tokens.reserve(static_cast<std::size_t>(argc - 1));
            for (int i = 1; i < argc; ++i)
            {
                if (argv[i] == nullptr)
                {
                    const auto result = usage_error("Process argument is null");
                    write_result(result, output, error, options.format);
                    return result.exit_code;
                }
                tokens.emplace_back(argv[i]);
            }

            auto format = options.format;
            bool json_option_seen = false;
            bool console_option_seen = false;
            bool console_mode = false;
            std::size_t first_command_token = 0;
            while (first_command_token < tokens.size())
            {
                const auto token = tokens[first_command_token];
                if (token == "--json")
                {
                    if (json_option_seen)
                    {
                        const auto result = usage_error("Global option '--json' was provided more than once");
                        write_result(result, output, error, format);
                        return result.exit_code;
                    }
                    json_option_seen = true;
                    format = output_format::json;
                    ++first_command_token;
                    continue;
                }
                if (token == "--console")
                {
                    if (console_option_seen)
                    {
                        const auto result = usage_error("Global option '--console' was provided more than once");
                        write_result(result, output, error, format);
                        return result.exit_code;
                    }
                    console_option_seen = true;
                    console_mode = true;
                    ++first_command_token;
                    continue;
                }
                break;
            }

            command_result result;
            if (console_mode && first_command_token == tokens.size())
            {
                options.format = format;
                return run_stream(input, output, error, options).exit_code;
            }
            if (console_mode && tokens.size() - first_command_token == 1)
            {
                result = dispatch(tokens[first_command_token]);
            }
            else
            {
                result = dispatch_tokens(std::span<const std::string_view>{ tokens }
                    .subspan(first_command_token));
            }

            write_result(result, output, error, format);
            return result.exit_code;
        }

        template<class Argv>
        int run_process(int argc, Argv argv, stream_options options = {}) const
        {
            return run_process(argc, argv, std::cin, std::cout, std::cerr, options);
        }

        [[nodiscard]] stream_result run_stream(std::istream& input,
            std::ostream& output, std::ostream& error,
            stream_options options = {}) const
        {
            stream_result summary;
            std::string line;
            while (std::getline(input, line))
            {
                const auto command_line = trim(line);
                if (command_line.empty())
                    continue;
                if (command_line == ":quit" || command_line == ":exit")
                {
                    summary.quit_requested = true;
                    break;
                }

                ++summary.commands_processed;
                const auto result = dispatch(command_line);
                write_result(result, output, error, options.format);
                if (result.exit_code != 0)
                {
                    ++summary.commands_failed;
                    if (summary.exit_code == 0)
                        summary.exit_code = result.exit_code;
                    if (options.stop_on_error)
                        break;
                }
            }

            if (input.bad())
            {
                write_result(runtime_error("Failed to read command stream"),
                    output, error, options.format);
                if (summary.exit_code == 0)
                    summary.exit_code = 1;
            }
            return summary;
        }

        // argc/argv follow the standard main() convention: argv[0] is the program name.
        [[nodiscard]] command_result dispatch(int argc, const char* const* argv) const
        {
            return dispatch_argv(argc, argv);
        }

        [[nodiscard]] command_result dispatch(int argc, char** argv) const
        {
            return dispatch_argv(argc, argv);
        }

        // Parses a command line such as: "tree.insert 42 --verbose".
        [[nodiscard]] command_result dispatch(std::string_view line) const
        {
            std::vector<std::string> owned_tokens;
            std::string tokenize_error;
            if (!tokenize(line, owned_tokens, tokenize_error))
                return usage_error("Invalid command line: " + tokenize_error);

            std::vector<std::string_view> tokens;
            tokens.reserve(owned_tokens.size());
            for (const auto& token : owned_tokens)
                tokens.emplace_back(token);
            return dispatch_tokens(tokens);
        }

    private:
        static void write_result(const command_result& result,
            std::ostream& output, std::ostream& error, output_format format)
        {
            if (format == output_format::json)
            {
                output << to_json(result) << '\n';
                output.flush();
                return;
            }
            output << result.stdout_text;
            error << result.stderr_text;
            output.flush();
            error.flush();
        }

        [[nodiscard]] static std::string to_json(const command_result& result)
        {
            std::string json;
            json.reserve(result.stdout_text.size() + result.stderr_text.size() + 48);
            json += "{\"exit_code\":";
            json += std::to_string(result.exit_code);
            json += ",\"stdout\":";
            append_json_string(json, result.stdout_text);
            json += ",\"stderr\":";
            append_json_string(json, result.stderr_text);
            json.push_back('}');
            return json;
        }

        static void append_json_string(std::string& output, std::string_view value)
        {
            constexpr char hex_digits[] = "0123456789abcdef";
            output.push_back('"');
            for (const unsigned char character : value)
            {
                switch (character)
                {
                case '"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default:
                    if (character < 0x20)
                    {
                        output += "\\u00";
                        output.push_back(hex_digits[(character >> 4) & 0x0f]);
                        output.push_back(hex_digits[character & 0x0f]);
                    }
                    else
                    {
                        output.push_back(static_cast<char>(character));
                    }
                    break;
                }
            }
            output.push_back('"');
        }

        [[nodiscard]] static std::string_view trim(std::string_view text) noexcept
        {
            std::size_t first = 0;
            while (first < text.size() && is_space(text[first]))
                ++first;
            std::size_t last = text.size();
            while (last > first && is_space(text[last - 1]))
                --last;
            return text.substr(first, last - first);
        }

        template<class Argv>
        [[nodiscard]] command_result dispatch_argv(int argc, Argv argv) const
        {
            if (argc < 0)
                return usage_error("Process argument count cannot be negative");
            if (argc > 0 && argv == nullptr)
                return usage_error("Process argument array is null");

            std::vector<std::string_view> tokens;
            if (argc > 1)
                tokens.reserve(static_cast<std::size_t>(argc - 1));
            for (int i = 1; i < argc; ++i)
            {
                if (argv[i] == nullptr)
                    return usage_error("Process argument is null");
                tokens.emplace_back(argv[i]);
            }
            return dispatch_process_tokens(tokens);
        }

        [[nodiscard]] command_result dispatch_process_tokens(
            std::span<const std::string_view> tokens) const
        {
            if (!tokens.empty() && tokens.front() == "--console")
            {
                if (tokens.size() == 1)
                    return usage_error("'--console' requires a command string");
                if (tokens.size() == 2)
                    return dispatch(tokens[1]);
                return dispatch_tokens(tokens.subspan(1));
            }
            return dispatch_tokens(tokens);
        }

        [[nodiscard]] command_result dispatch_tokens(
            std::span<const std::string_view> tokens) const
        {
            if (tokens.empty())
                return { global_help(), 0, {} };

            if (is_help_token(tokens.front()))
            {
                if (tokens.size() == 1)
                    return { global_help(), 0, {} };
                if (tokens.size() != 2)
                    return usage_error("Usage: --help [command]");
                const auto* descriptor = m_commands->find(tokens[1]);
                if (!descriptor)
                    return usage_error("Unknown command '" + std::string(tokens[1]) +
                        "'. Run with --help to list available commands.");
                return { command_help(*descriptor), 0, {} };
            }

            const auto* descriptor = m_commands->find(tokens.front());
            if (!descriptor)
                return usage_error("Unknown command '" + std::string(tokens.front()) +
                    "'. Run with --help to list available commands.");

            bool help_option_allowed = true;
            for (std::size_t i = 1; i < tokens.size(); ++i)
            {
                if (tokens[i] == "--")
                {
                    help_option_allowed = false;
                    continue;
                }
                if (help_option_allowed && is_help_token(tokens[i]))
                    return { command_help(*descriptor), 0, {} };
            }

            const auto& specs = descriptor->arguments;
            std::vector<std::optional<argument_value>> values(specs.size());
            std::vector<std::size_t> positional_indices;
            positional_indices.reserve(specs.size());
            std::vector<bool> seen_flags(specs.size(), false);
            for (std::size_t i = 0; i < specs.size(); ++i)
            {
                if (specs[i].form == argument_form::flag)
                {
                    if (specs[i].default_value)
                        values[i] = *specs[i].default_value;
                }
                else
                {
                    positional_indices.push_back(i);
                }
            }

            std::size_t next_positional = 0;
            bool positional_only = false;
            for (std::size_t token_index = 1; token_index < tokens.size(); ++token_index)
            {
                const auto token = tokens[token_index];
                if (!positional_only && token == "--")
                {
                    positional_only = true;
                    continue;
                }

                if (!positional_only && token.starts_with("--"))
                {
                    const auto equals = token.find('=');
                    const auto option_name = token.substr(0, equals);
                    std::size_t flag_index = specs.size();
                    for (std::size_t i = 0; i < specs.size(); ++i)
                    {
                        if (specs[i].form == argument_form::flag &&
                            specs[i].option_name == option_name)
                        {
                            flag_index = i;
                            break;
                        }
                    }

                    if (flag_index == specs.size())
                        return usage_error("Unknown option '" + std::string(option_name) +
                            "' for command '" + descriptor->id + "'. Run '" +
                            descriptor->id + " --help' for usage.");
                    if (equals != std::string_view::npos)
                        return usage_error("Flag '" + std::string(option_name) +
                            "' does not take a value");
                    if (seen_flags[flag_index])
                        return usage_error("Flag '" + specs[flag_index].option_name +
                            "' was provided more than once");

                    seen_flags[flag_index] = true;
                    values[flag_index] = true;
                    continue;
                }

                if (next_positional >= positional_indices.size())
                    return usage_error("Unexpected argument '" + std::string(token) +
                        "' for command '" + descriptor->id + "'. Run '" +
                        descriptor->id + " --help' for usage.");

                const auto argument_index = positional_indices[next_positional++];
                std::string parse_error;
                auto value = parse_argument(specs[argument_index], token, parse_error);
                if (!value)
                    return usage_error("Command '" + descriptor->id + "': " + parse_error);
                values[argument_index] = std::move(*value);
            }

            for (std::size_t i = 0; i < specs.size(); ++i)
            {
                if (values[i])
                    continue;
                if (specs[i].default_value)
                {
                    values[i] = *specs[i].default_value;
                    continue;
                }
                if (specs[i].required)
                    return usage_error("Missing required argument '<" + specs[i].name +
                        ">' for command '" + descriptor->id + "'.\n" +
                        command_usage(*descriptor));
                return usage_error("Argument '" + specs[i].name +
                    "' has no value or default");
            }

            registry::value_list typed_values;
            typed_values.reserve(values.size());
            for (auto& value : values)
                typed_values.push_back(std::move(*value));

            try
            {
                return m_commands->invoke(descriptor->id, typed_values);
            }
            catch (const registry_error& error)
            {
                return usage_error(error.what());
            }
            catch (const std::exception& error)
            {
                return runtime_error("Command '" + descriptor->id + "' failed: " + error.what());
            }
            catch (...)
            {
                return runtime_error("Command '" + descriptor->id + "' failed with an unknown error");
            }
        }

        [[nodiscard]] std::string global_help() const
        {
            std::string output =
                "Usage:\n"
                "  <program> <command> [arguments]\n"
                "  <program> --json <command> [arguments]\n"
                "  <program> --console \"<command> [arguments]\"\n"
                "  <program> --console  (read command lines from stdin)\n"
                "  <program> --json --console  (emit JSON lines)\n"
                "  <program> --help [command]\n\n"
                "Available commands:\n";
            const auto descriptors = m_commands->commands();
            if (descriptors.empty())
            {
                output += "  (no commands registered)\n";
            }
            else
            {
                for (const auto& descriptor : descriptors)
                {
                    output += "  ";
                    output += descriptor.id;
                    output += " - ";
                    output += descriptor.description;
                    output += '\n';
                }
            }
            output += "\nUse '--help <command>' or '<command> --help' for command details.\n";
            output += "Prefix a process command with '--json' for JSON output.\n";
            return output;
        }

        [[nodiscard]] static std::string_view type_name(argument_type type) noexcept
        {
            switch (type)
            {
            case argument_type::string: return "string";
            case argument_type::integer: return "integer";
            case argument_type::floating_point: return "number";
            case argument_type::flag: return "flag";
            }
            return "unknown";
        }

        [[nodiscard]] static std::string command_usage(const command_descriptor& descriptor)
        {
            std::string output = "Usage: " + descriptor.id;
            for (const auto& arg : descriptor.arguments)
            {
                if (arg.form == argument_form::positional)
                {
                    output += arg.required ? " <" : " [<";
                    output += arg.name;
                    output += arg.required ? ">" : ">]";
                }
                else
                {
                    output += " [";
                    output += arg.option_name;
                    output += "]";
                }
            }
            output += " [--help]\n";
            return output;
        }

        [[nodiscard]] static std::string command_help(const command_descriptor& descriptor)
        {
            std::string output = descriptor.description + "\n" + command_usage(descriptor);
            bool has_positionals = false;
            for (const auto& arg : descriptor.arguments)
            {
                if (arg.form == argument_form::positional)
                    has_positionals = true;
            }

            if (has_positionals)
            {
                output += "\nArguments:\n";
                for (const auto& arg : descriptor.arguments)
                {
                    if (arg.form != argument_form::positional)
                        continue;
                    output += "  ";
                    output += arg.name;
                    output += " (";
                    output += type_name(arg.type);
                    output += arg.required ? ", required) - " : ") - ";
                    output += arg.description;
                    output += '\n';
                }
            }

            output += "\nOptions:\n";
            for (const auto& arg : descriptor.arguments)
            {
                if (arg.form != argument_form::flag)
                    continue;
                output += "  ";
                output += arg.option_name;
                output += " (default: ";
                const auto* default_value = arg.default_value
                    ? std::get_if<bool>(&*arg.default_value) : nullptr;
                output += default_value && *default_value ? "true" : "false";
                output += ") - ";
                output += arg.description;
                output += '\n';
            }
            output += "  --help, -h - Show this help\n";
            return output;
        }

        [[nodiscard]] static bool is_help_token(std::string_view token) noexcept
        {
            return token == "--help" || token == "-h";
        }

        [[nodiscard]] static bool is_space(char ch) noexcept
        {
            return ch == ' ' || ch == '\t' || ch == '\n' ||
                ch == '\r' || ch == '\f' || ch == '\v';
        }

        [[nodiscard]] static bool tokenize(std::string_view line,
            std::vector<std::string>& tokens, std::string& error)
        {
            std::string current;
            char quote = 0;
            bool escaped = false;
            bool token_started = false;

            for (std::size_t i = 0; i < line.size(); ++i)
            {
                const char ch = line[i];
                if (escaped)
                {
                    current += ch;
                    escaped = false;
                    token_started = true;
                    continue;
                }
                if (ch == '\\' && quote != '\'')
                {
                    if (i + 1 < line.size())
                    {
                        const char next = line[i + 1];
                        const bool escape_next = next == '\\' ||
                            (quote == '"' && next == '"') ||
                            (quote == 0 && (is_space(next) || next == '\'' || next == '"'));
                        if (escape_next)
                        {
                            escaped = true;
                            token_started = true;
                            continue;
                        }
                    }
                    current += ch;
                    token_started = true;
                    continue;
                }
                if (quote != 0)
                {
                    if (ch == quote)
                        quote = 0;
                    else
                        current += ch;
                    continue;
                }
                if (ch == '\'' || ch == '"')
                {
                    quote = ch;
                    token_started = true;
                    continue;
                }
                if (is_space(ch))
                {
                    if (token_started)
                    {
                        tokens.push_back(std::move(current));
                        current.clear();
                        token_started = false;
                    }
                    continue;
                }
                current += ch;
                token_started = true;
            }

            if (quote != 0)
            {
                error = "unclosed quoted argument";
                return false;
            }
            if (token_started)
                tokens.push_back(std::move(current));
            return true;
        }

        [[nodiscard]] static std::optional<argument_value> parse_argument(
            const argument_spec& spec, std::string_view token, std::string& error)
        {
            switch (spec.type)
            {
            case argument_type::string:
                return argument_value{ std::string(token) };
            case argument_type::integer:
            {
                std::int64_t value{};
                const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
                if (result.ec == std::errc::result_out_of_range)
                {
                    error = "argument '" + spec.name + "' is outside the supported 64-bit integer range";
                    return std::nullopt;
                }
                if (result.ec != std::errc{} || result.ptr != token.data() + token.size())
                {
                    error = "argument '" + spec.name + "' expects an integer, got '" +
                        std::string(token) + "'";
                    return std::nullopt;
                }
                return argument_value{ value };
            }
            case argument_type::floating_point:
            {
                double value{};
                const auto result = std::from_chars(token.data(), token.data() + token.size(),
                    value, std::chars_format::general);
                if (result.ec == std::errc::result_out_of_range)
                {
                    error = "argument '" + spec.name + "' is outside the supported floating-point range";
                    return std::nullopt;
                }
                if (result.ec != std::errc{} || result.ptr != token.data() + token.size() ||
                    !std::isfinite(value))
                {
                    error = "argument '" + spec.name + "' expects a finite number, got '" +
                        std::string(token) + "'";
                    return std::nullopt;
                }
                return argument_value{ value };
            }
            case argument_type::flag:
                error = "flag '" + spec.option_name + "' cannot be used as a positional argument";
                return std::nullopt;
            }
            error = "argument '" + spec.name + "' has an unsupported type";
            return std::nullopt;
        }

        [[nodiscard]] static command_result usage_error(std::string message)
        {
            if (message.empty() || message.back() != '\n')
                message += '\n';
            return { {}, 2, std::move(message) };
        }

        [[nodiscard]] static command_result runtime_error(std::string message)
        {
            if (message.empty() || message.back() != '\n')
                message += '\n';
            return { {}, 1, std::move(message) };
        }

        const registry* m_commands{};
    };
}
