#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
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
        registry_error(error_code code, std::string message,
            std::string extension_id = {}, std::string mod_id = {})
            : std::runtime_error(std::move(message)),
              m_code(code),
              m_extension_id(std::move(extension_id)),
              m_mod_id(std::move(mod_id))
        {}

        [[nodiscard]] error_code code() const noexcept { return m_code; }
        [[nodiscard]] const std::string& extension_id() const noexcept { return m_extension_id; }
        [[nodiscard]] const std::string& mod_id() const noexcept { return m_mod_id; }

    private:
        error_code m_code;
        std::string m_extension_id;
        std::string m_mod_id;
    };

    namespace detail { class module_lifetime; }

    class modable
    {
    public:
        virtual ~modable();

        // Internal lifetime pin used by mod-created factory instances.
        void mngl_pin_module_lifetime(
            const std::shared_ptr<detail::module_lifetime>& lifetime);

    private:
        std::shared_ptr<detail::module_lifetime> m_module_lifetime;
    };

    namespace detail { struct instance_handle_hash; }

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
                explicit call_lease(std::shared_ptr<module_lifetime> owner)
                    : m_owner(std::move(owner)) {}
                call_lease(const call_lease&) = delete;
                call_lease& operator=(const call_lease&) = delete;
                call_lease(call_lease&& other) noexcept
                    : m_owner(std::exchange(other.m_owner, {})) {}
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
                : m_close_library(std::move(close_library)) {}

            [[nodiscard]] call_lease enter()
            {
                std::lock_guard lock(m_mutex);
                if (!m_accepting)
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
                return { m_active_calls, m_live_objects };
            }

            void finish_shutdown() noexcept
            {
                std::function<void()> unload;
                {
                    std::lock_guard lock(m_mutex);
                    if (!m_shutdown_requested || m_unload_called ||
                        m_active_calls != 0 || m_live_objects != 0)
                        return;
                    m_unload_called = true;
                    unload = std::move(m_unload_callback);
                }
                if (unload)
                {
                    try { unload(); }
                    catch (...) {}
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
                    try { close(); }
                    catch (...) {}
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
            bool m_accepting{ true };
            bool m_shutdown_requested{};
            bool m_unload_called{};
            bool m_library_closed{};
            std::size_t m_active_calls{};
            std::size_t m_live_objects{};
            std::function<void()> m_unload_callback;
            std::function<void()> m_close_library;
        };

    }

    inline modable::~modable()
    {
        if (m_module_lifetime)
            m_module_lifetime->release_object();
    }

    inline void modable::mngl_pin_module_lifetime(
        const std::shared_ptr<detail::module_lifetime>& lifetime)
    {
        if (!lifetime)
            return;
        if (m_module_lifetime)
            throw std::logic_error("Mod object already has a module lifetime pin");
        lifetime->pin_object();
        m_module_lifetime = lifetime;
    }

    namespace detail
    {

        template<class Signature> struct guarded_function;

        template<class R, class... Args>
        struct guarded_function<R(Args...)>
        {
            template<class F>
            static std::function<R(Args...)> make(
                std::shared_ptr<module_lifetime> lifetime, F&& callback)
            {
                std::function<R(Args...)> function(std::forward<F>(callback));
                return [lifetime = std::move(lifetime), function = std::move(function)]
                    (Args... args) -> R
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

            template<class F>
            static std::function<R(std::function<R(Args...)>, Args...)> make_around(
                std::shared_ptr<module_lifetime> lifetime, F&& callback)
            {
                using around_type = std::function<R(std::function<R(Args...)>, Args...)>;
                around_type function(std::forward<F>(callback));
                return [lifetime = std::move(lifetime), function = std::move(function)]
                    (std::function<R(Args...)> next, Args... args) -> R
                {
                    auto lease = lifetime->enter();
                    if constexpr (std::is_void_v<R>)
                    {
                        std::invoke(function, std::move(next), std::forward<Args>(args)...);
                        return;
                    }
                    else
                        return std::invoke(function, std::move(next),
                            std::forward<Args>(args)...);
                };
            }
        };

        template<class T>
        void pin_module_object(T& object,
            const std::shared_ptr<module_lifetime>& lifetime)
        {
            object.mngl_pin_module_lifetime(lifetime);
        }

        inline bool valid_id(std::string_view id) noexcept
        {
            if (id.empty())
                return false;

            const auto is_lower_or_digit = [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
            };

            if (!is_lower_or_digit(id.front()))
                return false;

            return std::all_of(id.begin(), id.end(), [&](char c) {
                return is_lower_or_digit(c) || c == '.' || c == '_' || c == '-';
            });
        }

        inline void require_id(std::string_view id, std::string_view kind)
        {
            if (!valid_id(id))
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

        inline void report(const std::shared_ptr<diagnostic_store>& store,
            error_code code, std::string mod_id, std::string extension_id, std::string message)
        {
            store->add(diagnostic{ code, std::move(mod_id),
                std::move(extension_id), std::move(message) });
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
        };

        struct class_registry_base
        {
            virtual ~class_registry_base() = default;
            virtual void remove_mod(std::string_view mod_id) = 0;
        };

        template<class Signature> struct function_slot_holder;
        template<class T> struct property_slot_holder;
        template<class Base> struct class_registry_holder;

        struct instance_handle_hash
        {
            std::size_t operator()(instance_handle value) const noexcept
            {
                return std::hash<std::uint64_t>{}(value.m_value);
            }
        };

        template<class R, bool = std::is_void_v<R>>
        struct next_result
        {
            using type = std::optional<R>;
        };

        template<class R>
        struct next_result<R, true>
        {
            using type = bool;
        };
    }

    template<class Signature>
    class function_slot;

    template<class R, class... Args>
    class function_slot<R(Args...)>
    {
        static_assert(!std::is_reference_v<R>, "Function slots cannot return references.");
        static_assert(std::is_void_v<R> || std::copy_constructible<R>,
            "Function slot return values must be copy constructible.");
        static_assert((!std::is_rvalue_reference_v<Args> && ...),
            "Function slots do not support rvalue-reference arguments.");
        static_assert(((std::is_lvalue_reference_v<Args> ||
            std::copy_constructible<std::remove_cvref_t<Args>>) && ...),
            "By-value function slot arguments must be copy constructible.");

    public:
        using signature = R(Args...);
        using callback = std::function<signature>;
        using next_callback = callback;
        using around_callback = std::function<R(next_callback, Args...)>;

        function_slot(const function_slot&) = delete;
        function_slot& operator=(const function_slot&) = delete;

        [[nodiscard]] const std::string& id() const noexcept { return m_id; }

        template<class F>
            requires std::constructible_from<callback, F>
        bool replace(std::string mod_id, F&& function)
        {
            detail::require_id(mod_id, "mod_id");
            callback callable(std::forward<F>(function));
            if (!callable)
                throw registry_error(error_code::invalid_value,
                    "Replacement callback is empty", m_id, mod_id);

            auto registration = std::make_shared<replacement>(
                std::move(mod_id), std::move(callable));
            std::lock_guard lock(m_state->mutex);

            for (const auto& existing : m_state->replacements)
            {
                if (existing->mod_id == registration->mod_id)
                    throw registry_error(error_code::duplicate_registration,
                        "Mod already registered a replacement for this slot",
                        m_id, registration->mod_id);
            }

            const auto has_active = std::any_of(
                m_state->replacements.begin(), m_state->replacements.end(),
                [](const auto& item) { return item->active.load(); });

            if (has_active)
            {
                const auto owner = registration->mod_id;
                registration->active = false;
                for (const auto& existing : m_state->replacements)
                    existing->active = false;
                m_state->replacements.push_back(std::move(registration));
                detail::report(m_state->diagnostics, error_code::registration_conflict,
                    owner, m_id, "Conflicting replacements were disabled for this slot");
                return false;
            }

            m_state->replacements.push_back(std::move(registration));
            return true;
        }

        template<class F>
            requires std::constructible_from<around_callback, F>
        void around(std::string mod_id, priority_t priority, F&& function)
        {
            detail::require_id(mod_id, "mod_id");
            around_callback callable(std::forward<F>(function));
            if (!callable)
                throw registry_error(error_code::invalid_value,
                    "Around callback is empty", m_id, mod_id);

            auto registration = std::make_shared<around_registration>(
                std::move(mod_id), priority, std::move(callable));
            std::lock_guard lock(m_state->mutex);

            if (std::any_of(m_state->around.begin(), m_state->around.end(),
                [&](const auto& item) { return item->mod_id == registration->mod_id; }))
                throw registry_error(error_code::duplicate_registration,
                    "Mod already registered an around callback for this slot",
                    m_id, registration->mod_id);

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
                    base = [this, selected, fallback](Args... call_args) -> R {
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

            std::sort(around.begin(), around.end(), [](const auto& left, const auto& right) {
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
            std::atomic_bool active{ true };

            replacement(std::string owner, callback value)
                : mod_id(std::move(owner)), function(std::move(value)) {}
        };

        struct around_registration
        {
            std::string mod_id;
            priority_t priority{};
            around_callback function;
            std::atomic_bool active{ true };

            around_registration(std::string owner, priority_t order, around_callback value)
                : mod_id(std::move(owner)), priority(order), function(std::move(value)) {}
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
            if (!fallback)
                throw registry_error(error_code::invalid_value,
                    "Function slot requires a default implementation", m_id);
            m_state->fallback = std::move(fallback);
            m_state->diagnostics = std::move(diagnostics);
        }

        void remove_mod(std::string_view mod_id)
        {
            std::lock_guard lock(m_state->mutex);
            std::erase_if(m_state->replacements,
                [&](const auto& item) { return item->mod_id == mod_id; });
            std::erase_if(m_state->around,
                [&](const auto& item) { return item->mod_id == mod_id; });
        }

        R invoke(const std::vector<std::shared_ptr<around_registration>>& around,
            std::size_t index, const callback& base, Args... args) const
        {
            if (index == around.size())
                return base(std::forward<Args>(args)...);

            const auto hook = around[index];
            using result_type = typename detail::next_result<R>::type;
            bool next_called = false;
            result_type next_result{};

            next_callback next = [&](Args... next_args) -> R {
                if (next_called)
                    throw std::logic_error("around next callback may only be called once");
                next_called = true;
                if constexpr (std::is_void_v<R>)
                {
                    invoke(around, index + 1, base, std::forward<Args>(next_args)...);
                    next_result = true;
                }
                else
                    next_result.emplace(invoke(
                        around, index + 1, base, std::forward<Args>(next_args)...));
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
                detail::report(m_state->diagnostics, error_code::callback_failed,
                    hook->mod_id, m_id, "Around callback threw; registration disabled");
                if (!next_called)
                    return invoke(around, index + 1, base, args...);
                if constexpr (!std::is_void_v<R>)
                    return *next_result;
            }
        }

        std::string m_id;
        std::shared_ptr<state> m_state;

        friend class registry;
        template<class> friend struct detail::function_slot_holder;
    };

    namespace detail
    {
        template<class R, class... Args>
        struct function_slot_holder<R(Args...)> : function_slot_base
        {
            using slot_type = function_slot<R(Args...)>;
            slot_type slot;

            function_slot_holder(std::string id, typename slot_type::callback fallback,
                std::shared_ptr<diagnostic_store> diagnostics)
                : slot(std::move(id), std::move(fallback), std::move(diagnostics)) {}

            void remove_mod(std::string_view mod_id) override { slot.remove_mod(mod_id); }
        };
    }

    template<class T>
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

        bool set_override(std::string mod_id, const T& value,
            priority_t priority = 0, instance_handle handle = {})
        {
            detail::require_id(mod_id, "mod_id");
            validate_handle(handle);
            if (!valid_value(value))
            {
                detail::report(m_diagnostics, error_code::invalid_value,
                    mod_id, m_id, "Property override rejected by validator");
                return false;
            }

            std::lock_guard lock(m_mutex);
            const auto existing = std::find_if(m_overrides.begin(), m_overrides.end(),
                [&](const auto& item) {
                    return item.mod_id == mod_id && item.handle == handle;
                });
            if (existing == m_overrides.end())
                m_overrides.push_back(override_value{
                    std::move(mod_id), priority, handle, value });
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
            std::erase_if(m_overrides, [&](const auto& item) {
                return item.mod_id == mod_id && item.handle == handle;
            });
        }

    private:
        struct override_value
        {
            std::string mod_id;
            priority_t priority{};
            instance_handle handle{};
            T value;
        };

        property_slot(std::string id, property_scope scope, T default_value,
            getter read, setter write, validator validate,
            std::function<bool(instance_handle)> handle_is_valid,
            std::shared_ptr<detail::diagnostic_store> diagnostics)
            : m_id(std::move(id)), m_scope(scope), m_default_value(std::move(default_value)),
              m_base_value(m_default_value), m_getter(std::move(read)),
              m_setter(std::move(write)), m_validator(std::move(validate)),
              m_handle_is_valid(std::move(handle_is_valid)),
              m_diagnostics(std::move(diagnostics))
        {}

        [[nodiscard]] bool valid_value(const T& value) const
        {
            return !m_validator || m_validator(value);
        }

        void validate_handle(instance_handle handle) const
        {
            if (m_scope == property_scope::instance)
            {
                if (!handle || !m_handle_is_valid(handle))
                    throw registry_error(error_code::invalid_instance,
                        "A live instance handle is required", m_id);
            }
            else if (handle)
                throw registry_error(error_code::invalid_instance,
                    "This property does not accept an instance handle", m_id);
        }

        void remove_mod(std::string_view mod_id)
        {
            std::lock_guard lock(m_mutex);
            std::erase_if(m_overrides,
                [&](const auto& item) { return item.mod_id == mod_id; });
        }

        void remove_instance(instance_handle handle)
        {
            std::lock_guard lock(m_mutex);
            m_instance_values.erase(handle);
            std::erase_if(m_overrides,
                [&](const auto& item) { return item.handle == handle; });
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
        template<class> friend struct detail::property_slot_holder;
    };

    namespace detail
    {
        template<class T>
        struct property_slot_holder : property_slot_base
        {
            property_slot<T> slot;

            property_slot_holder(std::string id, property_scope scope, T default_value,
                typename property_slot<T>::getter getter,
                typename property_slot<T>::setter setter,
                typename property_slot<T>::validator validator,
                std::function<bool(instance_handle)> handle_is_valid,
                std::shared_ptr<diagnostic_store> diagnostics)
                : slot(std::move(id), scope, std::move(default_value),
                    std::move(getter), std::move(setter), std::move(validator),
                    std::move(handle_is_valid), std::move(diagnostics)) {}

            void remove_mod(std::string_view mod_id) override { slot.remove_mod(mod_id); }
            void remove_instance(instance_handle handle) override { slot.remove_instance(handle); }
        };
    }

    template<class Base>
        requires std::derived_from<Base, modable>
    class class_registry
    {
    public:
        using factory = std::function<std::unique_ptr<Base>()>;

        class_registry(const class_registry&) = delete;
        class_registry& operator=(const class_registry&) = delete;

        [[nodiscard]] const std::string& id() const noexcept { return m_id; }

        template<class F>
            requires std::constructible_from<factory, F>
        void add_builtin(std::string implementation_id, F&& make)
        {
            detail::require_id(implementation_id, "implementation_id");
            factory create(std::forward<F>(make));
            if (!create)
                throw registry_error(error_code::invalid_value,
                    "Class factory is empty", m_id);

            std::lock_guard lock(m_mutex);
            if (m_implementations.contains(implementation_id))
                throw registry_error(error_code::duplicate_extension,
                    "Implementation ID is already registered", implementation_id);
            m_implementations.emplace(std::move(implementation_id), implementation{
                std::move(create), {}, {} });
        }

        template<class F>
            requires std::constructible_from<factory, F>
        bool add_implementation(std::string mod_id, std::string implementation_id, F&& make)
        {
            detail::require_id(mod_id, "mod_id");
            detail::require_id(implementation_id, "implementation_id");
            factory create(std::forward<F>(make));
            if (!create)
                throw registry_error(error_code::invalid_value,
                    "Class factory is empty", m_id, mod_id);

            std::lock_guard lock(m_mutex);
            if (m_implementations.contains(implementation_id))
            {
                detail::report(m_diagnostics, error_code::registration_conflict,
                    mod_id, implementation_id, "Implementation ID is already registered");
                return false;
            }
            m_implementations.emplace(std::move(implementation_id), implementation{
                std::move(create), std::move(mod_id), {} });
            return true;
        }

        template<class F>
            requires std::constructible_from<factory, F>
        bool replace_factory(std::string mod_id, std::string implementation_id, F&& make)
        {
            detail::require_id(mod_id, "mod_id");
            detail::require_id(implementation_id, "implementation_id");
            factory create(std::forward<F>(make));
            if (!create)
                throw registry_error(error_code::invalid_value,
                    "Class factory is empty", implementation_id, mod_id);

            std::lock_guard lock(m_mutex);
            const auto implementation = m_implementations.find(implementation_id);
            if (implementation == m_implementations.end() || !implementation->second.owner.empty())
            {
                detail::report(m_diagnostics, error_code::unknown_extension,
                    mod_id, implementation_id,
                    "Only an existing built-in implementation factory can be replaced");
                return false;
            }

            for (const auto& existing : implementation->second.replacements)
            {
                if (existing->mod_id == mod_id)
                    throw registry_error(error_code::duplicate_registration,
                        "Mod already replaced this implementation factory",
                        implementation_id, mod_id);
            }

            const auto has_active = std::any_of(
                implementation->second.replacements.begin(),
                implementation->second.replacements.end(),
                [](const auto& item) { return item->active.load(); });
            auto replacement = std::make_shared<factory_registration>(
                std::move(mod_id), std::move(create));
            if (has_active)
            {
                const auto owner = replacement->mod_id;
                replacement->active = false;
                for (const auto& existing : implementation->second.replacements)
                    existing->active = false;
                implementation->second.replacements.push_back(std::move(replacement));
                detail::report(m_diagnostics, error_code::registration_conflict,
                    owner, implementation_id, "Conflicting factory replacements were disabled");
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
                if (implementation == m_implementations.end())
                    throw registry_error(error_code::unknown_extension,
                        "Unknown class implementation", std::string(implementation_id));

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
                    detail::report(m_diagnostics, error_code::factory_failed,
                        replacement->mod_id, std::string(implementation_id),
                        "Replacement factory failed; built-in factory was used");
                    try { return fallback(); }
                    catch (...) {}
                }
                else
                    detail::report(m_diagnostics, error_code::factory_failed,
                        owner, std::string(implementation_id), "Class factory failed");
                return {};
            }
        }

    private:
        struct factory_registration
        {
            std::string mod_id;
            factory make;
            std::atomic_bool active{ true };

            factory_registration(std::string owner, factory value)
                : mod_id(std::move(owner)), make(std::move(value)) {}
        };

        struct implementation
        {
            factory make;
            std::string owner;
            std::vector<std::shared_ptr<factory_registration>> replacements;
        };

        class_registry(std::string id, std::shared_ptr<detail::diagnostic_store> diagnostics)
            : m_id(std::move(id)), m_diagnostics(std::move(diagnostics)) {}

        void remove_mod(std::string_view mod_id)
        {
            std::lock_guard lock(m_mutex);
            std::erase_if(m_implementations, [&](auto& pair) {
                return !pair.second.owner.empty() && pair.second.owner == mod_id;
            });
            for (auto& [_, implementation] : m_implementations)
                std::erase_if(implementation.replacements,
                    [&](const auto& item) { return item->mod_id == mod_id; });
        }

        std::string m_id;
        std::shared_ptr<detail::diagnostic_store> m_diagnostics;
        mutable std::mutex m_mutex;
        std::unordered_map<std::string, implementation> m_implementations;

        friend class registry;
        template<class> friend struct detail::class_registry_holder;
    };

    namespace detail
    {
        template<class Base>
        struct class_registry_holder : class_registry_base
        {
            class_registry<Base> classes;

            class_registry_holder(std::string id,
                std::shared_ptr<diagnostic_store> diagnostics)
                : classes(std::move(id), std::move(diagnostics)) {}

            void remove_mod(std::string_view mod_id) override { classes.remove_mod(mod_id); }
        };
    }

    class mod_context;

    class registry
    {
    public:
        registry() : m_diagnostics(std::make_shared<detail::diagnostic_store>()) {}
        registry(const registry&) = delete;
        registry& operator=(const registry&) = delete;
        registry(registry&&) = delete;
        registry& operator=(registry&&) = delete;

        template<class Signature, class F>
            requires std::constructible_from<std::function<Signature>, F>
        function_slot<Signature>& define_function(std::string id, F&& fallback)
        {
            detail::require_id(id, "extension_id");
            using holder = detail::function_slot_holder<Signature>;
            std::lock_guard lock(m_mutex);
            if (m_functions.contains(id) || m_properties.contains(id))
                throw registry_error(error_code::duplicate_extension,
                    "Extension ID is already registered", id);
            auto value = std::make_shared<holder>(std::move(id),
                std::function<Signature>(std::forward<F>(fallback)), m_diagnostics);
            auto* slot = &value->slot;
            m_functions.emplace(slot->id(), std::move(value));
            return *slot;
        }

        template<class T>
            requires std::copy_constructible<T>
        property_slot<T>& define_property(std::string id, property_scope scope,
            T default_value, typename property_slot<T>::getter getter = {},
            typename property_slot<T>::setter setter = {},
            typename property_slot<T>::validator validator = {})
        {
            detail::require_id(id, "extension_id");
            if (static_cast<bool>(getter) != static_cast<bool>(setter))
                throw registry_error(error_code::invalid_value,
                    "A property must provide both getter and setter, or neither", id);
            using holder = detail::property_slot_holder<T>;
            std::lock_guard lock(m_mutex);
            if (m_properties.contains(id) || m_functions.contains(id))
                throw registry_error(error_code::duplicate_extension,
                    "Extension ID is already registered", id);
            auto handle_is_valid = [this](instance_handle handle) {
                return is_live(handle);
            };
            auto value = std::make_shared<holder>(std::move(id), scope,
                std::move(default_value), std::move(getter), std::move(setter),
                std::move(validator), std::move(handle_is_valid), m_diagnostics);
            auto* slot = &value->slot;
            m_properties.emplace(slot->id(), std::move(value));
            return *slot;
        }

        template<class Base>
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
                if (!typed)
                    throw registry_error(error_code::type_mismatch,
                        "Class ID was already declared with a different base type", id);
                return typed->classes;
            }
            auto value = std::make_shared<holder>(id, m_diagnostics);
            auto* classes = &value->classes;
            m_classes.emplace(std::move(id), std::move(value));
            return *classes;
        }

        template<class Signature>
        function_slot<Signature>& function(std::string_view id)
        {
            using holder = detail::function_slot_holder<Signature>;
            std::lock_guard lock(m_mutex);
            const auto it = m_functions.find(std::string(id));
            if (it == m_functions.end())
                throw registry_error(error_code::unknown_extension,
                    "Unknown function slot", std::string(id));
            auto* typed = dynamic_cast<holder*>(it->second.get());
            if (!typed)
                throw registry_error(error_code::type_mismatch,
                    "Function slot signature does not match", std::string(id));
            return typed->slot;
        }

        template<class T>
            requires std::copy_constructible<T>
        property_slot<T>& property(std::string_view id)
        {
            using holder = detail::property_slot_holder<T>;
            std::lock_guard lock(m_mutex);
            const auto it = m_properties.find(std::string(id));
            if (it == m_properties.end())
                throw registry_error(error_code::unknown_extension,
                    "Unknown property", std::string(id));
            auto* typed = dynamic_cast<holder*>(it->second.get());
            if (!typed)
                throw registry_error(error_code::type_mismatch,
                    "Property type does not match", std::string(id));
            return typed->slot;
        }

        template<class Base>
            requires std::derived_from<Base, modable>
        class_registry<Base>& classes(std::string_view id)
        {
            using holder = detail::class_registry_holder<Base>;
            std::lock_guard lock(m_mutex);
            const auto it = m_classes.find(std::string(id));
            if (it == m_classes.end())
                throw registry_error(error_code::unknown_extension,
                    "Unknown class registry", std::string(id));
            auto* typed = dynamic_cast<holder*>(it->second.get());
            if (!typed)
                throw registry_error(error_code::type_mismatch,
                    "Class registry base type does not match", std::string(id));
            return typed->classes;
        }

        [[nodiscard]] instance_handle create_instance_handle()
        {
            std::lock_guard lock(m_mutex);
            if (m_next_handle == 0 || m_next_handle == std::numeric_limits<std::uint64_t>::max())
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
                for (const auto& [_, value] : m_functions) functions.push_back(value);
                for (const auto& [_, value] : m_properties) properties.push_back(value);
                for (const auto& [_, value] : m_classes) classes.push_back(value);
            }
            for (const auto& value : functions) value->remove_mod(mod_id);
            for (const auto& value : properties) value->remove_mod(mod_id);
            for (const auto& value : classes) value->remove_mod(mod_id);
        }

        [[nodiscard]] std::vector<diagnostic> diagnostics() const
        {
            return m_diagnostics->snapshot();
        }

        void record_diagnostic(diagnostic value)
        {
            m_diagnostics->add(std::move(value));
        }

        // Applies REGISTER_MODDABLE_* declarations once to this registry.
        void apply_declarations();

        [[nodiscard]] mod_context for_mod(std::string mod_id);

    private:
        [[nodiscard]] mod_context for_mod(std::string mod_id,
            std::shared_ptr<detail::module_lifetime> lifetime);

        [[nodiscard]] bool is_live(instance_handle handle) const
        {
            std::lock_guard lock(m_mutex);
            return m_live_handles.contains(handle);
        }

        std::shared_ptr<detail::diagnostic_store> m_diagnostics;
        mutable std::mutex m_mutex;
        std::uint64_t m_next_handle{ 1 };
        bool m_declarations_applied{};
        std::unordered_set<instance_handle, detail::instance_handle_hash> m_live_handles;
        std::unordered_map<std::string, std::shared_ptr<detail::function_slot_base>> m_functions;
        std::unordered_map<std::string, std::shared_ptr<detail::property_slot_base>> m_properties;
        std::unordered_map<std::string, std::shared_ptr<detail::class_registry_base>> m_classes;

        friend class mod_context;
        friend class mod_loader;
    };

    class mod_context
    {
    public:
        [[nodiscard]] const std::string& mod_id() const noexcept { return m_mod_id; }

        template<class Signature, class F>
        bool replace_function(std::string_view id, F&& function)
        {
            std::function<Signature> callback(std::forward<F>(function));
            if (m_lifetime)
                callback = detail::guarded_function<Signature>::make(
                    m_lifetime, std::move(callback));
            return m_registry->template function<Signature>(id).replace(
                m_mod_id, std::move(callback));
        }

        template<class Signature, class F>
        void around_function(std::string_view id, priority_t priority, F&& function)
        {
            using guarded = detail::guarded_function<Signature>;
            using around_callback = typename function_slot<Signature>::around_callback;
            around_callback callback(std::forward<F>(function));
            if (m_lifetime)
                callback = guarded::make_around(m_lifetime, std::move(callback));
            m_registry->template function<Signature>(id).around(
                m_mod_id, priority, std::move(callback));
        }

        template<class T>
            requires std::copy_constructible<T>
        bool set_property(std::string_view id, const T& value,
            priority_t priority = 0, instance_handle handle = {})
        {
            return m_registry->template property<T>(id).set_override(
                m_mod_id, value, priority, handle);
        }

        template<class Base, class F>
            requires std::derived_from<Base, modable>
        bool add_implementation(std::string_view class_id,
            std::string implementation_id, F&& make)
        {
            typename class_registry<Base>::factory factory(std::forward<F>(make));
            if (m_lifetime)
            {
                auto lifetime = m_lifetime;
                factory = [lifetime = std::move(lifetime), maker = std::move(factory)] {
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

        template<class Base, class F>
            requires std::derived_from<Base, modable>
        bool replace_factory(std::string_view class_id,
            std::string implementation_id, F&& make)
        {
            typename class_registry<Base>::factory factory(std::forward<F>(make));
            if (m_lifetime)
            {
                auto lifetime = m_lifetime;
                factory = [lifetime = std::move(lifetime), maker = std::move(factory)] {
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
            : m_registry(&owner), m_mod_id(std::move(mod_id)),
              m_lifetime(std::move(lifetime)) {}

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

    namespace detail
    {
        template<class F, class = void>
        struct function_signature;

        template<class R, class... Args>
        struct function_signature<R(*)(Args...), void>
        {
            using type = R(Args...);
            static auto bind(R(*function)(Args...)) { return function; }
        };

        template<class R, class... Args>
        struct function_signature<R(Args...), void> : function_signature<R(*)(Args...)> {};

        template<class R, class... Args>
        struct function_signature<std::function<R(Args...)>, void>
            : function_signature<R(*)(Args...)>
        {
            static auto bind(std::function<R(Args...)> function) { return function; }
        };

        template<class C, class R, class... Args>
        struct function_signature<R(C::*)(Args...), void>
        {
            using type = R(C&, Args...);
            static auto bind(R(C::*function)(Args...))
            {
                return [function](C& object, Args... args) -> R {
                    if constexpr (std::is_void_v<R>)
                        std::invoke(function, object, std::forward<Args>(args)...);
                    else
                        return std::invoke(function, object, std::forward<Args>(args)...);
                };
            }
        };

        template<class C, class R, class... Args>
        struct function_signature<R(C::*)(Args...) const, void>
        {
            using type = R(const C&, Args...);
            static auto bind(R(C::*function)(Args...) const)
            {
                return [function](const C& object, Args... args) -> R {
                    if constexpr (std::is_void_v<R>)
                        std::invoke(function, object, std::forward<Args>(args)...);
                    else
                        return std::invoke(function, object, std::forward<Args>(args)...);
                };
            }
        };

        template<class R, class... Args>
        struct function_signature<R(*)(Args...) noexcept, void>
            : function_signature<R(*)(Args...)> {};

        template<class C, class R, class... Args>
        struct function_signature<R(C::*)(Args...) noexcept, void>
            : function_signature<R(C::*)(Args...)> {};

        template<class C, class R, class... Args>
        struct function_signature<R(C::*)(Args...) const noexcept, void>
            : function_signature<R(C::*)(Args...) const> {};

        template<class T>
        struct call_operator_signature;

        template<class C, class R, class... Args>
        struct call_operator_signature<R(C::*)(Args...)>
        {
            using type = R(Args...);
        };

        template<class C, class R, class... Args>
        struct call_operator_signature<R(C::*)(Args...) const>
        {
            using type = R(Args...);
        };

        template<class C, class R, class... Args>
        struct call_operator_signature<R(C::*)(Args...) noexcept>
            : call_operator_signature<R(C::*)(Args...)> {};

        template<class C, class R, class... Args>
        struct call_operator_signature<R(C::*)(Args...) const noexcept>
            : call_operator_signature<R(C::*)(Args...) const> {};

        template<class F, class = void>
        struct callable_traits : function_signature<std::remove_cvref_t<F>> {};

        template<class F>
        struct callable_traits<F, std::void_t<decltype(&std::remove_cvref_t<F>::operator())>>
            : call_operator_signature<decltype(&std::remove_cvref_t<F>::operator())>
        {
            static auto bind(F&& function) { return std::forward<F>(function); }
        };

        template<class F>
        void declare_function(registry& target, std::string id, F&& fallback)
        {
            using traits = callable_traits<F>;
            using signature = typename traits::type;
            target.template define_function<signature>(
                std::move(id), traits::bind(std::forward<F>(fallback)));
        }

        struct declaration
        {
            std::string group_id;
            std::string public_id;
            std::string identity;
            std::function<void(registry&)> apply;
        };

        class declaration_catalog
        {
        public:
            static declaration_catalog& instance()
            {
                static declaration_catalog catalog;
                return catalog;
            }

            bool add(std::string group_id, std::string public_id,
                std::string identity, std::function<void(registry&)> apply)
            {
                std::lock_guard lock(m_mutex);
                if (!m_identities.insert(identity).second)
                    return false;
                m_declarations.push_back(declaration{
                    std::move(group_id), std::move(public_id),
                    std::move(identity), std::move(apply) });
                return true;
            }

            void apply_to(registry& target) const
            {
                std::vector<declaration> declarations;
                {
                    std::lock_guard lock(m_mutex);
                    declarations = m_declarations;
                }

                std::sort(declarations.begin(), declarations.end(),
                    [](const declaration& left, const declaration& right) {
                        if (left.group_id != right.group_id)
                            return left.group_id < right.group_id;
                        return left.identity < right.identity;
                    });

                for (std::size_t first = 0; first < declarations.size();)
                {
                    auto last = first + 1;
                    while (last < declarations.size() &&
                        declarations[last].group_id == declarations[first].group_id)
                        ++last;

                    if (last - first > 1)
                    {
                        target.record_diagnostic(diagnostic{
                            error_code::registration_conflict, {},
                            declarations[first].public_id,
                            "Multiple declarations use the same ID; all were skipped" });
                        first = last;
                        continue;
                    }

                    try
                    {
                        declarations[first].apply(target);
                    }
                    catch (const registry_error& error)
                    {
                        target.record_diagnostic(diagnostic{
                            error.code(), error.mod_id(),
                            error.extension_id().empty()
                                ? declarations[first].public_id
                                : error.extension_id(),
                            error.what() });
                    }
                    catch (const std::exception& error)
                    {
                        target.record_diagnostic(diagnostic{
                            error_code::callback_failed, {},
                            declarations[first].public_id, error.what() });
                    }
                    catch (...)
                    {
                        target.record_diagnostic(diagnostic{
                            error_code::callback_failed, {},
                            declarations[first].public_id,
                            "Declaration threw an unknown exception" });
                    }
                    first = last;
                }
            }

        private:
            mutable std::mutex m_mutex;
            std::unordered_set<std::string> m_identities;
            std::vector<declaration> m_declarations;
        };
    }

    inline void registry::apply_declarations()
    {
        {
            std::lock_guard lock(m_mutex);
            if (m_declarations_applied)
                return;
            m_declarations_applied = true;
        }
        detail::declaration_catalog::instance().apply_to(*this);
    }
}

#define MNGL_MODS_DETAIL_JOIN_IMPL(a, b) a##b
#define MNGL_MODS_DETAIL_JOIN(a, b) MNGL_MODS_DETAIL_JOIN_IMPL(a, b)
#define MNGL_MODS_DETAIL_DECLARE_IMPL(n, group_id, public_id, identity, callback) \
    namespace { [[maybe_unused]] const bool MNGL_MODS_DETAIL_JOIN(mngl_mod_declaration_, n) = \
        ::mngl::mods::detail::declaration_catalog::instance().add( \
            (group_id), (public_id), (identity), (callback)); }
#define MNGL_MODS_DETAIL_DECLARE(group_id, public_id, identity, callback) \
    MNGL_MODS_DETAIL_DECLARE_IMPL(__COUNTER__, group_id, public_id, identity, callback)

// These macros are intended for host declarations in a .cpp file.
#define REGISTER_MODDABLE_CLASS(Type, ClassId) \
    MNGL_MODS_DETAIL_DECLARE( \
        std::string("0:class:") + (ClassId), (ClassId), \
        std::string("class|") + #Type + "|" + #ClassId, \
        [](::mngl::mods::registry& target) { (void)target.define_class<Type>(ClassId); })

#define REGISTER_MODDABLE_FUNCTION(Type, ExtensionId, Fallback) \
    MNGL_MODS_DETAIL_DECLARE( \
        std::string("2:extension:") + (ExtensionId), (ExtensionId), \
        std::string("function|") + #Type + "|" + #ExtensionId + "|" + #Fallback, \
        [](::mngl::mods::registry& target) { \
            static_assert(std::derived_from<Type, ::mngl::mods::modable>, \
                "REGISTER_MODDABLE_FUNCTION owner must derive from mngl::mods::modable"); \
            ::mngl::mods::detail::declare_function(target, (ExtensionId), (Fallback)); })

#define REGISTER_MODDABLE_FUNCTION_AS(Type, Signature, ExtensionId, Fallback) \
    MNGL_MODS_DETAIL_DECLARE( \
        std::string("2:extension:") + (ExtensionId), (ExtensionId), \
        std::string("function|") + #Type + "|" + #Signature + "|" + #ExtensionId + "|" + #Fallback, \
        [](::mngl::mods::registry& target) { \
            static_assert(std::derived_from<Type, ::mngl::mods::modable>, \
                "REGISTER_MODDABLE_FUNCTION_AS owner must derive from mngl::mods::modable"); \
            target.define_function<Signature>((ExtensionId), (Fallback)); })

#define REGISTER_MODDABLE_PROPERTY(Type, ValueType, ExtensionId, Scope, DefaultValue, Getter, Setter) \
    MNGL_MODS_DETAIL_DECLARE( \
        std::string("2:extension:") + (ExtensionId), (ExtensionId), \
        std::string("property|") + #Type + "|" + #ValueType + "|" + #ExtensionId + \
            "|" + #Scope + "|" + #DefaultValue + "|" + #Getter + "|" + #Setter, \
        [](::mngl::mods::registry& target) { \
            static_assert(std::derived_from<Type, ::mngl::mods::modable>, \
                "REGISTER_MODDABLE_PROPERTY owner must derive from mngl::mods::modable"); \
            target.define_property<ValueType>((ExtensionId), (Scope), (DefaultValue), \
                (Getter), (Setter)); })

#define REGISTER_MODDABLE_VALIDATED_PROPERTY(Type, ValueType, ExtensionId, Scope, DefaultValue, Getter, Setter, Validator) \
    MNGL_MODS_DETAIL_DECLARE( \
        std::string("2:extension:") + (ExtensionId), (ExtensionId), \
        std::string("property|") + #Type + "|" + #ValueType + "|" + #ExtensionId + \
            "|" + #Scope + "|" + #DefaultValue + "|" + #Getter + "|" + #Setter + "|" + #Validator, \
        [](::mngl::mods::registry& target) { \
            static_assert(std::derived_from<Type, ::mngl::mods::modable>, \
                "REGISTER_MODDABLE_VALIDATED_PROPERTY owner must derive from mngl::mods::modable"); \
            target.define_property<ValueType>((ExtensionId), (Scope), (DefaultValue), \
                (Getter), (Setter), (Validator)); })

#define REGISTER_MODDABLE_VALUE_PROPERTY(Type, ValueType, ExtensionId, Scope, DefaultValue) \
    MNGL_MODS_DETAIL_DECLARE( \
        std::string("2:extension:") + (ExtensionId), (ExtensionId), \
        std::string("property|") + #Type + "|" + #ValueType + "|" + #ExtensionId + \
            "|" + #Scope + "|" + #DefaultValue, \
        [](::mngl::mods::registry& target) { \
            static_assert(std::derived_from<Type, ::mngl::mods::modable>, \
                "REGISTER_MODDABLE_VALUE_PROPERTY owner must derive from mngl::mods::modable"); \
            target.define_property<ValueType>((ExtensionId), (Scope), (DefaultValue)); })

#define REGISTER_MODDABLE_FACTORY(Base, ClassId, ImplementationId, Factory) \
    MNGL_MODS_DETAIL_DECLARE( \
        std::string("1:factory:") + (ClassId) + ":" + (ImplementationId), \
        (ImplementationId), \
        std::string("factory|") + #Base + "|" + #ClassId + "|" + \
            #ImplementationId + "|" + #Factory, \
        [](::mngl::mods::registry& target) { \
            target.classes<Base>(ClassId).add_builtin((ImplementationId), (Factory)); })
