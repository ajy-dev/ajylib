#include <gtest/gtest.h>
#include <ajy/container/ring_buffer.hpp>
#include <cstdint>
#include <limits>

using ajy::container::RingBuffer;

// ================================================================
// Construction & Capacity
// ================================================================

TEST(RingBufferUnitTest, DefaultCapacity)
{
	RingBuffer buf;

	EXPECT_EQ(buf.get_capacity(), RingBuffer::DEFAULT_CAPACITY);
	EXPECT_EQ(buf.get_used_size(), 0);
	EXPECT_EQ(buf.get_free_size(), RingBuffer::DEFAULT_CAPACITY);
}

TEST(RingBufferUnitTest, CapacityRoundedUpToPowerOfTwo)
{
	RingBuffer buf(1000);

	EXPECT_EQ(buf.get_capacity(), 1024);
}

TEST(RingBufferUnitTest, CapacityAlreadyPowerOfTwo)
{
	RingBuffer buf(1024);

	EXPECT_EQ(buf.get_capacity(), 1024);
}

// ================================================================
// Write / Read / Peek
// ================================================================

TEST(RingBufferUnitTest, WriteAndRead)
{
	RingBuffer buf;
	int value;
	int result;

	value = 42;
	result = 0;

	EXPECT_TRUE(buf.write(&value, sizeof(int)));
	EXPECT_EQ(buf.get_used_size(), sizeof(int));
	EXPECT_EQ(buf.get_free_size(), buf.get_capacity() - sizeof(int));

	EXPECT_TRUE(buf.read(&result, sizeof(int)));
	EXPECT_EQ(result, 42);
	EXPECT_EQ(buf.get_used_size(), 0);
}

TEST(RingBufferUnitTest, Peek)
{
	RingBuffer buf;
	int value;
	int result;

	value = 99;
	result = 0;

	buf.write(&value, sizeof(int));

	EXPECT_TRUE(buf.peek(&result, sizeof(int)));
	EXPECT_EQ(result, 99);
	EXPECT_EQ(buf.get_used_size(), sizeof(int));
}

TEST(RingBufferUnitTest, WriteZeroSize)
{
	RingBuffer buf;

	EXPECT_TRUE(buf.write(nullptr, 0));
	EXPECT_EQ(buf.get_used_size(), 0);
}

TEST(RingBufferUnitTest, ReadZeroSize)
{
	RingBuffer buf;

	EXPECT_TRUE(buf.read(nullptr, 0));
}

// ================================================================
// Clear
// ================================================================

TEST(RingBufferUnitTest, Clear)
{
	RingBuffer buf;
	int value;

	value = 1;
	buf.write(&value, sizeof(int));
	buf.clear();

	EXPECT_EQ(buf.get_used_size(), 0);
	EXPECT_EQ(buf.get_free_size(), buf.get_capacity());
}

// ================================================================
// Boundary Conditions
// ================================================================

TEST(RingBufferUnitTest, WriteWhenFull)
{
	RingBuffer buf(16);
	std::byte data[16];

	EXPECT_TRUE(buf.write(data, 16));
	EXPECT_FALSE(buf.write(data, 1));
}

TEST(RingBufferUnitTest, ReadWhenEmpty)
{
	RingBuffer buf;
	std::byte data[4];

	EXPECT_FALSE(buf.read(data, 4));
}

TEST(RingBufferUnitTest, PeekWhenEmpty)
{
	RingBuffer buf;
	std::byte data[4];

	EXPECT_FALSE(buf.peek(data, 4));
}

TEST(RingBufferUnitTest, WriteNullptr)
{
	RingBuffer buf;

	EXPECT_FALSE(buf.write(nullptr, 4));
}

TEST(RingBufferUnitTest, ReadNullptr)
{
	RingBuffer buf;
	int value;

	value = 1;
	buf.write(&value, sizeof(int));

	EXPECT_FALSE(buf.read(nullptr, sizeof(int)));
}

TEST(RingBufferUnitTest, ExactCapacityWriteRead)
{
	RingBuffer buf(16);
	std::byte write_data[16];
	std::byte read_data[16];

	for (int i = 0; i < 16; ++i)
		write_data[i] = static_cast<std::byte>(i);

	EXPECT_TRUE(buf.write(write_data, 16));
	EXPECT_TRUE(buf.read(read_data, 16));

	for (int i = 0; i < 16; ++i)
		EXPECT_EQ(read_data[i], write_data[i]);
}

// ================================================================
// Direct API
// ================================================================

TEST(RingBufferUnitTest, DirectWriteAndRead)
{
	RingBuffer buf;
	int value;
	int result;
	void *write_ptr;
	const void *read_ptr;

	value = 42;
	write_ptr = buf.get_direct_write_ptr();

	ASSERT_NE(write_ptr, nullptr);
	std::memcpy(write_ptr, &value, sizeof(int));
	buf.commit_direct_write(sizeof(int));

	EXPECT_EQ(buf.get_used_size(), sizeof(int));

	read_ptr = buf.get_direct_read_ptr();
	ASSERT_NE(read_ptr, nullptr);
	std::memcpy(&result, read_ptr, sizeof(int));
	buf.commit_direct_read(sizeof(int));

	EXPECT_EQ(result, 42);
	EXPECT_EQ(buf.get_used_size(), 0);
}

TEST(RingBufferUnitTest, DirectWriteSize)
{
	RingBuffer buf(16);
	std::byte data[4];

	EXPECT_EQ(buf.get_direct_write_size(), 16);

	buf.write(data, 4);
	EXPECT_EQ(buf.get_direct_write_size(), 12);
}

TEST(RingBufferUnitTest, DirectReadSize)
{
	RingBuffer buf(16);
	std::byte data[4];

	buf.write(data, 4);
	EXPECT_EQ(buf.get_direct_read_size(), 4);
}

TEST(RingBufferUnitTest, DirectPtrNullOnFailedAlloc)
{
	std::size_t size;
	std::byte *probe;

	size = std::numeric_limits<std::size_t>::max() / 2;

	// Verify that this size actually causes ENOMEM
	probe = new (std::nothrow) std::byte[size];
	if (probe)
	{
		delete[] probe;
		GTEST_SKIP() << "Allocation succeeded, cannot test ENOMEM on this system";
	}

	RingBuffer buf(size);

	EXPECT_EQ(buf.get_direct_write_ptr(), nullptr);
	EXPECT_EQ(buf.get_direct_read_ptr(), nullptr);
}

// ================================================================
// Wraparound
// ================================================================

TEST(RingBufferUnitTest, Wraparound)
{
	RingBuffer buf(16);
	std::byte write_data[12];
	std::byte read_data[12];

	for (int i = 0; i < 12; ++i)
		write_data[i] = static_cast<std::byte>(i);

	// Write 12 bytes, read 8 bytes to advance read_index to the middle
	buf.write(write_data, 12);
	buf.read(read_data, 8);

	// Writing 12 more bytes wraps around past the end of the buffer
	EXPECT_TRUE(buf.write(write_data, 12));

	// 4 remaining bytes + 12 newly written bytes = 16 bytes
	EXPECT_EQ(buf.get_used_size(), 16);

	// Read the remaining 4 bytes from before the wraparound
	EXPECT_TRUE(buf.read(read_data, 4));
	for (int i = 0; i < 4; ++i)
		EXPECT_EQ(read_data[i], write_data[i + 8]);
}

TEST(RingBufferUnitTest, DirectSizeAfterWraparound)
{
	RingBuffer buf(16);
	std::byte data[12];
	std::size_t direct_write_size;
	std::size_t direct_read_size;

	// Write 12, read 8 → read_index at 8, write_index at 12
	buf.write(data, 12);
	buf.read(data, 8);

	// Write 8 more → write_index wraps around to 4
	// Buffer state: [written(0~3)][free(4~7)][read_remaining(8~11)][written(12~15)]
	buf.write(data, 8);

	// Direct write size should be only up to end of buffer (4 bytes: 4~7)
	direct_write_size = buf.get_direct_write_size();
	EXPECT_EQ(direct_write_size, 4);

	// Direct read size should be only up to end of buffer (4 bytes: 8~15)
	direct_read_size = buf.get_direct_read_size();
	EXPECT_EQ(direct_read_size, 8);
}

TEST(RingBufferUnitTest, IndexUint64Wraparound)
{
	RingBuffer buf(16);
	int value;
	int result;
	std::uint64_t near_max;

	near_max = std::numeric_limits<std::uint64_t>::max() - sizeof(int) + 1;

	buf.commit_direct_write(near_max);
	buf.commit_direct_read(near_max);

	value = 42;
	result = 0;

	EXPECT_TRUE(buf.write(&value, sizeof(int)));
	EXPECT_TRUE(buf.read(&result, sizeof(int)));
	EXPECT_EQ(result, 42);

	// Verify continued correctness after wraparound
	value = 99;
	result = 0;

	EXPECT_TRUE(buf.write(&value, sizeof(int)));
	EXPECT_TRUE(buf.read(&result, sizeof(int)));
	EXPECT_EQ(result, 99);
}