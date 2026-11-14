#pragma once

#include "map.hpp"
#include "../atomic_file.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

namespace mngl::detail
{
	inline constexpr std::uintmax_t action_map_profile_max_bytes = 16u * 1024u * 1024u;
	inline constexpr std::string_view action_map_profile_header = "MNGL_ACTION_MAP_PROFILE 1";

	inline void append_action_profile_name(std::string& output, std::string_view name)
	{
		constexpr char hex[] = "0123456789ABCDEF";
		for (const char raw : name)
		{
			const auto byte = static_cast<std::uint8_t>(raw);
			if (byte >= 0x20u && byte <= 0x7Eu && byte != '%')
			{
				output.push_back(raw);
			}
			else
			{
				output.push_back('%');
				output.push_back(hex[byte >> 4]);
				output.push_back(hex[byte & 0x0Fu]);
			}
		}
	}

	[[nodiscard]] inline std::string decode_action_profile_name(std::string_view encoded,
	                                                            std::size_t line_number)
	{
		constexpr auto hex_value = [](char digit) -> int
		{
			if (digit >= '0' && digit <= '9')
				return digit - '0';
			if (digit >= 'A' && digit <= 'F')
				return digit - 'A' + 10;
			if (digit >= 'a' && digit <= 'f')
				return digit - 'a' + 10;
			return -1;
		};

		std::string result;
		result.reserve(encoded.size());
		for (std::size_t offset = 0; offset < encoded.size(); ++offset)
		{
			if (encoded[offset] != '%')
			{
				result.push_back(encoded[offset]);
				continue;
			}
			if (offset + 2 >= encoded.size()) [[unlikely]]
			{
				throw std::invalid_argument("Invalid action map profile escape on line " +
				                            std::to_string(line_number));
			}
			const int high = hex_value(encoded[offset + 1]);
			const int low = hex_value(encoded[offset + 2]);
			if (high < 0 || low < 0) [[unlikely]]
			{
				throw std::invalid_argument("Invalid action map profile escape on line " +
				                            std::to_string(line_number));
			}
			result.push_back(static_cast<char>((high << 4) | low));
			offset += 2;
		}
		return result;
	}

	template <typename Integer>
	inline void append_action_profile_number(std::string& output, Integer value)
	{
		char buffer[64]{};
		const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
		if (result.ec != std::errc{}) [[unlikely]]
		{
			throw std::runtime_error("Failed to serialize an action map profile number");
		}
		output.append(buffer, static_cast<std::size_t>(result.ptr - buffer));
	}

	inline void append_action_profile_number(std::string& output, float value)
	{
		char buffer[64]{};
		const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general,
		                                  std::numeric_limits<float>::max_digits10);
		if (result.ec != std::errc{}) [[unlikely]]
		{
			throw std::runtime_error("Failed to serialize an action map profile number");
		}
		output.append(buffer, static_cast<std::size_t>(result.ptr - buffer));
	}

	template <typename Enum>
	[[nodiscard]] inline std::uint64_t action_profile_enum_value(Enum value)
	{
		using underlying_type = std::underlying_type_t<Enum>;
		const auto raw = static_cast<underlying_type>(value);
		if constexpr (std::is_signed_v<underlying_type>)
		{
			if (raw < 0) [[unlikely]]
			{
				throw std::invalid_argument("Action map profile enum values must be non-negative");
			}
		}
		return static_cast<std::uint64_t>(raw);
	}

	template <typename Device>
	[[nodiscard]] inline std::optional<std::uint64_t> action_profile_device_value(
	    const std::optional<Device>& value)
	{
		if (!value)
		{
			return std::nullopt;
		}
		if constexpr (std::is_signed_v<Device>)
		{
			if (*value < 0) [[unlikely]]
			{
				throw std::invalid_argument("Action map profile device IDs must be non-negative");
			}
		}
		return static_cast<std::uint64_t>(*value);
	}

	[[nodiscard]] inline bool action_profile_uses_axis_parameters(std::string_view type) noexcept
	{
		return type == "mouse_axis" || type == "joystick_axis" || type == "gamepad_axis" ||
		       type == "touch_pressure" || type == "pen_axis";
	}

	struct action_profile_binding
	{
		std::string_view type;
		std::uint64_t control{};
		std::optional<std::uint64_t> device;
		float scale{1.0f};
		std::optional<float> dead_zone;
		std::optional<float> threshold;
	};

	[[nodiscard]] inline std::uint64_t parse_action_profile_unsigned(std::string_view value,
	                                                                 std::size_t line_number)
	{
		if (value.empty()) [[unlikely]]
		{
			throw std::invalid_argument("Missing integer in action map profile line " +
			                            std::to_string(line_number));
		}
		std::uint64_t result = 0;
		const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
		if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) [[unlikely]]
		{
			throw std::invalid_argument("Invalid integer in action map profile line " +
			                            std::to_string(line_number));
		}
		return result;
	}

	[[nodiscard]] inline float parse_action_profile_float(std::string_view value, std::size_t line_number)
	{
		if (value.empty()) [[unlikely]]
		{
			throw std::invalid_argument("Missing number in action map profile line " +
			                            std::to_string(line_number));
		}
		float result = 0.0f;
		const auto parsed =
		    std::from_chars(value.data(), value.data() + value.size(), result, std::chars_format::general);
		if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !std::isfinite(result))
		    [[unlikely]]
		{
			throw std::invalid_argument("Invalid number in action map profile line " +
			                            std::to_string(line_number));
		}
		return result;
	}

	template <typename Enum>
	[[nodiscard]] inline Enum action_profile_enum(std::uint64_t value)
	{
		using underlying_type = std::underlying_type_t<Enum>;
		if (value > static_cast<std::uint64_t>(std::numeric_limits<underlying_type>::max())) [[unlikely]]
		{
			throw std::invalid_argument("Action map profile enum value is out of range");
		}
		return static_cast<Enum>(static_cast<underlying_type>(value));
	}

	template <typename Integer>
	[[nodiscard]] inline Integer action_profile_integer(std::uint64_t value)
	{
		if (value > static_cast<std::uint64_t>(std::numeric_limits<Integer>::max())) [[unlikely]]
		{
			throw std::invalid_argument("Action map profile integer is out of range");
		}
		return static_cast<Integer>(value);
	}

	template <typename Device>
	[[nodiscard]] inline std::optional<Device> action_profile_device(
	    const std::optional<std::uint64_t>& value)
	{
		if (!value)
		{
			return std::nullopt;
		}
		if (*value > static_cast<std::uint64_t>(std::numeric_limits<Device>::max())) [[unlikely]]
		{
			throw std::invalid_argument("Action map profile device ID is out of range");
		}
		return static_cast<Device>(*value);
	}

	inline void apply_action_profile_binding(action_map& map, const action_profile_binding& binding,
	                                         std::string_view action, std::size_t line_number)
	{
		const float dead_zone = binding.dead_zone.value_or(0.0f);
		const float threshold = binding.threshold.value_or(0.5f);
		try
		{
			if (binding.type == "key")
			{
				map.bind_key(action, action_profile_enum<scancode>(binding.control),
				             action_profile_device<SDL_KeyboardID>(binding.device), binding.scale);
			}
			else if (binding.type == "mouse_button")
			{
				map.bind_mouse_button(action, action_profile_enum<mouse_button>(binding.control),
				                      action_profile_device<SDL_MouseID>(binding.device), binding.scale);
			}
			else if (binding.type == "mouse_axis")
			{
				map.bind_mouse_axis(action, action_profile_enum<mouse_axis>(binding.control), binding.scale,
				                    dead_zone, threshold, action_profile_device<SDL_MouseID>(binding.device));
			}
			else if (binding.type == "joystick_button")
			{
				map.bind_joystick_button(action, action_profile_integer<std::size_t>(binding.control),
				                         action_profile_device<SDL_JoystickID>(binding.device), binding.scale);
			}
			else if (binding.type == "gamepad_button")
			{
				map.bind_gamepad_button(action, action_profile_enum<gamepad_button>(binding.control),
				                        action_profile_device<SDL_JoystickID>(binding.device), binding.scale);
			}
			else if (binding.type == "touch_contact")
			{
				if (binding.control != 0) [[unlikely]]
					throw std::invalid_argument("Touch contact profile control must be zero");
				map.bind_touch_contact(action, action_profile_device<SDL_TouchID>(binding.device), binding.scale);
			}
			else if (binding.type == "pen_button")
			{
				const auto device = action_profile_device<SDL_PenID>(binding.device);
				if (binding.control == 0)
				{
					map.bind_pen_tip(action, device, binding.scale);
				}
				else
				{
					map.bind_pen_button(action, action_profile_enum<pen_button>(binding.control), device,
					                    binding.scale);
				}
			}
			else if (binding.type == "joystick_axis")
			{
				map.bind_joystick_axis(action, action_profile_integer<std::size_t>(binding.control),
				                       binding.scale, dead_zone, threshold,
				                       action_profile_device<SDL_JoystickID>(binding.device));
			}
			else if (binding.type == "gamepad_axis")
			{
				map.bind_gamepad_axis(action, action_profile_enum<gamepad_axis>(binding.control),
				                      binding.scale, dead_zone, threshold,
				                      action_profile_device<SDL_JoystickID>(binding.device));
			}
			else if (binding.type == "touch_pressure")
			{
				if (binding.control != 0) [[unlikely]]
					throw std::invalid_argument("Touch pressure profile control must be zero");
				map.bind_touch_pressure(action, binding.scale, dead_zone, threshold,
				                        action_profile_device<SDL_TouchID>(binding.device));
			}
			else if (binding.type == "pen_axis")
			{
				map.bind_pen_axis(action, action_profile_enum<pen_axis_type>(binding.control), binding.scale,
				                  dead_zone, threshold, action_profile_device<SDL_PenID>(binding.device));
			}
			else
			{
				throw std::invalid_argument("Unknown action map profile binding type");
			}
		}
		catch (const std::invalid_argument& error)
		{
			throw std::invalid_argument("Invalid action map profile line " + std::to_string(line_number) +
			                            ": " + error.what());
		}
	}

	inline void parse_action_map_profile(std::string_view contents, action_map& result)
	{
		const std::size_t header_end = contents.find('\n');
		std::string_view header = header_end == std::string_view::npos ? contents : contents.substr(0, header_end);
		if (!header.empty() && header.back() == '\r')
		{
			header.remove_suffix(1);
		}
		if (header != action_map_profile_header) [[unlikely]]
		{
			throw std::invalid_argument("Invalid action map profile header or version");
		}
		if (header_end == std::string_view::npos)
		{
			return;
		}

		std::size_t line_number = 2;
		std::size_t line_begin = header_end + 1;
		while (line_begin < contents.size())
		{
			const std::size_t line_end = contents.find('\n', line_begin);
			std::string_view line = contents.substr(
			    line_begin, line_end == std::string_view::npos ? line_end : line_end - line_begin);
			if (!line.empty() && line.back() == '\r')
			{
				line.remove_suffix(1);
			}
			if (line.empty()) [[unlikely]]
			{
				throw std::invalid_argument("Empty action map profile record on line " +
				                            std::to_string(line_number));
			}

			std::array<std::string_view, 7> fields{};
			std::size_t field_count = 0;
			std::size_t field_begin = 0;
			while (field_begin <= line.size())
			{
				if (field_count >= fields.size()) [[unlikely]]
				{
					throw std::invalid_argument("Too many fields in action map profile line " +
					                            std::to_string(line_number));
				}
				const std::size_t separator = line.find('\t', field_begin);
				fields[field_count++] = line.substr(
				    field_begin, separator == std::string_view::npos ? separator : separator - field_begin);
				if (separator == std::string_view::npos)
				{
					break;
				}
				field_begin = separator + 1;
			}

			const std::string_view type = fields[0];
			const bool has_axis_parameters = action_profile_uses_axis_parameters(type);
			const std::size_t required_fields = has_axis_parameters ? 7 : 5;
			if (field_count != required_fields) [[unlikely]]
			{
				throw std::invalid_argument("Wrong field count in action map profile line " +
				                            std::to_string(line_number));
			}

			action_profile_binding binding;
			binding.type = type;
			binding.control = parse_action_profile_unsigned(fields[2], line_number);
			if (fields[3] != "*")
			{
				binding.device = parse_action_profile_unsigned(fields[3], line_number);
			}
			binding.scale = parse_action_profile_float(fields[4], line_number);
			if (has_axis_parameters)
			{
				binding.dead_zone = parse_action_profile_float(fields[5], line_number);
				binding.threshold = parse_action_profile_float(fields[6], line_number);
			}
			const std::string action = decode_action_profile_name(fields[1], line_number);
			apply_action_profile_binding(result, binding, action, line_number);

			if (line_end == std::string_view::npos)
			{
				break;
			}
			line_begin = line_end + 1;
			++line_number;
		}
	}
} // namespace mngl::detail

namespace mngl
{
	inline void action_map::save_profile(const std::filesystem::path& path) const
	{
		if (path.empty()) [[unlikely]]
		{
			throw std::invalid_argument("Action map profile path cannot be empty");
		}

		std::vector<std::string_view> action_names;
		action_names.reserve(m_storage->actions.size());
		for (const auto& [name, bindings] : m_storage->actions)
		{
			(void)bindings;
			action_names.emplace_back(name);
		}
		std::sort(action_names.begin(), action_names.end());

		std::string contents(detail::action_map_profile_header);
		contents.push_back('\n');
		bool first_binding = true;
		for (const auto name : action_names)
		{
			const auto found = m_storage->actions.find(name);
			if (found == m_storage->actions.end())
			{
				continue;
			}
			for (const auto& item : found->second)
			{
				detail::action_profile_binding record;
				std::visit(
				    [&](const auto& source)
				    {
					    using T = std::decay_t<decltype(source)>;
					    if constexpr (std::is_same_v<T, key_binding>)
					    {
						    record.type = "key";
						    record.control = detail::action_profile_enum_value(source.code);
						    record.device = detail::action_profile_device_value(source.device);
						    record.scale = source.scale;
					    }
					    else if constexpr (std::is_same_v<T, mouse_button_binding>)
					    {
						    record.type = "mouse_button";
						    record.control = detail::action_profile_enum_value(source.button);
						    record.device = detail::action_profile_device_value(source.device);
						    record.scale = source.scale;
					    }
					    else if constexpr (std::is_same_v<T, mouse_axis_binding>)
					    {
						    record.type = "mouse_axis";
						    record.control = detail::action_profile_enum_value(source.axis);
						    record.device = detail::action_profile_device_value(source.device);
						    record.scale = source.scale;
						    record.dead_zone = source.dead_zone;
						    record.threshold = source.threshold;
					    }
					    else if constexpr (std::is_same_v<T, joystick_button_binding>)
					    {
						    record.type = "joystick_button";
						    record.control = static_cast<std::uint64_t>(source.button);
						    record.device = detail::action_profile_device_value(source.device);
						    record.scale = source.scale;
					    }
					    else if constexpr (std::is_same_v<T, gamepad_button_binding>)
					    {
						    record.type = "gamepad_button";
						    record.control = detail::action_profile_enum_value(source.button);
						    record.device = detail::action_profile_device_value(source.device);
						    record.scale = source.scale;
					    }
					    else if constexpr (std::is_same_v<T, touch_contact_binding>)
					    {
						    record.type = "touch_contact";
						    record.device = detail::action_profile_device_value(source.device);
						    record.scale = source.scale;
					    }
					    else if constexpr (std::is_same_v<T, pen_button_binding>)
					    {
						    record.type = "pen_button";
						    record.control = static_cast<std::uint64_t>(source.button);
						    record.device = detail::action_profile_device_value(source.device);
						    record.scale = source.scale;
					    }
					    else if constexpr (std::is_same_v<T, joystick_axis_binding>)
					    {
						    record.type = "joystick_axis";
						    record.control = static_cast<std::uint64_t>(source.axis);
						    record.device = detail::action_profile_device_value(source.device);
						    record.scale = source.scale;
						    record.dead_zone = source.dead_zone;
						    record.threshold = source.threshold;
					    }
					    else if constexpr (std::is_same_v<T, gamepad_axis_binding>)
					    {
						    record.type = "gamepad_axis";
						    record.control = detail::action_profile_enum_value(source.axis);
						    record.device = detail::action_profile_device_value(source.device);
						    record.scale = source.scale;
						    record.dead_zone = source.dead_zone;
						    record.threshold = source.threshold;
					    }
					    else if constexpr (std::is_same_v<T, touch_pressure_binding>)
					    {
						    record.type = "touch_pressure";
						    record.device = detail::action_profile_device_value(source.device);
						    record.scale = source.scale;
						    record.dead_zone = source.dead_zone;
						    record.threshold = source.threshold;
					    }
					    else
					    {
						    record.type = "pen_axis";
						    record.control = detail::action_profile_enum_value(source.axis);
						    record.device = detail::action_profile_device_value(source.device);
						    record.scale = source.scale;
						    record.dead_zone = source.dead_zone;
						    record.threshold = source.threshold;
					    }
				    },
				    item);

				if (!first_binding)
				{
					contents.push_back('\n');
				}
				first_binding = false;
				contents += record.type;
				contents.push_back('\t');
				detail::append_action_profile_name(contents, name);
				contents.push_back('\t');
				detail::append_action_profile_number(contents, record.control);
				contents.push_back('\t');
				if (record.device)
				{
					detail::append_action_profile_number(contents, *record.device);
				}
				else
				{
					contents.push_back('*');
				}
				contents.push_back('\t');
				detail::append_action_profile_number(contents, record.scale);
				if (record.dead_zone && record.threshold)
				{
					contents.push_back('\t');
					detail::append_action_profile_number(contents, *record.dead_zone);
					contents.push_back('\t');
					detail::append_action_profile_number(contents, *record.threshold);
				}
				if (contents.size() > detail::action_map_profile_max_bytes) [[unlikely]]
				{
					throw std::length_error("Action map profile exceeds the supported size");
				}
			}
		}
		if (!first_binding)
		{
			contents.push_back('\n');
		}
		if (contents.size() > detail::action_map_profile_max_bytes) [[unlikely]]
		{
			throw std::length_error("Action map profile exceeds the supported size");
		}

		detail::atomic_temporary_file temporary_file(path);
		auto& file = temporary_file.stream();
		file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
		temporary_file.close();
		temporary_file.replace(path);
	}

	inline void action_map::load_profile(const std::filesystem::path& path)
	{
		if (path.empty()) [[unlikely]]
		{
			throw std::invalid_argument("Action map profile path cannot be empty");
		}
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file.is_open()) [[unlikely]]
		{
			throw std::runtime_error("Failed to open action map profile file");
		}
		const std::streamoff file_size = file.tellg();
		if (file_size < 0) [[unlikely]]
		{
			throw std::runtime_error("Failed to determine action map profile size");
		}
		const auto unsigned_size = static_cast<std::uintmax_t>(file_size);
		if (unsigned_size > detail::action_map_profile_max_bytes ||
		    unsigned_size > std::numeric_limits<std::size_t>::max() ||
		    unsigned_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
		    [[unlikely]]
		{
			throw std::length_error("Action map profile exceeds the supported size");
		}
		if (!file.seekg(0, std::ios::beg)) [[unlikely]]
		{
			throw std::runtime_error("Failed to seek in action map profile file");
		}
		std::string contents(static_cast<std::size_t>(unsigned_size), '\0');
		if (!contents.empty() &&
		    !file.read(contents.data(), static_cast<std::streamsize>(contents.size()))) [[unlikely]]
		{
			throw std::runtime_error("Failed to read action map profile file");
		}

		action_map loaded;
		detail::parse_action_map_profile(contents, loaded);
		m_storage->actions.swap(loaded.m_storage->actions);
		++m_storage->revision;
	}
} // namespace mngl

