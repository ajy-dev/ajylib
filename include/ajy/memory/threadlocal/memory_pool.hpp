/**
 * File: memory_pool.hpp
 * Path: ajylib/include/ajy/memory/threadlocal/memory_pool.hpp
 * Description:
 * 	A thread-local memory pool declaration.
 * 	Lock-free alloc/free via per-thread free lists.
 * 	Falls back to a shared lockfree global pool on exhaustion
 * 	or excess, enabling safe cross-thread memory reuse.
 * Note:
 * 	The global pool is shared across all MemoryPool<T> instances of the
 * 	same type and is initialized with lockfree::MemoryPool<T> defaults.
 * 	Requires a 64-bit platform with 48-bit canonical addresses.
 * Author: ajy-dev
 * Created: 2026-06-17
 * Updated: 2026-07-07
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_THREADLOCAL_MEMORY_POOL_HPP
#define AJY_MEMORY_THREADLOCAL_MEMORY_POOL_HPP

#include <ajy/memory/concepts.hpp>
#include <ajy/memory/lockfree/memory_pool.hpp>
#include <ajy/memory/shared_types.hpp>

#include <atomic>
#include <cstddef>
#include <mutex>
#include <stack>
#include <type_traits>
#include <vector>

namespace ajy::memory::threadlocal
{
	template <PoolableType T>
	class MemoryPool
	{
	public:
		static constexpr std::size_t DEFAULT_MAX_SIZE = 1024;
		static constexpr std::size_t DEFAULT_BATCH_SIZE = 256;

		explicit MemoryPool(std::size_t max_size = DEFAULT_MAX_SIZE, std::size_t batch_size = DEFAULT_BATCH_SIZE) noexcept;
		~MemoryPool(void) noexcept;

		MemoryPool(const MemoryPool &other) = delete;
		MemoryPool &operator=(const MemoryPool &other) = delete;
		MemoryPool(MemoryPool &&other) = delete;
		MemoryPool &operator=(MemoryPool &&other) = delete;

		T *alloc(void) noexcept;
		void free(T *ptr) noexcept;

		template <typename... Args>
		T *create(Args &&...args) noexcept(std::is_nothrow_constructible<T, Args...>::value);

		void destroy(T *ptr) noexcept(std::is_nothrow_destructible<T>::value)
		requires std::destructible<T>;

		std::size_t get_in_use_count(void) const noexcept;

	private:
		struct TLSSlot
		{
			TLSSlot(void) noexcept;
			~TLSSlot(void) noexcept;

			TLSSlot(const TLSSlot &other) = delete;
			TLSSlot &operator=(const TLSSlot &other) = delete;
			TLSSlot(TLSSlot &&other) noexcept = default;
			TLSSlot &operator=(TLSSlot &&other) noexcept = default;

			FreeNode *head;
			std::size_t count;
		};

		class IndexAllocator
		{
		public:
			IndexAllocator(void) = default;
			~IndexAllocator(void) = default;

			IndexAllocator(const IndexAllocator &other) = delete;
			IndexAllocator &operator=(const IndexAllocator &other) = delete;
			IndexAllocator(IndexAllocator &&other) = delete;
			IndexAllocator &operator=(IndexAllocator &&other) = delete;

			std::size_t acquire(void) noexcept;
			void release(std::size_t index) noexcept;

		private:
			std::size_t count = 0;
			std::stack<std::size_t> free_indices;
			std::mutex lock;
		};

		static lockfree::MemoryPool<T> &get_global_pool(void) noexcept;

		void push(FreeNode *node) noexcept;
		FreeNode *pop(void) noexcept;
		bool has_tls_slot(void) const noexcept;
		void refill_tls_from_global(void) noexcept;
		void drain_tls_to_global(void) noexcept;

		inline static thread_local std::vector<TLSSlot> tls_freelists;
		inline static IndexAllocator index_allocator;

		std::size_t tls_index;
		std::size_t max_size;
		std::size_t batch_size;

		std::atomic<std::size_t> in_use_count;
	};
}

#include <ajy/memory/threadlocal/memory_pool.tpp>

#endif
