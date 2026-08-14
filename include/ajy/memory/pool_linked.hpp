/**
 * File: pool_linked.hpp
 * Path: ajylib/include/ajy/memory/pool_linked.hpp
 * Description:
 * 	An intrusive free-list link for pooled objects.
 * Note:
 * 	ObjectPool keeps its objects constructed between uses, so unlike MemoryPool
 * 	it cannot overlay a FreeNode on their storage. Without a link of its own it
 * 	has to box every pointer in a separate node, which means a second lock-free
 * 	structure underneath the first: four compare-exchanges and three dependent
 * 	loads across cores per acquire/release pair. This member removes the box.
 *
 * 	The link is meaningful only while the object sits in the free list. It is
 * 	not carried by copy or move: a new object is not in any list.
 * Author: ajy-dev
 * Created: 2026-08-14
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_POOL_LINKED_HPP
#define AJY_MEMORY_POOL_LINKED_HPP

namespace ajy::memory
{
	class PoolLinked
	{
	public:
		PoolLinked(void) noexcept
			: pool_next(nullptr)
		{
		}

		~PoolLinked(void) noexcept
		{
		}

		PoolLinked(const PoolLinked &other) noexcept
			: pool_next(nullptr)
		{
			(void)other;
		}

		PoolLinked &operator=(const PoolLinked &other) noexcept
		{
			(void)other;

			return *this;
		}

		PoolLinked(PoolLinked &&other) noexcept
			: pool_next(nullptr)
		{
			(void)other;
		}

		PoolLinked &operator=(PoolLinked &&other) noexcept
		{
			(void)other;

			return *this;
		}

		void *get_pool_next(void) const noexcept
		{
			return this->pool_next;
		}

		void set_pool_next(void *next) noexcept
		{
			this->pool_next = next;
		}

	private:
		void *pool_next;
	};
}

#endif
