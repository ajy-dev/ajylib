/**
 * File: queue.tpp
 * Path: ajylib/include/ajy/container/mpsc/queue.tpp
 * Description:
 * 	A bounded multi-producer, single-consumer queue definition.
 * Author: ajy-dev
 * Created: 2026-08-28
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_MPSC_QUEUE_TPP
#define AJY_CONTAINER_MPSC_QUEUE_TPP

#include <ajy/container/mpsc/queue.hpp>

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <utility>

namespace ajy::container::mpsc
{
	template <MpscQueueableType T>
	Queue<T>::Queue(std::size_t queue_capacity) noexcept
	{
		this->capacity = std::bit_ceil(queue_capacity);
		this->mask = this->capacity - 1;
		this->slots = new(std::nothrow) Slot[this->capacity];
		if (!this->slots)
		{
			this->capacity = 0;
			return;
		}

		for (std::size_t i = 0; i < this->capacity; ++i)
			this->slots[i].sequence.store(i, std::memory_order_relaxed);
	}

	template <MpscQueueableType T>
	Queue<T>::~Queue(void) noexcept
	{
		while (this->dequeue().has_value())
			;

		delete[] this->slots;
	}

	template <MpscQueueableType T>
	bool Queue<T>::enqueue(T &&value) noexcept
	{
		Slot *slot;
		std::uint64_t position;

		if (!this->slots)
			return false;

		position = this->write_index.load(std::memory_order_relaxed);

		while (true)
		{
			std::uint64_t gap;

			slot = &this->slots[position & this->mask];
			gap = slot->sequence.load(std::memory_order_acquire) - position;

			if (!gap) // sequence == position
			{
				if (this->write_index.compare_exchange_weak(position, position + 1, std::memory_order_relaxed))
					break;
			}
			else if (gap >= std::numeric_limits<std::uint64_t>::max() - this->mask) // sequence < position
				return false;
			else // sequence > position
				position = this->write_index.load(std::memory_order_relaxed);
		}

		::new(slot->storage) T(std::move(value));
		slot->sequence.store(position + 1, std::memory_order_release);

		return true;
	}

	template <MpscQueueableType T>
	std::optional<T> Queue<T>::dequeue(void) noexcept
	{
		Slot *slot;
		T *stored;
		std::uint64_t position;
		std::optional<T> value;

		if (!this->slots)
			return std::nullopt;

		position = this->read_index.load(std::memory_order_relaxed);
		slot = &this->slots[position & this->mask];

		if (slot->sequence.load(std::memory_order_acquire) != position + 1)
			return std::nullopt;

		stored = std::launder(reinterpret_cast<T *>(slot->storage));

		value.emplace(std::move(*stored));
		stored->~T();

		slot->sequence.store(position + this->mask + 1, std::memory_order_release);
		this->read_index.store(position + 1, std::memory_order_relaxed);

		return value;
	}

	template <MpscQueueableType T>
	bool Queue<T>::is_empty(void) const noexcept
	{
		std::uint64_t position;

		if (!this->slots)
			return true;

		position = this->read_index.load(std::memory_order_relaxed);

		return this->slots[position & this->mask].sequence.load(std::memory_order_acquire) != position + 1;
	}

	template <MpscQueueableType T>
	std::size_t Queue<T>::get_capacity(void) const noexcept
	{
		return this->capacity;
	}

	template <MpscQueueableType T>
	std::size_t Queue<T>::get_size(void) const noexcept
	{
		return static_cast<std::size_t>(
			this->write_index.load(std::memory_order_relaxed) - this->read_index.load(std::memory_order_relaxed));
	}
}

#endif
