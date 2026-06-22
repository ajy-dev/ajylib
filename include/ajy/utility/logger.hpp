/**
 * File: logger.hpp
 * Path: ajylib/include/ajy/utility/logger.hpp
 * Description:
 * 	A thread-safe asynchronous logger declaration.
 * Note:
 * 	Each Logger instance runs a dedicated logging thread and uses
 * 	a lock-free queue for non-blocking log calls from worker threads.
 * 	Multiple named Logger instances can be created and accessed by
 * 	index (O(1)) or by name (O(n), convenience only).
 * Author: ajy-dev
 * Created: 2026-06-17
 * Updated: 2026-06-22
 * Version: 0.1.0
 */

#ifndef AJY_UTILITY_LOGGER_HPP
#define AJY_UTILITY_LOGGER_HPP

#include <ajy/container/lockfree/queue.hpp>
#include <ajy/io/output_device.hpp>
#include <ajy/memory/lockfree/memory_pool.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace ajy::utility
{
	class Logger
	{
	public:
		enum class LogLevel
		{
			Debug,
			Info,
			Warning,
			Error,
			Fatal
		};

		~Logger(void) noexcept;

		Logger(const Logger &other) = delete;
		Logger &operator=(const Logger &other) = delete;
		Logger(Logger &&other) = delete;
		Logger &operator=(Logger &&other) = delete;

		static constexpr std::size_t MAX_MESSAGE_LENGTH = 256;
		static constexpr std::size_t MAX_NAME_LENGTH = 32;
		static constexpr std::size_t MAX_LOGGER_COUNT = 128;
		static constexpr std::size_t INVALID_INDEX = MAX_LOGGER_COUNT;
		static constexpr std::size_t DUPLICATE_NAME = MAX_LOGGER_COUNT + 1;
		static constexpr std::size_t INVALID_NAME = MAX_LOGGER_COUNT + 2;
		static constexpr std::size_t CAPACITY_EXCEEDED = MAX_LOGGER_COUNT + 3;
		static constexpr std::size_t ALLOC_FAILED = MAX_LOGGER_COUNT + 4;

		static std::size_t create(std::string_view name) noexcept;
		static std::size_t create(std::string_view name, std::unique_ptr<io::OutputDevice> sink) noexcept;
		static Logger *get(std::size_t index) noexcept;
		static Logger *get(std::string_view name) noexcept;

		LogLevel get_threshold(void) const noexcept;
		std::string_view get_name(void) const noexcept;
		void set_threshold(LogLevel level) noexcept;

		bool log(LogLevel level, std::string_view str) noexcept;
		bool log(LogLevel level, const char *fmtstr, ...) noexcept;

	private:
		static constexpr std::size_t TIME_FIELD_LENGTH = 21;
		static constexpr std::size_t LEVEL_FIELD_LENGTH = 9;
		static constexpr std::size_t COUNTER_FIELD_LENGTH = 22;
		static constexpr std::size_t NAME_FIELD_LENGTH = MAX_NAME_LENGTH + 2;
		static constexpr std::size_t SEPARATOR_LENGTH = 1;
		static constexpr std::size_t QUOTE_LENGTH = 2;
		static constexpr std::size_t NEWLINE_LENGTH = 1;
		static constexpr std::size_t MESSAGE_FIELD_LENGTH = MAX_MESSAGE_LENGTH - 1;
		static constexpr std::size_t MAX_LINE_LENGTH =
			TIME_FIELD_LENGTH
			+ LEVEL_FIELD_LENGTH
			+ COUNTER_FIELD_LENGTH
			+ NAME_FIELD_LENGTH
			+ SEPARATOR_LENGTH
			+ QUOTE_LENGTH
			+ MESSAGE_FIELD_LENGTH
			+ NEWLINE_LENGTH;

		struct LogEntry
		{
			std::uint64_t counter;
			std::chrono::system_clock::time_point timestamp;
			LogLevel level;
			char message[MAX_MESSAGE_LENGTH];
		};

		static constexpr LogEntry *STOP_SENTINEL = nullptr;

		explicit Logger(std::string_view name, std::unique_ptr<io::OutputDevice> sink) noexcept;

		static void logging_thread_proc(Logger *logger) noexcept;

		static void format_time(const std::chrono::system_clock::time_point &timepoint, char *out) noexcept;
		static void format_level(LogLevel level, char *out) noexcept;
		static void format_counter(std::uint64_t counter, char *out) noexcept;
		static void format_name(const std::string &name, char *out) noexcept;
		static std::size_t format_entry(const LogEntry *entry, const std::string &name, char *out) noexcept;

		bool is_enabled(LogLevel level) const noexcept;
		bool start(void) noexcept;
		void stop(void) noexcept;

		inline static std::atomic<std::uint64_t> global_log_counter;
		inline static std::array<std::unique_ptr<Logger>, MAX_LOGGER_COUNT> instances;
		inline static std::atomic<std::size_t> logger_instance_counter;
		inline static std::mutex instance_creation_mutex;

		std::string name;
		std::unique_ptr<io::OutputDevice> sink;
		memory::lockfree::MemoryPool<LogEntry> pool;
		container::lockfree::Queue<LogEntry *> queue;
		std::atomic<LogLevel> threshold;
		std::atomic<bool> running;
		std::atomic<bool> wake_flag;
		std::thread logging_thread;
	};
}

#endif