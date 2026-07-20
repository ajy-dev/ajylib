/**
 * File: object_pool.tpp
 * Path: ajylib/include/ajy/memory/lockfree/object_pool.tpp
 * Description:
 * 	A lockfree object pool definition.
 * Author: ajy-dev
 * Created: 2026-07-20
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_LOCKFREE_OBJECT_POOL_TPP
#define AJY_MEMORY_LOCKFREE_OBJECT_POOL_TPP

#include <ajy/memory/lockfree/object_pool.hpp>

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ajy::memory::lockfree
{
	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::ObjectPool(std::size_t initial_capacity, Args... args) noexcept(std::is_nothrow_constructible<std::tuple<Args...>, Args...>::value)
		: ctor_args(std::forward<Args>(args)...)
		, storage(initial_capacity)
		, free_objects(initial_capacity)
	{
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::~ObjectPool(void) noexcept(std::is_nothrow_destructible<T>::value)
	{
		std::optional<T *> recycled;

		while ((recycled = this->free_objects.pop()).has_value())
			this->storage.destroy(recycled.value());
	}

	template <ObjectPoolableType T, typename... Args>
	T *ObjectPool<T, Args...>::acquire(void) noexcept(std::is_nothrow_constructible<T, Args...>::value)
	{
		std::optional<T *> recycled;
		T *object;

		recycled = this->free_objects.pop();
		if (recycled.has_value())
			object = recycled.value();
		else
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
		this->free_objects.push(object);
		this->in_use_count.fetch_sub(1, std::memory_order_relaxed);
	}

	template <ObjectPoolableType T, typename... Args>
	std::size_t ObjectPool<T, Args...>::get_in_use_count(void) const noexcept
	{
		return this->in_use_count.load(std::memory_order_relaxed);
	}
}

#endif
