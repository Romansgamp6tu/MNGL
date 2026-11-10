#pragma once

#include "core.hpp"

namespace mngl::mods
{
	template <class Signature> class function_slot;

	template <class R, class... Args> class function_slot<R(Args...)>
	{
		static_assert(!std::is_reference_v<R>, "Function slots cannot return references.");
		static_assert(std::is_void_v<R> || std::copy_constructible<R>,
		              "Function slot return values must be copy constructible.");
		static_assert((!std::is_rvalue_reference_v<Args> && ...),
		              "Function slots do not support rvalue-reference arguments.");
		static_assert(
		    ((std::is_lvalue_reference_v<Args> || std::copy_constructible<std::remove_cvref_t<Args>>) && ...),
		    "By-value function slot arguments must be copy constructible.");

	public:
		using signature = R(Args...);
		using callback = std::function<signature>;
		using next_callback = callback;
		using around_callback = std::function<R(next_callback, Args...)>;

		function_slot(const function_slot&) = delete;
		function_slot& operator=(const function_slot&) = delete;

		[[nodiscard]] const std::string& id() const noexcept { return m_id; }

		template <class F>
		    requires std::constructible_from<callback, F>
		bool replace(std::string mod_id, F&& function)
		{
			detail::require_id(mod_id, "mod_id");
			callback callable(std::forward<F>(function));
			if (!callable) [[unlikely]]
				throw registry_error(error_code::invalid_value, "Replacement callback is empty", m_id,
				                     mod_id);

			auto registration = std::make_shared<replacement>(std::move(mod_id), std::move(callable));
			std::lock_guard lock(m_state->mutex);

			for (const auto& existing : m_state->replacements)
			{
				if (existing->mod_id == registration->mod_id) [[unlikely]]
					throw registry_error(error_code::duplicate_registration,
					                     "Mod already registered a replacement for this slot", m_id,
					                     registration->mod_id);
			}

			const auto has_active = std::any_of(m_state->replacements.begin(), m_state->replacements.end(),
			                                    [](const auto& item) { return item->active.load(); });

			if (has_active)
			{
				const auto owner = registration->mod_id;
				registration->active = false;
				for (const auto& existing : m_state->replacements)
					existing->active = false;
				m_state->replacements.push_back(std::move(registration));
				detail::report(m_state->diagnostics, error_code::registration_conflict, owner, m_id,
				               "Conflicting replacements were disabled for this slot");
				return false;
			}

			m_state->replacements.push_back(std::move(registration));
			return true;
		}

		template <class F>
		    requires std::constructible_from<around_callback, F>
		void around(std::string mod_id, priority_t priority, F&& function)
		{
			detail::require_id(mod_id, "mod_id");
			around_callback callable(std::forward<F>(function));
			if (!callable) [[unlikely]]
				throw registry_error(error_code::invalid_value, "Around callback is empty", m_id, mod_id);

			auto registration =
			    std::make_shared<around_registration>(std::move(mod_id), priority, std::move(callable));
			std::lock_guard lock(m_state->mutex);

			if (std::any_of(m_state->around.begin(), m_state->around.end(),
			                [&](const auto& item) { return item->mod_id == registration->mod_id; })) [[unlikely]]
				throw registry_error(error_code::duplicate_registration,
				                     "Mod already registered an around callback for this slot", m_id,
				                     registration->mod_id);

			m_state->around.push_back(std::move(registration));
		}

		R operator()(Args... args) const
		{
			callback base;
			std::vector<std::shared_ptr<around_registration>> around;
			{
				std::lock_guard lock(m_state->mutex);
				const auto fallback = m_state->fallback;
				std::shared_ptr<replacement> selected;
				for (const auto& replacement : m_state->replacements)
					if (replacement->active.load())
						selected = replacement;

				if (!selected)
					base = fallback;
				else
					base = [this, selected, fallback](Args... call_args) -> R
					{
						try
						{
							if constexpr (std::is_void_v<R>)
							{
								selected->function(call_args...);
								return;
							}
							else
								return selected->function(call_args...);
						}
						catch (...)
						{
							selected->active = false;
							detail::report(m_state->diagnostics, error_code::callback_failed,
							               selected->mod_id, m_id,
							               "Replacement callback threw; default implementation was used");
							if constexpr (std::is_void_v<R>)
							{
								fallback(call_args...);
								return;
							}
							else
								return fallback(call_args...);
						}
					};

				for (const auto& hook : m_state->around)
					if (hook->active.load())
						around.push_back(hook);
			}

			std::sort(around.begin(), around.end(),
			          [](const auto& left, const auto& right)
			          {
				          if (left->priority != right->priority)
					          return left->priority > right->priority;
				          return left->mod_id < right->mod_id;
			          });

			return invoke(around, 0, base, args...);
		}

	private:
		struct replacement
		{
			std::string mod_id;
			callback function;
			std::atomic_bool active{true};

			replacement(std::string owner, callback value)
			    : mod_id(std::move(owner)), function(std::move(value))
			{
			}
		};

		struct around_registration
		{
			std::string mod_id;
			priority_t priority{};
			around_callback function;
			std::atomic_bool active{true};

			around_registration(std::string owner, priority_t order, around_callback value)
			    : mod_id(std::move(owner)), priority(order), function(std::move(value))
			{
			}
		};

		struct state
		{
			std::mutex mutex;
			callback fallback;
			std::vector<std::shared_ptr<replacement>> replacements;
			std::vector<std::shared_ptr<around_registration>> around;
			std::shared_ptr<detail::diagnostic_store> diagnostics;
		};

		function_slot(std::string id, callback fallback,
		              std::shared_ptr<detail::diagnostic_store> diagnostics)
		    : m_id(std::move(id)), m_state(std::make_shared<state>())
		{
			if (!fallback) [[unlikely]]
				throw registry_error(error_code::invalid_value,
				                     "Function slot requires a default implementation", m_id);
			m_state->fallback = std::move(fallback);
			m_state->diagnostics = std::move(diagnostics);
		}

		void remove_mod(std::string_view mod_id)
		{
			std::lock_guard lock(m_state->mutex);
			std::erase_if(m_state->replacements, [&](const auto& item) { return item->mod_id == mod_id; });
			std::erase_if(m_state->around, [&](const auto& item) { return item->mod_id == mod_id; });
		}

		R invoke(const std::vector<std::shared_ptr<around_registration>>& around, std::size_t index,
		         const callback& base, Args... args) const
		{
			if (index == around.size())
				return base(std::forward<Args>(args)...);

			const auto hook = around[index];
			using result_type = typename detail::next_result<R>::type;
			bool next_called = false;
			result_type next_result{};

			next_callback next = [&](Args... next_args) -> R
			{
				if (next_called) [[unlikely]]
					throw std::logic_error("around next callback may only be called once");
				next_called = true;
				if constexpr (std::is_void_v<R>)
				{
					invoke(around, index + 1, base, std::forward<Args>(next_args)...);
					next_result = true;
				}
				else
					next_result.emplace(invoke(around, index + 1, base, std::forward<Args>(next_args)...));
				if constexpr (!std::is_void_v<R>)
					return *next_result;
			};

			try
			{
				return hook->function(next, args...);
			}
			catch (...)
			{
				hook->active = false;
				detail::report(m_state->diagnostics, error_code::callback_failed, hook->mod_id, m_id,
				               "Around callback threw; registration disabled");
				if (!next_called)
					return invoke(around, index + 1, base, args...);
				if constexpr (!std::is_void_v<R>)
					return *next_result;
			}
		}

		std::string m_id;
		std::shared_ptr<state> m_state;

		friend class registry;
		template <class> friend struct detail::function_slot_holder;
	};

	namespace detail
	{
		template <class R, class... Args> struct function_slot_holder<R(Args...)> : function_slot_base
		{
			using slot_type = function_slot<R(Args...)>;
			slot_type slot;

			function_slot_holder(std::string id, typename slot_type::callback fallback,
			                     std::shared_ptr<diagnostic_store> diagnostics)
			    : slot(std::move(id), std::move(fallback), std::move(diagnostics))
			{
			}

			void remove_mod(std::string_view mod_id) override { slot.remove_mod(mod_id); }
		};
	} // namespace detail

	template <class T>
	    requires std::copy_constructible<T>
	class property_slot
	{
	public:
		using getter = std::function<T(instance_handle)>;
		using setter = std::function<bool(instance_handle, const T&)>;
		using validator = std::function<bool(const T&)>;

		property_slot(const property_slot&) = delete;
		property_slot& operator=(const property_slot&) = delete;

		[[nodiscard]] const std::string& id() const noexcept { return m_id; }
		[[nodiscard]] property_scope scope() const noexcept { return m_scope; }

		[[nodiscard]] T get(instance_handle handle = {}) const
		{
			validate_handle(handle);
			{
				std::lock_guard lock(m_mutex);
				const override_value* winner = nullptr;
				for (const auto& item : m_overrides)
				{
					if (m_scope == property_scope::instance && item.handle != handle)
						continue;
					if (!winner || item.priority > winner->priority ||
					    (item.priority == winner->priority && item.mod_id < winner->mod_id))
						winner = &item;
				}
				if (winner)
					return winner->value;
			}

			if (m_getter)
				return m_getter(handle);

			std::lock_guard lock(m_mutex);
			if (m_scope == property_scope::instance)
			{
				const auto it = m_instance_values.find(handle);
				return it == m_instance_values.end() ? m_default_value : it->second;
			}
			return m_base_value;
		}

		bool set(instance_handle handle, const T& value)
		{
			validate_handle(handle);
			if (!valid_value(value))
				return false;
			if (m_setter)
				return m_setter(handle, value);

			std::lock_guard lock(m_mutex);
			if (m_scope == property_scope::instance)
				m_instance_values.insert_or_assign(handle, value);
			else
				m_base_value = value;
			return true;
		}

		bool set_override(std::string mod_id, const T& value, priority_t priority = 0,
		                  instance_handle handle = {})
		{
			detail::require_id(mod_id, "mod_id");
			validate_handle(handle);
			if (!valid_value(value))
			{
				detail::report(m_diagnostics, error_code::invalid_value, mod_id, m_id,
				               "Property override rejected by validator");
				return false;
			}

			std::lock_guard lock(m_mutex);
			const auto existing = std::find_if(m_overrides.begin(), m_overrides.end(), [&](const auto& item)
			                                   { return item.mod_id == mod_id && item.handle == handle; });
			if (existing == m_overrides.end())
				m_overrides.push_back(override_value{std::move(mod_id), priority, handle, value});
			else
			{
				existing->priority = priority;
				existing->value = value;
			}
			return true;
		}

		void clear_override(std::string_view mod_id, instance_handle handle = {})
		{
			validate_handle(handle);
			std::lock_guard lock(m_mutex);
			std::erase_if(m_overrides,
			              [&](const auto& item) { return item.mod_id == mod_id && item.handle == handle; });
		}

	private:
		[[nodiscard]] std::vector<mod_setting> get_mod_settings() const
		{
			if constexpr (!detail::setting_codec<T>::supported)
				return {};
			else
			{
				if (m_scope == property_scope::instance)
					return {};
				std::lock_guard lock(m_mutex);
				std::vector<mod_setting> result;
				for (const auto& item : m_overrides)
					if (!item.handle)
						result.push_back(mod_setting{
						    item.mod_id, m_id, std::string(detail::setting_codec<T>::type_name),
						    detail::setting_codec<T>::encode(item.value), m_scope, item.priority});
				return result;
			}
		}

		bool set_mod_setting(std::string_view mod_id, std::string_view value, std::string& error)
		{
			if (m_scope == property_scope::instance)
			{
				error = "instance-scoped properties cannot be saved as mod settings";
				return false;
			}
			if constexpr (!detail::setting_codec<T>::supported)
			{
				error = "this property type does not support persisted settings";
				return false;
			}
			else
			{
				priority_t priority{};
				{
					std::lock_guard lock(m_mutex);
					const auto found =
					    std::find_if(m_overrides.begin(), m_overrides.end(),
						             [&](const auto& item) { return item.mod_id == mod_id && !item.handle; });
					if (found == m_overrides.end())
					{
						error = "mod does not currently override this property";
						return false;
					}
					priority = found->priority;
				}

				try
				{
					const auto decoded = detail::setting_codec<T>::decode(value);
					if (!set_override(std::string(mod_id), decoded, priority))
					{
						error = "property validator rejected the setting value";
						return false;
					}
					return true;
				}
				catch (const std::exception& exception)
				{
					error = exception.what();
					return false;
				}
			}
		}

		struct override_value
		{
			std::string mod_id;
			priority_t priority{};
			instance_handle handle{};
			T value;
		};

		property_slot(std::string id, property_scope scope, T default_value, getter read, setter write,
		              validator validate, std::function<bool(instance_handle)> handle_is_valid,
		              std::shared_ptr<detail::diagnostic_store> diagnostics)
		    : m_id(std::move(id)), m_scope(scope), m_default_value(std::move(default_value)),
		      m_base_value(m_default_value), m_getter(std::move(read)), m_setter(std::move(write)),
		      m_validator(std::move(validate)), m_handle_is_valid(std::move(handle_is_valid)),
		      m_diagnostics(std::move(diagnostics))
		{
		}

		[[nodiscard]] bool valid_value(const T& value) const { return !m_validator || m_validator(value); }

		void validate_handle(instance_handle handle) const
		{
			if (m_scope == property_scope::instance)
			{
				if (!handle || !m_handle_is_valid(handle)) [[unlikely]]
					throw registry_error(error_code::invalid_instance, "A live instance handle is required",
					                     m_id);
			}
			else if (handle) [[unlikely]]
				throw registry_error(error_code::invalid_instance,
				                     "This property does not accept an instance handle", m_id);
		}

		void remove_mod(std::string_view mod_id)
		{
			std::lock_guard lock(m_mutex);
			std::erase_if(m_overrides, [&](const auto& item) { return item.mod_id == mod_id; });
		}

		void remove_instance(instance_handle handle)
		{
			std::lock_guard lock(m_mutex);
			m_instance_values.erase(handle);
			std::erase_if(m_overrides, [&](const auto& item) { return item.handle == handle; });
		}

		std::string m_id;
		property_scope m_scope;
		T m_default_value;
		T m_base_value;
		getter m_getter;
		setter m_setter;
		validator m_validator;
		std::function<bool(instance_handle)> m_handle_is_valid;
		std::shared_ptr<detail::diagnostic_store> m_diagnostics;
		mutable std::mutex m_mutex;
		std::vector<override_value> m_overrides;
		std::unordered_map<instance_handle, T, detail::instance_handle_hash> m_instance_values;

		friend class registry;
		template <class> friend struct detail::property_slot_holder;
	};

	namespace detail
	{
		template <class T> struct property_slot_holder : property_slot_base
		{
			property_slot<T> slot;

			property_slot_holder(std::string id, property_scope scope, T default_value,
			                     typename property_slot<T>::getter getter,
			                     typename property_slot<T>::setter setter,
			                     typename property_slot<T>::validator validator,
			                     std::function<bool(instance_handle)> handle_is_valid,
			                     std::shared_ptr<diagnostic_store> diagnostics)
			    : slot(std::move(id), scope, std::move(default_value), std::move(getter), std::move(setter),
				       std::move(validator), std::move(handle_is_valid), std::move(diagnostics))
			{
			}

			void remove_mod(std::string_view mod_id) override { slot.remove_mod(mod_id); }
			void remove_instance(instance_handle handle) override { slot.remove_instance(handle); }
			[[nodiscard]] std::vector<mod_setting> get_mod_settings() const override
			{
				return slot.get_mod_settings();
			}
			bool set_mod_setting(std::string_view mod_id, std::string_view value, std::string& error) override
			{
				return slot.set_mod_setting(mod_id, value, error);
			}
		};
	} // namespace detail

	template <class Base>
	    requires std::derived_from<Base, modable>
	class class_registry
	{
	public:
		using factory = std::function<std::unique_ptr<Base>()>;

		class_registry(const class_registry&) = delete;
		class_registry& operator=(const class_registry&) = delete;

		[[nodiscard]] const std::string& id() const noexcept { return m_id; }

		template <class F>
		    requires std::constructible_from<factory, F>
		void add_builtin(std::string implementation_id, F&& make)
		{
			detail::require_id(implementation_id, "implementation_id");
			factory create(std::forward<F>(make));
			if (!create) [[unlikely]]
				throw registry_error(error_code::invalid_value, "Class factory is empty", m_id);

			std::lock_guard lock(m_mutex);
			if (m_implementations.contains(implementation_id)) [[unlikely]]
				throw registry_error(error_code::duplicate_extension,
				                     "Implementation ID is already registered", implementation_id);
			m_implementations.emplace(std::move(implementation_id),
			                          implementation{std::move(create), {}, {}});
		}

		template <class F>
		    requires std::constructible_from<factory, F>
		bool add_implementation(std::string mod_id, std::string implementation_id, F&& make)
		{
			detail::require_id(mod_id, "mod_id");
			detail::require_id(implementation_id, "implementation_id");
			factory create(std::forward<F>(make));
			if (!create) [[unlikely]]
				throw registry_error(error_code::invalid_value, "Class factory is empty", m_id, mod_id);

			std::lock_guard lock(m_mutex);
			if (m_implementations.contains(implementation_id))
			{
				detail::report(m_diagnostics, error_code::registration_conflict, mod_id, implementation_id,
				               "Implementation ID is already registered");
				return false;
			}
			m_implementations.emplace(std::move(implementation_id),
			                          implementation{std::move(create), std::move(mod_id), {}});
			return true;
		}

		template <class F>
		    requires std::constructible_from<factory, F>
		bool replace_factory(std::string mod_id, std::string implementation_id, F&& make)
		{
			detail::require_id(mod_id, "mod_id");
			detail::require_id(implementation_id, "implementation_id");
			factory create(std::forward<F>(make));
			if (!create) [[unlikely]]
				throw registry_error(error_code::invalid_value, "Class factory is empty", implementation_id,
				                     mod_id);

			std::lock_guard lock(m_mutex);
			const auto implementation = m_implementations.find(implementation_id);
			if (implementation == m_implementations.end() || !implementation->second.owner.empty())
			{
				detail::report(m_diagnostics, error_code::unknown_extension, mod_id, implementation_id,
				               "Only an existing built-in implementation factory can be replaced");
				return false;
			}

			for (const auto& existing : implementation->second.replacements)
			{
				if (existing->mod_id == mod_id) [[unlikely]]
					throw registry_error(error_code::duplicate_registration,
					                     "Mod already replaced this implementation factory",
					                     implementation_id, mod_id);
			}

			const auto has_active = std::any_of(implementation->second.replacements.begin(),
			                                    implementation->second.replacements.end(),
			                                    [](const auto& item) { return item->active.load(); });
			auto replacement = std::make_shared<factory_registration>(std::move(mod_id), std::move(create));
			if (has_active)
			{
				const auto owner = replacement->mod_id;
				replacement->active = false;
				for (const auto& existing : implementation->second.replacements)
					existing->active = false;
				implementation->second.replacements.push_back(std::move(replacement));
				detail::report(m_diagnostics, error_code::registration_conflict, owner, implementation_id,
				               "Conflicting factory replacements were disabled");
				return false;
			}
			implementation->second.replacements.push_back(std::move(replacement));
			return true;
		}

		[[nodiscard]] std::unique_ptr<Base> create(std::string_view implementation_id) const
		{
			factory selected;
			factory fallback;
			std::shared_ptr<factory_registration> replacement;
			std::string owner;
			{
				std::lock_guard lock(m_mutex);
				const auto implementation = m_implementations.find(std::string(implementation_id));
				if (implementation == m_implementations.end()) [[unlikely]]
					throw registry_error(error_code::unknown_extension, "Unknown class implementation",
					                     std::string(implementation_id));

				fallback = implementation->second.make;
				owner = implementation->second.owner;
				if (owner.empty())
					for (const auto& candidate : implementation->second.replacements)
						if (candidate->active.load())
							replacement = candidate;
				selected = replacement ? replacement->make : fallback;
			}

			try
			{
				auto object = selected();
				if (object)
					return object;
				throw std::runtime_error("Class factory returned null");
			}
			catch (...)
			{
				if (replacement)
				{
					replacement->active = false;
					detail::report(m_diagnostics, error_code::factory_failed, replacement->mod_id,
					               std::string(implementation_id),
					               "Replacement factory failed; built-in factory was used");
					try
					{
						return fallback();
					}
					catch (...)
					{
					}
				}
				else
					detail::report(m_diagnostics, error_code::factory_failed, owner,
					               std::string(implementation_id), "Class factory failed");
				return {};
			}
		}

	private:
		struct factory_registration
		{
			std::string mod_id;
			factory make;
			std::atomic_bool active{true};

			factory_registration(std::string owner, factory value)
			    : mod_id(std::move(owner)), make(std::move(value))
			{
			}
		};

		struct implementation
		{
			factory make;
			std::string owner;
			std::vector<std::shared_ptr<factory_registration>> replacements;
		};

		class_registry(std::string id, std::shared_ptr<detail::diagnostic_store> diagnostics)
		    : m_id(std::move(id)), m_diagnostics(std::move(diagnostics))
		{
		}

		void remove_mod(std::string_view mod_id)
		{
			std::lock_guard lock(m_mutex);
			std::erase_if(m_implementations, [&](auto& pair)
			              { return !pair.second.owner.empty() && pair.second.owner == mod_id; });
			for (auto& [_, implementation] : m_implementations)
				std::erase_if(implementation.replacements,
				              [&](const auto& item) { return item->mod_id == mod_id; });
		}

		std::string m_id;
		std::shared_ptr<detail::diagnostic_store> m_diagnostics;
		mutable std::mutex m_mutex;
		std::unordered_map<std::string, implementation> m_implementations;

		friend class registry;
		template <class> friend struct detail::class_registry_holder;
	};

	namespace detail
	{
		template <class Base> struct class_registry_holder : class_registry_base
		{
			class_registry<Base> classes;

			class_registry_holder(std::string id, std::shared_ptr<diagnostic_store> diagnostics)
			    : classes(std::move(id), std::move(diagnostics))
			{
			}

			void remove_mod(std::string_view mod_id) override { classes.remove_mod(mod_id); }
		};
	} // namespace detail

} // namespace mngl::mods
