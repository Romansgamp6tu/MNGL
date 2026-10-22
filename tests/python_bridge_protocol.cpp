#include <tree_python_bridge.hpp>

#include <array>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	using namespace tree_practicum::python_bridge;

	constexpr std::string_view valid_id =
		"0123456789abcdef0123456789abcdef";
	constexpr std::string_view valid_token = "a1b2c3d4";

	void require(bool condition, std::string_view message)
	{
		if (!condition)
		{
			throw std::runtime_error(std::string(message));
		}
	}

	template<typename Action>
	void require_throws(Action&& action, std::string_view message)
	{
		try
		{
			std::forward<Action>(action)();
		}
		catch (const std::exception&)
		{
			return;
		}
		throw std::runtime_error(std::string(message));
	}

	void test_request_id_validation()
	{
		require(valid_request_id(valid_id), "Expected UUID hex request ID to be valid");
		require(!valid_request_id("short"), "Short request IDs must be rejected");
		require(
			!valid_request_id("0123456789abcdef0123456789abcdeg"),
			"Request IDs containing non-hex digits must be rejected");
		require(
			!valid_request_id("0123456789ABCDEF0123456789ABCDEF"),
			"Uppercase request IDs must be rejected");
	}

	void test_hex_round_trip_binary()
	{
		const std::string binary{ '\0', '\x01', '\x7f', '\x80', '\xff' };
		require(hex_decode(hex_encode(binary)) == binary,
			"Hex encoding must round-trip arbitrary bytes");
	}

	void test_hex_rejects_odd_length()
	{
		require_throws(
			[] { (void)hex_decode("abc"); },
			"Hex decoder must reject odd-length input");
	}

	void test_hex_rejects_non_hex()
	{
		require_throws(
			[] { (void)hex_decode("0x"); },
			"Hex decoder must reject non-hex digits");
	}

	void test_field_parser_accepts_crlf_and_equals_in_values()
	{
		const auto parsed = parse_fields("key=value=with=equals\r\nnext=ok\r\n");
		require(parsed.at("key") == "value=with=equals",
			"Field parser must split on the first equals sign only");
		require(parsed.at("next") == "ok", "Field parser must accept CRLF lines");
	}

	void test_field_parser_rejects_duplicate_fields()
	{
		require_throws(
			[] { (void)parse_fields("key=one\nkey=two\n"); },
			"Field parser must reject duplicate field names");
	}

	void test_field_parser_rejects_malformed_fields()
	{
		require_throws(
			[] { (void)parse_fields("missing-separator\n"); },
			"Field parser must reject lines without separators");
		require_throws(
			[] { (void)parse_fields("=empty-name\n"); },
			"Field parser must reject empty field names");
	}

	void test_request_round_trip_utf8_and_delimiters()
	{
		const request original{
			std::string(valid_id),
			std::string(valid_token),
			"insert",
			R"(C:\Деревья\file=name.tree)",
			"ключ\nс переводом строки"
		};
		const auto decoded = decode_request(
			encode_request(original),
			valid_token);
		require(decoded.request_id == original.request_id,
			"Request ID must survive protocol round trip");
		require(decoded.operation == original.operation,
			"Operation must survive protocol round trip");
		require(decoded.tree_reference == original.tree_reference,
			"UTF-8 tree path must survive protocol round trip");
		require(decoded.key == original.key,
			"Multiline key payload must survive protocol round trip");
	}

	void test_request_rejects_wrong_token()
	{
		const request value{
			std::string(valid_id),
			std::string(valid_token),
			"find",
			"tree.tree",
			"12"
		};
		require_throws(
			[&] { (void)decode_request(encode_request(value), "wrong-token"); },
			"Request decoder must reject a mismatched session token");
	}

	void test_request_rejects_unsupported_version()
	{
		const request value{
			std::string(valid_id),
			std::string(valid_token),
			"find",
			"tree.tree",
			"12"
		};
		auto encoded = encode_request(value);
		encoded.replace(9, 1, "2");
		require_throws(
			[&] { (void)decode_request(encoded, valid_token); },
			"Request decoder must reject unknown protocol versions");
	}

	void test_request_rejects_invalid_id_and_missing_fields()
	{
		auto encoded = encode_request(request{
			std::string(valid_id), std::string(valid_token), "find", "tree", "1" });
		const auto id_start = encoded.find("request_id=") + 11;
		encoded.replace(id_start, 32, "not-an-id");
		require_throws(
			[&] { (void)decode_request(encoded, valid_token); },
			"Request decoder must reject invalid request IDs");
		require_throws(
			[] { (void)decode_request("protocol=1\n", valid_token); },
			"Request decoder must reject requests with missing fields");
	}

	void test_request_rejects_bad_hex_payload()
	{
		auto encoded = encode_request(request{
			std::string(valid_id), std::string(valid_token), "find", "tree", "1" });
		const auto key_start = encoded.find("key_hex=") + 8;
		encoded.replace(key_start, 2, "1");
		require_throws(
			[&] { (void)decode_request(encoded, valid_token); },
			"Request decoder must reject malformed hex payloads");
	}

	void test_request_encoder_rejects_invalid_id()
	{
		require_throws(
			[]
			{
				(void)encode_request(request{
					"not-a-request-id", std::string(valid_token),
					"find", "tree.tree", "1" });
			},
			"Request encoder must reject invalid request IDs");
	}

	void test_response_round_trip_unicode_and_error()
	{
		const response original{
			std::string(valid_id),
			2,
			"operation=insert\nключ=42\n",
			"ошибка\nподробности"
		};
		const auto decoded = decode_response(
			encode_response(original, valid_token),
			valid_id,
			valid_token);
		require(decoded.exit_code == original.exit_code,
			"Response exit code must survive protocol round trip");
		require(decoded.stdout_text == original.stdout_text,
			"UTF-8 stdout must survive protocol round trip");
		require(decoded.stderr_text == original.stderr_text,
			"UTF-8 stderr must survive protocol round trip");
	}

	void test_response_rejects_mismatched_id_and_token()
	{
		const response value{ std::string(valid_id), 0, "", "" };
		const auto encoded = encode_response(value, valid_token);
		require_throws(
			[&] { (void)decode_response(encoded, "ffffffffffffffffffffffffffffffff", valid_token); },
			"Response decoder must reject a mismatched request ID");
		require_throws(
			[&] { (void)decode_response(encoded, valid_id, "wrong-token"); },
			"Response decoder must reject a mismatched token");
	}

	void test_response_rejects_invalid_exit_code()
	{
		const auto encoded =
			"protocol=1\nrequest_id=" + std::string(valid_id) +
			"\ntoken=" + std::string(valid_token) +
			"\nexit_code=nope\nstdout_hex=\nstderr_hex=\n";
		require_throws(
			[&] { (void)decode_response(encoded, valid_id, valid_token); },
			"Response decoder must reject non-integer exit codes");
	}

	void test_response_rejects_unsupported_version()
	{
		auto encoded = encode_response(
			response{ std::string(valid_id), 0, "", "" },
			valid_token);
		encoded.replace(9, 1, "9");
		require_throws(
			[&] { (void)decode_response(encoded, valid_id, valid_token); },
			"Response decoder must reject unknown protocol versions");
	}

	void test_response_rejects_invalid_payload_hex()
	{
		auto encoded = encode_response(
			response{ std::string(valid_id), 0, "", "" },
			valid_token);
		const auto payload_start = encoded.find("stdout_hex=") + 11;
		encoded.insert(payload_start, "x0");
		require_throws(
			[&] { (void)decode_response(encoded, valid_id, valid_token); },
			"Response decoder must reject invalid stdout hex");
	}

	void test_response_rejects_missing_fields_and_trailing_exit_code_data()
	{
		const auto valid = encode_response(
			response{ std::string(valid_id), 0, "", "" },
			valid_token);
		const auto stderr_field = valid.find("stderr_hex=");
		auto missing_field = valid;
		missing_field.erase(stderr_field);
		require_throws(
			[&] { (void)decode_response(missing_field, valid_id, valid_token); },
			"Response decoder must reject missing fields");

		auto trailing_data = valid;
		const auto code_start = trailing_data.find("exit_code=") + 10;
		trailing_data.replace(code_start, 1, "0x");
		require_throws(
			[&] { (void)decode_response(trailing_data, valid_id, valid_token); },
			"Response decoder must reject trailing exit-code data");
	}

	using test_case = std::pair<std::string_view, std::function<void()>>;
}

int main()
{
	const std::array<test_case, 19> tests{{
		{ "request ID validation", test_request_id_validation },
		{ "binary hex round trip", test_hex_round_trip_binary },
		{ "odd hex length", test_hex_rejects_odd_length },
		{ "non-hex payload", test_hex_rejects_non_hex },
		{ "CRLF and equals field parsing", test_field_parser_accepts_crlf_and_equals_in_values },
		{ "duplicate fields", test_field_parser_rejects_duplicate_fields },
		{ "malformed fields", test_field_parser_rejects_malformed_fields },
		{ "UTF-8 request round trip", test_request_round_trip_utf8_and_delimiters },
		{ "wrong request token", test_request_rejects_wrong_token },
		{ "unsupported request version", test_request_rejects_unsupported_version },
		{ "invalid request ID and missing fields", test_request_rejects_invalid_id_and_missing_fields },
		{ "malformed request hex", test_request_rejects_bad_hex_payload },
		{ "request encoder ID validation", test_request_encoder_rejects_invalid_id },
		{ "UTF-8 response round trip", test_response_round_trip_unicode_and_error },
		{ "response identity and token", test_response_rejects_mismatched_id_and_token },
		{ "response exit-code validation", test_response_rejects_invalid_exit_code },
		{ "unsupported response version", test_response_rejects_unsupported_version },
		{ "malformed response hex", test_response_rejects_invalid_payload_hex },
		{ "missing fields and trailing exit-code data", test_response_rejects_missing_fields_and_trailing_exit_code_data }
	}};

	std::size_t failures = 0;
	for (const auto& [name, action] : tests)
	{
		try
		{
			action();
			std::cout << "[PASS] " << name << '\n';
		}
		catch (const std::exception& error)
		{
			++failures;
			std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
		}
	}
	std::cout << tests.size() - failures << '/' << tests.size()
		<< " protocol test groups passed\n";
	return failures == 0 ? 0 : 1;
}
