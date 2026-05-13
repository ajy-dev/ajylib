/**
 * File: memory_pool.hpp
 * Path: library/include/ajy/memory/lockfree/memory_pool.hpp
 * Description:
 * 	Lock-free memory pool with mutex-guarded expansion.
 * 	Requires a 64-bit platform with 48-bit canonical addresses.
 * Author: ajy-dev
 * Created: 2026-05-13
 * Updated: 2026-05-14
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_LOCKFREE_MEMORY_POOL_HPP
# define AJY_MEMORY_LOCKFREE_MEMORY_POOL_HPP

# include <cstddef>
# include <cstdint>
# include <concepts>
# include <type_traits>
# include <atomic>
# include <mutex>
# include <ajy/memory/concepts.hpp>
# include <ajy/memory/shared_types.hpp>

namespace ajy::memory::lockfree
{
	template <PoolableType T>
	class MemoryPool
	{
	public:
		static constexpr std::size_t DEFAULT_CAPACITY = 1024;

		explicit MemoryPool(std::size_t initial_capacity = DEFAULT_CAPACITY);
		~MemoryPool(void) noexcept;

		MemoryPool(const MemoryPool &other) = delete;
		MemoryPool &operator=(const MemoryPool &other) = delete;
		MemoryPool(MemoryPool &&other) = delete;
		MemoryPool &operator=(MemoryPool &&other) = delete;

		T *alloc(void);
		void free(T *ptr) noexcept;

		template <typename... Args>
		T *create(Args&&... args) requires std::constructible_from<T, Args...>;
		void destroy(T *ptr) noexcept(std::is_nothrow_destructible<T>::value) requires std::destructible<T>;
	private:
		static_assert(sizeof(std::uintptr_t) == 8, "Requires 64-bit platform");

		static std::uintptr_t pack(memory::FreeNode *ptr, std::uint16_t tag) noexcept;
		static memory::FreeNode *unpack_ptr(std::uintptr_t raw) noexcept;
		static std::uint16_t unpack_tag(std::uintptr_t raw) noexcept;

		void push(memory::FreeNode *node) noexcept;
		memory::FreeNode *pop(void) noexcept;

		void expand(std::size_t capacity);
		
		static memory::FreeNode *build_free_list(memory::Slot<T> *slots, std::size_t capacity) noexcept;
		void push_range(memory::FreeNode *first, memory::FreeNode *last) noexcept;

		std::atomic<std::uintptr_t> head;
		memory::Chunk<T> *chunks;
		std::size_t expand_size;
		std::mutex expand_lock;
	};
}

# include <ajy/memory/lockfree/memory_pool.tpp>

#endif