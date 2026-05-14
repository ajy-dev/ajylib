/**
 * File: queue.hpp
 * Path: library/include/ajy/container/lockfree/queue.hpp
 * Description:
 * 	A lock-free queue declaration using tagged pointers.
 * 	Michael-Scott queue style implementation.
 * 	Requires a 64-bit platform with 48-bit canonical addresses.
 * Author: ajy-dev
 * Created: 2026-05-14
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_LOCKFREE_QUEUE_HPP
# define AJY_CONTAINER_LOCKFREE_QUEUE_HPP

# include <concepts>
# include <type_traits>
# include <optional>
# include <atomic>
# include <cstdint>
# include <ajy/memory/lockfree/memory_pool.hpp>

namespace ajy::container::lockfree
{
	template <std::copyable T>
	class Queue
	{
	public:
		Queue(void);
		~Queue(void) noexcept(std::is_nothrow_destructible<T>::value);

		Queue(const Queue &other) = delete;
		Queue &operator=(const Queue &other) = delete;
		Queue(Queue &&other) = delete;
		Queue &operator=(Queue &&other) = delete;

		void enqueue(T value);
		std::optional<T> dequeue(void) noexcept(std::is_nothrow_copy_constructible<T>::value);
	private:
		struct Node
		{
			std::optional<T> data;
			std::atomic<std::uintptr_t> next;

			Node(void) noexcept;
			explicit Node(T value) noexcept(std::is_nothrow_move_constructible<T>::value);
		};

		static_assert(sizeof(std::uintptr_t) == 8, "Requires 64-bit platform");

		static std::uintptr_t pack(Node *ptr, std::uint16_t tag) noexcept;
		static Node *unpack_ptr(std::uintptr_t raw) noexcept;
		static std::uint16_t unpack_tag(std::uintptr_t raw) noexcept;

		std::atomic<std::uintptr_t> head;
		std::atomic<std::uintptr_t> tail;
		std::atomic<std::size_t> size;
		memory::lockfree::MemoryPool<Node> pool;
	};
}

# include <ajy/container/lockfree/queue.tpp>

#endif