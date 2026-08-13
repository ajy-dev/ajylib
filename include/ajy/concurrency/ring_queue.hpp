/**
 * File: ring_queue.hpp
 * Path: ajylib/include/ajy/concurrency/ring_queue.hpp
 * Description:
 *	A bounded multi-producer, single-consumer queue declaration.
 * Note:
 *	Measurement scaffolding, not yet a library candidate.
 *	Capacity is fixed at construction and rounded up to a power of two, so
 *	the storage never grows and slots stay contiguous -- unlike a linked
 *	queue, whose nodes scatter over time and turn every dequeue into a DRAM
 *	access. Enqueue costs one fetch_add and one release store; dequeue,
 *	being single-consumer, needs no atomic read-modify-write at all.
 *	The single-consumer restriction is not checked. Calling dequeue from
 *	more than one thread corrupts the queue.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONCURRENCY_RING_QUEUE_HPP
#define AJY_CONCURRENCY_RING_QUEUE_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>

namespace ajy::concurrency
{
	template <typename T>
	class RingQueue
	{
		static_assert(std::is_nothrow_move_constructible<T>::value, "T must be nothrow move constructible.");

	public:
		static constexpr std::size_t DEFAULT_CAPACITY = 65536;

		explicit RingQueue(std::size_t capacity = DEFAULT_CAPACITY) noexcept;
		~RingQueue(void) noexcept;

		RingQueue(const RingQueue &other) = delete;
		RingQueue &operator=(const RingQueue &other) = delete;
		RingQueue(RingQueue &&other) = delete;
		RingQueue &operator=(RingQueue &&other) = delete;

		bool enqueue(T &&value) noexcept;
		std::optional<T> dequeue(void) noexcept;

		bool is_empty(void) const noexcept;
		std::size_t get_capacity(void) const noexcept;
		std::size_t get_size(void) const noexcept;

	private:
		static constexpr std::size_t CACHE_LINE_SIZE = 64;

		struct Slot
		{
			std::atomic<std::size_t> sequence;
			alignas(alignof(T)) unsigned char storage[sizeof(T)];
		};

		static std::size_t round_up_to_power_of_two(std::size_t value) noexcept;

		std::unique_ptr<Slot[]> slots;
		std::size_t mask;

		alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> write_index;
		alignas(CACHE_LINE_SIZE) std::size_t read_index;
	};
}

#include <ajy/concurrency/ring_queue.tpp>

#endif
