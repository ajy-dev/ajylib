/**
 * File: object_pool.tpp
 * Path: ajylib/include/ajy/memory/lockfree/object_pool.tpp
 * Description:
 * 	A lockfree object pool definition.
 * Author: ajy-dev
 * Created: 2026-07-20
 * Updated: 2026-08-14
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_LOCKFREE_OBJECT_POOL_TPP
#define AJY_MEMORY_LOCKFREE_OBJECT_POOL_TPP

#include <ajy/memory/lockfree/object_pool.hpp>

#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ajy::memory::lockfree
{
	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::ObjectPool(std::size_t initial_capacity, Args... args) noexcept(std::is_nothrow_constructible<std::tuple<Args...>, Args...>::value)
		: ctor_args(std::forward<Args>(args)...)
		, storage(initial_capacity)
		, free_head(0)
		, in_use_count(0)
	{
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::~ObjectPool(void) noexcept(std::is_nothrow_destructible<T>::value)
	{
		T *recycled;

		while ((recycled = this->pop()) != nullptr)
			this->storage.destroy(recycled);
	}

	template <ObjectPoolableType T, typename... Args>
	T *ObjectPool<T, Args...>::acquire(void) noexcept(std::is_nothrow_constructible<T, Args...>::value)
	{
		T *object;

		object = this->pop();
		if (!object)
			object = std::apply(
				[this](Args &...args)
				{
					return this->storage.create(args...);
				},
				this->ctor_args);

		if (object)
			this->in_use_count.fetch_add(1, std::memory_order_relaxed);

		return object;
	}

	template <ObjectPoolableType T, typename... Args>
	void ObjectPool<T, Args...>::release(T *object) noexcept(noexcept(std::declval<T &>().clear()))
	{
		if (!object)
			return;

		object->clear();
		this->push(object);
		this->in_use_count.fetch_sub(1, std::memory_order_relaxed);
	}

	template <ObjectPoolableType T, typename... Args>
	std::uintptr_t ObjectPool<T, Args...>::pack(T *ptr, std::uint16_t tag) noexcept
	{
		static constexpr std::uintptr_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;
		static constexpr unsigned int TAG_SHIFT = 48;

		const std::uintptr_t ptr_bits = reinterpret_cast<std::uintptr_t>(ptr) & PTR_MASK;
		const std::uintptr_t tag_bits = static_cast<std::uintptr_t>(tag) << TAG_SHIFT;

		return tag_bits | ptr_bits;
	}

	template <ObjectPoolableType T, typename... Args>
	T *ObjectPool<T, Args...>::unpack_ptr(std::uintptr_t raw) noexcept
	{
		static constexpr std::uintptr_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;

		return reinterpret_cast<T *>(raw & PTR_MASK);
	}

	template <ObjectPoolableType T, typename... Args>
	std::uint16_t ObjectPool<T, Args...>::unpack_tag(std::uintptr_t raw) noexcept
	{
		static constexpr unsigned int TAG_SHIFT = 48;

		return static_cast<std::uint16_t>(raw >> TAG_SHIFT);
	}

	template <ObjectPoolableType T, typename... Args>
	void ObjectPool<T, Args...>::push(T *object) noexcept
	{
		std::uintptr_t old_head;
		std::uintptr_t new_head;

		old_head = this->free_head.load(std::memory_order_relaxed);

		do
		{
			object->set_pool_next(this->unpack_ptr(old_head));
			new_head = this->pack(object, static_cast<std::uint16_t>(this->unpack_tag(old_head) + 1));
		}
		while (!this->free_head.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_relaxed));
	}

	template <ObjectPoolableType T, typename... Args>
	T *ObjectPool<T, Args...>::pop(void) noexcept
	{
		std::uintptr_t old_head;

		old_head = this->free_head.load(std::memory_order_acquire);

		while (true)
		{
			T *old_ptr;
			std::uintptr_t new_head;

			old_ptr = this->unpack_ptr(old_head);
			if (!old_ptr)
				return nullptr;

			new_head = this->pack(static_cast<T *>(old_ptr->get_pool_next()), static_cast<std::uint16_t>(this->unpack_tag(old_head) + 1));

			if (this->free_head.compare_exchange_weak(old_head, new_head, std::memory_order_acquire, std::memory_order_relaxed))
				return old_ptr;
		}
	}

	template <ObjectPoolableType T, typename... Args>
	std::size_t ObjectPool<T, Args...>::get_in_use_count(void) const noexcept
	{
		return this->in_use_count.load(std::memory_order_relaxed);
	}
}

#endif
