/**
 * File: memory_pool_test.cpp
 * Path: ajylib/tests/memory/threadlocal/memory_pool_test.cpp
 * Description:
 * 	Unit tests for ajy::memory::threadlocal::MemoryPool.
 * Author: ajy-dev
 * Created: 2026-06-17
 * Updated: Never
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/memory/threadlocal/memory_pool.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <thread>

using ajy::memory::threadlocal::MemoryPool;

namespace
{
	struct Trivial
	{
		int value;
	};

	struct NonTrivial
	{
		int *counter;

		explicit NonTrivial(int *c)
			: counter(c)
		{
			++(*this->counter);
		}

		~NonTrivial()
		{
			--(*this->counter);
		}
	};

	struct Base
	{
		virtual ~Base() = default;
		virtual int value(void) const = 0;
	};

	struct Derived : Base
	{
		int val;

		explicit Derived(int v)
			: val(v)
		{
		}

		int value(void) const override
		{
			return this->val;
		}
	};

	struct ThrowingCtor
	{
		ThrowingCtor()
		{
			throw std::runtime_error("ctor failed");
		}
	};
}

// ----------------------------------------------------------------
// Construction / State
// ----------------------------------------------------------------

TEST(ThreadlocalMemoryPoolUnitTest, InitialAllocSucceeds)
{
	MemoryPool<Trivial> pool;
	Trivial *ptr = pool.alloc();
	EXPECT_NE(ptr, nullptr);
	pool.free(ptr);
}

// ----------------------------------------------------------------
// alloc / free
// ----------------------------------------------------------------

TEST(ThreadlocalMemoryPoolUnitTest, AllocFreeReusesMemory)
{
	MemoryPool<Trivial> pool;
	Trivial *ptr1 = pool.alloc();
	ASSERT_NE(ptr1, nullptr);
	pool.free(ptr1);
	Trivial *ptr2 = pool.alloc();
	EXPECT_EQ(ptr1, ptr2);
}

TEST(ThreadlocalMemoryPoolUnitTest, FreeNullptr)
{
	MemoryPool<Trivial> pool;
	EXPECT_NO_FATAL_FAILURE(pool.free(nullptr));
}

TEST(ThreadlocalMemoryPoolUnitTest, AllocExhaustThenExpand)
{
	MemoryPool<Trivial> pool(2, 1);
	Trivial *ptr1 = pool.alloc();
	Trivial *ptr2 = pool.alloc();
	ASSERT_NE(ptr1, nullptr);
	ASSERT_NE(ptr2, nullptr);
	Trivial *ptr3 = pool.alloc();
	EXPECT_NE(ptr3, nullptr);
	pool.free(ptr1);
	pool.free(ptr2);
	pool.free(ptr3);
}

TEST(ThreadlocalMemoryPoolUnitTest, AllocReturnsAlignedPtr)
{
	MemoryPool<Trivial> pool;
	Trivial *ptr = pool.alloc();
	ASSERT_NE(ptr, nullptr);
	EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % alignof(Trivial), 0u);
	pool.free(ptr);
}

// ----------------------------------------------------------------
// create / destroy
// ----------------------------------------------------------------

TEST(ThreadlocalMemoryPoolUnitTest, CreateConstructsAndTracksObject)
{
	int counter = 0;
	MemoryPool<NonTrivial> pool;
	NonTrivial *ptr = pool.create(&counter);
	ASSERT_NE(ptr, nullptr);
	EXPECT_EQ(counter, 1);
	pool.destroy(ptr);
	EXPECT_EQ(counter, 0);
}

TEST(ThreadlocalMemoryPoolUnitTest, DestroyCallsDestructor)
{
	int counter = 0;
	MemoryPool<NonTrivial> pool;
	NonTrivial *ptr = pool.create(&counter);
	ASSERT_NE(ptr, nullptr);
	pool.destroy(ptr);
	EXPECT_EQ(counter, 0);
}

TEST(ThreadlocalMemoryPoolUnitTest, DestroyNullptr)
{
	MemoryPool<NonTrivial> pool;
	EXPECT_NO_FATAL_FAILURE(pool.destroy(nullptr));
}

TEST(ThreadlocalMemoryPoolUnitTest, CreateThrowingCtorFreesMemory)
{
	MemoryPool<ThrowingCtor> pool(2, 1);
	EXPECT_THROW(pool.create(), std::runtime_error);
	ThrowingCtor *ptr1 = pool.alloc();
	ThrowingCtor *ptr2 = pool.alloc();
	EXPECT_NE(ptr1, nullptr);
	EXPECT_NE(ptr2, nullptr);
	pool.free(ptr1);
	pool.free(ptr2);
}

TEST(ThreadlocalMemoryPoolUnitTest, CreateDerivedVirtualDispatch)
{
	MemoryPool<Derived> pool;
	Derived *ptr = pool.create(42);
	ASSERT_NE(ptr, nullptr);
	EXPECT_EQ(ptr->value(), 42);
	pool.destroy(ptr);
}

TEST(ThreadlocalMemoryPoolUnitTest, MultipleObjectsTracked)
{
	int counter = 0;
	MemoryPool<NonTrivial> pool;
	NonTrivial *ptr1 = pool.create(&counter);
	NonTrivial *ptr2 = pool.create(&counter);
	NonTrivial *ptr3 = pool.create(&counter);
	ASSERT_NE(ptr1, nullptr);
	ASSERT_NE(ptr2, nullptr);
	ASSERT_NE(ptr3, nullptr);
	EXPECT_EQ(counter, 3);
	pool.destroy(ptr1);
	pool.destroy(ptr2);
	pool.destroy(ptr3);
	EXPECT_EQ(counter, 0);
}

// ----------------------------------------------------------------
// Cross-thread free
// ----------------------------------------------------------------

TEST(ThreadlocalMemoryPoolUnitTest, CrossThreadFree)
{
	MemoryPool<Trivial> pool;
	Trivial *ptr = pool.alloc();
	ASSERT_NE(ptr, nullptr);

	std::thread t([&pool, ptr]()
		{
			pool.free(ptr);
		});
	t.join();

	Trivial *ptr2 = pool.alloc();
	EXPECT_NE(ptr2, nullptr);
	pool.free(ptr2);
}

// ----------------------------------------------------------------
// Multiple instances of the same type
// ----------------------------------------------------------------

TEST(ThreadlocalMemoryPoolUnitTest, TwoInstancesIndependent)
{
	MemoryPool<Trivial> pool_a;
	MemoryPool<Trivial> pool_b;

	Trivial *ptr_a = pool_a.alloc();
	Trivial *ptr_b = pool_b.alloc();

	ASSERT_NE(ptr_a, nullptr);
	ASSERT_NE(ptr_b, nullptr);
	EXPECT_NE(ptr_a, ptr_b);

	pool_a.free(ptr_a);
	pool_b.free(ptr_b);
}

TEST(ThreadlocalMemoryPoolUnitTest, TwoInstancesNoSlotCrossover)
{
	MemoryPool<Trivial> pool_a;
	MemoryPool<Trivial> pool_b;

	Trivial *ptr_a = pool_a.alloc();
	ASSERT_NE(ptr_a, nullptr);
	pool_a.free(ptr_a);

	Trivial *ptr_a2 = pool_a.alloc();
	EXPECT_EQ(ptr_a, ptr_a2);

	pool_a.free(ptr_a2);
	pool_b.free(pool_b.alloc());
}
