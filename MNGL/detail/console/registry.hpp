#pragma once

#include "core.hpp"

namespace mngl::console
{
	class registry;

	template <class... Args> class command_builder
	{
	public:
		command_builder(const command_builder&) = delete;
		command_builder& operator=(const command_builder&) = delete;
		command_builder(command_builder&&) noexcept = default;
		command_builder& operator=(command_builder&&) noexcept = default;

		template <class T>
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

		template <class F> registry& handler(F&& callback);

	private:
		command_builder(registry& owner, command_descriptor descriptor)
		    : m_owner(&owner), m_descriptor(std::move(descriptor))
		{
		}

		template <class T> [[nodiscard]] auto append(argument_spec spec) &&
		{
			m_descriptor.arguments.push_back(std::move(spec));
			return command_builder<Args..., T>(*m_owner, std::move(m_descriptor));
		}

		registry* m_owner{};
		command_descriptor m_descriptor;

		template <class...> friend class command_builder;
		friend class registry;
	};

	class registry
	{
	public:
		using value_list = std::vector<argument_value>;

		[[nodiscard]] command_builder<> add_command(std::string id, std::string description)
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
			std::sort(result.begin(), result.end(),
			          [](const auto& left, const auto& right) { return left.id < right.id; });
			return result;
		}

		[[nodiscard]] command_result invoke(std::string_view id, std::span<const argument_value> values) const
		{
			const auto found = m_commands.find(std::string(id));
			if (found == m_commands.end()) [[unlikely]]
				throw registry_error(error_code::unknown_command, "Unknown command '" + std::string(id) + "'",
				                     std::string(id));

			const auto& entry = found->second;
			if (values.size() != entry.descriptor.arguments.size()) [[unlikely]]
				throw registry_error(error_code::argument_count_mismatch,
				                     "Command argument count does not match its declaration",
				                     entry.descriptor.id);

			for (std::size_t i = 0; i < values.size(); ++i)
			{
				const auto& spec = entry.descriptor.arguments[i];
				const bool matches =
				    (spec.type == argument_type::string && std::holds_alternative<std::string>(values[i])) ||
				    (spec.type == argument_type::integer &&
					 std::holds_alternative<std::int64_t>(values[i])) ||
				    (spec.type == argument_type::floating_point &&
					 std::holds_alternative<double>(values[i])) ||
				    (spec.type == argument_type::flag && std::holds_alternative<bool>(values[i]));
				if (!matches) [[unlikely]]
					throw registry_error(error_code::argument_type_mismatch,
					                     "Argument '" + spec.name + "' has the wrong type",
					                     entry.descriptor.id);
			}

			return entry.handler(values, entry.descriptor);
		}

	private:
		using erased_handler =
		    std::function<command_result(std::span<const argument_value>, const command_descriptor&)>;

		struct command_entry
		{
			command_descriptor descriptor;
			erased_handler handler;
		};

		void register_command(command_descriptor descriptor, erased_handler handler)
		{
			if (!detail::is_valid_command_id(descriptor.id)) [[unlikely]]
				throw registry_error(error_code::invalid_command_id,
				                     "Command IDs must contain letters, digits, '_', '-' or '.', "
				                     "must not start with '-' or contain empty dot-separated segments",
				                     descriptor.id);
			if (descriptor.description.empty()) [[unlikely]]
				throw registry_error(error_code::invalid_command_description,
				                     "Command description must not be empty", descriptor.id);

			std::vector<std::string> names;
			std::vector<std::string> options;
			for (const auto& arg : descriptor.arguments)
			{
				if (!detail::is_valid_argument_name(arg.name) || arg.description.empty()) [[unlikely]]
					throw registry_error(
					    error_code::invalid_argument,
					    "Argument names must be non-empty and use letters, digits, '_', or '-'; "
					    "argument descriptions must not be empty",
					    descriptor.id);
				if (std::find(names.begin(), names.end(), arg.name) != names.end()) [[unlikely]]
					throw registry_error(error_code::invalid_argument,
					                     "Duplicate argument name '" + arg.name + "'", descriptor.id);
				names.push_back(arg.name);

				if (arg.form == argument_form::flag)
				{
					if (arg.name.front() == '-' || arg.name.back() == '-') [[unlikely]]
						throw registry_error(error_code::invalid_argument,
						                     "Flag names must not start or end with '-'", descriptor.id);
					if (arg.name == "help") [[unlikely]]
						throw registry_error(error_code::invalid_argument,
						                     "The '--help' flag is reserved by the console dispatcher",
						                     descriptor.id);
					if (arg.type != argument_type::flag || arg.option_name != "--" + arg.name) [[unlikely]]
						throw registry_error(error_code::invalid_argument, "Flag metadata is inconsistent",
						                     descriptor.id);
					if (std::find(options.begin(), options.end(), arg.option_name) != options.end()) [[unlikely]]
						throw registry_error(error_code::invalid_argument,
						                     "Duplicate flag '" + arg.option_name + "'", descriptor.id);
					options.push_back(arg.option_name);
				}
				else if (arg.type == argument_type::flag || !arg.option_name.empty()) [[unlikely]]
				{
					throw registry_error(error_code::invalid_argument,
					                     "Positional arguments cannot be declared as flags", descriptor.id);
				}
			}

			const auto command_id = descriptor.id;
			const auto insert_result =
			    m_commands.emplace(command_id, command_entry{std::move(descriptor), std::move(handler)});
			if (!insert_result.second) [[unlikely]]
				throw registry_error(error_code::duplicate_command,
				                     "Command '" + command_id + "' is already registered", command_id);
		}

		std::unordered_map<std::string, command_entry> m_commands;

		template <class...> friend class command_builder;
	};

	template <class... Args> template <class F> registry& command_builder<Args...>::handler(F&& callback)
	{
		if (!m_owner) [[unlikely]]
			throw std::logic_error("Command builder has already been consumed");
		auto* owner = m_owner;
		auto erased = detail::make_handler<Args...>(std::forward<F>(callback));
		owner->register_command(std::move(m_descriptor), std::move(erased));
		m_owner = nullptr;
		return *owner;
	}

} // namespace mngl::console
