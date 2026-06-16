/**
 * File: queue_test.cpp
 * Path: ajylib/tests/container/lockfree/queue_test.cpp
 * Description:
 *	Unit tests for ajy::container::lockfree::Queue.
 * Author: ajy-dev
 * Created: 2026-06-16
 * Updated: Never
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/container/lockfree/queue.hpp>

#include <cstddef>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>

using ajy::container::lockfree::Queue;

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
			if (this->counter)
				--(*this->counter);
		}

		NonTrivial(const NonTrivial &other)
			: counter(other.counter)
		{
			if (this->counter)
				++(*this->counter);
		}

		NonTrivial(NonTrivial &&other) noexcept
			: counter(other.counter)
		{
			other.counter = nullptr;
		}
	};

	struct ThrowingMoveCtor
	{
		ThrowingMoveCtor(void) = default;
		ThrowingMoveCtor(const ThrowingMoveCtor &) = default;

		ThrowingMoveCtor(ThrowingMoveCtor &&)
		{
			throw std::runtime_error("move failed");
		}
	};
}

// ----------------------------------------------------------------
// Construction / State
// ----------------------------------------------------------------

TEST(LockfreeQueueUnitTest, EnqueueSucceedsWithDefaultCapacity)
{
	Queue<int> queue;
	EXPECT_TRUE(queue.enqueue(42));
}

TEST(LockfreeQueueUnitTest, ZeroCapacityEnqueueFails)
{
	Queue<int> queue(0);
	EXPECT_FALSE(queue.enqueue(1));
}

TEST(LockfreeQueueUnitTest, ZeroCapacityDequeueReturnsNullopt)
{
	Queue<int> queue(0);
	EXPECT_FALSE(queue.dequeue().has_value());
}

// ----------------------------------------------------------------
// enqueue / dequeue
// ----------------------------------------------------------------

TEST(LockfreeQueueUnitTest, BasicEnqueueDequeueRvalue)
{
	Queue<int> queue(16);
	ASSERT_TRUE(queue.enqueue(42));
	std::optional<int> result = queue.dequeue();
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, 42);
}

TEST(LockfreeQueueUnitTest, BasicEnqueueDequeueLvalue)
{
	Queue<int> queue(16);
	int n = 42;
	ASSERT_TRUE(queue.enqueue(n));
	std::optional<int> result = queue.dequeue();
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, 42);
}

TEST(LockfreeQueueUnitTest, LvalueEnqueuePreservesOriginal)
{
	Queue<Trivial> queue(16);
	Trivial obj{99};
	ASSERT_TRUE(queue.enqueue(obj));
	EXPECT_EQ(obj.value, 99);
	std::optional<Trivial> result = queue.dequeue();
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->value, 99);
}

TEST(LockfreeQueueUnitTest, DequeueEmptyReturnsNullopt)
{
	Queue<int> queue(16);
	EXPECT_FALSE(queue.dequeue().has_value());
}

// ----------------------------------------------------------------
// FIFO Ordering
// ----------------------------------------------------------------

TEST(LockfreeQueueUnitTest, FIFOOrdering)
{
	Queue<int> queue(16);
	ASSERT_TRUE(queue.enqueue(1));
	ASSERT_TRUE(queue.enqueue(2));
	ASSERT_TRUE(queue.enqueue(3));
	EXPECT_EQ(*queue.dequeue(), 1);
	EXPECT_EQ(*queue.dequeue(), 2);
	EXPECT_EQ(*queue.dequeue(), 3);
	EXPECT_FALSE(queue.dequeue().has_value());
}

// ----------------------------------------------------------------
// Non-trivial
// ----------------------------------------------------------------

TEST(LockfreeQueueUnitTest, DestructorCleansUpRemainingNodes)
{
	int counter = 0;
	{
		Queue<NonTrivial> queue(16);
		NonTrivial obj1(&counter);
		NonTrivial obj2(&counter);
		ASSERT_TRUE(queue.enqueue(std::move(obj1)));
		ASSERT_TRUE(queue.enqueue(std::move(obj2)));
		EXPECT_EQ(counter, 2);
	}
	EXPECT_EQ(counter, 0);
}

// ----------------------------------------------------------------
// Throwing move constructor
// ----------------------------------------------------------------

TEST(LockfreeQueueUnitTest, ThrowingMoveCtorPropagatesException)
{
	Queue<ThrowingMoveCtor> queue(16);
	ThrowingMoveCtor obj;
	EXPECT_THROW(queue.enqueue(std::move(obj)), std::runtime_error);
	EXPECT_FALSE(queue.dequeue().has_value());
}

// ----------------------------------------------------------------
// Allocation failure
// ----------------------------------------------------------------

TEST(LockfreeQueueUnitTest, AllocationFailureEnqueueFails)
{
	std::size_t size;
	std::byte *probe;

	size = std::numeric_limits<std::size_t>::max() / 2;

	probe = new(std::nothrow) std::byte[size];
	if (probe)
	{
		delete[] probe;
		GTEST_SKIP() << "Allocation succeeded, cannot test ENOMEM on this system";
	}

	Queue<int> queue(size);
	EXPECT_FALSE(queue.enqueue(1));
}
