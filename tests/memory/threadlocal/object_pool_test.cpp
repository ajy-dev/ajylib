/**
 * File: object_pool_test.cpp
 * Path: ajylib/tests/memory/threadlocal/object_pool_test.cpp
 * Description:
 *	Unit tests for ajy::memory::threadlocal::ObjectPool.
 * Author: ajy-dev
 * Created: 2026-08-16
 * Updated: Never
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/memory/threadlocal/object_pool.hpp>

#include <atomic>
#include <cstddef>
#include <set>
#include <thread>
#include <vector>

using ajy::memory::threadlocal::ObjectPool;

namespace
{
	// Tracks live instance count and per-instance clear() calls, and exposes
	// clear() as required by ObjectPoolableType. live_count observes that the
	// pool destroys survivors (never leaks) and does not destroy on release.
	struct Tracked
	{
		static inline std::atomic<int> live_count = 0;

		std::size_t capacity;
		int clear_count;

		explicit Tracked(std::size_t capacity)
			: capacity(capacity)
			, clear_count(0)
		{
			Tracked::live_count.fetch_add(1);
		}

		~Tracked(void)
		{
			Tracked::live_count.fetch_sub(1);
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

	// Must not be used elsewhere; the TLS vector is per-type and never shrinks.
	struct Exclusive
	{
		std::size_t capacity;

		explicit Exclusive(std::size_t capacity)
			: capacity(capacity)
		{
		}

		void clear(void) noexcept
		{
		}
	};
}

// ----------------------------------------------------------------
// acquire / release
// ----------------------------------------------------------------

TEST(ThreadlocalObjectPoolUnitTest, AcquireConstructsWithStoredArgs)
{
	ObjectPool<Tracked, std::size_t> pool(16, 8, 4, 492);
	Tracked *object = pool.acquire();
	ASSERT_NE(object, nullptr);
	EXPECT_EQ(object->capacity, 492u);
	pool.release(object);
}

TEST(ThreadlocalObjectPoolUnitTest, ReleaseReusesSameObject)
{
	ObjectPool<Tracked, std::size_t> pool(16, 8, 4, 492);
	Tracked *first = pool.acquire();
	ASSERT_NE(first, nullptr);
	pool.release(first);
	Tracked *second = pool.acquire();
	EXPECT_EQ(first, second);
	pool.release(second);
}

TEST(ThreadlocalObjectPoolUnitTest, ReleaseDoesNotDestroy)
{
	ObjectPool<Tracked, std::size_t> pool(16, 8, 4, 492);
	Tracked *object = pool.acquire();
	ASSERT_NE(object, nullptr);
	int before = Tracked::live_count.load();
	pool.release(object);
	EXPECT_EQ(Tracked::live_count.load(), before);
}

TEST(ThreadlocalObjectPoolUnitTest, ReleaseCallsClear)
{
	ObjectPool<Tracked, std::size_t> pool(16, 8, 4, 492);
	Tracked *object = pool.acquire();
	ASSERT_NE(object, nullptr);
	EXPECT_EQ(object->clear_count, 0);
	pool.release(object);
	Tracked *reused = pool.acquire();
	ASSERT_EQ(reused, object);
	EXPECT_GE(reused->clear_count, 1);
	pool.release(reused);
}

TEST(ThreadlocalObjectPoolUnitTest, ReleaseNullptr)
{
	ObjectPool<Tracked, std::size_t> pool(16, 8, 4, 492);
	EXPECT_NO_FATAL_FAILURE(pool.release(nullptr));
}

TEST(ThreadlocalObjectPoolUnitTest, AcquireBeyondCapacityExpands)
{
	ObjectPool<Tracked, std::size_t> pool(1, 8, 1, 492);
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

TEST(ThreadlocalObjectPoolUnitTest, InUseCountReflectsOutstanding)
{
	ObjectPool<Tracked, std::size_t> pool(16, 8, 4, 492);
	EXPECT_EQ(pool.get_in_use_count(), 0u);
	Tracked *a = pool.acquire();
	Tracked *b = pool.acquire();
	EXPECT_EQ(pool.get_in_use_count(), 2u);
	pool.release(a);
	EXPECT_EQ(pool.get_in_use_count(), 1u);
	pool.release(b);
	EXPECT_EQ(pool.get_in_use_count(), 0u);
}

TEST(ThreadlocalObjectPoolUnitTest, InUseCountUnchangedByReuse)
{
	ObjectPool<Tracked, std::size_t> pool(16, 8, 4, 492);
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

TEST(ThreadlocalObjectPoolUnitTest, DestructorDestroysRecycledObjects)
{
	Tracked::live_count = 0;
	{
		ObjectPool<Tracked, std::size_t> pool(16, 8, 4, 492);
		Tracked *a = pool.acquire();
		Tracked *b = pool.acquire();
		pool.release(a);
		pool.release(b);
		EXPECT_GT(Tracked::live_count.load(), 0);
	}
	EXPECT_EQ(Tracked::live_count.load(), 0);
}

TEST(ThreadlocalObjectPoolUnitTest, RepeatedInstancesDoNotAccumulate)
{
	Tracked::live_count = 0;
	for (int i = 0; i < 100; ++i)
	{
		ObjectPool<Tracked, std::size_t> pool(4, 2, 1, 492);
		Tracked *object = pool.acquire();
		pool.release(object);
	}
	EXPECT_EQ(Tracked::live_count.load(), 0);
}

TEST(ThreadlocalObjectPoolUnitTest, GlobalPoolOutlivesInstanceUntilThreadReleases)
{
	Tracked::live_count = 0;
	{
		std::atomic<int> phase = 0;
		ObjectPool<Tracked, std::size_t> *pool = new ObjectPool<Tracked, std::size_t>(4, 2, 1, 492);

		// The worker keeps slots in its own TLS slot, which the instance
		// destructor cannot reach, so the global pool must survive.
		std::thread worker([&pool, &phase]()
			{
				Tracked *object = pool->acquire();
				pool->release(object);
				phase = 1;
				while (phase != 2)
					std::this_thread::yield();
			});

		while (phase != 1)
			std::this_thread::yield();

		delete pool;
		EXPECT_GT(Tracked::live_count.load(), 0);

		phase = 2;
		worker.join();
	}
	EXPECT_EQ(Tracked::live_count.load(), 0);
}

// ----------------------------------------------------------------
// Multiple instances of the same type
// ----------------------------------------------------------------

TEST(ThreadlocalObjectPoolUnitTest, TwoInstancesDoNotShareObjects)
{
	ObjectPool<Tracked, std::size_t> pool_a(4, 2, 1, 492);
	ObjectPool<Tracked, std::size_t> pool_b(4, 2, 1, 492);

	Tracked *a1 = pool_a.acquire();
	Tracked *b1 = pool_b.acquire();
	Tracked *a2 = pool_a.acquire();
	Tracked *b2 = pool_b.acquire();

	ASSERT_NE(a1, nullptr);
	ASSERT_NE(b1, nullptr);
	ASSERT_NE(a2, nullptr);
	ASSERT_NE(b2, nullptr);

	EXPECT_NE(a1, b1);
	EXPECT_NE(a1, b2);
	EXPECT_NE(a2, b1);
	EXPECT_NE(a2, b2);

	pool_a.release(a1);
	pool_a.release(a2);
	pool_b.release(b1);
	pool_b.release(b2);
}

TEST(ThreadlocalObjectPoolUnitTest, ReusedIndexDoesNotLeakForeignObjects)
{
	std::atomic<int> phase = 0;
	std::set<std::size_t> observed;
	ObjectPool<Exclusive, std::size_t> *first = new ObjectPool<Exclusive, std::size_t>(4, 2, 1, 492);
	ObjectPool<Exclusive, std::size_t> *second = nullptr;

	// The worker fills its TLS slot from the first instance, then the second
	// instance takes over the released index and must not hand those out.
	std::thread worker([&first, &second, &phase, &observed]()
		{
			for (int i = 0; i < 4; ++i)
			{
				Exclusive *object = first->acquire();
				first->release(object);
			}

			phase = 1;
			while (phase != 2)
				std::this_thread::yield();

			for (int i = 0; i < 8; ++i)
			{
				Exclusive *object = second->acquire();
				observed.insert(object->capacity);
				second->release(object);
			}

			phase = 3;
			while (phase != 4)
				std::this_thread::yield();
		});

	while (phase != 1)
		std::this_thread::yield();

	delete first;
	second = new ObjectPool<Exclusive, std::size_t>(4, 2, 1, 159);
	phase = 2;

	while (phase != 3)
		std::this_thread::yield();

	EXPECT_EQ(observed.size(), 1u);
	EXPECT_EQ(*observed.begin(), 159u);

	phase = 4;
	worker.join();
	delete second;
}

// ----------------------------------------------------------------
// Concurrency
// ----------------------------------------------------------------

TEST(ThreadlocalObjectPoolUnitTest, ConcurrentAcquireReleaseKeepsObjectsValid)
{
	static constexpr int THREAD_COUNT = 8;
	static constexpr int ITERATION_COUNT = 20000;

	ObjectPool<Tracked, std::size_t> pool(64, 32, 16, 492);
	std::atomic<bool> corrupted = false;
	std::vector<std::thread> workers;

	for (int i = 0; i < THREAD_COUNT; ++i)
		workers.emplace_back([&pool, &corrupted]()
			{
				for (int j = 0; j < ITERATION_COUNT; ++j)
				{
					Tracked *object = pool.acquire();

					if (!object || object->capacity != 492u)
						corrupted = true;

					pool.release(object);
				}
			});

	for (std::thread &worker : workers)
		worker.join();

	EXPECT_FALSE(corrupted.load());
	EXPECT_EQ(pool.get_in_use_count(), 0u);
}

TEST(ThreadlocalObjectPoolUnitTest, CrossThreadRelease)
{
	ObjectPool<Tracked, std::size_t> pool(16, 8, 4, 492);
	Tracked *object = pool.acquire();
	ASSERT_NE(object, nullptr);

	std::thread worker([&pool, object]()
		{
			pool.release(object);
		});
	worker.join();

	EXPECT_EQ(pool.get_in_use_count(), 0u);

	Tracked *reacquired = pool.acquire();
	EXPECT_NE(reacquired, nullptr);
	pool.release(reacquired);
}
