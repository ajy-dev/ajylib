/**
 * File: memory_pool.hpp
 * Path: ajylib/include/ajy/memory/lockfree/memory_pool.hpp
 * Description:
 * 	A lockfree memory pool declaration.
 * 	MPMC-safe via CAS with tagged pointers.
 * Note:
 * 	OS-level deallocation is deferred until pool destruction.
 * 	Tagged pointers use upper 16 bits for ABA prevention.
 * 	Requires a 64-bit platform with 48-bit canonical addresses.
 * Author: ajy-dev
 * Created: 2026-06-16
 * Updated: 2026-07-20
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_LOCKFREE_MEMORY_POOL_HPP
#define AJY_MEMORY_LOCKFREE_MEMORY_POOL_HPP

#include <ajy/memory/concepts.hpp>
#include <ajy/memory/shared_types.hpp>

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <type_traits>

namespace ajy::memory::lockfree
{
	template <MemoryPoolableType T>
	class MemoryPool
	{
	public:
		static constexpr std::size_t DEFAULT_CAPACITY = 1024;

		explicit MemoryPool(std::size_t initial_capacity = DEFAULT_CAPACITY) noexcept;
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
		static_assert(sizeof(std::uintptr_t) == 8, "Requires 64-bit platform");

		static std::uintptr_t pack(FreeNode *ptr, std::uint16_t tag) noexcept;
		static FreeNode *unpack_ptr(std::uintptr_t raw) noexcept;
		static std::uint16_t unpack_tag(std::uintptr_t raw) noexcept;

		void push(FreeNode *node) noexcept;
		FreeNode *pop(void) noexcept;

		bool expand(std::size_t capacity) noexcept;

		static FreeNode *build_free_list(Slot<T> *slots, std::size_t capacity) noexcept;
		void push_range(FreeNode *first, FreeNode *last) noexcept;

		std::atomic<std::uintptr_t> head;
		std::size_t expand_size;
		std::mutex expand_lock;
		Chunk<T> *chunks;
		std::atomic<std::size_t> in_use_count;
	};
}

#include <ajy/memory/lockfree/memory_pool.tpp>

#endif
