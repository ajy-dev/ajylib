/**
 * File: object_pool_test.cpp
 * Path: ajylib/tests/memory/lockfree/object_pool_test.cpp
 * Description:
 * 	Unit tests for ajy::memory::lockfree::ObjectPool.
 * Author: ajy-dev
 * Created: 2026-07-20
 * Updated: Never
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/memory/lockfree/object_pool.hpp>

#include <cstddef>

using ajy::memory::lockfree::ObjectPool;

namespace
{
	// Tracks live instance count and per-instance clear() calls, and exposes
	// clear() as required by ObjectPoolableType. live_count observes that the
	// pool destroys survivors (never leaks) and does not destroy on release.
	struct Tracked
	{
		static inline int live_count = 0;

		std::size_t capacity;
		int clear_count;

		explicit Tracked(std::size_t capacity)
			: capacity(capacity)
			, clear_count(0)
		{
			++Tracked::live_count;
		}

		~Tracked(void)
		{
			--Tracked::live_count;
		}

		Tracked(const Tracked &other) = delete;
		Tracked &operator=(const Tracked &other) = delete;
		Tracked(Tracked &&other) = delete;
		Tracked &operator=(Tracked &&other) = delete;

		void clear(void)
		{
			++this->clear_count;
		}
	};
}

// ----------------------------------------------------------------
// acquire / release
// ----------------------------------------------------------------

TEST(ObjectPoolUnitTest, AcquireConstructsWithStoredArgs)
{
	ObjectPool<Tracked, std::size_t> pool(16, 492);
	Tracked *object = pool.acquire();
	ASSERT_NE(object, nullptr);
	EXPECT_EQ(object->capacity, 492u);
	pool.release(object);
}

TEST(ObjectPoolUnitTest, ReleaseReusesSameObject)
{
	ObjectPool<Tracked, std::size_t> pool(16, 492);
	Tracked *first = pool.acquire();
	ASSERT_NE(first, nullptr);
	pool.release(first);
	Tracked *second = pool.acquire();
	EXPECT_EQ(first, second);
	pool.release(second);
}

TEST(ObjectPoolUnitTest, ReleaseDoesNotDestroy)
{
	Tracked::live_count = 0;
	ObjectPool<Tracked, std::size_t> pool(16, 492);
	Tracked *object = pool.acquire();
	ASSERT_NE(object, nullptr);
	EXPECT_EQ(Tracked::live_count, 1);
	pool.release(object);
	EXPECT_EQ(Tracked::live_count, 1);
}

TEST(ObjectPoolUnitTest, ReleaseCallsClear)
{
	ObjectPool<Tracked, std::size_t> pool(16, 492);
	Tracked *object = pool.acquire();
	ASSERT_NE(object, nullptr);
	EXPECT_EQ(object->clear_count, 0);
	pool.release(object);
	Tracked *reused = pool.acquire();
	ASSERT_EQ(reused, object);
	EXPECT_EQ(reused->clear_count, 1);
	pool.release(reused);
}

TEST(ObjectPoolUnitTest, ReleaseNullptr)
{
	ObjectPool<Tracked, std::size_t> pool(16, 492);
	EXPECT_NO_FATAL_FAILURE(pool.release(nullptr));
}

TEST(ObjectPoolUnitTest, AcquireBeyondCapacityExpands)
{
	ObjectPool<Tracked, std::size_t> pool(1, 492);
	Tracked *first = pool.acquire();
	Tracked *second = pool.acquire();
	ASSERT_NE(first, nullptr);
	ASSERT_NE(second, nullptr);
	EXPECT_NE(first, second);
	pool.release(first);
	pool.release(second);
}

// ----------------------------------------------------------------
// in-use count
// ----------------------------------------------------------------

TEST(ObjectPoolUnitTest, InUseCountReflectsOutstanding)
{
	ObjectPool<Tracked, std::size_t> pool(16, 492);
	EXPECT_EQ(pool.get_in_use_count(), 0u);
	Tracked *a = pool.acquire();
	Tracked *b = pool.acquire();
	EXPECT_EQ(pool.get_in_use_count(), 2u);
	pool.release(a);
	EXPECT_EQ(pool.get_in_use_count(), 1u);
	pool.release(b);
	EXPECT_EQ(pool.get_in_use_count(), 0u);
}

TEST(ObjectPoolUnitTest, InUseCountUnchangedByReuse)
{
	ObjectPool<Tracked, std::size_t> pool(16, 492);
	Tracked *object = pool.acquire();
	pool.release(object);
	EXPECT_EQ(pool.get_in_use_count(), 0u);
	Tracked *reused = pool.acquire();
	EXPECT_EQ(pool.get_in_use_count(), 1u);
	pool.release(reused);
}

// ----------------------------------------------------------------
// destruction
// ----------------------------------------------------------------

TEST(ObjectPoolUnitTest, DestructorDestroysRecycledObjects)
{
	Tracked::live_count = 0;
	{
		ObjectPool<Tracked, std::size_t> pool(16, 492);
		Tracked *a = pool.acquire();
		Tracked *b = pool.acquire();
		pool.release(a);
		pool.release(b);
		EXPECT_EQ(Tracked::live_count, 2);
	}
	EXPECT_EQ(Tracked::live_count, 0);
}
