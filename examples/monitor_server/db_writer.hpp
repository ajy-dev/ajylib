/**
 * File: db_writer.hpp
 * Path: ajylib/examples/monitor_server/db_writer.hpp
 * Description:
 *	A dedicated DB writer thread for the monitor server example. The content
 *	thread hands off aggregate rows in batches; this thread drains them and
 *	INSERTs into the monthly monitorlog_YYYYMM table (MySQL C API).
 * Note:
 *	The MYSQL connection is opened and used entirely within the writer thread
 *	(MySQL connections are not shared across threads). Pointers the client
 *	library returns are released only through mysql_* calls, never freed here,
 *	so the /MT exe and the /MD libmysql.dll never cross the CRT heap boundary.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef MONITOR_SERVER_DB_WRITER_HPP
#define MONITOR_SERVER_DB_WRITER_HPP

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

struct MYSQL; // forward-declared; <mysql.h> is included only in the .cpp

struct AggregateRow
{
	std::int32_t server_no;
	std::uint8_t data_type;
	std::int32_t avg;
	std::int32_t min;
	std::int32_t max;
};

class DbWriter
{
public:
	DbWriter(void) noexcept;
	~DbWriter(void) noexcept;

	DbWriter(const DbWriter &other) = delete;
	DbWriter &operator=(const DbWriter &other) = delete;
	DbWriter(DbWriter &&other) = delete;
	DbWriter &operator=(DbWriter &&other) = delete;

	bool start(void) noexcept;
	void stop(void) noexcept;

	void enqueue(std::vector<AggregateRow> batch) noexcept;

private:
	static void writer_thread_proc(DbWriter *writer) noexcept;

	bool connect(void) noexcept;
	void disconnect(void) noexcept;
	void write_batch(const std::vector<AggregateRow> &batch) noexcept;
	void write_row(const char *table_name, const AggregateRow &row) noexcept;

	MYSQL *connection; // owned by libmysql; released only via mysql_close

	std::deque<std::vector<AggregateRow>> queue; // guarded by mutex
	std::mutex mutex;
	std::condition_variable cv;
	bool wake; // guarded by mutex
	std::atomic<bool> running;
	std::thread thread;
};

#endif
