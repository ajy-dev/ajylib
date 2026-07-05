/**
 * File: queue.tpp
 * Path: ajylib/include/ajy/container/lockfree/queue.tpp
 * Description:
 * 	A Michael-Scott style lockfree queue definition.
 * Author: ajy-dev
 * Created: 2026-06-16
 * Updated: 2026-07-05
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_LOCKFREE_QUEUE_TPP
#define AJY_CONTAINER_LOCKFREE_QUEUE_TPP

#include <ajy/container/lockfree/queue.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace ajy::container::lockfree
{
	template <std::copy_constructible T>
	Queue<T>::Queue(std::size_t initial_capacity) noexcept
		: pool(initial_capacity)
	{
		Node *dummy;
		std::uintptr_t tagged_dummy;

		dummy = this->pool.create();
		tagged_dummy = this->pack(dummy, 0);
		this->head.store(tagged_dummy, std::memory_order_relaxed);
		this->tail.store(tagged_dummy, std::memory_order_relaxed);
	}

	template <std::copy_constructible T>
	Queue<T>::~Queue(void) noexcept(std::is_nothrow_destructible<T>::value)
	{
		Node *current;

		current = this->unpack_ptr(this->head.load(std::memory_order_acquire));
		while (current)
		{
			Node *next;

			next = this->unpack_ptr(current->next.load(std::memory_order_relaxed));
			this->pool.destroy(current);
			current = next;
		}
	}

	template <std::copy_constructible T>
	bool Queue<T>::enqueue(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value)
	{
		Node *new_node;

		if (!this->unpack_ptr(this->head.load(std::memory_order_relaxed)))
			return false;

		new_node = this->pool.create(std::move(value));
		if (!new_node)
			return false;

		this->enqueue_node(new_node);
		return true;
	}

	template <std::copy_constructible T>
	bool Queue<T>::enqueue(const T &value) noexcept(std::is_nothrow_copy_constructible<T>::value)
	{
		Node *new_node;

		if (!this->unpack_ptr(this->head.load(std::memory_order_relaxed)))
			return false;

		new_node = this->pool.create(value);
		if (!new_node)
			return false;

		this->enqueue_node(new_node);
		return true;
	}

	template <std::copy_constructible T>
	std::optional<T> Queue<T>::dequeue(void) noexcept(std::is_nothrow_copy_constructible<T>::value)
	{
		if (!this->unpack_ptr(this->head.load(std::memory_order_relaxed)))
			return std::nullopt;

		while (true)
		{
			std::uintptr_t old_head_raw;
			std::uintptr_t old_tail_raw;
			std::uintptr_t next_raw;
			Node *old_head_ptr;
			Node *next_ptr;

			old_head_raw = this->head.load(std::memory_order_acquire);
			old_tail_raw = this->tail.load(std::memory_order_acquire);
			old_head_ptr = this->unpack_ptr(old_head_raw);
			next_raw = old_head_ptr->next.load(std::memory_order_acquire);
			next_ptr = this->unpack_ptr(next_raw);

			if (old_head_raw != this->head.load(std::memory_order_acquire))
				continue;

			if (old_head_ptr == this->unpack_ptr(old_tail_raw))
			{
				std::uintptr_t new_tail_raw;

				if (!next_ptr)
					return std::nullopt;

				new_tail_raw = this->pack(next_ptr, this->unpack_tag(old_tail_raw) + 1);
				this->tail.compare_exchange_strong(old_tail_raw, new_tail_raw, std::memory_order_release, std::memory_order_relaxed);
			}
			else
			{
				std::uintptr_t new_head_raw;
				std::optional<T> ret(next_ptr->data);

				new_head_raw = this->pack(next_ptr, this->unpack_tag(old_head_raw) + 1);

				if (this->head.compare_exchange_weak(old_head_raw, new_head_raw, std::memory_order_acquire, std::memory_order_relaxed))
				{
					this->pool.destroy(old_head_ptr);
					return ret;
				}
			}
		}
	}

	template <std::copy_constructible T>
	Queue<T>::Node::Node(void) noexcept
		: data(std::nullopt)
		, next(Queue<T>::pack(nullptr, 0))
	{
	}

	template <std::copy_constructible T>
	Queue<T>::Node::Node(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value)
		: data(std::move(value))
		, next(Queue<T>::pack(nullptr, 0))
	{
	}

	template <std::copy_constructible T>
	Queue<T>::Node::Node(const T &value) noexcept(std::is_nothrow_copy_constructible<T>::value)
		: data(value)
		, next(Queue<T>::pack(nullptr, 0))
	{
	}

	template <std::copy_constructible T>
	std::uintptr_t Queue<T>::pack(Node *ptr, std::uint16_t tag) noexcept
	{
		static constexpr std::uintptr_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;
		static constexpr unsigned int TAG_SHIFT = 48;

		const std::uintptr_t ptr_bits = reinterpret_cast<std::uintptr_t>(ptr) & PTR_MASK;
		const std::uintptr_t tag_bits = static_cast<std::uintptr_t>(tag) << TAG_SHIFT;

		return tag_bits | ptr_bits;
	}

	template <std::copy_constructible T>
	typename Queue<T>::Node *Queue<T>::unpack_ptr(std::uintptr_t raw) noexcept
	{
		static constexpr std::uintptr_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;

		return reinterpret_cast<Node *>(raw & PTR_MASK);
	}

	template <std::copy_constructible T>
	std::uint16_t Queue<T>::unpack_tag(std::uintptr_t raw) noexcept
	{
		static constexpr unsigned int TAG_SHIFT = 48;

		return static_cast<std::uint16_t>(raw >> TAG_SHIFT);
	}

	template <std::copy_constructible T>
	void Queue<T>::enqueue_node(Node *node) noexcept
	{
		while (true)
		{
			std::uintptr_t old_tail_raw;
			Node *old_tail_ptr;
			std::uintptr_t next_raw;

			old_tail_raw = this->tail.load(std::memory_order_acquire);
			old_tail_ptr = this->unpack_ptr(old_tail_raw);
			next_raw = old_tail_ptr->next.load(std::memory_order_acquire);

			if (!this->unpack_ptr(next_raw))
			{
				std::uintptr_t new_next_raw;
				std::uintptr_t new_tail_raw;

				new_next_raw = this->pack(node, this->unpack_tag(next_raw) + 1);
				if (old_tail_ptr->next.compare_exchange_weak(next_raw, new_next_raw, std::memory_order_release, std::memory_order_relaxed))
				{
					new_tail_raw = this->pack(node, this->unpack_tag(old_tail_raw) + 1);
					this->tail.compare_exchange_strong(old_tail_raw, new_tail_raw, std::memory_order_release, std::memory_order_relaxed);
					return;
				}
			}
			else
			{
				std::uintptr_t new_tail_raw;

				new_tail_raw = this->pack(this->unpack_ptr(next_raw), this->unpack_tag(old_tail_raw) + 1);
				this->tail.compare_exchange_weak(old_tail_raw, new_tail_raw, std::memory_order_release, std::memory_order_relaxed);
			}
		}
	}
}

#endif
