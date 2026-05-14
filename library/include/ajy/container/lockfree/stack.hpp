/**
 * File: stack.hpp
 * Path: library/include/ajy/container/lockfree/stack.hpp
 * Description:
 * 	A lock-free stack declaration using tagged pointers.
 * 	Treiber stack style implementation.
 * 	Requires a 64-bit platform with 48-bit canonical addresses.
 * Author: ajy-dev
 * Created: 2026-05-14
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_LOCKFREE_STACK_HPP
# define AJY_CONTAINER_LOCKFREE_STACK_HPP

# include <concepts>
# include <type_traits>
# include <cstdint>
# include <optional>
# include <atomic>
# include <ajy/memory/lockfree/memory_pool.hpp>

namespace ajy::container::lockfree
{
	template <std::movable T>
	class Stack
	{
	public:
		Stack(void);
		~Stack(void) noexcept(std::is_nothrow_destructible<T>::value);

		Stack(const Stack &other) = delete;
		Stack &operator=(const Stack &other) = delete;
		Stack(Stack &&other) = delete;
		Stack &operator=(Stack &&other) = delete;

		void push(T value);
		std::optional<T> pop(void) noexcept(std::is_nothrow_move_constructible<T>::value);
	private:
		struct Node
		{
			T data;
			Node *next;
			
			explicit Node(T value) noexcept(std::is_nothrow_move_constructible<T>::value);
		};

		static_assert(sizeof(std::uintptr_t) == 8, "Require 64-bit platform");
		
		static std::uintptr_t pack(Node *ptr, std::uint16_t tag) noexcept;
		static Node *unpack_ptr(std::uintptr_t raw) noexcept;
		static std::uint16_t unpack_tag(std::uintptr_t raw) noexcept;

		std::atomic<std::uintptr_t> top;
		memory::lockfree::MemoryPool<Node> pool;
	};
}

# include <ajy/container/lockfree/stack.tpp>

#endif