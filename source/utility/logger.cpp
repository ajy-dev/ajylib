/**
 * File: logger.cpp
 * Path: ajylib/source/utility/logger.cpp
 * Description:
 * 	A thread-safe asynchronous logger definition.
 * Author: ajy-dev
 * Created: 2026-06-17
 * Updated: 2026-06-28
 * Version: 0.1.0
 */

#include <ajy/utility/logger.hpp>
#include <ajy/io/stdio/output_device.hpp>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <exception>
#include <memory>
#include <new>
#include <optional>
#include <system_error>

namespace ajy::utility
{
	Logger::Logger(std::string_view name, std::unique_ptr<io::OutputDevice> sink) noexcept
		: name(name)
		, sink(std::move(sink))
		, threshold(LogLevel::Info)
		, running(false)
		, wake_flag(false)
	{
	}

	Logger::~Logger(void) noexcept
	{
		this->stop();
	}

	std::size_t Logger::create(std::string_view name) noexcept
	{
		io::stdio::OutputDevice *sink;

		sink = new(std::nothrow) io::stdio::OutputDevice();
		if (!sink)
			return ALLOC_FAILED;

		return Logger::create(name, std::unique_ptr<io::OutputDevice>(sink));
	}

	std::size_t Logger::create(std::string_view name, std::unique_ptr<io::OutputDevice> sink) noexcept
	{
		try
		{
			std::lock_guard<std::mutex> guard(Logger::instance_creation_mutex);
			std::size_t count;
			std::size_t index;
			Logger *logger_ptr;

			if (name.empty() || name.size() > MAX_NAME_LENGTH)
				return INVALID_NAME;

			count = Logger::logger_instance_counter.load(std::memory_order_relaxed);

			if (count >= MAX_LOGGER_COUNT)
				return CAPACITY_EXCEEDED;

			for (std::size_t i = 0; i < count; ++i)
			{
				if (Logger::instances[i]->name == name)
					return DUPLICATE_NAME;
			}

			index = count;

			try
			{
				Logger::instances[index] = std::unique_ptr<Logger>(new Logger(name, std::move(sink)));
			}
			catch (...)
			{
				return ALLOC_FAILED;
			}

			logger_ptr = Logger::instances[index].get();

			if (!logger_ptr->start())
			{
				Logger::instances[index].reset();
				return ALLOC_FAILED;
			}

			Logger::logger_instance_counter.fetch_add(1, std::memory_order_release);

			return index;
		}
		catch (const std::system_error &error)
		{
			std::fprintf(stderr, "ajy::utility::Logger::create() failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}
	}

	Logger *Logger::get(std::size_t index) noexcept
	{
		std::size_t count;

		count = Logger::logger_instance_counter.load(std::memory_order_acquire);

		if (index >= count)
			return nullptr;

		return Logger::instances[index].get();
	}

	Logger *Logger::get(std::string_view name) noexcept
	{
		std::size_t count;

		count = Logger::logger_instance_counter.load(std::memory_order_acquire);

		for (std::size_t i = 0; i < count; ++i)
		{
			if (Logger::instances[i]->name == name)
				return Logger::instances[i].get();
		}

		return nullptr;
	}

	Logger::LogLevel Logger::get_threshold(void) const noexcept
	{
		return this->threshold.load(std::memory_order_relaxed);
	}

	std::string_view Logger::get_name(void) const noexcept
	{
		return this->name;
	}

	void Logger::set_threshold(LogLevel level) noexcept
	{
		this->threshold.store(level, std::memory_order_relaxed);
	}

	bool Logger::log(LogLevel level, std::string_view str) noexcept
	{
		LogEntry *entry;
		std::size_t len;

		if (!this->running.load(std::memory_order_relaxed))
			return false;

		if (!this->is_enabled(level))
			return true;

		entry = this->pool.alloc();
		if (!entry)
			return false;

		entry->counter = Logger::global_log_counter.fetch_add(1, std::memory_order_relaxed);
		entry->timestamp = std::chrono::system_clock::now();
		entry->level = level;

		len = std::min(str.size(), MESSAGE_FIELD_LENGTH);
		std::memcpy(entry->message, str.data(), len);
		entry->message[len] = '\0';

		if (!this->queue.enqueue(entry))
		{
			this->pool.free(entry);
			return false;
		}

		this->wake_flag.store(true, std::memory_order_relaxed);
		this->wake_flag.notify_one();

		return true;
	}

	bool Logger::log(LogLevel level, const char *fmtstr, ...) noexcept
	{
		LogEntry *entry;
		va_list args;
		int ret;

		if (!this->running.load(std::memory_order_relaxed))
			return false;

		if (!this->is_enabled(level))
			return true;

		if (!fmtstr)
			return false;

		entry = this->pool.alloc();
		if (!entry)
			return false;

		entry->counter = Logger::global_log_counter.fetch_add(1, std::memory_order_relaxed);
		entry->timestamp = std::chrono::system_clock::now();
		entry->level = level;

		va_start(args, fmtstr);
		ret = std::vsnprintf(entry->message, MAX_MESSAGE_LENGTH, fmtstr, args);
		va_end(args);

		if (ret < 0 || !this->queue.enqueue(entry))
		{
			this->pool.free(entry);
			return false;
		}

		this->wake_flag.store(true, std::memory_order_relaxed);
		this->wake_flag.notify_one();

		return true;
	}

	void Logger::logging_thread_proc(Logger *logger) noexcept
	{
		bool stop_requested;

		stop_requested = false;
		while (!stop_requested)
		{
			logger->wake_flag.wait(false, std::memory_order_relaxed);
			logger->wake_flag.store(false, std::memory_order_relaxed);

			while (true)
			{
				std::optional<LogEntry *> entry_optional;
				LogEntry *entry;
				char line[MAX_LINE_LENGTH + 1];
				std::size_t written;

				entry_optional = logger->queue.dequeue();
				if (!entry_optional.has_value())
					break;

				entry = *entry_optional;
				if (entry == Logger::STOP_SENTINEL)
				{
					stop_requested = true;
					break;
				}

				written = Logger::format_entry(entry, logger->name, line);
				logger->pool.free(entry);

				if (written > 0 && logger->sink)
					logger->sink->write(line, written);
			}

			if (logger->sink)
				logger->sink->flush();
		}
	}

	void Logger::format_time(const std::chrono::system_clock::time_point &timepoint, char *out) noexcept
	{
		std::time_t time;
		std::tm tm;

		time = std::chrono::system_clock::to_time_t(timepoint);
#if defined(_WIN32) || defined(_WIN64)
		::localtime_s(&tm, &time);
#else
		::localtime_r(&time, &tm);
#endif
		std::snprintf(
			out,
			TIME_FIELD_LENGTH + 1,
			"[%04d-%02d-%02d %02d:%02d:%02d]",
			tm.tm_year + 1900,
			tm.tm_mon + 1,
			tm.tm_mday,
			tm.tm_hour,
			tm.tm_min,
			tm.tm_sec);
	}

	void Logger::format_level(LogLevel level, char *out) noexcept
	{
		const char *str;

		switch (level)
		{
		case LogLevel::Debug:
			str = "[DEBUG]  ";
			break;
		case LogLevel::Info:
			str = "[INFO]   ";
			break;
		case LogLevel::Warning:
			str = "[WARNING]";
			break;
		case LogLevel::Error:
			str = "[ERROR]  ";
			break;
		case LogLevel::Fatal:
			str = "[FATAL]  ";
			break;
		default:
			str = "[UNKNOWN]";
			break;
		}

		std::memcpy(out, str, LEVEL_FIELD_LENGTH);
	}

	void Logger::format_counter(std::uint64_t counter, char *out) noexcept
	{
		std::snprintf(out, COUNTER_FIELD_LENGTH + 1, "[%020llu]", static_cast<unsigned long long>(counter));
	}

	void Logger::format_name(const std::string &name, char *out) noexcept
	{
		std::size_t len;

		len = std::min(name.size(), MAX_NAME_LENGTH);
		out[0] = '[';
		std::memcpy(out + 1, name.c_str(), len);
		out[1 + len] = ']';
		std::memset(out + 2 + len, ' ', MAX_NAME_LENGTH - len);
	}

	std::size_t Logger::format_entry(const LogEntry *entry, const std::string &name, char *out) noexcept
	{
		char *ptr;
		std::size_t msg_len;

		ptr = out;

		Logger::format_time(entry->timestamp, ptr);
		ptr += TIME_FIELD_LENGTH;

		Logger::format_level(entry->level, ptr);
		ptr += LEVEL_FIELD_LENGTH;

		Logger::format_counter(entry->counter, ptr);
		ptr += COUNTER_FIELD_LENGTH;

		Logger::format_name(name, ptr);
		ptr += NAME_FIELD_LENGTH;

		*ptr = ' ';
		ptr += SEPARATOR_LENGTH;

		*ptr = '"';
		++ptr;

		msg_len = std::min(std::strlen(entry->message), MESSAGE_FIELD_LENGTH);
		std::memcpy(ptr, entry->message, msg_len);
		ptr += msg_len;

		*ptr = '"';
		++ptr;

		*ptr = '\n';
		ptr += NEWLINE_LENGTH;

		return static_cast<std::size_t>(ptr - out);
	}

	bool Logger::is_enabled(LogLevel level) const noexcept
	{
		return level >= this->threshold.load(std::memory_order_relaxed);
	}

	bool Logger::start(void) noexcept
	{
		if (!this->sink || !this->sink->open())
			return false;

		this->running.store(true, std::memory_order_relaxed);

		try
		{
			this->logging_thread = std::thread(Logger::logging_thread_proc, this);
		}
		catch (...)
		{
			this->running.store(false, std::memory_order_relaxed);
			this->sink->close();
			return false;
		}

		return true;
	}

	void Logger::stop(void) noexcept
	{
		if (!this->running.load(std::memory_order_relaxed))
			return;

		this->running.store(false, std::memory_order_relaxed);
		this->queue.enqueue(Logger::STOP_SENTINEL);
		this->wake_flag.store(true, std::memory_order_relaxed);
		this->wake_flag.notify_one();

		if (this->logging_thread.joinable())
			this->logging_thread.join();

		if (this->sink)
			this->sink->close();
	}
}
