#pragma once

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

namespace tree_practicum::python_bridge
{
	inline constexpr std::string_view protocol_version = "1";

	struct request
	{
		std::string request_id;
		std::string token;
		std::string operation;
		std::string tree_reference;
		std::string key;
	};

	struct response
	{
		std::string request_id;
		int exit_code = 0;
		std::string stdout_text;
		std::string stderr_text;
	};

	using fields = std::unordered_map<std::string, std::string>;

	[[nodiscard]] inline bool valid_request_id(
		std::string_view value) noexcept
	{
		return value.size() == 32 && std::all_of(
			value.begin(),
			value.end(),
			[](unsigned char character)
			{
				return (character >= '0' && character <= '9') ||
					(character >= 'a' && character <= 'f');
			});
	}

	[[nodiscard]] inline std::string hex_encode(std::string_view value)
	{
		constexpr char digits[] = "0123456789abcdef";
		std::string result;
		result.reserve(value.size() * 2);
		for (const unsigned char byte : value)
		{
			result.push_back(digits[byte >> 4]);
			result.push_back(digits[byte & 0x0f]);
		}
		return result;
	}

	[[nodiscard]] inline std::string hex_decode(std::string_view value)
	{
		if (value.size() % 2 != 0)
		{
			throw std::invalid_argument("Hex payload must have an even length.");
		}
		const auto digit = [](char character) -> unsigned char
		{
			if (character >= '0' && character <= '9')
				return static_cast<unsigned char>(character - '0');
			if (character >= 'a' && character <= 'f')
				return static_cast<unsigned char>(character - 'a' + 10);
			if (character >= 'A' && character <= 'F')
				return static_cast<unsigned char>(character - 'A' + 10);
			throw std::invalid_argument("Hex payload contains a non-hex digit.");
		};

		std::string result;
		result.reserve(value.size() / 2);
		for (std::size_t index = 0; index < value.size(); index += 2)
		{
			result.push_back(static_cast<char>(
				(digit(value[index]) << 4) | digit(value[index + 1])));
		}
		return result;
	}

	[[nodiscard]] inline fields parse_fields(std::string_view contents)
	{
		fields result;
		while (!contents.empty())
		{
			const auto line_end = contents.find('\n');
			auto line = contents.substr(0, line_end);
			if (!line.empty() && line.back() == '\r')
			{
				line.remove_suffix(1);
			}
			if (!line.empty())
			{
				const auto separator = line.find('=');
				if (separator == std::string_view::npos || separator == 0)
				{
					throw std::invalid_argument("Malformed bridge protocol field.");
				}
				const std::string name(line.substr(0, separator));
				if (!result.emplace(
					name,
					std::string(line.substr(separator + 1))).second)
				{
					throw std::invalid_argument("Duplicate bridge protocol field.");
				}
			}
			if (line_end == std::string_view::npos)
				break;
			contents.remove_prefix(line_end + 1);
		}
		return result;
	}

	[[nodiscard]] inline std::string_view require_field(
		const fields& values,
		std::string_view name)
	{
		const auto found = values.find(std::string(name));
		if (found == values.end())
		{
			throw std::invalid_argument(
				"Missing bridge protocol field: " + std::string(name));
		}
		return found->second;
	}

	inline void validate_request_id(std::string_view value)
	{
		if (!valid_request_id(value))
		{
			throw std::invalid_argument("Invalid bridge request ID.");
		}
	}

	[[nodiscard]] inline std::string encode_request(const request& value)
	{
		validate_request_id(value.request_id);
		return "protocol=" + std::string(protocol_version) + "\n" +
			"request_id=" + value.request_id + "\n" +
			"token=" + value.token + "\n" +
			"operation=" + value.operation + "\n" +
			"tree_reference_hex=" + hex_encode(value.tree_reference) + "\n" +
			"key_hex=" + hex_encode(value.key) + "\n";
	}

	[[nodiscard]] inline request decode_request(
		std::string_view contents,
		std::string_view expected_token)
	{
		const auto values = parse_fields(contents);
		if (require_field(values, "protocol") != protocol_version)
		{
			throw std::invalid_argument("Unsupported bridge protocol version.");
		}
		request result;
		result.request_id = require_field(values, "request_id");
		validate_request_id(result.request_id);
		result.token = require_field(values, "token");
		if (result.token != expected_token)
		{
			throw std::invalid_argument("Bridge session token does not match.");
		}
		result.operation = require_field(values, "operation");
		result.tree_reference = hex_decode(
			require_field(values, "tree_reference_hex"));
		result.key = hex_decode(require_field(values, "key_hex"));
		return result;
	}

	[[nodiscard]] inline std::string encode_response(
		const response& value,
		std::string_view token)
	{
		validate_request_id(value.request_id);
		return "protocol=" + std::string(protocol_version) + "\n" +
			"request_id=" + value.request_id + "\n" +
			"token=" + std::string(token) + "\n" +
			"exit_code=" + std::to_string(value.exit_code) + "\n" +
			"stdout_hex=" + hex_encode(value.stdout_text) + "\n" +
			"stderr_hex=" + hex_encode(value.stderr_text) + "\n";
	}

	[[nodiscard]] inline response decode_response(
		std::string_view contents,
		std::string_view expected_request_id,
		std::string_view expected_token)
	{
		validate_request_id(expected_request_id);
		const auto values = parse_fields(contents);
		if (require_field(values, "protocol") != protocol_version)
		{
			throw std::invalid_argument("Unsupported bridge protocol version.");
		}
		if (require_field(values, "request_id") != expected_request_id)
		{
			throw std::invalid_argument("Bridge response request ID does not match.");
		}
		if (require_field(values, "token") != expected_token)
		{
			throw std::invalid_argument("Bridge response session token does not match.");
		}

		response result;
		result.request_id = expected_request_id;
		const auto exit_code = require_field(values, "exit_code");
		const auto [end, error] = std::from_chars(
			exit_code.data(),
			exit_code.data() + exit_code.size(),
			result.exit_code);
		if (error != std::errc{} || end != exit_code.data() + exit_code.size())
		{
			throw std::invalid_argument("Invalid bridge response exit code.");
		}
		result.stdout_text = hex_decode(require_field(values, "stdout_hex"));
		result.stderr_text = hex_decode(require_field(values, "stderr_hex"));
		return result;
	}
}
