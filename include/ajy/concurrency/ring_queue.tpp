/**
 * File: ring_queue.tpp
 * Path: ajylib/include/ajy/concurrency/ring_queue.tpp
 * Description:
 *	A bounded multi-producer, single-consumer queue definition.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONCURRENCY_RING_QUEUE_TPP
#define AJY_CONCURRENCY_RING_QUEUE_TPP

#include <ajy/concurrency/ring_queue.hpp>

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <memory>
#include <new>
#include <optional>
#include <utility>

namespace ajy::concurrency
{
	template <typename T>
	RingQueue<T>::RingQueue(std::size_t capacity) noexcept
		: mask(0)
		, write_index(0)
		, read_index(0)
	{
		std::size_t rounded;
		std::size_t i;

		rounded = round_up_to_power_of_two(capacity ? capacity : DEFAULT_CAPACITY);

		try
		{
			this->slots = std::make_unique<Slot[]>(rounded);
		}
		catch (const std::bad_alloc &error)
		{
			std::fprintf(stderr, "RingQueue::RingQueue(): slot allocation failed: %s\n", error.what());
			std::terminate();
		}

		for (i = 0; i < rounded; ++i)
			this->slots[i].sequence.store(i, std::memory_order_relaxed);

		this->mask = rounded - 1;
	}

	template <typename T>
	RingQueue<T>::~RingQueue(void) noexcept
	{
		while (this->dequeue().has_value())
			;
	}

	template <typename T>
	bool RingQueue<T>::enqueue(T &&value) noexcept
	{
		Slot *slot;
		std::size_t position;

		position = this->write_index.load(std::memory_order_relaxed);

		for (;;)
		{
			std::ptrdiff_t difference;

			slot = &this->slots[position & this->mask];
			difference = static_cast<std::ptrdiff_t>(slot->sequence.load(std::memory_order_acquire))
				- static_cast<std::ptrdiff_t>(position);

			if (!difference)
			{
				if (this->write_index.compare_exchange_weak(position, position + 1, std::memory_order_relaxed))
					break;
			}
			else if (difference < 0)
				return false;
			else
				position = this->write_index.load(std::memory_order_relaxed);
		}

		::new(slot->storage) T(std::move(value));
		slot->sequence.store(position + 1, std::memory_order_release);

		return true;
	}

	template <typename T>
	std::optional<T> RingQueue<T>::dequeue(void) noexcept
	{
		Slot *slot;
		T *stored;
		std::size_t position;
		std::optional<T> value;

		position = this->read_index;
		slot = &this->slots[position & this->mask];

		if (slot->sequence.load(std::memory_order_acquire) != position + 1)
			return std::nullopt;

		stored = reinterpret_cast<T *>(slot->storage);

		value.emplace(std::move(*stored));
		stored->~T();

		slot->sequence.store(position + this->mask + 1, std::memory_order_release);
		this->read_index = position + 1;

		return value;
	}

	template <typename T>
	bool RingQueue<T>::is_empty(void) const noexcept
	{
		return this->slots[this->read_index & this->mask].sequence.load(std::memory_order_acquire) != this->read_index + 1;
	}

	template <typename T>
	std::size_t RingQueue<T>::get_capacity(void) const noexcept
	{
		return this->mask + 1;
	}

	template <typename T>
	std::size_t RingQueue<T>::get_size(void) const noexcept
	{
		std::size_t write;
		std::size_t read;

		write = this->write_index.load(std::memory_order_relaxed);
		read = this->read_index;

		return (write > read) ? (write - read) : 0;
	}

	template <typename T>
	std::size_t RingQueue<T>::round_up_to_power_of_two(std::size_t value) noexcept
	{
		std::size_t result;

		result = 1;
		while (result < value)
			result <<= 1;

		return result;
	}
}

#endif
