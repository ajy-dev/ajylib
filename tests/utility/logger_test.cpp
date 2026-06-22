/**
 * File: logger_test.cpp
 * Path: ajylib/tests/utility/logger_test.cpp
 * Description:
 * 	Unit tests for ajy::utility::Logger.
 * Author: ajy-dev
 * Created: 2026-06-17
 * Updated: 2026-06-22
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/io/stdio/output_device.hpp>
#include <ajy/utility/logger.hpp>

#include <cstddef>
#include <memory>
#include <new>
#include <string>

using ajy::utility::Logger;

// ----------------------------------------------------------------
// create
// ----------------------------------------------------------------

TEST(LoggerUnitTest, CreateReturnsValidIndex)
{
	std::size_t index;

	index = Logger::create("test_create_valid");
	EXPECT_LT(index, Logger::INVALID_INDEX);
}

TEST(LoggerUnitTest, CreateEmptyNameReturnsInvalidName)
{
	EXPECT_EQ(Logger::create(""), Logger::INVALID_NAME);
}

TEST(LoggerUnitTest, CreateNameTooLongReturnsInvalidName)
{
	std::string long_name(Logger::MAX_NAME_LENGTH + 1, 'a');

	EXPECT_EQ(Logger::create(long_name), Logger::INVALID_NAME);
}

TEST(LoggerUnitTest, CreateDuplicateNameReturnsDuplicateName)
{
	Logger::create("test_duplicate");
	EXPECT_EQ(Logger::create("test_duplicate"), Logger::DUPLICATE_NAME);
}

TEST(LoggerUnitTest, CreateWithCustomSink)
{
	ajy::io::stdio::OutputDevice *sink;
	std::size_t index;

	sink = new(std::nothrow) ajy::io::stdio::OutputDevice(ajy::io::stdio::OutputDevice::Target::STDOUT);
	ASSERT_NE(sink, nullptr);

	index = Logger::create("test_custom_sink", std::unique_ptr<ajy::io::OutputDevice>(sink));
	EXPECT_LT(index, Logger::INVALID_INDEX);
}

// ----------------------------------------------------------------
// get
// ----------------------------------------------------------------

TEST(LoggerUnitTest, GetByIndexReturnsLogger)
{
	std::size_t index;

	index = Logger::create("test_get_index");
	ASSERT_LT(index, Logger::INVALID_INDEX);
	EXPECT_NE(Logger::get(index), nullptr);
}

TEST(LoggerUnitTest, GetByIndexOutOfRangeReturnsNullptr)
{
	EXPECT_EQ(Logger::get(Logger::INVALID_INDEX), nullptr);
}

TEST(LoggerUnitTest, GetByNameReturnsLogger)
{
	Logger::create("test_get_name");
	EXPECT_NE(Logger::get("test_get_name"), nullptr);
}

TEST(LoggerUnitTest, GetByNameNotFoundReturnsNullptr)
{
	EXPECT_EQ(Logger::get("nonexistent_logger"), nullptr);
}

// ----------------------------------------------------------------
// log
// ----------------------------------------------------------------

TEST(LoggerUnitTest, LogStringViewReturnsTrue)
{
	std::size_t index;
	Logger *logger;

	index = Logger::create("test_log_sv");
	ASSERT_LT(index, Logger::INVALID_INDEX);

	logger = Logger::get(index);
	ASSERT_NE(logger, nullptr);
	EXPECT_TRUE(logger->log(Logger::LogLevel::Info, "hello"));
}

TEST(LoggerUnitTest, LogFmtstrReturnsTrue)
{
	std::size_t index;
	Logger *logger;

	index = Logger::create("test_log_fmt");
	ASSERT_LT(index, Logger::INVALID_INDEX);

	logger = Logger::get(index);
	ASSERT_NE(logger, nullptr);
	EXPECT_TRUE(logger->log(Logger::LogLevel::Info, "value: %d", 42));
}

TEST(LoggerUnitTest, LogBelowThresholdReturnsTrue)
{
	std::size_t index;
	Logger *logger;

	index = Logger::create("test_log_threshold");
	ASSERT_LT(index, Logger::INVALID_INDEX);

	logger = Logger::get(index);
	ASSERT_NE(logger, nullptr);
	logger->set_threshold(Logger::LogLevel::Error);
	EXPECT_TRUE(logger->log(Logger::LogLevel::Debug, "should be ignored"));
}

TEST(LoggerUnitTest, LogNullFmtstrReturnsFalse)
{
	std::size_t index;
	Logger *logger;

	index = Logger::create("test_log_null_fmt");
	ASSERT_LT(index, Logger::INVALID_INDEX);

	logger = Logger::get(index);
	ASSERT_NE(logger, nullptr);
	EXPECT_FALSE(logger->log(Logger::LogLevel::Info, nullptr));
}

// ----------------------------------------------------------------
// threshold
// ----------------------------------------------------------------

TEST(LoggerUnitTest, SetGetThreshold)
{
	std::size_t index;
	Logger *logger;

	index = Logger::create("test_threshold");
	ASSERT_LT(index, Logger::INVALID_INDEX);

	logger = Logger::get(index);
	ASSERT_NE(logger, nullptr);
	logger->set_threshold(Logger::LogLevel::Warning);
	EXPECT_EQ(logger->get_threshold(), Logger::LogLevel::Warning);
}

// ----------------------------------------------------------------
// get_name
// ----------------------------------------------------------------

TEST(LoggerUnitTest, GetName)
{
	std::size_t index;
	Logger *logger;

	index = Logger::create("test_get_name_val");
	ASSERT_LT(index, Logger::INVALID_INDEX);

	logger = Logger::get(index);
	ASSERT_NE(logger, nullptr);
	EXPECT_EQ(logger->get_name(), "test_get_name_val");
}
