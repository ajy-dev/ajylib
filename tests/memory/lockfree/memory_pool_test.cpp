/**
 * File: memory_pool_test.cpp
 * Path: ajylib/tests/memory/lockfree/memory_pool_test.cpp
 * Description:
 * 	Unit tests for ajy::memory::lockfree::MemoryPool.
 * Author: ajy-dev
 * Created: 2026-06-16
 * Updated: Never
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/memory/lockfree/memory_pool.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>

using ajy::memory::lockfree::MemoryPool;

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

TEST(MemoryPoolUnitTest, InitialAllocSucceeds)
{
	MemoryPool<Trivial> pool(16);
	Trivial *ptr = pool.alloc();
	EXPECT_NE(ptr, nullptr);
	pool.free(ptr);
}

TEST(MemoryPoolUnitTest, ZeroCapacityConstruction)
{
	MemoryPool<Trivial> pool(0);
	Trivial *ptr = pool.alloc();
	EXPECT_EQ(ptr, nullptr);
}

// ----------------------------------------------------------------
// alloc / free
// ----------------------------------------------------------------

TEST(MemoryPoolUnitTest, AllocFreeReusesMemory)
{
	MemoryPool<Trivial> pool(1);
	Trivial *ptr1 = pool.alloc();
	ASSERT_NE(ptr1, nullptr);
	pool.free(ptr1);
	Trivial *ptr2 = pool.alloc();
	EXPECT_EQ(ptr1, ptr2);
}

TEST(MemoryPoolUnitTest, FreeNullptr)
{
	MemoryPool<Trivial> pool(16);
	EXPECT_NO_FATAL_FAILURE(pool.free(nullptr));
}

TEST(MemoryPoolUnitTest, AllocExhaustThenExpand)
{
	MemoryPool<Trivial> pool(2);
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

TEST(MemoryPoolUnitTest, AllocReturnsAlignedPtr)
{
	MemoryPool<Trivial> pool(16);
	Trivial *ptr = pool.alloc();
	ASSERT_NE(ptr, nullptr);
	EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % alignof(Trivial), 0u);
	pool.free(ptr);
}

// ----------------------------------------------------------------
// create / destroy
// ----------------------------------------------------------------

TEST(MemoryPoolUnitTest, CreateConstructsAndTracksObject)
{
	int counter = 0;
	MemoryPool<NonTrivial> pool(16);
	NonTrivial *ptr = pool.create(&counter);
	ASSERT_NE(ptr, nullptr);
	EXPECT_EQ(counter, 1);
	pool.destroy(ptr);
	EXPECT_EQ(counter, 0);
}

TEST(MemoryPoolUnitTest, DestroyCallsDestructor)
{
	int counter = 0;
	MemoryPool<NonTrivial> pool(16);
	NonTrivial *ptr = pool.create(&counter);
	ASSERT_NE(ptr, nullptr);
	pool.destroy(ptr);
	EXPECT_EQ(counter, 0);
}

TEST(MemoryPoolUnitTest, DestroyNullptr)
{
	MemoryPool<NonTrivial> pool(16);
	EXPECT_NO_FATAL_FAILURE(pool.destroy(nullptr));
}

TEST(MemoryPoolUnitTest, CreateThrowingCtorFreesMemory)
{
	MemoryPool<ThrowingCtor> pool(2);
	EXPECT_THROW(pool.create(), std::runtime_error);
	ThrowingCtor *ptr1 = pool.alloc();
	ThrowingCtor *ptr2 = pool.alloc();
	EXPECT_NE(ptr1, nullptr);
	EXPECT_NE(ptr2, nullptr);
	pool.free(ptr1);
	pool.free(ptr2);
}

TEST(MemoryPoolUnitTest, CreateDerivedVirtualDispatch)
{
	MemoryPool<Derived> pool(16);
	Derived *ptr = pool.create(42);
	ASSERT_NE(ptr, nullptr);
	EXPECT_EQ(ptr->value(), 42);
	pool.destroy(ptr);
}

TEST(MemoryPoolUnitTest, MultipleObjectsTracked)
{
	int counter = 0;
	MemoryPool<NonTrivial> pool(16);
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
