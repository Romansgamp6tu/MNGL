#pragma once

#include "registry.hpp"

namespace mngl::mods
{
	namespace detail
	{
		template <class F, class = void> struct function_signature;

		template <class R, class... Args> struct function_signature<R (*)(Args...), void>
		{
			using type = R(Args...);
			static auto bind(R (*function)(Args...)) { return function; }
		};

		template <class R, class... Args>
		struct function_signature<R(Args...), void> : function_signature<R (*)(Args...)>
		{
		};

		template <class R, class... Args>
		struct function_signature<std::function<R(Args...)>, void> : function_signature<R (*)(Args...)>
		{
			static auto bind(std::function<R(Args...)> function) { return function; }
		};

		template <class C, class R, class... Args> struct function_signature<R (C::*)(Args...), void>
		{
			using type = R(C&, Args...);
			static auto bind(R (C::*function)(Args...))
			{
				return [function](C& object, Args... args) -> R
				{
					if constexpr (std::is_void_v<R>)
						std::invoke(function, object, std::forward<Args>(args)...);
					else
						return std::invoke(function, object, std::forward<Args>(args)...);
				};
			}
		};

		template <class C, class R, class... Args> struct function_signature<R (C::*)(Args...) const, void>
		{
			using type = R(const C&, Args...);
			static auto bind(R (C::*function)(Args...) const)
			{
				return [function](const C& object, Args... args) -> R
				{
					if constexpr (std::is_void_v<R>)
						std::invoke(function, object, std::forward<Args>(args)...);
					else
						return std::invoke(function, object, std::forward<Args>(args)...);
				};
			}
		};

		template <class R, class... Args>
		struct function_signature<R (*)(Args...) noexcept, void> : function_signature<R (*)(Args...)>
		{
		};

		template <class C, class R, class... Args>
		struct function_signature<R (C::*)(Args...) noexcept, void> : function_signature<R (C::*)(Args...)>
		{
		};

		template <class C, class R, class... Args>
		struct function_signature<R (C::*)(Args...) const noexcept, void>
		    : function_signature<R (C::*)(Args...) const>
		{
		};

		template <class T> struct call_operator_signature;

		template <class C, class R, class... Args> struct call_operator_signature<R (C::*)(Args...)>
		{
			using type = R(Args...);
		};

		template <class C, class R, class... Args> struct call_operator_signature<R (C::*)(Args...) const>
		{
			using type = R(Args...);
		};

		template <class C, class R, class... Args>
		struct call_operator_signature<R (C::*)(Args...) noexcept>
		    : call_operator_signature<R (C::*)(Args...)>
		{
		};

		template <class C, class R, class... Args>
		struct call_operator_signature<R (C::*)(Args...) const noexcept>
		    : call_operator_signature<R (C::*)(Args...) const>
		{
		};

		template <class F, class = void> struct callable_traits : function_signature<std::remove_cvref_t<F>>
		{
		};

		template <class F>
		struct callable_traits<F, std::void_t<decltype(&std::remove_cvref_t<F>::operator())>>
		    : call_operator_signature<decltype(&std::remove_cvref_t<F>::operator())>
		{
			static auto bind(F&& function) { return std::forward<F>(function); }
		};

		template <class F> void declare_function(registry& target, std::string id, F&& fallback)
		{
			using traits = callable_traits<F>;
			using signature = typename traits::type;
			target.template define_function<signature>(std::move(id),
			                                           traits::bind(std::forward<F>(fallback)));
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

			bool add(std::string group_id, std::string public_id, std::string identity,
			         std::function<void(registry&)> apply)
			{
				std::lock_guard lock(m_mutex);
				if (!m_identities.insert(identity).second)
					return false;
				m_declarations.push_back(declaration{std::move(group_id), std::move(public_id),
				                                     std::move(identity), std::move(apply)});
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
				          [](const declaration& left, const declaration& right)
				          {
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
						target.record_diagnostic(
						    diagnostic{error_code::registration_conflict,
							           {},
							           declarations[first].public_id,
							           "Multiple declarations use the same ID; all were skipped"});
						first = last;
						continue;
					}

					try
					{
						declarations[first].apply(target);
					}
					catch (const registry_error& error)
					{
						target.record_diagnostic(diagnostic{error.code(), error.mod_id(),
						                                    error.extension_id().empty()
						                                        ? declarations[first].public_id
																: error.extension_id(),
						                                    error.what()});
					}
					catch (const std::exception& error)
					{
						target.record_diagnostic(diagnostic{
						    error_code::callback_failed, {}, declarations[first].public_id, error.what()});
					}
					catch (...)
					{
						target.record_diagnostic(diagnostic{error_code::callback_failed,
						                                    {},
						                                    declarations[first].public_id,
						                                    "Declaration threw an unknown exception"});
					}
					first = last;
				}
			}

		private:
			mutable std::mutex m_mutex;
			std::unordered_set<std::string> m_identities;
			std::vector<declaration> m_declarations;
		};
	} // namespace detail

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

} // namespace mngl::mods
