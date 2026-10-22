#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <utility>

namespace mngl::mods::command_catalog
{
	struct parameter
	{
		std::string name;
		std::string label;
		std::string type;
		std::string hint;
	};

	struct command
	{
		std::string algorithm_id;
		std::string name;
		std::string signature;
		std::string description;
		std::vector<parameter> parameters;
	};

	struct catalog
	{
		std::uint64_t schema_version = 0;
		std::vector<command> commands;
	};

	namespace detail
	{
		struct json_value
		{
			using object = std::map<std::string, json_value>;
			using array = std::vector<json_value>;
			using value_type = std::variant<
				std::nullptr_t,
				bool,
				std::uint64_t,
				std::int64_t,
				double,
				std::string,
				array,
				object>;

			value_type value;
		};

		class parser
		{
		public:
			explicit parser(std::string_view source) : m_source(source) {}

			[[nodiscard]] json_value parse()
			{
				auto result = parse_value();
				skip_space();
				if (m_position != m_source.size())
				{
					fail("unexpected data after JSON value");
				}
				return result;
			}

		private:
			[[noreturn]] void fail(std::string_view message) const
			{
				throw std::runtime_error(
					"invalid command catalog JSON at byte " +
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

			bool take(char expected)
			{
				if (m_position < m_source.size() &&
					m_source[m_position] == expected)
				{
					++m_position;
					return true;
				}
				return false;
			}

			void expect(char expected)
			{
				if (!take(expected))
				{
					fail(std::string("expected '") + expected + "'");
				}
			}

			static void append_utf8(std::string& output, std::uint32_t codepoint)
			{
				if (codepoint <= 0x7f)
				{
					output.push_back(static_cast<char>(codepoint));
				}
				else if (codepoint <= 0x7ff)
				{
					output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
					output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
				}
				else if (codepoint <= 0xffff)
				{
					output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
					output.push_back(static_cast<char>(
						0x80 | ((codepoint >> 6) & 0x3f)));
					output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
				}
				else
				{
					output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
					output.push_back(static_cast<char>(
						0x80 | ((codepoint >> 12) & 0x3f)));
					output.push_back(static_cast<char>(
						0x80 | ((codepoint >> 6) & 0x3f)));
					output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
				}
			}

			[[nodiscard]] std::uint32_t parse_hex4()
			{
				if (m_position + 4 > m_source.size())
				{
					fail("incomplete Unicode escape");
				}
				std::uint32_t result = 0;
				for (int i = 0; i < 4; ++i)
				{
					const char digit = m_source[m_position++];
					result <<= 4;
					if (digit >= '0' && digit <= '9')
					{
						result |= static_cast<std::uint32_t>(digit - '0');
					}
					else if (digit >= 'a' && digit <= 'f')
					{
						result |= static_cast<std::uint32_t>(digit - 'a' + 10);
					}
					else if (digit >= 'A' && digit <= 'F')
					{
						result |= static_cast<std::uint32_t>(digit - 'A' + 10);
					}
					else
					{
						fail("invalid Unicode escape");
					}
				}
				return result;
			}

			[[nodiscard]] std::string parse_string()
			{
				expect('"');
				std::string result;
				while (m_position < m_source.size())
				{
					const auto character = static_cast<unsigned char>(
						m_source[m_position++]);
					if (character == '"')
					{
						return result;
					}
					if (character < 0x20)
					{
						fail("unescaped control character in string");
					}
					if (character != '\\')
					{
						result.push_back(static_cast<char>(character));
						continue;
					}
					if (m_position == m_source.size())
					{
						fail("incomplete string escape");
					}
					switch (m_source[m_position++])
					{
					case '"': result.push_back('"'); break;
					case '\\': result.push_back('\\'); break;
					case '/': result.push_back('/'); break;
					case 'b': result.push_back('\b'); break;
					case 'f': result.push_back('\f'); break;
					case 'n': result.push_back('\n'); break;
					case 'r': result.push_back('\r'); break;
					case 't': result.push_back('\t'); break;
					case 'u':
					{
						auto codepoint = parse_hex4();
						if (codepoint >= 0xd800 && codepoint <= 0xdbff)
						{
							if (m_position + 2 > m_source.size() ||
								m_source[m_position] != '\\' ||
								m_source[m_position + 1] != 'u')
							{
								fail("missing low surrogate");
							}
							m_position += 2;
						const auto low = parse_hex4();
						if (low < 0xdc00 || low > 0xdfff)
							fail("invalid low surrogate");
							codepoint = 0x10000 +
								((codepoint - 0xd800) << 10) + (low - 0xdc00);
						}
						else if (codepoint >= 0xdc00 && codepoint <= 0xdfff)
						{
							fail("unexpected low surrogate");
						}
						append_utf8(result, codepoint);
						break;
					}
					default: fail("unknown string escape");
					}
				}
				fail("unterminated string");
			}

			[[nodiscard]] json_value parse_object()
			{
				expect('{');
				json_value::object result;
				skip_space();
				if (take('}')) return { std::move(result) };
				while (true)
				{
					skip_space();
					auto key = parse_string();
					skip_space();
					expect(':');
					skip_space();
					auto value = parse_value();
					if (!result.emplace(std::move(key), std::move(value)).second)
					{
						fail("duplicate object key");
					}
					skip_space();
					if (take('}')) break;
					expect(',');
				}
				return { std::move(result) };
			}

			[[nodiscard]] json_value parse_array()
			{
				expect('[');
				json_value::array result;
				skip_space();
				if (take(']')) return { std::move(result) };
				while (true)
				{
					skip_space();
					result.push_back(parse_value());
					skip_space();
					if (take(']')) break;
					expect(',');
				}
				return { std::move(result) };
			}

			[[nodiscard]] json_value parse_value()
			{
				skip_space();
				if (m_position == m_source.size()) fail("expected JSON value");
				const char character = m_source[m_position];
				if (character == '"') return { parse_string() };
				if (character == '{') return parse_object();
				if (character == '[') return parse_array();
				if (character == 't' && m_source.substr(m_position, 4) == "true")
				{
					m_position += 4;
					return { true };
				}
				if (character == 'f' && m_source.substr(m_position, 5) == "false")
				{
					m_position += 5;
					return { false };
				}
				if (character == 'n' && m_source.substr(m_position, 4) == "null")
				{
					m_position += 4;
					return { nullptr };
				}
				if ((character < '0' || character > '9') && character != '-')
					fail("expected a JSON value");

				const std::size_t start = m_position;
				const bool negative = character == '-';
				if (negative) ++m_position;
				if (m_position == m_source.size() ||
					m_source[m_position] < '0' || m_source[m_position] > '9')
					fail("invalid JSON number");
				if (m_source[m_position] == '0')
				{
					++m_position;
					if (m_position < m_source.size() &&
						m_source[m_position] >= '0' && m_source[m_position] <= '9')
						fail("JSON numbers cannot contain leading zeroes");
				}
				else
				{
					while (m_position < m_source.size() &&
						m_source[m_position] >= '0' && m_source[m_position] <= '9')
						++m_position;
				}
				bool floating_point = false;
				if (m_position < m_source.size() && m_source[m_position] == '.')
				{
					floating_point = true;
					++m_position;
					const auto fraction_start = m_position;
					while (m_position < m_source.size() &&
						m_source[m_position] >= '0' && m_source[m_position] <= '9')
						++m_position;
					if (fraction_start == m_position)
						fail("JSON fraction requires at least one digit");
				}
				if (m_position < m_source.size() &&
					(m_source[m_position] == 'e' || m_source[m_position] == 'E'))
				{
					floating_point = true;
					++m_position;
					if (m_position < m_source.size() &&
						(m_source[m_position] == '+' || m_source[m_position] == '-'))
						++m_position;
					const auto exponent_start = m_position;
					while (m_position < m_source.size() &&
						m_source[m_position] >= '0' && m_source[m_position] <= '9')
						++m_position;
					if (exponent_start == m_position)
						fail("JSON exponent requires at least one digit");
				}

				const auto token = m_source.substr(start, m_position - start);
				if (floating_point)
				{
					double number = 0.0;
					const auto parsed = std::from_chars(
						token.data(), token.data() + token.size(), number,
						std::chars_format::general);
					if (parsed.ec != std::errc{} ||
						parsed.ptr != token.data() + token.size() ||
						!std::isfinite(number))
						fail("JSON floating-point number is out of range");
					return { number };
				}
				if (negative)
				{
					std::int64_t number = 0;
					const auto parsed = std::from_chars(
						token.data(), token.data() + token.size(), number);
					if (parsed.ec != std::errc{} ||
						parsed.ptr != token.data() + token.size())
						fail("JSON signed integer is out of range");
					return { number };
				}
				std::uint64_t number = 0;
				const auto parsed = std::from_chars(
					token.data(), token.data() + token.size(), number);
				if (parsed.ec != std::errc{} ||
					parsed.ptr != token.data() + token.size())
					fail("JSON unsigned integer is out of range");
				return { number };
			}

			std::string_view m_source;
			std::size_t m_position = 0;
		};
	}

	[[nodiscard]] inline catalog parse(std::string_view source)
	{
		const auto root_value = detail::parser(source).parse();
		const auto* root = std::get_if<detail::json_value::object>(
			&root_value.value);
		if (!root || root->size() != 2)
		{
			throw std::runtime_error(
				"command catalog must have schema_version and commands fields");
		}
		const auto version_it = root->find("schema_version");
		const auto commands_it = root->find("commands");
		if (version_it == root->end() || commands_it == root->end())
		{
			throw std::runtime_error(
				"command catalog is missing required fields");
		}
		const auto* version = std::get_if<std::uint64_t>(
			&version_it->second.value);
		const auto* commands = std::get_if<detail::json_value::array>(
			&commands_it->second.value);
		if (!version || *version != 1 || !commands)
		{
			throw std::runtime_error(
				"unsupported command catalog schema or invalid commands array");
		}

		catalog result;
		result.schema_version = *version;
		if (commands->size() > 4096)
		{
			throw std::runtime_error("command catalog contains too many entries");
		}
		result.commands.reserve(commands->size());
		for (const auto& item : *commands)
		{
			const auto* object = std::get_if<detail::json_value::object>(
				&item.value);
			if (!object || object->size() < 4 || object->size() > 5)
			{
				throw std::runtime_error(
					"each command must have algorithm_id, name, signature, description, and optional parameters fields");
			}
			for (const auto& [field, _] : *object)
			{
				if (field != "algorithm_id" && field != "name" &&
					field != "signature" && field != "description" &&
					field != "parameters")
				{
					throw std::runtime_error(
						"unknown command catalog field " + field);
				}
			}
			const auto get_string = [&](std::string_view field) -> std::string
			{
				const auto found = object->find(std::string(field));
				if (found == object->end())
				{
					throw std::runtime_error(
						"command catalog entry is missing " + std::string(field));
				}
				const auto* value = std::get_if<std::string>(&found->second.value);
				if (!value || value->empty())
				{
					throw std::runtime_error(
						"command catalog field " + std::string(field) +
						" must be a non-empty string");
				}
				return *value;
			};
			command parsed{
				get_string("algorithm_id"),
				get_string("name"),
				get_string("signature"),
				get_string("description"),
				{} };
			if (const auto parameters_it = object->find("parameters");
				parameters_it != object->end())
			{
				const auto* parameters = std::get_if<detail::json_value::array>(
					&parameters_it->second.value);
				if (!parameters || parameters->size() > 16)
				{
					throw std::runtime_error(
						"command parameters must be an array with at most 16 entries");
				}
				parsed.parameters.reserve(parameters->size());
				for (const auto& parameter_value : *parameters)
				{
					const auto* parameter_object =
						std::get_if<detail::json_value::object>(
							&parameter_value.value);
					if (!parameter_object || parameter_object->size() < 3 ||
						parameter_object->size() > 4)
					{
						throw std::runtime_error(
							"each command parameter must have name, label, type, and optional hint fields");
					}
					for (const auto& [field, _] : *parameter_object)
					{
						if (field != "name" && field != "label" &&
							field != "type" && field != "hint")
						{
							throw std::runtime_error(
								"unknown command parameter field " + field);
						}
					}
					const auto get_parameter_string = [&](std::string_view field,
						bool required) -> std::string
					{
						const auto found = parameter_object->find(std::string(field));
						if (found == parameter_object->end())
						{
							if (!required) return {};
							throw std::runtime_error(
								"command parameter is missing " + std::string(field));
						}
						const auto* value = std::get_if<std::string>(
							&found->second.value);
						if (!value || (required && value->empty()))
						{
							throw std::runtime_error(
								"command parameter field " + std::string(field) +
								" must be a non-empty string");
						}
						return *value;
					};
					parameter parsed_parameter{
						get_parameter_string("name", true),
						get_parameter_string("label", true),
						get_parameter_string("type", true),
						get_parameter_string("hint", false) };
					if (parsed_parameter.type != "text" &&
						parsed_parameter.type != "bit_string" &&
						parsed_parameter.type != "int64" &&
						parsed_parameter.type != "double" &&
						parsed_parameter.type != "boolean")
					{
						throw std::runtime_error(
							"unsupported command parameter type " +
							parsed_parameter.type);
					}
					if (std::any_of(parsed.parameters.begin(), parsed.parameters.end(),
						[&](const auto& existing)
						{
							return existing.name == parsed_parameter.name;
						}))
					{
						throw std::runtime_error(
							"command parameter names must be unique");
					}
					parsed.parameters.push_back(std::move(parsed_parameter));
				}
			}
			result.commands.push_back(std::move(parsed));
		}
		return result;
	}
}
