/**
 * File: stack_test.cpp
 * Path: ajylib/tests/container/lockfree/stack_test.cpp
 * Description:
 *	Unit tests for ajy::container::lockfree::Stack.
 * Author: ajy-dev
 * Created: 2026-06-16
 * Updated: Never
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/container/lockfree/stack.hpp>

#include <cstddef>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>

using ajy::container::lockfree::Stack;

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

		NonTrivial(NonTrivial &&other) noexcept
			: counter(other.counter)
		{
			other.counter = nullptr;
		}
	};

	struct MoveOnly
	{
		int value;

		explicit MoveOnly(int v)
			: value(v)
		{
		}

		MoveOnly(const MoveOnly &) = delete;
		MoveOnly &operator=(const MoveOnly &) = delete;
		MoveOnly(MoveOnly &&) = default;
		MoveOnly &operator=(MoveOnly &&) = default;
	};

	struct ThrowingMoveCtor
	{
		ThrowingMoveCtor(void) = default;

		ThrowingMoveCtor(ThrowingMoveCtor &&)
		{
			throw std::runtime_error("move failed");
		}
	};
}

// ----------------------------------------------------------------
// Construction / State
// ----------------------------------------------------------------

TEST(LockfreeStackUnitTest, PushSucceedsWithDefaultCapacity)
{
	Stack<int> stack;
	EXPECT_TRUE(stack.push(42));
}

TEST(LockfreeStackUnitTest, ZeroCapacityPushFails)
{
	Stack<int> stack(0);
	EXPECT_FALSE(stack.push(1));
}

// ----------------------------------------------------------------
// push / pop
// ----------------------------------------------------------------

TEST(LockfreeStackUnitTest, BasicPushPopRvalue)
{
	Stack<int> stack(16);
	ASSERT_TRUE(stack.push(42));
	std::optional<int> result = stack.pop();
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, 42);
}

TEST(LockfreeStackUnitTest, BasicPushPopLvalue)
{
	Stack<int> stack(16);
	int n = 42;
	ASSERT_TRUE(stack.push(n));
	std::optional<int> result = stack.pop();
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, 42);
}

TEST(LockfreeStackUnitTest, LvaluePushCopyConstructible)
{
	Stack<Trivial> stack(16);
	Trivial obj{99};
	ASSERT_TRUE(stack.push(obj));
	std::optional<Trivial> result = stack.pop();
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->value, 99);
}

TEST(LockfreeStackUnitTest, PopEmptyReturnsNullopt)
{
	Stack<int> stack(16);
	EXPECT_FALSE(stack.pop().has_value());
}

// ----------------------------------------------------------------
// LIFO Ordering
// ----------------------------------------------------------------

TEST(LockfreeStackUnitTest, LIFOOrdering)
{
	Stack<int> stack(16);
	ASSERT_TRUE(stack.push(1));
	ASSERT_TRUE(stack.push(2));
	ASSERT_TRUE(stack.push(3));
	EXPECT_EQ(*stack.pop(), 3);
	EXPECT_EQ(*stack.pop(), 2);
	EXPECT_EQ(*stack.pop(), 1);
	EXPECT_FALSE(stack.pop().has_value());
}

// ----------------------------------------------------------------
// Move-only
// ----------------------------------------------------------------

TEST(LockfreeStackUnitTest, MoveOnlyTypePush)
{
	Stack<MoveOnly> stack(16);
	ASSERT_TRUE(stack.push(MoveOnly{7}));
	std::optional<MoveOnly> result = stack.pop();
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->value, 7);
}

// ----------------------------------------------------------------
// Non-trivial
// ----------------------------------------------------------------

TEST(LockfreeStackUnitTest, DestructorCleansUpRemainingNodes)
{
	int counter = 0;
	{
		Stack<NonTrivial> stack(16);
		NonTrivial obj1(&counter);
		NonTrivial obj2(&counter);
		ASSERT_TRUE(stack.push(std::move(obj1)));
		ASSERT_TRUE(stack.push(std::move(obj2)));
		EXPECT_EQ(counter, 2);
	}
	EXPECT_EQ(counter, 0);
}

TEST(LockfreeStackUnitTest, NonTrivialLifetimeTracking)
{
	int counter = 0;
	Stack<NonTrivial> stack(16);
	{
		NonTrivial obj(&counter);
		EXPECT_EQ(counter, 1);
		ASSERT_TRUE(stack.push(std::move(obj)));
		EXPECT_EQ(counter, 1);
	}
	EXPECT_EQ(counter, 1);
	{
		std::optional<NonTrivial> result = stack.pop();
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(counter, 1);
	}
	EXPECT_EQ(counter, 0);
}

// ----------------------------------------------------------------
// Throwing move constructor
// ----------------------------------------------------------------

TEST(LockfreeStackUnitTest, ThrowingMoveCtorPropagatesException)
{
	Stack<ThrowingMoveCtor> stack(16);
	ThrowingMoveCtor obj;
	EXPECT_THROW(stack.push(std::move(obj)), std::runtime_error);
	EXPECT_FALSE(stack.pop().has_value());
}

// ----------------------------------------------------------------
// Allocation failure
// ----------------------------------------------------------------

TEST(LockfreeStackUnitTest, AllocationFailurePushFails)
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

	Stack<int> stack(size);
	EXPECT_FALSE(stack.push(1));
}
