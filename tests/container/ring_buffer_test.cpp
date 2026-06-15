/**
 * File: ring_buffer_test.cpp
 * Path: ajylib/tests/container/ring_buffer_test.cpp
 * Description:
 *	Unit tests for ajy::container::RingBuffer.
 * Author: ajy-dev
 * Created: 2026-06-15
 * Updated: Never
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/container/ring_buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

using ajy::container::RingBuffer;

// ----------------------------------------------------------------
// Construction / State
// ----------------------------------------------------------------

TEST(RingBufferUnitTest, DefaultCapacityIsPowerOfTwo)
{
	RingBuffer rb(1000);
	EXPECT_EQ(rb.get_capacity(), 1024u);
}

TEST(RingBufferUnitTest, InitialState)
{
	RingBuffer rb(16);
	EXPECT_EQ(rb.get_used_size(), 0u);
	EXPECT_EQ(rb.get_free_size(), 16u);
}

// ----------------------------------------------------------------
// Write / Read / Peek
// ----------------------------------------------------------------

TEST(RingBufferUnitTest, BasicWriteRead)
{
	RingBuffer rb(16);
	const char src[] = "hello";
	char dst[6] = {};

	ASSERT_TRUE(rb.write(src, 5));
	EXPECT_EQ(rb.get_used_size(), 5u);
	ASSERT_TRUE(rb.read(dst, 5));
	EXPECT_EQ(std::memcmp(dst, src, 5), 0);
	EXPECT_EQ(rb.get_used_size(), 0u);
}

TEST(RingBufferUnitTest, PeekDoesNotConsume)
{
	RingBuffer rb(16);
	const char src[] = "hello";
	char dst[6] = {};

	ASSERT_TRUE(rb.write(src, 5));
	ASSERT_TRUE(rb.peek(dst, 5));
	EXPECT_EQ(rb.get_used_size(), 5u);
	EXPECT_EQ(std::memcmp(dst, src, 5), 0);
}

TEST(RingBufferUnitTest, WriteFailsWhenFull)
{
	RingBuffer rb(8);
	const char src[8] = {};

	EXPECT_TRUE(rb.write(src, 8));
	EXPECT_FALSE(rb.write(src, 1));
}

TEST(RingBufferUnitTest, ReadFailsWhenEmpty)
{
	RingBuffer rb(16);
	char dst[4] = {};

	EXPECT_FALSE(rb.read(dst, 1));
}

TEST(RingBufferUnitTest, WrapAround)
{
	RingBuffer rb(8);
	const char src[] = "abcde";
	char fill[6] = "fffff";
	char dst[6] = {};

	ASSERT_TRUE(rb.write(fill, 5));
	ASSERT_TRUE(rb.read(dst, 5));
	ASSERT_TRUE(rb.write(src, 5));
	ASSERT_TRUE(rb.read(dst, 5));
	EXPECT_EQ(std::memcmp(dst, src, 5), 0);
}

// ----------------------------------------------------------------
// Zero-size
// ----------------------------------------------------------------

TEST(RingBufferUnitTest, ZeroSizeWrite)
{
	RingBuffer rb(16);
	EXPECT_TRUE(rb.write(nullptr, 0));
}

TEST(RingBufferUnitTest, ZeroSizeRead)
{
	RingBuffer rb(16);
	EXPECT_TRUE(rb.read(nullptr, 0));
}

// ----------------------------------------------------------------
// Null
// ----------------------------------------------------------------

TEST(RingBufferUnitTest, NullSrcWrite)
{
	RingBuffer rb(16);
	EXPECT_FALSE(rb.write(nullptr, 4));
}

TEST(RingBufferUnitTest, NullDstRead)
{
	RingBuffer rb(16);
	const char src[4] = {};

	rb.write(src, 4);
	EXPECT_FALSE(rb.read(nullptr, 4));
}

TEST(RingBufferUnitTest, NullDstPeek)
{
	RingBuffer rb(16);
	const char src[4] = {};

	rb.write(src, 4);
	EXPECT_FALSE(rb.peek(nullptr, 4));
}

// ----------------------------------------------------------------
// Clear
// ----------------------------------------------------------------

TEST(RingBufferUnitTest, ClearResetsState)
{
	RingBuffer rb(16);
	const char src[8] = {};

	EXPECT_TRUE(rb.write(src, 8));
	rb.clear();
	EXPECT_EQ(rb.get_used_size(), 0u);
	EXPECT_EQ(rb.get_free_size(), 16u);
}

// ----------------------------------------------------------------
// Move
// ----------------------------------------------------------------

TEST(RingBufferUnitTest, MoveConstructor)
{
	RingBuffer rb(16);
	const char src[] = "hello";

	ASSERT_TRUE(rb.write(src, 5));

	RingBuffer rb2(std::move(rb));
	EXPECT_EQ(rb.get_capacity(), 0u);
	EXPECT_EQ(rb2.get_used_size(), 5u);
}

TEST(RingBufferUnitTest, MoveAssignment)
{
	RingBuffer rb(16);
	const char src[] = "hello";

	ASSERT_TRUE(rb.write(src, 5));

	RingBuffer rb2(16);
	rb2 = std::move(rb);
	EXPECT_EQ(rb.get_capacity(), 0u);
	EXPECT_EQ(rb2.get_used_size(), 5u);
}

// ----------------------------------------------------------------
// Allocation failure
// ----------------------------------------------------------------

TEST(RingBufferUnitTest, DirectPtrNullOnAllocationFailure)
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

	RingBuffer rb(size);
	EXPECT_EQ(rb.get_direct_write_ptr(), nullptr);
	EXPECT_EQ(rb.get_direct_read_ptr(), nullptr);
}

// ----------------------------------------------------------------
// Direct API
// ----------------------------------------------------------------

TEST(RingBufferUnitTest, DirectWriteRead)
{
	RingBuffer rb(16);
	const char src[] = "hello";
	char dst[6] = {};

	ASSERT_GE(rb.get_direct_write_size(), 5u);
	ASSERT_NE(rb.get_direct_write_ptr(), nullptr);
	std::memcpy(rb.get_direct_write_ptr(), src, 5);
	rb.commit_direct_write(5);
	EXPECT_EQ(rb.get_used_size(), 5u);

	ASSERT_GE(rb.get_direct_read_size(), 5u);
	ASSERT_NE(rb.get_direct_read_ptr(), nullptr);
	std::memcpy(dst, rb.get_direct_read_ptr(), 5);
	rb.commit_direct_read(5);
	EXPECT_EQ(std::memcmp(dst, src, 5), 0);
	EXPECT_EQ(rb.get_used_size(), 0u);
}

TEST(RingBufferUnitTest, DirectSizeAtWrapAround)
{
	RingBuffer rb(8);
	char fill[6] = "fffff";
	char dst[6] = {};

	EXPECT_TRUE(rb.write(fill, 5));
	EXPECT_TRUE(rb.read(dst, 5));
	EXPECT_EQ(rb.get_direct_write_size(), 3u);
}

// ----------------------------------------------------------------
// Index wrap-around (uint64_t overflow)
// ----------------------------------------------------------------

TEST(RingBufferUnitTest, IndexWrapAround)
{
	RingBuffer rb(8);
	const char src[] = "hello";
	char dst[6] = {};

	std::uint64_t near_max = std::numeric_limits<std::uint64_t>::max() - 4;
	rb.commit_direct_write(near_max);
	rb.commit_direct_read(near_max);

	ASSERT_TRUE(rb.write(src, 5));
	ASSERT_TRUE(rb.read(dst, 5));
	EXPECT_EQ(std::memcmp(dst, src, 5), 0);
}
