/**
 * File: queue.hpp
 * Path: ajylib/include/ajy/container/mpsc/queue.hpp
 * Description:
 * 	A bounded multi-producer, single-consumer queue declaration.
 * 	MPSC-safe via per-slot sequence counters.
 * Note:
 * 	Capacity is always rounded up to the nearest power of two.
 * 	sequence holds the ticket a slot is waiting for, not a slot index.
 * 	Not lock-free: a producer that stalls between claiming a ticket and
 * 	publishing its slot blocks the consumer and later producers.
 * 	The single-consumer restriction is not checked.
 * Author: ajy-dev
 * Created: 2026-08-28
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_MPSC_QUEUE_HPP
#define AJY_CONTAINER_MPSC_QUEUE_HPP

#include <ajy/container/concepts.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace ajy::container::mpsc
{
	template <MpscQueueableType T>
	class Queue
	{
	public:
		static constexpr std::size_t DEFAULT_CAPACITY = 1024;

		explicit Queue(std::size_t queue_capacity = DEFAULT_CAPACITY) noexcept;
		~Queue(void) noexcept;

		Queue(const Queue &other) = delete;
		Queue &operator=(const Queue &other) = delete;
		Queue(Queue &&other) = delete;
		Queue &operator=(Queue &&other) = delete;

		bool enqueue(T &&value) noexcept;
		std::optional<T> dequeue(void) noexcept;

		bool is_empty(void) const noexcept;
		std::size_t get_capacity(void) const noexcept;
		std::size_t get_size(void) const noexcept;

	private:
		static constexpr std::size_t CACHE_LINE_SIZE = 64;

		struct Slot
		{
			std::atomic<std::uint64_t> sequence;
			alignas(T) std::byte storage[sizeof(T)];
		};

		Slot *slots = nullptr;
		std::size_t capacity;
		std::size_t mask;
		alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t> write_index;
		alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t> read_index;
	};
}

#include <ajy/container/mpsc/queue.tpp>

#endif
