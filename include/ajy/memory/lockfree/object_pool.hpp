/**
 * File: object_pool.hpp
 * Path: ajylib/include/ajy/memory/lockfree/object_pool.hpp
 * Description:
 * 	A lockfree object pool declaration.
 * Note:
 * 	The pooled object is reset via clear() on release, not destroyed,
 * 	so its internal state (e.g. a heap buffer) is retained across reuse.
 * Author: ajy-dev
 * Created: 2026-07-20
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_LOCKFREE_OBJECT_POOL_HPP
#define AJY_MEMORY_LOCKFREE_OBJECT_POOL_HPP

#include <ajy/container/lockfree/stack.hpp>
#include <ajy/memory/concepts.hpp>
#include <ajy/memory/lockfree/memory_pool.hpp>

#include <atomic>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ajy::memory::lockfree
{
	template <ObjectPoolableType T, typename... Args>
	class ObjectPool
	{
	public:
		static constexpr std::size_t DEFAULT_CAPACITY = 1024;

		explicit ObjectPool(std::size_t initial_capacity = DEFAULT_CAPACITY, Args... args) noexcept(std::is_nothrow_constructible<std::tuple<Args...>, Args...>::value);
		~ObjectPool(void) noexcept(std::is_nothrow_destructible<T>::value);

		ObjectPool(const ObjectPool &other) = delete;
		ObjectPool &operator=(const ObjectPool &other) = delete;
		ObjectPool(ObjectPool &&other) = delete;
		ObjectPool &operator=(ObjectPool &&other) = delete;

		T *acquire(void) noexcept(std::is_nothrow_constructible<T, Args...>::value);
		void release(T *object) noexcept(noexcept(std::declval<T &>().clear()));

		std::size_t get_in_use_count(void) const noexcept;

	private:
		std::tuple<Args...> ctor_args;
		MemoryPool<T> storage;
		container::lockfree::Stack<T *> free_objects;
		std::atomic<std::size_t> in_use_count;
	};
}

#include <ajy/memory/lockfree/object_pool.tpp>

#endif
