/**
 * File: serialization_buffer_test.cpp
 * Path: ajylib/tests/container/serialization_buffer_test.cpp
 * Description:
 *	Unit tests for ajy::container::SerializationBuffer.
 * Author: ajy-dev
 * Created: 2026-06-15
 * Updated: Never
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/container/serialization_buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

using ajy::container::SerializationBuffer;

// ----------------------------------------------------------------
// Construction / State
// ----------------------------------------------------------------

TEST(SerializationBufferUnitTest, DefaultCapacity)
{
	SerializationBuffer sb;
	EXPECT_EQ(sb.get_capacity(), 1400u);
}

TEST(SerializationBufferUnitTest, InitialState)
{
	SerializationBuffer sb(64);
	EXPECT_EQ(sb.get_data_size(), 0u);
	EXPECT_EQ(sb.get_free_size(), 64u);
}

TEST(SerializationBufferUnitTest, AllocationFailure)
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

	SerializationBuffer sb(size);
	EXPECT_EQ(sb.get_capacity(), 0u);
}

// ----------------------------------------------------------------
// Serialize / Deserialize
// ----------------------------------------------------------------

TEST(SerializationBufferUnitTest, BasicSerializeDeserialize)
{
	SerializationBuffer sb(64);
	const char src[] = "hello";
	char dst[6] = {};

	ASSERT_TRUE(sb.serialize(src, 5));
	ASSERT_TRUE(sb.deserialize(dst, 5));
	EXPECT_EQ(std::memcmp(dst, src, 5), 0);
}

TEST(SerializationBufferUnitTest, SerializeUpdatesSizes)
{
	SerializationBuffer sb(64);
	const char src[16] = {};

	ASSERT_TRUE(sb.serialize(src, 16));
	EXPECT_EQ(sb.get_data_size(), 16u);
	EXPECT_EQ(sb.get_free_size(), 48u);
}

TEST(SerializationBufferUnitTest, SerializeFailsWhenFull)
{
	SerializationBuffer sb(8);
	const char src[8] = {};

	EXPECT_TRUE(sb.serialize(src, 8));
	EXPECT_FALSE(sb.serialize(src, 1));
}

TEST(SerializationBufferUnitTest, DeserializeFailsWhenEmpty)
{
	SerializationBuffer sb(64);
	char dst[4] = {};

	EXPECT_FALSE(sb.deserialize(dst, 1));
}

TEST(SerializationBufferUnitTest, MultipleTypesInOrder)
{
	SerializationBuffer sb(64);
	const std::int32_t src_i = 42;
	const float src_f = 3.14f;
	std::int32_t dst_i = 0;
	float dst_f = 0.0f;

	ASSERT_TRUE(sb.serialize(&src_i, sizeof(src_i)));
	ASSERT_TRUE(sb.serialize(&src_f, sizeof(src_f)));
	ASSERT_TRUE(sb.deserialize(&dst_i, sizeof(dst_i)));
	ASSERT_TRUE(sb.deserialize(&dst_f, sizeof(dst_f)));
	EXPECT_EQ(dst_i, src_i);
	EXPECT_EQ(dst_f, src_f);
}

// ----------------------------------------------------------------
// Zero-size
// ----------------------------------------------------------------

TEST(SerializationBufferUnitTest, ZeroSizeSerialize)
{
	SerializationBuffer sb(64);
	EXPECT_TRUE(sb.serialize(nullptr, 0));
}

TEST(SerializationBufferUnitTest, ZeroSizeDeserialize)
{
	SerializationBuffer sb(64);
	EXPECT_TRUE(sb.deserialize(nullptr, 0));
}

// ----------------------------------------------------------------
// Null
// ----------------------------------------------------------------

TEST(SerializationBufferUnitTest, NullSrcSerialize)
{
	SerializationBuffer sb(64);
	EXPECT_FALSE(sb.serialize(nullptr, 4));
}

TEST(SerializationBufferUnitTest, NullDstDeserialize)
{
	SerializationBuffer sb(64);
	const char src[4] = {};

	ASSERT_TRUE(sb.serialize(src, 4));
	EXPECT_FALSE(sb.deserialize(nullptr, 4));
}

// ----------------------------------------------------------------
// Clear
// ----------------------------------------------------------------

TEST(SerializationBufferUnitTest, ClearResetsState)
{
	SerializationBuffer sb(64);
	const char src[32] = {};

	ASSERT_TRUE(sb.serialize(src, 32));
	sb.clear();
	EXPECT_EQ(sb.get_data_size(), 0u);
	EXPECT_EQ(sb.get_free_size(), 64u);
}

// ----------------------------------------------------------------
// Move
// ----------------------------------------------------------------

TEST(SerializationBufferUnitTest, MoveConstructor)
{
	SerializationBuffer sb(64);
	const char src[] = "hello";

	ASSERT_TRUE(sb.serialize(src, 5));

	SerializationBuffer sb2(std::move(sb));
	EXPECT_EQ(sb.get_capacity(), 0u);
	EXPECT_EQ(sb2.get_data_size(), 5u);
}

TEST(SerializationBufferUnitTest, MoveAssignment)
{
	SerializationBuffer sb(64);
	const char src[] = "hello";

	ASSERT_TRUE(sb.serialize(src, 5));

	SerializationBuffer sb2(64);
	sb2 = std::move(sb);
	EXPECT_EQ(sb.get_capacity(), 0u);
	EXPECT_EQ(sb2.get_data_size(), 5u);
}

// ----------------------------------------------------------------
// Direct API
// ----------------------------------------------------------------

TEST(SerializationBufferUnitTest, BufferPtrNullOnAllocationFailure)
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

	SerializationBuffer sb(size);
	EXPECT_EQ(sb.get_buffer_ptr(), nullptr);
}

TEST(SerializationBufferUnitTest, CommitDirectSerialize)
{
	SerializationBuffer sb(64);
	const char src[] = "hello";

	ASSERT_NE(sb.get_buffer_ptr(), nullptr);
	std::memcpy(sb.get_buffer_ptr(), src, 5);
	sb.commit_direct_serialize(5);
	EXPECT_EQ(sb.get_data_size(), 5u);
}

TEST(SerializationBufferUnitTest, CommitDirectDeserialize)
{
	SerializationBuffer sb(64);
	const char src[] = "hello";
	char dst[6] = {};

	ASSERT_TRUE(sb.serialize(src, 5));
	std::memcpy(dst, sb.get_buffer_ptr(), 5);
	sb.commit_direct_deserialize(5);
	EXPECT_EQ(sb.get_data_size(), 0u);
	EXPECT_EQ(std::memcmp(dst, src, 5), 0);
}

// ----------------------------------------------------------------
// << / >> operators
// ----------------------------------------------------------------

TEST(SerializationBufferUnitTest, OperatorSerializeDeserialize)
{
	SerializationBuffer sb(64);
	const std::int32_t src = 1234;
	std::int32_t dst = 0;

	sb << src;
	sb >> dst;
	EXPECT_EQ(dst, src);
}

TEST(SerializationBufferUnitTest, OperatorChaining)
{
	SerializationBuffer sb(64);
	const std::int32_t src_i = 42;
	const float src_f = 1.5f;
	const std::uint8_t src_b = 255;
	std::int32_t dst_i = 0;
	float dst_f = 0.0f;
	std::uint8_t dst_b = 0;

	sb << src_i << src_f << src_b;
	sb >> dst_i >> dst_f >> dst_b;
	EXPECT_EQ(dst_i, src_i);
	EXPECT_EQ(dst_f, src_f);
	EXPECT_EQ(dst_b, src_b);
}
