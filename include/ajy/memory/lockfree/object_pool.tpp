/**
 * File: object_pool.tpp
 * Path: ajylib/include/ajy/memory/lockfree/object_pool.tpp
 * Description:
 *	A lockfree object pool definition.
 * Author: ajy-dev
 * Created: 2026-07-20
 * Updated: 2026-08-16
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_LOCKFREE_OBJECT_POOL_TPP
#define AJY_MEMORY_LOCKFREE_OBJECT_POOL_TPP

#include <ajy/memory/lockfree/object_pool.hpp>

#include <cstddef>
#include <cstdint>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ajy::memory::lockfree
{
	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::ObjectPool(std::size_t initial_capacity, Args... args) noexcept(std::is_nothrow_constructible<std::tuple<Args...>, Args...>::value)
		: ctor_args(std::forward<Args>(args)...)
		, storage(initial_capacity)
		, free_head(this->pack(nullptr, 0))
	{
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::~ObjectPool(void) noexcept(std::is_nothrow_destructible<T>::value)
	{
		ObjectSlot<T> *recycled;

		while ((recycled = this->pop()) != nullptr)
		{
			this->to_object(recycled)->~T();
			this->storage.destroy(recycled);
		}
	}

	template <ObjectPoolableType T, typename... Args>
	T *ObjectPool<T, Args...>::acquire(void) noexcept(std::is_nothrow_constructible<T, Args...>::value)
	{
		ObjectSlot<T> *slot;
		T *object;

		slot = this->pop();
		if (slot)
			object = this->to_object(slot);
		else
		{
			object = this->construct();
			if (!object)
				return nullptr;
		}

		this->in_use_count.fetch_add(1, std::memory_order_relaxed);

		return object;
	}

	template <ObjectPoolableType T, typename... Args>
	void ObjectPool<T, Args...>::release(T *object) noexcept(noexcept(std::declval<T &>().clear()))
	{
		if (!object)
			return;

		object->clear();
		this->push(this->to_slot(object));
		this->in_use_count.fetch_sub(1, std::memory_order_relaxed);
	}

	template <ObjectPoolableType T, typename... Args>
	std::size_t ObjectPool<T, Args...>::get_in_use_count(void) const noexcept
	{
		return this->in_use_count.load(std::memory_order_relaxed);
	}

	template <ObjectPoolableType T, typename... Args>
	std::uintptr_t ObjectPool<T, Args...>::pack(ObjectSlot<T> *ptr, std::uint16_t tag) noexcept
	{
		static constexpr std::uintptr_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;
		static constexpr unsigned int TAG_SHIFT = 48;

		const std::uintptr_t ptr_bits = reinterpret_cast<std::uintptr_t>(ptr) & PTR_MASK;
		const std::uintptr_t tag_bits = static_cast<std::uintptr_t>(tag) << TAG_SHIFT;

		return tag_bits | ptr_bits;
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectSlot<T> *ObjectPool<T, Args...>::unpack_ptr(std::uintptr_t raw) noexcept
	{
		static constexpr std::uintptr_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;

		return reinterpret_cast<ObjectSlot<T> *>(raw & PTR_MASK);
	}

	template <ObjectPoolableType T, typename... Args>
	std::uint16_t ObjectPool<T, Args...>::unpack_tag(std::uintptr_t raw) noexcept
	{
		static constexpr unsigned int TAG_SHIFT = 48;

		return static_cast<std::uint16_t>(raw >> TAG_SHIFT);
	}

	template <ObjectPoolableType T, typename... Args>
	T *ObjectPool<T, Args...>::construct(void) noexcept(std::is_nothrow_constructible<T, Args...>::value)
	{
		ObjectSlot<T> *slot;

		slot = this->storage.create();
		if (!slot)
			return nullptr;

		if constexpr (std::is_nothrow_constructible<T, Args...>::value)
		{
			return std::apply(
				[slot](Args &...args)
				{
					return ::new(slot->storage) T(args...);
				},
				this->ctor_args);
		}
		else
		{
			try
			{
				return std::apply(
					[slot](Args &...args)
					{
						return ::new(slot->storage) T(args...);
					},
					this->ctor_args);
			}
			catch (...)
			{
				this->storage.destroy(slot);
				throw;
			}
		}
	}

	template <ObjectPoolableType T, typename... Args>
	T *ObjectPool<T, Args...>::to_object(ObjectSlot<T> *slot) noexcept
	{
		return std::launder(reinterpret_cast<T *>(slot->storage));
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectSlot<T> *ObjectPool<T, Args...>::to_slot(T *object) noexcept
	{
		return reinterpret_cast<ObjectSlot<T> *>(reinterpret_cast<std::byte *>(object) - offsetof(ObjectSlot<T>, storage));
	}

	template <ObjectPoolableType T, typename... Args>
	void ObjectPool<T, Args...>::push(ObjectSlot<T> *slot) noexcept
	{
		std::uintptr_t old_head;
		std::uintptr_t new_head;

		old_head = this->free_head.load(std::memory_order_relaxed);

		do
		{
			slot->pool_next = this->unpack_ptr(old_head);
			new_head = this->pack(slot, static_cast<std::uint16_t>(this->unpack_tag(old_head) + 1));
		}
		while (!this->free_head.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_relaxed));
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectSlot<T> *ObjectPool<T, Args...>::pop(void) noexcept
	{
		std::uintptr_t old_head;

		old_head = this->free_head.load(std::memory_order_acquire);

		while (true)
		{
			ObjectSlot<T> *old_ptr;
			std::uintptr_t new_head;

			old_ptr = this->unpack_ptr(old_head);
			if (!old_ptr)
				return nullptr;

			new_head = this->pack(static_cast<ObjectSlot<T> *>(old_ptr->pool_next), static_cast<std::uint16_t>(this->unpack_tag(old_head) + 1));

			if (this->free_head.compare_exchange_weak(old_head, new_head, std::memory_order_acquire, std::memory_order_relaxed))
				return old_ptr;
		}
	}
}

#endif
