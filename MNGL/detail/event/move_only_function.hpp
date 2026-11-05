#pragma once

#include <cstddef>
#include <concepts>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace mngl::detail
{
#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
	template <typename Signature> using move_only_function = std::move_only_function<Signature>;
#else
	// Keep move-only event callbacks working with standard libraries that lack the C++23 wrapper.
	template <typename Signature> class move_only_function;

	template <typename Return, typename... Args> class move_only_function<Return(Args...)>
	{
		static constexpr std::size_t storage_size = sizeof(void*) * 3;
		struct storage_type
		{
			alignas(std::max_align_t) std::byte bytes[storage_size];
		};

		struct operations
		{
			Return (*invoke)(void*, Args...);
			void (*destroy)(void*) noexcept;
			void (*move)(void*, void*) noexcept;
		};

		template <typename Function> static constexpr bool stores_inline =
		    sizeof(Function) <= storage_size && alignof(Function) <= alignof(storage_type) &&
		    std::is_nothrow_move_constructible_v<Function>;

		template <typename Function> static Return invoke_inline(void* storage, Args... args)
		{
			auto& function = *static_cast<Function*>(storage);
			if constexpr (std::is_void_v<Return>)
			{
				std::invoke(function, std::forward<Args>(args)...);
			}
			else
			{
				return std::invoke(function, std::forward<Args>(args)...);
			}
		}

		template <typename Function> static Return invoke_heap(void* storage, Args... args)
		{
			auto& function = **static_cast<Function**>(storage);
			if constexpr (std::is_void_v<Return>)
			{
				std::invoke(function, std::forward<Args>(args)...);
			}
			else
			{
				return std::invoke(function, std::forward<Args>(args)...);
			}
		}

		template <typename Function> static void destroy_inline(void* storage) noexcept
		{
			std::destroy_at(static_cast<Function*>(storage));
		}

		template <typename Function> static void destroy_heap(void* storage) noexcept
		{
			delete *static_cast<Function**>(storage);
		}

		template <typename Function> static void move_inline(void* source, void* destination) noexcept
		{
			std::construct_at(static_cast<Function*>(destination), std::move(*static_cast<Function*>(source)));
			std::destroy_at(static_cast<Function*>(source));
		}

		template <typename Function> static void move_heap(void* source, void* destination) noexcept
		{
			*static_cast<Function**>(destination) = std::exchange(*static_cast<Function**>(source), nullptr);
		}

		template <typename Function> static const operations& operations_for() noexcept
		{
			if constexpr (stores_inline<Function>)
			{
				static constexpr operations table{invoke_inline<Function>, destroy_inline<Function>,
				                                  move_inline<Function>};
				return table;
			}
			else
			{
				static constexpr operations table{invoke_heap<Function>, destroy_heap<Function>, move_heap<Function>};
				return table;
			}
		}

	public:
		move_only_function() noexcept = default;
		move_only_function(std::nullptr_t) noexcept {}

		template <typename Function>
			requires(!std::same_as<std::remove_cvref_t<Function>, move_only_function> &&
			         std::is_invocable_r_v<Return, std::decay_t<Function>&, Args...>)
		move_only_function(Function&& function)
		{
			using stored_function = std::decay_t<Function>;
			if constexpr (stores_inline<stored_function>)
			{
				std::construct_at(reinterpret_cast<stored_function*>(m_storage.bytes),
				                  std::forward<Function>(function));
			}
			else
			{
				*reinterpret_cast<stored_function**>(m_storage.bytes) =
				    new stored_function(std::forward<Function>(function));
			}
			m_operations = &operations_for<stored_function>();
		}

		move_only_function(const move_only_function&) = delete;
		move_only_function& operator=(const move_only_function&) = delete;

		move_only_function(move_only_function&& other) noexcept { take_from(other); }

		move_only_function& operator=(move_only_function&& other) noexcept
		{
			if (this != &other)
			{
				reset();
				take_from(other);
			}
			return *this;
		}

		move_only_function& operator=(std::nullptr_t) noexcept
		{
			reset();
			return *this;
		}

		~move_only_function() { reset(); }

		void reset() noexcept
		{
			if (m_operations)
			{
				m_operations->destroy(m_storage.bytes);
				m_operations = nullptr;
			}
		}

		[[nodiscard]] explicit operator bool() const noexcept { return m_operations != nullptr; }

		Return operator()(Args... args)
		{
			if (!m_operations)
			{
				throw std::bad_function_call();
			}
			if constexpr (std::is_void_v<Return>)
			{
				m_operations->invoke(m_storage.bytes, std::forward<Args>(args)...);
			}
			else
			{
				return m_operations->invoke(m_storage.bytes, std::forward<Args>(args)...);
			}
		}

	private:
		void take_from(move_only_function& other) noexcept
		{
			if (other.m_operations)
			{
				m_operations = other.m_operations;
				m_operations->move(other.m_storage.bytes, m_storage.bytes);
				other.m_operations = nullptr;
			}
		}

		storage_type m_storage{};
		const operations* m_operations = nullptr;
	};
#endif
} // namespace mngl::detail
