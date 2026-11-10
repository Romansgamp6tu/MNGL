#pragma once

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mngl::mods
{
	class mod_loader;

	using priority_t = std::int32_t;

	enum class error_code
	{
		invalid_id,
		duplicate_extension,
		unknown_extension,
		type_mismatch,
		duplicate_registration,
		registration_conflict,
		invalid_instance,
		invalid_value,
		callback_failed,
		factory_failed
	};

	enum class property_scope
	{
		global,
		class_default,
		instance
	};

	struct mod_setting
	{
		std::string mod_id;
		std::string property_id;
		std::string type;
		std::string value;
		property_scope scope{property_scope::global};
		priority_t priority{};
	};

	struct diagnostic
	{
		error_code code{};
		std::string mod_id;
		std::string extension_id;
		std::string message;
	};

	class registry_error : public std::runtime_error
	{
	public:
		registry_error(error_code code, std::string message, std::string extension_id = {},
		               std::string mod_id = {})
		    : std::runtime_error(std::move(message)), m_code(code), m_extension_id(std::move(extension_id)),
		      m_mod_id(std::move(mod_id))
		{
		}

		[[nodiscard]] error_code code() const noexcept { return m_code; }
		[[nodiscard]] const std::string& extension_id() const noexcept { return m_extension_id; }
		[[nodiscard]] const std::string& mod_id() const noexcept { return m_mod_id; }

	private:
		error_code m_code;
		std::string m_extension_id;
		std::string m_mod_id;
	};

	namespace detail
	{
		class module_lifetime;
	}

	class modable
	{
	public:
		virtual ~modable();

		// Internal lifetime pin used by mod-created factory instances.
		void mngl_pin_module_lifetime(const std::shared_ptr<detail::module_lifetime>& lifetime);

	private:
		std::shared_ptr<detail::module_lifetime> m_module_lifetime;
	};

	namespace detail
	{
		struct instance_handle_hash;
	}

	class instance_handle
	{
	public:
		instance_handle() = default;
		[[nodiscard]] explicit operator bool() const noexcept { return m_value != 0; }
		friend bool operator==(instance_handle, instance_handle) = default;

	private:
		explicit instance_handle(std::uint64_t value) : m_value(value) {}
		std::uint64_t m_value{};
		friend class registry;
		friend struct detail::instance_handle_hash;
	};

	namespace detail
	{
		class module_lifetime : public std::enable_shared_from_this<module_lifetime>
		{
		public:
			class call_lease
			{
			public:
				call_lease() = default;
				explicit call_lease(std::shared_ptr<module_lifetime> owner) : m_owner(std::move(owner)) {}
				call_lease(const call_lease&) = delete;
				call_lease& operator=(const call_lease&) = delete;
				call_lease(call_lease&& other) noexcept : m_owner(std::exchange(other.m_owner, {})) {}
				call_lease& operator=(call_lease&& other) noexcept
				{
					if (this != &other)
					{
						release();
						m_owner = std::exchange(other.m_owner, {});
					}
					return *this;
				}
				~call_lease() { release(); }

			private:
				void release() noexcept
				{
					if (m_owner)
					{
						auto owner = std::exchange(m_owner, {});
						owner->leave_call();
					}
				}

				std::shared_ptr<module_lifetime> m_owner;
			};

			explicit module_lifetime(std::function<void()> close_library)
			    : m_close_library(std::move(close_library))
			{
			}

			[[nodiscard]] call_lease enter()
			{
				std::lock_guard lock(m_mutex);
				if (!m_accepting) [[unlikely]]
					throw std::runtime_error("Mod is unloading");
				++m_active_calls;
				return call_lease(shared_from_this());
			}

			void pin_object()
			{
				std::lock_guard lock(m_mutex);
				++m_live_objects;
			}

			void release_object() noexcept
			{
				std::lock_guard lock(m_mutex);
				if (m_live_objects != 0)
					--m_live_objects;
			}

			void request_shutdown(std::function<void()> unload_callback) noexcept
			{
				{
					std::lock_guard lock(m_mutex);
					m_accepting = false;
					if (!m_shutdown_requested)
					{
						m_shutdown_requested = true;
						m_unload_callback = std::move(unload_callback);
					}
				}
			}

			void stop_accepting() noexcept
			{
				std::lock_guard lock(m_mutex);
				m_accepting = false;
			}

			[[nodiscard]] bool is_accepting() const noexcept
			{
				std::lock_guard lock(m_mutex);
				return m_accepting;
			}

			[[nodiscard]] std::pair<std::size_t, std::size_t> outstanding() const noexcept
			{
				std::lock_guard lock(m_mutex);
				return {m_active_calls, m_live_objects};
			}

			void finish_shutdown() noexcept
			{
				std::function<void()> unload;
				{
					std::lock_guard lock(m_mutex);
					if (!m_shutdown_requested || m_unload_called || m_active_calls != 0 ||
					    m_live_objects != 0)
						return;
					m_unload_called = true;
					unload = std::move(m_unload_callback);
				}
				if (unload)
				{
					try
					{
						unload();
					}
					catch (...)
					{
					}
				}
			}

			void close_library() noexcept
			{
				std::function<void()> close;
				{
					std::lock_guard lock(m_mutex);
					if (m_library_closed)
						return;
					m_library_closed = true;
					close = std::move(m_close_library);
				}
				if (close)
				{
					try
					{
						close();
					}
					catch (...)
					{
					}
				}
			}

			// The loader closes the native handle after collection, from host code.
			// In particular, object destruction must not unload its own module frame.
			~module_lifetime() = default;

		private:
			void leave_call() noexcept
			{
				std::lock_guard lock(m_mutex);
				if (m_active_calls != 0)
					--m_active_calls;
			}

			mutable std::mutex m_mutex;
			bool m_accepting{true};
			bool m_shutdown_requested{};
			bool m_unload_called{};
			bool m_library_closed{};
			std::size_t m_active_calls{};
			std::size_t m_live_objects{};
			std::function<void()> m_unload_callback;
			std::function<void()> m_close_library;
		};

	} // namespace detail

	inline modable::~modable()
	{
		if (m_module_lifetime)
			m_module_lifetime->release_object();
	}

	inline void modable::mngl_pin_module_lifetime(const std::shared_ptr<detail::module_lifetime>& lifetime)
	{
		if (!lifetime)
			return;
		if (m_module_lifetime) [[unlikely]]
			throw std::logic_error("Mod object already has a module lifetime pin");
		lifetime->pin_object();
		m_module_lifetime = lifetime;
	}

	namespace detail
	{

		template <class Signature> struct guarded_function;

		template <class R, class... Args> struct guarded_function<R(Args...)>
		{
			template <class F>
			static std::function<R(Args...)> make(std::shared_ptr<module_lifetime> lifetime, F&& callback)
			{
				std::function<R(Args...)> function(std::forward<F>(callback));
				return [lifetime = std::move(lifetime), function = std::move(function)](Args... args) -> R
				{
					auto lease = lifetime->enter();
					if constexpr (std::is_void_v<R>)
					{
						std::invoke(function, std::forward<Args>(args)...);
						return;
					}
					else
						return std::invoke(function, std::forward<Args>(args)...);
				};
			}

			template <class F>
			static std::function<R(std::function<R(Args...)>, Args...)> make_around(
			    std::shared_ptr<module_lifetime> lifetime, F&& callback)
			{
				using around_type = std::function<R(std::function<R(Args...)>, Args...)>;
				around_type function(std::forward<F>(callback));
				return [lifetime = std::move(lifetime),
				        function = std::move(function)](std::function<R(Args...)> next, Args... args) -> R
				{
					auto lease = lifetime->enter();
					if constexpr (std::is_void_v<R>)
					{
						std::invoke(function, std::move(next), std::forward<Args>(args)...);
						return;
					}
					else
						return std::invoke(function, std::move(next), std::forward<Args>(args)...);
				};
			}
		};

		template <class T> void pin_module_object(T& object, const std::shared_ptr<module_lifetime>& lifetime)
		{
			object.mngl_pin_module_lifetime(lifetime);
		}

		inline bool valid_id(std::string_view id) noexcept
		{
			if (id.empty())
				return false;

			const auto is_lower_or_digit = [](char c)
			{ return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };

			if (!is_lower_or_digit(id.front()))
				return false;

			return std::all_of(id.begin(), id.end(), [&](char c)
			                   { return is_lower_or_digit(c) || c == '.' || c == '_' || c == '-'; });
		}

		inline void require_id(std::string_view id, std::string_view kind)
		{
			if (!valid_id(id)) [[unlikely]]
				throw registry_error(error_code::invalid_id,
				                     std::string(kind) + " must match [a-z0-9][a-z0-9._-]*", std::string(id));
		}

		struct diagnostic_store
		{
			mutable std::mutex mutex;
			std::vector<diagnostic> values;

			void add(diagnostic value)
			{
				std::lock_guard lock(mutex);
				values.push_back(std::move(value));
			}

			[[nodiscard]] std::vector<diagnostic> snapshot() const
			{
				std::lock_guard lock(mutex);
				return values;
			}
		};

		inline void report(const std::shared_ptr<diagnostic_store>& store, error_code code,
		                   std::string mod_id, std::string extension_id, std::string message)
		{
			store->add(diagnostic{code, std::move(mod_id), std::move(extension_id), std::move(message)});
		}

		struct function_slot_base
		{
			virtual ~function_slot_base() = default;
			virtual void remove_mod(std::string_view mod_id) = 0;
		};

		struct property_slot_base
		{
			virtual ~property_slot_base() = default;
			virtual void remove_mod(std::string_view mod_id) = 0;
			virtual void remove_instance(instance_handle handle) = 0;
			[[nodiscard]] virtual std::vector<mod_setting> get_mod_settings() const = 0;
			virtual bool set_mod_setting(std::string_view mod_id, std::string_view value,
			                             std::string& error) = 0;
		};

		struct class_registry_base
		{
			virtual ~class_registry_base() = default;
			virtual void remove_mod(std::string_view mod_id) = 0;
		};

		template <class Signature> struct function_slot_holder;
		template <class T> struct property_slot_holder;
		template <class Base> struct class_registry_holder;

		struct instance_handle_hash
		{
			std::size_t operator()(instance_handle value) const noexcept
			{
				return std::hash<std::uint64_t>{}(value.m_value);
			}
		};

		template <class R, bool = std::is_void_v<R>> struct next_result
		{
			using type = std::optional<R>;
		};

		template <class R> struct next_result<R, true>
		{
			using type = bool;
		};

		template <class T, class = void> struct setting_codec
		{
			static constexpr bool supported = false;
		};

		template <> struct setting_codec<std::string>
		{
			static constexpr bool supported = true;
			static constexpr std::string_view type_name = "string";
			static std::string encode(const std::string& value) { return value; }
			static std::string decode(std::string_view value) { return std::string(value); }
		};

		template <> struct setting_codec<bool>
		{
			static constexpr bool supported = true;
			static constexpr std::string_view type_name = "bool";
			static std::string encode(bool value) { return value ? "true" : "false"; }
			static bool decode(std::string_view value)
			{
				if (value == "true")
					return true;
				if (value == "false")
					return false;
				throw std::runtime_error("expected true or false");
			}
		};

		template <std::integral T>
		    requires(!std::same_as<T, bool>)
		struct setting_codec<T>
		{
			static constexpr bool supported = true;
			static constexpr std::string_view type_name = []
			{
				if constexpr (std::is_signed_v<T>)
					return sizeof(T) == 1   ? std::string_view("int8")
					       : sizeof(T) == 2 ? std::string_view("int16")
					       : sizeof(T) == 4 ? std::string_view("int32")
					                        : std::string_view("int64");
				else
					return sizeof(T) == 1   ? std::string_view("uint8")
					       : sizeof(T) == 2 ? std::string_view("uint16")
					       : sizeof(T) == 4 ? std::string_view("uint32")
					                        : std::string_view("uint64");
			}();

			static std::string encode(T value)
			{
				if constexpr (std::is_signed_v<T>)
					return std::to_string(static_cast<std::int64_t>(value));
				else
					return std::to_string(static_cast<std::uint64_t>(value));
			}

			static T decode(std::string_view value)
			{
				if constexpr (std::is_signed_v<T>)
				{
					std::int64_t parsed{};
					const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
					if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
					    parsed < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
					    parsed > static_cast<std::int64_t>(std::numeric_limits<T>::max())) [[unlikely]]
						throw std::runtime_error("integer value is out of range or malformed");
					return static_cast<T>(parsed);
				}
				else
				{
					std::uint64_t parsed{};
					const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
					if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
					    parsed > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) [[unlikely]]
						throw std::runtime_error("integer value is out of range or malformed");
					return static_cast<T>(parsed);
				}
			}
		};

		template <std::floating_point T> struct setting_codec<T>
		{
			static constexpr bool supported = sizeof(T) == sizeof(float) || sizeof(T) == sizeof(double);
			static constexpr std::string_view type_name =
			    sizeof(T) == sizeof(float) ? std::string_view("float32") : std::string_view("float64");

			static std::string encode(T value)
			{
				if (!std::isfinite(value)) [[unlikely]]
					throw std::runtime_error("non-finite settings values cannot be saved");
				char buffer[128]{};
				const auto result =
				    std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general,
					              std::numeric_limits<T>::max_digits10);
				if (result.ec != std::errc{}) [[unlikely]]
					throw std::runtime_error("could not encode floating-point setting");
				return std::string(buffer, result.ptr);
			}

			static T decode(std::string_view value)
			{
				std::istringstream input{std::string(value)};
				input.imbue(std::locale::classic());
				T parsed{};
				input >> std::noskipws >> parsed;
				if (!input || input.peek() != std::char_traits<char>::eof() || !std::isfinite(parsed)) [[unlikely]]
					throw std::runtime_error("floating-point value is malformed or non-finite");
				return parsed;
			}
		};
	} // namespace detail

} // namespace mngl::mods
