/**
 * File: object_pool.hpp
 * Path: ajylib/include/ajy/memory/threadlocal/object_pool.hpp
 * Description:
 *	A thread-local object pool declaration.
 *	Lock-free acquire/release via per-thread free lists.
 *	Falls back to a lockfree global pool on exhaustion or excess,
 *	enabling safe cross-thread object reuse.
 * Note:
 *	The pooled object is reset via clear() on release, not destroyed,
 *	so its internal state (e.g. a heap buffer) is retained across reuse.
 *	Because the object is built from ctor_args, the global pool cannot be
 *	shared across instances; each instance owns one and it is reference
 *	counted so that it outlives every thread still holding its slots.
 *	Requires a 64-bit platform with 48-bit canonical addresses.
 * Author: ajy-dev
 * Created: 2026-08-16
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_THREADLOCAL_OBJECT_POOL_HPP
#define AJY_MEMORY_THREADLOCAL_OBJECT_POOL_HPP

#include <ajy/memory/concepts.hpp>
#include <ajy/memory/lockfree/object_pool.hpp>
#include <ajy/memory/shared_types.hpp>

#include <atomic>
#include <cstddef>
#include <mutex>
#include <stack>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace ajy::memory::threadlocal
{
	template <ObjectPoolableType T, typename... Args>
	class ObjectPool
	{
	public:
		static constexpr std::size_t DEFAULT_CAPACITY = 1024;
		static constexpr std::size_t DEFAULT_MAX_SIZE = 1024;
		static constexpr std::size_t DEFAULT_BATCH_SIZE = 256;

		explicit ObjectPool(std::size_t initial_capacity = DEFAULT_CAPACITY, std::size_t max_size = DEFAULT_MAX_SIZE, std::size_t batch_size = DEFAULT_BATCH_SIZE, Args... args) noexcept(std::is_nothrow_constructible<std::tuple<Args...>, Args...>::value);
		~ObjectPool(void) noexcept(std::is_nothrow_destructible<T>::value);

		ObjectPool(const ObjectPool &other) = delete;
		ObjectPool &operator=(const ObjectPool &other) = delete;
		ObjectPool(ObjectPool &&other) = delete;
		ObjectPool &operator=(ObjectPool &&other) = delete;

		T *acquire(void) noexcept(std::is_nothrow_constructible<T, Args...>::value);
		void release(T *object) noexcept(noexcept(std::declval<T &>().clear()));

		std::size_t get_in_use_count(void) const noexcept;

	private:
		class GlobalPool
		{
		public:
			explicit GlobalPool(std::size_t initial_capacity, Args... args) noexcept(std::is_nothrow_constructible<std::tuple<Args...>, Args...>::value);
			~GlobalPool(void) noexcept(std::is_nothrow_destructible<T>::value);

			GlobalPool(const GlobalPool &other) = delete;
			GlobalPool &operator=(const GlobalPool &other) = delete;
			GlobalPool(GlobalPool &&other) = delete;
			GlobalPool &operator=(GlobalPool &&other) = delete;

		private:
			friend class ObjectPool<T, Args...>;

			void add_ref(void) noexcept;
			void release_ref(void) noexcept(std::is_nothrow_destructible<T>::value);

			lockfree::ObjectPool<T, Args...> pool;
			std::atomic<std::size_t> ref_count;
		};

		struct TLSSlot
		{
			TLSSlot(void) noexcept;
			~TLSSlot(void) noexcept;

			TLSSlot(const TLSSlot &other) = delete;
			TLSSlot &operator=(const TLSSlot &other) = delete;
			TLSSlot(TLSSlot &&other) noexcept;
			TLSSlot &operator=(TLSSlot &&other) = delete;

			ObjectSlot<T> *head;
			std::size_t count;
			GlobalPool *owner;
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

		static_assert(std::is_standard_layout<ObjectSlot<T>>::value, "ObjectSlot must be standard layout");

		static T *to_object(ObjectSlot<T> *slot) noexcept;
		static ObjectSlot<T> *to_slot(T *object) noexcept;
		static void discard_slot_contents(TLSSlot &slot) noexcept(std::is_nothrow_destructible<T>::value);

		TLSSlot *claim_slot(void) noexcept(std::is_nothrow_destructible<T>::value);
		void push(TLSSlot &slot, ObjectSlot<T> *object_slot) noexcept;
		ObjectSlot<T> *pop(TLSSlot &slot) noexcept;
		void refill_tls_from_global(TLSSlot &slot) noexcept(std::is_nothrow_constructible<T, Args...>::value);
		void drain_tls_to_global(TLSSlot &slot) noexcept;

		inline static thread_local std::vector<TLSSlot> tls_freelists;
		inline static IndexAllocator index_allocator;

		GlobalPool *global;
		std::size_t tls_index;
		std::size_t max_size;
		std::size_t batch_size;

		std::atomic<std::size_t> in_use_count;
	};
}

#include <ajy/memory/threadlocal/object_pool.tpp>

#endif
