/**
 * File: queue.hpp
 * Path: ajylib/include/ajy/container/lockfree/queue.hpp
 * Description:
 * 	A Michael-Scott style lockfree queue declaration.
 * 	MPMC-safe via CAS with tagged pointers.
 * Note:
 * 	ABA prevention via tagged pointers (upper 16 bits) combined
 * 	with memory pool node reuse deferral until queue destruction.
 * 	OS-level deallocation is deferred until queue destruction.
 * 	Requires a 64-bit platform with 48-bit canonical addresses.
 * Author: ajy-dev
 * Created: 2026-06-16
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_LOCKFREE_QUEUE_HPP
#define AJY_CONTAINER_LOCKFREE_QUEUE_HPP

#include <ajy/memory/lockfree/memory_pool.hpp>

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace ajy::container::lockfree
{
	template <std::copy_constructible T>
	class Queue
	{
	public:
		static constexpr std::size_t DEFAULT_CAPACITY = 1024;

		explicit Queue(std::size_t initial_capacity = DEFAULT_CAPACITY) noexcept;
		~Queue(void) noexcept(std::is_nothrow_destructible<T>::value);

		Queue(const Queue &other) = delete;
		Queue &operator=(const Queue &other) = delete;
		Queue(Queue &&other) = delete;
		Queue &operator=(Queue &&other) = delete;

		bool enqueue(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value);
		bool enqueue(const T &value) noexcept(std::is_nothrow_copy_constructible<T>::value);

		std::optional<T> dequeue(void) noexcept(std::is_nothrow_copy_constructible<T>::value);

	private:
		struct Node
		{
			std::optional<T> data;
			std::atomic<std::uintptr_t> next;

			Node(void) noexcept;
			explicit Node(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value);
			explicit Node(const T &value) noexcept(std::is_nothrow_copy_constructible<T>::value);
		};

		static_assert(sizeof(std::uintptr_t) == 8, "Requires 64-bit platform");

		static std::uintptr_t pack(Node *ptr, std::uint16_t tag) noexcept;
		static Node *unpack_ptr(std::uintptr_t raw) noexcept;
		static std::uint16_t unpack_tag(std::uintptr_t raw) noexcept;

		void enqueue_node(Node *node) noexcept;

		std::atomic<std::uintptr_t> head;
		std::atomic<std::uintptr_t> tail;
		memory::lockfree::MemoryPool<Node> pool;
	};
}

#include <ajy/container/lockfree/queue.tpp>

#endif
