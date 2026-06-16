/**
 * File: stack.hpp
 * Path: ajylib/include/ajy/container/lockfree/stack.hpp
 * Description:
 * 	A Treiber style lockfree stack declaration.
 * 	MPMC-safe via CAS with tagged pointers.
 * Note:
 * 	ABA prevention via tagged pointers (upper 16 bits) combined
 * 	with memory pool node reuse deferral until pool destruction.
 * 	OS-level deallocation is deferred until stack destruction.
 * 	Requires a 64-bit platform with 48-bit canonical addresses.
 * Author: ajy-dev
 * Created: 2026-06-16
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_LOCKFREE_STACK_HPP
#define AJY_CONTAINER_LOCKFREE_STACK_HPP

#include <ajy/memory/lockfree/memory_pool.hpp>

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace ajy::container::lockfree
{
	template <std::move_constructible T>
	class Stack
	{
	public:
		static constexpr std::size_t DEFAULT_CAPACITY = 1024;

		explicit Stack(std::size_t initial_capacity = DEFAULT_CAPACITY) noexcept;
		~Stack(void) noexcept(std::is_nothrow_destructible<T>::value);

		Stack(const Stack &other) = delete;
		Stack &operator=(const Stack &other) = delete;
		Stack(Stack &&other) = delete;
		Stack &operator=(Stack &&other) = delete;

		bool push(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value);
		bool push(const T &value) noexcept(std::is_nothrow_copy_constructible<T>::value)
		requires std::copy_constructible<T>;

		std::optional<T> pop(void) noexcept(std::is_nothrow_move_constructible<T>::value);

	private:
		struct Node
		{
			T data;
			Node *next;

			explicit Node(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value);
			explicit Node(const T &value) noexcept(std::is_nothrow_copy_constructible<T>::value)
			requires std::copy_constructible<T>;
		};

		static_assert(sizeof(std::uintptr_t) == 8, "Requires 64-bit platform");

		static std::uintptr_t pack(Node *ptr, std::uint16_t tag) noexcept;
		static Node *unpack_ptr(std::uintptr_t raw) noexcept;
		static std::uint16_t unpack_tag(std::uintptr_t raw) noexcept;

		void push_node(Node *node) noexcept;

		std::atomic<std::uintptr_t> top;
		memory::lockfree::MemoryPool<Node> pool;
	};
}

#include <ajy/container/lockfree/stack.tpp>

#endif
