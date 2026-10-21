#include <MNGL/console.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace
{
    struct command_state
    {
        std::int64_t echo_calls{};
        std::int64_t sum_calls{};
    };
}

int main(int argc, char** argv)
{
    command_state state;
    mngl::console::registry commands;

    commands.add_command("echo", "Print text; --upper converts ASCII letters to uppercase")
        .argument<std::string>("text", "Text to print")
        .flag("upper", "Convert ASCII letters to uppercase")
        .handler([&state](const std::string& text, bool upper)
        {
            ++state.echo_calls;
            auto output = text;
            if (upper)
            {
                for (char& character : output)
                {
                    if (character >= 'a' && character <= 'z')
                        character = static_cast<char>(character - 'a' + 'A');
                }
            }
            output.push_back('\n');
            return output;
        });

    commands.add_command("sum", "Add two signed 64-bit integers")
        .argument<std::int64_t>("left", "First addend")
        .argument<std::int64_t>("right", "Second addend")
        .handler([&state](std::int64_t left, std::int64_t right)
            -> mngl::console::command_result
        {
            if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) ||
                (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right))
            {
                return { {}, 2, "sum is outside the int64 range\n" };
            }

            ++state.sum_calls;
            return { std::to_string(left + right) + '\n', 0, {} };
        });

    commands.add_command("status", "Show command counters for this process")
        .handler([&state]
        {
            return "echo_calls=" + std::to_string(state.echo_calls) +
                " sum_calls=" + std::to_string(state.sum_calls) + '\n';
        });

    return mngl::console::dispatcher{ commands }.run_process(argc, argv);
}
