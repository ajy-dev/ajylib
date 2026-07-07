/**
 * File: memory_pool.tpp
 * Path: ajylib/include/ajy/memory/threadlocal/memory_pool.tpp
 * Description:
 * 	A thread-local memory pool definition.
 * Author: ajy-dev
 * Created: 2026-06-17
 * Updated: 2026-07-07
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_THREADLOCAL_MEMORY_POOL_TPP
#define AJY_MEMORY_THREADLOCAL_MEMORY_POOL_TPP

#include <ajy/memory/threadlocal/memory_pool.hpp>

#include <cassert>
#include <cstdio>
#include <exception>
#include <mutex>
#include <new>
#include <system_error>
#include <type_traits>
#include <utility>

namespace ajy::memory::threadlocal
{
	template <PoolableType T>
	MemoryPool<T>::MemoryPool(std::size_t max_size, std::size_t batch_size) noexcept
		: tls_index(this->index_allocator.acquire())
		, max_size(max_size)
		, batch_size(batch_size)
	{
	}

	template <PoolableType T>
	MemoryPool<T>::~MemoryPool(void) noexcept
	{
		if (this->has_tls_slot())
		{
			TLSSlot &slot = this->tls_freelists[this->tls_index];

			slot.~TLSSlot();
			::new(&slot) TLSSlot();
		}

		this->index_allocator.release(this->tls_index);
	}

	template <PoolableType T>
	T *MemoryPool<T>::alloc(void) noexcept
	{
		FreeNode *node;

		if (!this->has_tls_slot())
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

		if (!this->tls_freelists[this->tls_index].head)
			this->refill_tls_from_global();

		node = this->pop();
		if (node)
			this->in_use_count.fetch_add(1, std::memory_order_relaxed);

		return reinterpret_cast<T *>(node);
	}

	template <PoolableType T>
	void MemoryPool<T>::free(T *ptr) noexcept
	{
		if (!ptr)
			return;

		this->in_use_count.fetch_sub(1, std::memory_order_relaxed);

		if (!this->has_tls_slot())
		{
			try
			{
				this->tls_freelists.resize(this->tls_index + 1);
			}
			catch (...)
			{
				this->get_global_pool().free(ptr);
				return;
			}
		}

		this->push(reinterpret_cast<FreeNode *>(ptr));

		if (this->tls_freelists[this->tls_index].count > this->max_size)
			this->drain_tls_to_global();
	}

	template <PoolableType T>
	template <typename... Args>
	T *MemoryPool<T>::create(Args &&...args) noexcept(std::is_nothrow_constructible<T, Args...>::value)
	{
		T *ptr;

		ptr = this->alloc();
		if (!ptr)
			return nullptr;

		if constexpr (std::is_nothrow_constructible<T, Args...>::value)
		{
			return ::new(ptr) T(std::forward<Args>(args)...);
		}
		else
		{
			try
			{
				return ::new(ptr) T(std::forward<Args>(args)...);
			}
			catch (...)
			{
				this->free(ptr);
				throw;
			}
		}
	}

	template <PoolableType T>
	void MemoryPool<T>::destroy(T *ptr) noexcept(std::is_nothrow_destructible<T>::value)
	requires std::destructible<T>
	{
		if (!ptr)
			return;

		ptr->~T();
		this->free(ptr);
	}

	template <PoolableType T>
	std::size_t MemoryPool<T>::get_in_use_count(void) const noexcept
	{
		return this->in_use_count.load(std::memory_order_relaxed);
	}

	template <PoolableType T>
	MemoryPool<T>::TLSSlot::TLSSlot(void) noexcept
		: head(nullptr)
		, count(0)
	{
	}

	template <PoolableType T>
	MemoryPool<T>::TLSSlot::~TLSSlot(void) noexcept
	{
		FreeNode *current;

		current = this->head;
		while (current)
		{
			FreeNode *next;

			next = current->next;
			MemoryPool<T>::get_global_pool().free(reinterpret_cast<T *>(current));
			current = next;
		}
	}

	template <PoolableType T>
	std::size_t MemoryPool<T>::IndexAllocator::acquire(void) noexcept
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
		catch (const std::system_error &error)
		{
			std::fprintf(stderr, "ajy::memory::threadlocal::MemoryPool::IndexAllocator::acquire() failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}
	}

	template <PoolableType T>
	void MemoryPool<T>::IndexAllocator::release(std::size_t index) noexcept
	{
		try
		{
			std::lock_guard<std::mutex> guard(this->lock);

			this->free_indices.push(index);
		}
		catch (const std::system_error &error)
		{
			std::fprintf(stderr, "ajy::memory::threadlocal::MemoryPool::IndexAllocator::release() failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}
		catch (const std::bad_alloc &error)
		{
			std::fprintf(stderr, "ajy::memory::threadlocal::MemoryPool::IndexAllocator::release() failed: %s\n", error.what());
			std::terminate();
		}
	}

	template <PoolableType T>
	lockfree::MemoryPool<T> &MemoryPool<T>::get_global_pool(void) noexcept
	{
		static lockfree::MemoryPool<T> *instance = new lockfree::MemoryPool<T>();

		return *instance;
	}

	template <PoolableType T>
	void MemoryPool<T>::push(FreeNode *node) noexcept
	{
		assert(this->has_tls_slot() && "MemoryPool: TLS slot must exist before push");

		TLSSlot &slot = this->tls_freelists[this->tls_index];

		node->next = slot.head;
		slot.head = node;
		++slot.count;
	}

	template <PoolableType T>
	FreeNode *MemoryPool<T>::pop(void) noexcept
	{
		assert(this->has_tls_slot() && "MemoryPool: TLS slot must exist before pop");

		TLSSlot &slot = this->tls_freelists[this->tls_index];
		FreeNode *node;

		if (!slot.head)
			return nullptr;

		node = slot.head;
		slot.head = node->next;
		--slot.count;

		return node;
	}

	template <PoolableType T>
	bool MemoryPool<T>::has_tls_slot(void) const noexcept
	{
		return this->tls_freelists.size() > this->tls_index;
	}

	template <PoolableType T>
	void MemoryPool<T>::refill_tls_from_global(void) noexcept
	{
		for (std::size_t i = 0; i < this->batch_size; ++i)
		{
			T *ptr;

			ptr = this->get_global_pool().alloc();
			if (!ptr)
				return;

			this->push(reinterpret_cast<FreeNode *>(ptr));
		}
	}

	template <PoolableType T>
	void MemoryPool<T>::drain_tls_to_global(void) noexcept
	{
		for (std::size_t i = 0; i < this->batch_size; ++i)
		{
			FreeNode *node;

			node = this->pop();
			if (!node)
				return;

			this->get_global_pool().free(reinterpret_cast<T *>(node));
		}
	}
}

#endif
