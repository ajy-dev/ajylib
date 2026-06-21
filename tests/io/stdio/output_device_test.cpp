/**
 * File: output_device_test.cpp
 * Path: ajylib/tests/io/stdio/output_device_test.cpp
 * Description:
 * 	Unit tests for ajy::io::stdio::OutputDevice.
 * Author: ajy-dev
 * Created: 2026-06-22
 * Updated: Never
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/io/stdio/output_device.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using ajy::io::stdio::OutputDevice;

namespace
{
	std::filesystem::path make_temp_path(const char *suffix)
	{
		std::filesystem::path path;

		path = std::filesystem::temp_directory_path() / (std::string("ajylib_stdio_output_device_test_") + suffix + ".tmp");
		std::filesystem::remove(path);

		return path;
	}
}

// ----------------------------------------------------------------
// open / close (STDOUT / STDERR)
// ----------------------------------------------------------------

TEST(StdioOutputDeviceUnitTest, OpenCloseStderr)
{
	OutputDevice device(OutputDevice::Target::STDERR);
	EXPECT_TRUE(device.open());
	device.close();
}

TEST(StdioOutputDeviceUnitTest, OpenCloseStdout)
{
	OutputDevice device(OutputDevice::Target::STDOUT);
	EXPECT_TRUE(device.open());
	device.close();
}

// ----------------------------------------------------------------
// write / flush (STDOUT / STDERR)
// ----------------------------------------------------------------

TEST(StdioOutputDeviceUnitTest, WriteToStderr)
{
	OutputDevice device(OutputDevice::Target::STDERR);
	ASSERT_TRUE(device.open());
	EXPECT_GT(device.write("test\n", 5), 0u);
	device.close();
}

TEST(StdioOutputDeviceUnitTest, FlushStderr)
{
	OutputDevice device(OutputDevice::Target::STDERR);
	ASSERT_TRUE(device.open());
	EXPECT_TRUE(device.flush());
	device.close();
}

// ----------------------------------------------------------------
// open / close (FILE)
// ----------------------------------------------------------------

TEST(StdioOutputDeviceUnitTest, OpenCloseFile)
{
	std::filesystem::path path;

	path = make_temp_path("open_close");

	{
		OutputDevice device(path);
		EXPECT_TRUE(device.open());
		device.close();
	}

	std::filesystem::remove(path);
}

TEST(StdioOutputDeviceUnitTest, OpenFailsWithEmptyPath)
{
	std::filesystem::path empty_path;
	OutputDevice device(empty_path);

	EXPECT_FALSE(device.open());
}

// ----------------------------------------------------------------
// write / flush (FILE)
// ----------------------------------------------------------------

TEST(StdioOutputDeviceUnitTest, WriteAndReadBackFile)
{
	std::filesystem::path path;
	std::ifstream in;
	std::ostringstream oss;

	path = make_temp_path("write_read_back");

	{
		OutputDevice device(path);
		ASSERT_TRUE(device.open());
		EXPECT_EQ(device.write("hello", 5), 5u);
		EXPECT_TRUE(device.flush());
		device.close();
	}

	in.open(path, std::ios::binary);
	ASSERT_TRUE(in.is_open());
	oss << in.rdbuf();
	EXPECT_EQ(oss.str(), "hello");
	in.close();

	std::filesystem::remove(path);
}

TEST(StdioOutputDeviceUnitTest, WriteBeforeOpenReturnsZero)
{
	OutputDevice device(make_temp_path("write_before_open"));
	EXPECT_EQ(device.write("test", 4), 0u);
}

TEST(StdioOutputDeviceUnitTest, WriteAppendsOnReopen)
{
	std::filesystem::path path;
	std::ifstream in;
	std::ostringstream oss;

	path = make_temp_path("append");

	{
		OutputDevice device(path);
		ASSERT_TRUE(device.open());
		device.write("first;", 6);
		device.close();
	}
	{
		OutputDevice device(path);
		ASSERT_TRUE(device.open());
		device.write("second", 6);
		device.close();
	}

	in.open(path, std::ios::binary);
	ASSERT_TRUE(in.is_open());
	oss << in.rdbuf();
	EXPECT_EQ(oss.str(), "first;second");
	in.close();

	std::filesystem::remove(path);
}
