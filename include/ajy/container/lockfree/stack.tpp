/**
 * File: stack.tpp
 * Path: ajylib/include/ajy/container/lockfree/stack.tpp
 * Description:
 * 	A Treiber style lockfree stack definition.
 * Author: ajy-dev
 * Created: 2026-06-16
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_LOCKFREE_STACK_TPP
#define AJY_CONTAINER_LOCKFREE_STACK_TPP

#include <ajy/container/lockfree/stack.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace ajy::container::lockfree
{
	template <std::move_constructible T>
	Stack<T>::Stack(std::size_t initial_capacity) noexcept
		: top(this->pack(nullptr, 0))
		, pool(initial_capacity)
	{
	}

	template <std::move_constructible T>
	Stack<T>::~Stack(void) noexcept(std::is_nothrow_destructible<T>::value)
	{
		Node *current;

		current = this->unpack_ptr(this->top.load(std::memory_order_acquire));
		while (current)
		{
			Node *next;

			next = current->next;
			this->pool.destroy(current);
			current = next;
		}
	}

	template <std::move_constructible T>
	bool Stack<T>::push(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value)
	{
		Node *new_node;

		new_node = this->pool.create(std::move(value));
		if (!new_node)
			return false;

		this->push_node(new_node);
		return true;
	}

	template <std::move_constructible T>
	bool Stack<T>::push(const T &value) noexcept(std::is_nothrow_copy_constructible<T>::value)
	requires std::copy_constructible<T>
	{
		Node *new_node;

		new_node = this->pool.create(value);
		if (!new_node)
			return false;

		this->push_node(new_node);
		return true;
	}

	template <std::move_constructible T>
	std::optional<T> Stack<T>::pop(void) noexcept(std::is_nothrow_move_constructible<T>::value)
	{
		std::uintptr_t old_top;

		old_top = this->top.load(std::memory_order_acquire);
		while (true)
		{
			Node *old_ptr;
			std::uintptr_t new_top;

			old_ptr = this->unpack_ptr(old_top);
			if (!old_ptr)
				return std::nullopt;

			new_top = this->pack(old_ptr->next, this->unpack_tag(old_top) + 1);

			if (this->top.compare_exchange_weak(old_top, new_top, std::memory_order_acquire, std::memory_order_relaxed))
			{
				std::optional<T> ret(std::move(old_ptr->data));

				this->pool.destroy(old_ptr);
				return ret;
			}
		}
	}

	template <std::move_constructible T>
	Stack<T>::Node::Node(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value)
		: data(std::move(value))
		, next(nullptr)
	{
	}

	template <std::move_constructible T>
	Stack<T>::Node::Node(const T &value) noexcept(std::is_nothrow_copy_constructible<T>::value)
	requires std::copy_constructible<T>
		: data(value)
		, next(nullptr)
	{
	}

	template <std::move_constructible T>
	std::uintptr_t Stack<T>::pack(Node *ptr, std::uint16_t tag) noexcept
	{
		static constexpr std::uintptr_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;
		static constexpr unsigned int TAG_SHIFT = 48;

		const std::uintptr_t ptr_bits = reinterpret_cast<std::uintptr_t>(ptr) & PTR_MASK;
		const std::uintptr_t tag_bits = static_cast<std::uintptr_t>(tag) << TAG_SHIFT;

		return tag_bits | ptr_bits;
	}

	template <std::move_constructible T>
	typename Stack<T>::Node *Stack<T>::unpack_ptr(std::uintptr_t raw) noexcept
	{
		static constexpr std::uintptr_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;

		return reinterpret_cast<Node *>(raw & PTR_MASK);
	}

	template <std::move_constructible T>
	std::uint16_t Stack<T>::unpack_tag(std::uintptr_t raw) noexcept
	{
		static constexpr unsigned int TAG_SHIFT = 48;

		return static_cast<std::uint16_t>(raw >> TAG_SHIFT);
	}

	template <std::move_constructible T>
	void Stack<T>::push_node(Node *node) noexcept
	{
		std::uintptr_t old_top;
		std::uintptr_t new_top;

		old_top = this->top.load(std::memory_order_relaxed);

		do
		{
			node->next = this->unpack_ptr(old_top);
			new_top = this->pack(node, this->unpack_tag(old_top) + 1);
		}
		while (!this->top.compare_exchange_weak(old_top, new_top, std::memory_order_release, std::memory_order_relaxed));
	}
}

#endif
