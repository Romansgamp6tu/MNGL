#pragma once

#include "slots.hpp"

namespace mngl::mods
{
	class mod_context;

	class registry
	{
	public:
		registry() : m_diagnostics(std::make_shared<detail::diagnostic_store>()) {}
		registry(const registry&) = delete;
		registry& operator=(const registry&) = delete;
		registry(registry&&) = delete;
		registry& operator=(registry&&) = delete;

		template <class Signature, class F>
		    requires std::constructible_from<std::function<Signature>, F>
		function_slot<Signature>& define_function(std::string id, F&& fallback)
		{
			detail::require_id(id, "extension_id");
			using holder = detail::function_slot_holder<Signature>;
			std::lock_guard lock(m_mutex);
			if (m_functions.contains(id) || m_properties.contains(id)) [[unlikely]]
				throw registry_error(error_code::duplicate_extension, "Extension ID is already registered",
				                     id);
			auto value = std::make_shared<holder>(
			    std::move(id), std::function<Signature>(std::forward<F>(fallback)), m_diagnostics);
			auto* slot = &value->slot;
			m_functions.emplace(slot->id(), std::move(value));
			return *slot;
		}

		template <class T>
		    requires std::copy_constructible<T>
		property_slot<T>& define_property(std::string id, property_scope scope, T default_value,
		                                  typename property_slot<T>::getter getter = {},
		                                  typename property_slot<T>::setter setter = {},
		                                  typename property_slot<T>::validator validator = {})
		{
			detail::require_id(id, "extension_id");
			if (static_cast<bool>(getter) != static_cast<bool>(setter)) [[unlikely]]
				throw registry_error(error_code::invalid_value,
				                     "A property must provide both getter and setter, or neither", id);
			using holder = detail::property_slot_holder<T>;
			std::lock_guard lock(m_mutex);
			if (m_properties.contains(id) || m_functions.contains(id)) [[unlikely]]
				throw registry_error(error_code::duplicate_extension, "Extension ID is already registered",
				                     id);
			auto handle_is_valid = [this](instance_handle handle) { return is_live(handle); };
			auto value = std::make_shared<holder>(std::move(id), scope, std::move(default_value),
			                                      std::move(getter), std::move(setter), std::move(validator),
			                                      std::move(handle_is_valid), m_diagnostics);
			auto* slot = &value->slot;
			m_properties.emplace(slot->id(), std::move(value));
			return *slot;
		}

		template <class Base>
		    requires std::derived_from<Base, modable>
		class_registry<Base>& define_class(std::string id)
		{
			detail::require_id(id, "class_id");
			using holder = detail::class_registry_holder<Base>;
			std::lock_guard lock(m_mutex);
			const auto existing = m_classes.find(id);
			if (existing != m_classes.end())
			{
				auto* typed = dynamic_cast<holder*>(existing->second.get());
				if (!typed) [[unlikely]]
					throw registry_error(error_code::type_mismatch,
					                     "Class ID was already declared with a different base type", id);
				return typed->classes;
			}
			auto value = std::make_shared<holder>(id, m_diagnostics);
			auto* classes = &value->classes;
			m_classes.emplace(std::move(id), std::move(value));
			return *classes;
		}

		template <class Signature> function_slot<Signature>& function(std::string_view id)
		{
			using holder = detail::function_slot_holder<Signature>;
			std::lock_guard lock(m_mutex);
			const auto it = m_functions.find(std::string(id));
			if (it == m_functions.end()) [[unlikely]]
				throw registry_error(error_code::unknown_extension, "Unknown function slot", std::string(id));
			auto* typed = dynamic_cast<holder*>(it->second.get());
			if (!typed) [[unlikely]]
				throw registry_error(error_code::type_mismatch, "Function slot signature does not match",
				                     std::string(id));
			return typed->slot;
		}

		template <class T>
		    requires std::copy_constructible<T>
		property_slot<T>& property(std::string_view id)
		{
			using holder = detail::property_slot_holder<T>;
			std::lock_guard lock(m_mutex);
			const auto it = m_properties.find(std::string(id));
			if (it == m_properties.end()) [[unlikely]]
				throw registry_error(error_code::unknown_extension, "Unknown property", std::string(id));
			auto* typed = dynamic_cast<holder*>(it->second.get());
			if (!typed) [[unlikely]]
				throw registry_error(error_code::type_mismatch, "Property type does not match",
				                     std::string(id));
			return typed->slot;
		}

		template <class Base>
		    requires std::derived_from<Base, modable>
		class_registry<Base>& classes(std::string_view id)
		{
			using holder = detail::class_registry_holder<Base>;
			std::lock_guard lock(m_mutex);
			const auto it = m_classes.find(std::string(id));
			if (it == m_classes.end()) [[unlikely]]
				throw registry_error(error_code::unknown_extension, "Unknown class registry",
				                     std::string(id));
			auto* typed = dynamic_cast<holder*>(it->second.get());
			if (!typed) [[unlikely]]
				throw registry_error(error_code::type_mismatch, "Class registry base type does not match",
				                     std::string(id));
			return typed->classes;
		}

		[[nodiscard]] instance_handle create_instance_handle()
		{
			std::lock_guard lock(m_mutex);
			if (m_next_handle == 0 || m_next_handle == std::numeric_limits<std::uint64_t>::max()) [[unlikely]]
				throw std::overflow_error("MNGL mod instance handle space exhausted");
			const instance_handle handle(m_next_handle++);
			m_live_handles.insert(handle);
			return handle;
		}

		void release_instance_handle(instance_handle handle)
		{
			std::vector<std::shared_ptr<detail::property_slot_base>> properties;
			{
				std::lock_guard lock(m_mutex);
				if (!m_live_handles.erase(handle))
					return;
				for (const auto& [_, value] : m_properties)
					properties.push_back(value);
			}
			for (const auto& value : properties)
				value->remove_instance(handle);
		}

		void remove_mod(std::string_view mod_id)
		{
			detail::require_id(mod_id, "mod_id");
			std::vector<std::shared_ptr<detail::function_slot_base>> functions;
			std::vector<std::shared_ptr<detail::property_slot_base>> properties;
			std::vector<std::shared_ptr<detail::class_registry_base>> classes;
			{
				std::lock_guard lock(m_mutex);
				for (const auto& [_, value] : m_functions)
					functions.push_back(value);
				for (const auto& [_, value] : m_properties)
					properties.push_back(value);
				for (const auto& [_, value] : m_classes)
					classes.push_back(value);
			}
			for (const auto& value : functions)
				value->remove_mod(mod_id);
			for (const auto& value : properties)
				value->remove_mod(mod_id);
			for (const auto& value : classes)
				value->remove_mod(mod_id);
		}

		// Returns the active, non-instance property overrides that a host can expose.
		[[nodiscard]] std::vector<mod_setting> settings() const
		{
			std::vector<std::shared_ptr<detail::property_slot_base>> properties;
			{
				std::lock_guard lock(m_mutex);
				for (const auto& [_, value] : m_properties)
					properties.push_back(value);
			}

			std::vector<mod_setting> result;
			for (const auto& property : properties)
			{
				auto entries = property->get_mod_settings();
				result.insert(result.end(), std::make_move_iterator(entries.begin()),
				              std::make_move_iterator(entries.end()));
			}
			std::sort(result.begin(), result.end(),
			          [](const auto& left, const auto& right)
			          {
				          if (left.mod_id != right.mod_id)
					          return left.mod_id < right.mod_id;
				          return left.property_id < right.property_id;
			          });
			return result;
		}

		// Values use the text form shown by settings(); validators still apply.
		bool set_setting(std::string_view mod_id, std::string_view property_id, std::string_view value,
		                 std::string* error = nullptr)
		{
			std::lock_guard settings_io_lock(m_settings_io_mutex);
			detail::require_id(mod_id, "mod_id");
			detail::require_id(property_id, "property_id");
			std::shared_ptr<detail::property_slot_base> property;
			{
				std::lock_guard lock(m_mutex);
				const auto found = m_properties.find(std::string(property_id));
				if (found != m_properties.end())
					property = found->second;
			}

			std::string message;
			if (!property)
				message = "unknown property";
			else if (!property->set_mod_setting(mod_id, value, message))
			{
				// The property supplies a type- or validator-specific explanation.
			}
			else
			{
				const auto entries = property->get_mod_settings();
				const auto found =
				    std::find_if(entries.begin(), entries.end(),
					             [&](const mod_setting& setting) { return setting.mod_id == mod_id; });
				if (found != entries.end())
				{
					std::lock_guard lock(m_settings_mutex);
					m_saved_settings.insert_or_assign(std::pair(found->mod_id, found->property_id), *found);
				}
				if (error)
					error->clear();
				return true;
			}

			if (message.empty())
				message = "setting value was rejected";
			if (error)
				*error = message;
			m_diagnostics->add(diagnostic{error_code::invalid_value, std::string(mod_id),
			                              std::string(property_id), message});
			return false;
		}

		// Load after mods have registered their overrides. Unknown entries are retained
		// so settings for temporarily absent mods survive a save.
		[[nodiscard]] std::vector<diagnostic> load_settings(const std::filesystem::path& path)
		{
			std::lock_guard settings_io_lock(m_settings_io_mutex);
			std::vector<diagnostic> diagnostics;
			std::map<std::pair<std::string, std::string>, mod_setting> records;
			try
			{
				std::error_code file_error;
				if (!std::filesystem::exists(path, file_error))
				{
					if (file_error) [[unlikely]]
						throw std::runtime_error("cannot inspect settings file: " + file_error.message());
					return diagnostics;
				}
				constexpr std::uintmax_t max_settings_size = 4 * 1024 * 1024;
				if (std::filesystem::file_size(path) > max_settings_size) [[unlikely]]
					throw std::runtime_error("settings file exceeds the 4 MiB size limit");

				std::ifstream input(path);
				if (!input) [[unlikely]]
					throw std::runtime_error("cannot open settings file");
				std::string line;
				if (!std::getline(input, line) || line != "MNGL_MOD_SETTINGS\t1") [[unlikely]]
					throw std::runtime_error("unsupported settings file header or version");

				std::size_t line_number = 1;
				while (std::getline(input, line))
				{
					++line_number;
					if (!line.empty() && line.back() == '\r')
						line.pop_back();
					if (line.empty() || line.front() == '#')
						continue;
					const auto first = line.find('\t');
					const auto second =
					    first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
					const auto third =
					    second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
					if (first == std::string::npos || second == std::string::npos ||
					    third == std::string::npos || line.find('\t', third + 1) != std::string::npos) [[unlikely]]
						throw std::runtime_error("malformed settings record on line " +
						                         std::to_string(line_number));

					auto mod_id = decode_setting_field(std::string_view(line).substr(0, first));
					auto property_id =
					    decode_setting_field(std::string_view(line).substr(first + 1, second - first - 1));
					auto type =
					    decode_setting_field(std::string_view(line).substr(second + 1, third - second - 1));
					auto value = decode_setting_field(std::string_view(line).substr(third + 1));
					detail::require_id(mod_id, "mod_id");
					detail::require_id(property_id, "property_id");
					if (type.empty()) [[unlikely]]
						throw std::runtime_error("empty property type on line " +
						                         std::to_string(line_number));
					mod_setting setting{mod_id, property_id, type, value};
					if (!records.emplace(std::pair(mod_id, property_id), std::move(setting)).second) [[unlikely]]
						throw std::runtime_error("duplicate setting on line " + std::to_string(line_number));
				}
				if (!input.eof()) [[unlikely]]
					throw std::runtime_error("failed while reading settings file");
			}
			catch (const std::exception& exception)
			{
				diagnostics.push_back(diagnostic{error_code::invalid_value, {}, {}, exception.what()});
				for (const auto& item : diagnostics)
					m_diagnostics->add(item);
				return diagnostics;
			}

			{
				std::lock_guard lock(m_settings_mutex);
				m_saved_settings = records;
			}

			for (const auto& [key, saved] : records)
			{
				std::shared_ptr<detail::property_slot_base> property;
				{
					std::lock_guard lock(m_mutex);
					const auto found = m_properties.find(saved.property_id);
					if (found != m_properties.end())
						property = found->second;
				}
				if (!property)
				{
					diagnostics.push_back(
					    diagnostic{error_code::unknown_extension, saved.mod_id, saved.property_id,
						           "saved setting refers to an unknown property; entry was preserved"});
					continue;
				}

				const auto active = property->get_mod_settings();
				const auto current =
				    std::find_if(active.begin(), active.end(),
					             [&](const mod_setting& setting) { return setting.mod_id == saved.mod_id; });
				if (current == active.end())
				{
					diagnostics.push_back(
					    diagnostic{error_code::invalid_value, saved.mod_id, saved.property_id,
						           "saved setting has no active mod override; entry was preserved"});
					continue;
				}
				if (current->type != saved.type)
				{
					diagnostics.push_back(
					    diagnostic{error_code::type_mismatch, saved.mod_id, saved.property_id,
						           "saved setting type '" + saved.type + "' does not match '" +
						               current->type + "'; entry was preserved"});
					continue;
				}

				std::string error_message;
				if (!property->set_mod_setting(saved.mod_id, saved.value, error_message))
					diagnostics.push_back(diagnostic{error_code::invalid_value, saved.mod_id,
					                                 saved.property_id,
					                                 "saved setting was rejected: " + error_message});
			}

			for (const auto& item : diagnostics)
				m_diagnostics->add(item);
			return diagnostics;
		}

		void save_settings(const std::filesystem::path& path) const
		{
			std::lock_guard settings_io_lock(m_settings_io_mutex);
			auto records = settings();
			{
				std::lock_guard lock(m_settings_mutex);
				for (const auto& setting : records)
					m_saved_settings.insert_or_assign(std::pair(setting.mod_id, setting.property_id),
					                                  setting);
				records.clear();
				records.reserve(m_saved_settings.size());
				for (const auto& [_, setting] : m_saved_settings)
					records.push_back(setting);
			}

			if (!path.parent_path().empty())
				std::filesystem::create_directories(path.parent_path());
			auto temporary = path;
			temporary += ".tmp";
			{
				std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
				if (!output) [[unlikely]]
					throw std::runtime_error("cannot create temporary settings file: " + temporary.string());
				output << "MNGL_MOD_SETTINGS\t1\n"
				       << "# columns: mod_id, property_id, type, value (percent-encoded UTF-8)\n";
				for (const auto& setting : records)
					output << encode_setting_field(setting.mod_id) << '\t'
					       << encode_setting_field(setting.property_id) << '\t'
					       << encode_setting_field(setting.type) << '\t'
					       << encode_setting_field(setting.value) << '\n';
				output.flush();
				if (!output) [[unlikely]]
					throw std::runtime_error("failed while writing settings file: " + temporary.string());
			}
			replace_settings_file(temporary, path);
		}

		[[nodiscard]] std::vector<diagnostic> diagnostics() const { return m_diagnostics->snapshot(); }

		void record_diagnostic(diagnostic value) { m_diagnostics->add(std::move(value)); }

		// Applies REGISTER_MODDABLE_* declarations once to this registry.
		void apply_declarations();

		[[nodiscard]] mod_context for_mod(std::string mod_id);

	private:
		[[nodiscard]] static std::string encode_setting_field(std::string_view value)
		{
			constexpr char hex[] = "0123456789ABCDEF";
			std::string result;
			result.reserve(value.size());
			for (const unsigned char byte : value)
			{
				if (byte >= 0x21 && byte <= 0x7e && byte != '%')
					result.push_back(static_cast<char>(byte));
				else
				{
					result.push_back('%');
					result.push_back(hex[byte >> 4]);
					result.push_back(hex[byte & 0x0f]);
				}
			}
			return result;
		}

		[[nodiscard]] static std::string decode_setting_field(std::string_view value)
		{
			const auto hex_value = [](char character) -> int
			{
				if (character >= '0' && character <= '9')
					return character - '0';
				if (character >= 'A' && character <= 'F')
					return character - 'A' + 10;
				if (character >= 'a' && character <= 'f')
					return character - 'a' + 10;
				return -1;
			};
			std::string result;
			result.reserve(value.size());
			for (std::size_t i = 0; i < value.size(); ++i)
			{
				if (value[i] != '%')
				{
					result.push_back(value[i]);
					continue;
				}
				if (i + 2 >= value.size()) [[unlikely]]
					throw std::runtime_error("invalid percent escape in settings file");
				const auto high = hex_value(value[i + 1]);
				const auto low = hex_value(value[i + 2]);
				if (high < 0 || low < 0) [[unlikely]]
					throw std::runtime_error("invalid percent escape in settings file");
				result.push_back(static_cast<char>((high << 4) | low));
				i += 2;
			}
			return result;
		}

		static void replace_settings_file(const std::filesystem::path& temporary,
		                                  const std::filesystem::path& target)
		{
			std::error_code error;
			std::filesystem::rename(temporary, target, error);
			if (!error)
				return;
			if (!std::filesystem::exists(target)) [[unlikely]]
				throw std::runtime_error("cannot replace settings file: " + error.message());

			auto backup = target;
			backup += ".bak";
			std::error_code ignored;
			std::filesystem::remove(backup, ignored);
			error.clear();
			std::filesystem::rename(target, backup, error);
			if (error) [[unlikely]]
				throw std::runtime_error("cannot back up existing settings file: " + error.message());

			error.clear();
			std::filesystem::rename(temporary, target, error);
			if (error)
			{
				std::error_code restore_error;
				std::filesystem::rename(backup, target, restore_error);
				throw std::runtime_error("cannot install new settings file: " + error.message());
			}
			std::filesystem::remove(backup, ignored);
		}

		[[nodiscard]] mod_context for_mod(std::string mod_id,
		                                  std::shared_ptr<detail::module_lifetime> lifetime);

		[[nodiscard]] bool is_live(instance_handle handle) const
		{
			std::lock_guard lock(m_mutex);
			return m_live_handles.contains(handle);
		}

		std::shared_ptr<detail::diagnostic_store> m_diagnostics;
		mutable std::mutex m_mutex;
		std::uint64_t m_next_handle{1};
		bool m_declarations_applied{};
		std::unordered_set<instance_handle, detail::instance_handle_hash> m_live_handles;
		std::unordered_map<std::string, std::shared_ptr<detail::function_slot_base>> m_functions;
		std::unordered_map<std::string, std::shared_ptr<detail::property_slot_base>> m_properties;
		std::unordered_map<std::string, std::shared_ptr<detail::class_registry_base>> m_classes;
		mutable std::mutex m_settings_mutex;
		mutable std::mutex m_settings_io_mutex;
		mutable std::map<std::pair<std::string, std::string>, mod_setting> m_saved_settings;

		friend class mod_context;
		friend class mod_loader;
	};

	class mod_context
	{
	public:
		[[nodiscard]] const std::string& mod_id() const noexcept { return m_mod_id; }

		template <class Signature, class F> bool replace_function(std::string_view id, F&& function)
		{
			std::function<Signature> callback(std::forward<F>(function));
			if (m_lifetime)
				callback = detail::guarded_function<Signature>::make(m_lifetime, std::move(callback));
			return m_registry->template function<Signature>(id).replace(m_mod_id, std::move(callback));
		}

		template <class Signature, class F>
		void around_function(std::string_view id, priority_t priority, F&& function)
		{
			using guarded = detail::guarded_function<Signature>;
			using around_callback = typename function_slot<Signature>::around_callback;
			around_callback callback(std::forward<F>(function));
			if (m_lifetime)
				callback = guarded::make_around(m_lifetime, std::move(callback));
			m_registry->template function<Signature>(id).around(m_mod_id, priority, std::move(callback));
		}

		template <class T>
		    requires std::copy_constructible<T>
		bool set_property(std::string_view id, const T& value, priority_t priority = 0,
		                  instance_handle handle = {})
		{
			return m_registry->template property<T>(id).set_override(m_mod_id, value, priority, handle);
		}

		template <class Base, class F>
		    requires std::derived_from<Base, modable>
		bool add_implementation(std::string_view class_id, std::string implementation_id, F&& make)
		{
			typename class_registry<Base>::factory factory(std::forward<F>(make));
			if (m_lifetime)
			{
				auto lifetime = m_lifetime;
				factory = [lifetime = std::move(lifetime), maker = std::move(factory)]
				{
					auto lease = lifetime->enter();
					auto object = maker();
					if (object)
						detail::pin_module_object(*object, lifetime);
					return object;
				};
			}
			return m_registry->template classes<Base>(class_id).add_implementation(
			    m_mod_id, std::move(implementation_id), std::move(factory));
		}

		template <class Base, class F>
		    requires std::derived_from<Base, modable>
		bool replace_factory(std::string_view class_id, std::string implementation_id, F&& make)
		{
			typename class_registry<Base>::factory factory(std::forward<F>(make));
			if (m_lifetime)
			{
				auto lifetime = m_lifetime;
				factory = [lifetime = std::move(lifetime), maker = std::move(factory)]
				{
					auto lease = lifetime->enter();
					auto object = maker();
					if (object)
						detail::pin_module_object(*object, lifetime);
					return object;
				};
			}
			return m_registry->template classes<Base>(class_id).replace_factory(
			    m_mod_id, std::move(implementation_id), std::move(factory));
		}

	private:
		mod_context(registry& owner, std::string mod_id,
		            std::shared_ptr<detail::module_lifetime> lifetime = {})
		    : m_registry(&owner), m_mod_id(std::move(mod_id)), m_lifetime(std::move(lifetime))
		{
		}

		registry* m_registry;
		std::string m_mod_id;
		std::shared_ptr<detail::module_lifetime> m_lifetime;
		friend class registry;
	};

	inline mod_context registry::for_mod(std::string mod_id)
	{
		detail::require_id(mod_id, "mod_id");
		return mod_context(*this, std::move(mod_id));
	}

	inline mod_context registry::for_mod(std::string mod_id,
	                                     std::shared_ptr<detail::module_lifetime> lifetime)
	{
		detail::require_id(mod_id, "mod_id");
		return mod_context(*this, std::move(mod_id), std::move(lifetime));
	}

} // namespace mngl::mods
