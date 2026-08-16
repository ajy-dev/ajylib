/**
 * File: object_pool.tpp
 * Path: ajylib/include/ajy/memory/threadlocal/object_pool.tpp
 * Description:
 *	A thread-local object pool definition.
 * Author: ajy-dev
 * Created: 2026-08-16
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_THREADLOCAL_OBJECT_POOL_TPP
#define AJY_MEMORY_THREADLOCAL_OBJECT_POOL_TPP

#include <ajy/memory/threadlocal/object_pool.hpp>

#include <cstddef>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

namespace ajy::memory::threadlocal
{
	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::ObjectPool(std::size_t initial_capacity, std::size_t max_size, std::size_t batch_size, Args... args) noexcept(std::is_nothrow_constructible<std::tuple<Args...>, Args...>::value)
		: global(new(std::nothrow) GlobalPool(initial_capacity, std::forward<Args>(args)...))
		, tls_index(this->index_allocator.acquire())
		, max_size(max_size)
		, batch_size(batch_size)
	{
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::~ObjectPool(void) noexcept(std::is_nothrow_destructible<T>::value)
	{
		if (this->global && this->tls_freelists.size() > this->tls_index)
		{
			TLSSlot &slot = this->tls_freelists[this->tls_index];

			if (slot.owner == this->global)
				this->discard_slot_contents(slot);
		}

		this->index_allocator.release(this->tls_index);

		if (this->global)
			this->global->release_ref();
	}

	template <ObjectPoolableType T, typename... Args>
	T *ObjectPool<T, Args...>::acquire(void) noexcept(std::is_nothrow_constructible<T, Args...>::value)
	{
		TLSSlot *slot;
		ObjectSlot<T> *object_slot;

		slot = this->claim_slot();
		if (!slot)
			return nullptr;

		if (!slot->head)
			this->refill_tls_from_global(*slot);

		object_slot = this->pop(*slot);
		if (!object_slot)
			return nullptr;

		this->in_use_count.fetch_add(1, std::memory_order_relaxed);

		return this->to_object(object_slot);
	}

	template <ObjectPoolableType T, typename... Args>
	void ObjectPool<T, Args...>::release(T *object) noexcept(noexcept(std::declval<T &>().clear()))
	{
		TLSSlot *slot;

		if (!object)
			return;

		this->in_use_count.fetch_sub(1, std::memory_order_relaxed);

		slot = this->claim_slot();
		if (!slot)
		{
			this->global->pool.release(object);

			return;
		}

		object->clear();
		this->push(*slot, this->to_slot(object));

		if (slot->count > this->max_size)
			this->drain_tls_to_global(*slot);
	}

	template <ObjectPoolableType T, typename... Args>
	std::size_t ObjectPool<T, Args...>::get_in_use_count(void) const noexcept
	{
		return this->in_use_count.load(std::memory_order_relaxed);
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::GlobalPool::GlobalPool(std::size_t initial_capacity, Args... args) noexcept(std::is_nothrow_constructible<std::tuple<Args...>, Args...>::value)
		: pool(initial_capacity, std::forward<Args>(args)...)
		, ref_count(1)
	{
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::GlobalPool::~GlobalPool(void) noexcept(std::is_nothrow_destructible<T>::value)
	{
	}

	template <ObjectPoolableType T, typename... Args>
	void ObjectPool<T, Args...>::GlobalPool::add_ref(void) noexcept
	{
		this->ref_count.fetch_add(1, std::memory_order_relaxed);
	}

	template <ObjectPoolableType T, typename... Args>
	void ObjectPool<T, Args...>::GlobalPool::release_ref(void) noexcept(std::is_nothrow_destructible<T>::value)
	{
		if (this->ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
			delete this;
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::TLSSlot::TLSSlot(void) noexcept
		: head(nullptr)
		, count(0)
		, owner(nullptr)
	{
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::TLSSlot::~TLSSlot(void) noexcept
	{
		ObjectPool<T, Args...>::discard_slot_contents(*this);
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectPool<T, Args...>::TLSSlot::TLSSlot(TLSSlot &&other) noexcept
		: head(other.head)
		, count(other.count)
		, owner(other.owner)
	{
		other.head = nullptr;
		other.count = 0;
		other.owner = nullptr;
	}

	template <ObjectPoolableType T, typename... Args>
	std::size_t ObjectPool<T, Args...>::IndexAllocator::acquire(void) noexcept
	{
		try
		{
			std::lock_guard<std::mutex> guard(this->lock);

			if (!this->free_indices.empty())
			{
				std::size_t index;

				index = this->free_indices.top();
				this->free_indices.pop();

				return index;
			}

			return this->count++;
		}
		catch (...)
		{
			return 0;
		}
	}

	template <ObjectPoolableType T, typename... Args>
	void ObjectPool<T, Args...>::IndexAllocator::release(std::size_t index) noexcept
	{
		try
		{
			std::lock_guard<std::mutex> guard(this->lock);

			this->free_indices.push(index);
		}
		catch (...)
		{
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
	void ObjectPool<T, Args...>::discard_slot_contents(TLSSlot &slot) noexcept(std::is_nothrow_destructible<T>::value)
	{
		while (slot.head)
		{
			ObjectSlot<T> *next;

			next = static_cast<ObjectSlot<T> *>(slot.head->pool_next);
			slot.owner->pool.release(ObjectPool<T, Args...>::to_object(slot.head));
			slot.head = next;
		}

		slot.count = 0;

		if (slot.owner)
		{
			slot.owner->release_ref();
			slot.owner = nullptr;
		}
	}

	template <ObjectPoolableType T, typename... Args>
	typename ObjectPool<T, Args...>::TLSSlot *ObjectPool<T, Args...>::claim_slot(void) noexcept(std::is_nothrow_destructible<T>::value)
	{
		if (!this->global)
			return nullptr;

		if (this->tls_freelists.size() <= this->tls_index)
		{
			try
			{
				this->tls_freelists.resize(this->tls_index + 1);
			}
			catch (...)
			{
				return nullptr;
			}
		}

		TLSSlot &slot = this->tls_freelists[this->tls_index];

		if (slot.owner != this->global)
		{
			this->discard_slot_contents(slot);

			slot.owner = this->global;
			slot.owner->add_ref();
		}

		return &slot;
	}

	template <ObjectPoolableType T, typename... Args>
	void ObjectPool<T, Args...>::push(TLSSlot &slot, ObjectSlot<T> *object_slot) noexcept
	{
		object_slot->pool_next = slot.head;
		slot.head = object_slot;
		++slot.count;
	}

	template <ObjectPoolableType T, typename... Args>
	ObjectSlot<T> *ObjectPool<T, Args...>::pop(TLSSlot &slot) noexcept
	{
		ObjectSlot<T> *object_slot;

		if (!slot.head)
			return nullptr;

		object_slot = slot.head;
		slot.head = static_cast<ObjectSlot<T> *>(object_slot->pool_next);
		--slot.count;

		return object_slot;
	}

	template <ObjectPoolableType T, typename... Args>
	void ObjectPool<T, Args...>::refill_tls_from_global(TLSSlot &slot) noexcept(std::is_nothrow_constructible<T, Args...>::value)
	{
		for (std::size_t i = 0; i < this->batch_size; ++i)
		{
			T *object;

			object = this->global->pool.acquire();
			if (!object)
				return;

			this->push(slot, this->to_slot(object));
		}
	}

	template <ObjectPoolableType T, typename... Args>
	void ObjectPool<T, Args...>::drain_tls_to_global(TLSSlot &slot) noexcept
	{
		for (std::size_t i = 0; i < this->batch_size; ++i)
		{
			ObjectSlot<T> *object_slot;

			object_slot = this->pop(slot);
			if (!object_slot)
				return;

			this->global->pool.release(this->to_object(object_slot));
		}
	}
}

#endif
