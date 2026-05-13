/**
 * File: memory_pool.tpp
 * Path: library/include/ajy/memory/lockfree/memory_pool.tpp
 * Description: Definition of ajy::memory::lockfree::MemoryPool.
 * Author: ajy-dev
 * Created: 2026-05-13
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_LOCKFREE_MEMORY_POOL_TPP
# define AJY_MEMORY_LOCKFREE_MEMORY_POOL_TPP

# include <ajy/memory/lockfree/memory_pool.hpp>
# include <utility>
# include <new>

namespace ajy::memory::lockfree
{
	template <PoolableType T>
	MemoryPool<T>::MemoryPool(std::size_t initial_capacity)
		: head(this->pack(nullptr, 0))
		, chunks(nullptr)
		, expand_size(initial_capacity)
	{
		this->expand(this->expand_size);
	}

	template <PoolableType T>
	MemoryPool<T>::~MemoryPool(void) noexcept
	{
		memory::Chunk<T> *current;
		
		current = this->chunks;
		while (current)
		{
			memory::Chunk<T> *next;
			
			next = current->next;

			delete[] current->slots;
			delete current;

			current = next;
		}
	}

	template <PoolableType T>
	T *MemoryPool<T>::alloc(void)
	{
		memory::FreeNode *node;
		
		node = this->pop();
		if (node)
			return reinterpret_cast<T *>(node);
		
		{
			std::lock_guard<std::mutex> guard(this->expand_lock);

			while (!(node = this->pop()))
				this->expand(this->expand_size);
		}

		return reinterpret_cast<T *>(node);
	}

	template <PoolableType T>
	void MemoryPool<T>::free(T *ptr) noexcept
	{
		memory::FreeNode *node;

		if (!ptr)
			return;
		
		node = reinterpret_cast<memory::FreeNode *>(ptr);
		this->push(node);
	}

	template <PoolableType T>
	template <typename... Args>
	T *MemoryPool<T>::create(Args&&... args) requires std::constructible_from<T, Args...>
	{
		T *ptr;
		
		ptr = this->alloc();
		
		try
		{
			return ::new (ptr) T(std::forward<Args>(args)...);
		}
		catch (...)
		{
			this->free(ptr);
			throw;
		}
	}

	template <PoolableType T>
	void MemoryPool<T>::destroy(T *ptr) noexcept(std::is_nothrow_destructible<T>::value) requires std::destructible<T>
	{
		if (!ptr)
			return;

		ptr->~T();
		this->free(ptr);
	}

	template <PoolableType T>
	std::uintptr_t MemoryPool<T>::pack(memory::FreeNode *ptr, std::uint16_t tag) noexcept
	{
		static constexpr std::uintptr_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;
		static constexpr unsigned int TAG_SHIFT = 48;

		const std::uintptr_t ptr_bits = reinterpret_cast<std::uintptr_t>(ptr) & PTR_MASK;
		const std::uintptr_t tag_bits = static_cast<std::uintptr_t>(tag) << TAG_SHIFT;

		return tag_bits | ptr_bits;
	}

	template <PoolableType T>
	memory::FreeNode *MemoryPool<T>::unpack_ptr(std::uintptr_t raw) noexcept
	{
		static constexpr std::uintptr_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;
		
		return reinterpret_cast<memory::FreeNode *>(raw & PTR_MASK);
	}

	template <PoolableType T>
	std::uint16_t MemoryPool<T>::unpack_tag(std::uintptr_t raw) noexcept
	{
		static constexpr unsigned int TAG_SHIFT = 48;

		return static_cast<std::uint16_t>(raw >> TAG_SHIFT);
	}

	template <PoolableType T>
	void MemoryPool<T>::push(memory::FreeNode *node) noexcept
	{
		while (true)
		{
			std::uintptr_t old_head;
			std::uintptr_t new_head;

			old_head = this->head.load(std::memory_order_relaxed);

			node->next = this->unpack_ptr(old_head);
			new_head = this->pack(node, this->unpack_tag(old_head) + 1);

			if (this->head.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_relaxed))
				return;
		}
	}

	template <PoolableType T>
	memory::FreeNode *MemoryPool<T>::pop(void) noexcept
	{
		while (true)
		{
			std::uintptr_t old_head;
			std::uintptr_t new_head;
			memory::FreeNode *old_ptr;
			memory::FreeNode *next_node;

			old_head = this->head.load(std::memory_order_acquire);
			old_ptr = this->unpack_ptr(old_head);
			if (!old_ptr)
				return nullptr;
			
			next_node = old_ptr->next;
			new_head = this->pack(next_node, this->unpack_tag(old_head) + 1);

			if (this->head.compare_exchange_weak(old_head, new_head, std::memory_order_acquire, std::memory_order_relaxed))
				return old_ptr;
		}
	}

	template <PoolableType T>
	void MemoryPool<T>::expand(std::size_t capacity)
	{
		memory::Slot<T> *new_slots;
		memory::Chunk<T> *new_chunk;
		memory::FreeNode *first_node;
		memory::FreeNode *last_node;

		if (!capacity)
			return;
		
		new_slots = new memory::Slot<T>[capacity];
		new_chunk = new memory::Chunk<T>;

		new_chunk->slots = new_slots;
		new_chunk->capacity = capacity;
		new_chunk->next = this->chunks;
		this->chunks = new_chunk;

		first_node = this->build_free_list(new_slots, capacity);
		last_node = &new_slots[capacity - 1].free_node;

		this->push_range(first_node, last_node);
	}

	template <PoolableType T>
	memory::FreeNode *MemoryPool<T>::build_free_list(memory::Slot<T> *slots, std::size_t capacity) noexcept
	{
		if (!capacity)
			return nullptr;
		
		for (std::size_t i = 0; i < capacity - 1; ++i)
			slots[i].free_node.next = &slots[i + 1].free_node;
		
		slots[capacity - 1].free_node.next = nullptr;

		return &slots[0].free_node;
	}

	template <PoolableType T>
	void MemoryPool<T>::push_range(memory::FreeNode *first, memory::FreeNode *last) noexcept
	{
		if (!first || !last)
			return;
		
		while (true)
		{
			std::uintptr_t old_head;
			std::uintptr_t new_head;

			old_head = this->head.load(std::memory_order_relaxed);

			last->next = this->unpack_ptr(old_head);
			new_head = this->pack(first, this->unpack_tag(old_head) + 1);

			if (this->head.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_relaxed))
				return;
		}
	}
}

#endif