/**
 * File: db_writer.cpp
 * Path: ajylib/examples/monitor_server/db_writer.cpp
 * Description:
 *	Definition of DbWriter (declared in db_writer.hpp): the writer thread
 *	lifecycle, the batch queue handoff, and the INSERT / monthly-rollover
 *	logic against monitorlog_YYYYMM.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#include <monitor_server/db_writer.hpp>
#include <monitor_server/monitor_server_config.hpp>

#include <mysql.h>
#include <mysqld_error.h>

#include <cstdio>
#include <ctime>
#include <exception>
#include <mutex>
#include <system_error>
#include <utility>

DbWriter::DbWriter(void) noexcept
	: connection(nullptr)
	, wake(false)
	, running(false)
{
}

DbWriter::~DbWriter(void) noexcept
{
	this->stop();
}

bool DbWriter::start(void) noexcept
{
	if (this->thread.joinable())
		return false;

	this->running.store(true, std::memory_order_relaxed);

	try
	{
		this->thread = std::thread(writer_thread_proc, this);
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "DbWriter::start(): std::thread failed: [Code: %d] %s\n", error.code().value(), error.what());
		this->running.store(false, std::memory_order_relaxed);
		return false;
	}

	return true;
}

void DbWriter::stop(void) noexcept
{
	if (!this->thread.joinable())
		return;

	{
		std::lock_guard<std::mutex> lock(this->mutex);
		this->running.store(false, std::memory_order_relaxed);
		this->wake = true;
	}
	this->cv.notify_one();

	try
	{
		this->thread.join();
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "DbWriter::stop(): std::thread::join failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
}

void DbWriter::enqueue(std::vector<AggregateRow> batch) noexcept
{
	{
		std::lock_guard<std::mutex> lock(this->mutex);
		try
		{
			this->queue.push_back(std::move(batch));
		}
		catch (const std::bad_alloc &error)
		{
			std::fprintf(stderr, "DbWriter::enqueue(): queue.push_back() failed: %s\n", error.what());
			std::terminate();
		}
		this->wake = true;
	}
	this->cv.notify_one();
}

bool DbWriter::connect(void) noexcept
{
	MYSQL *handle;

	// mysql_init(nullptr) lets the client library allocate the MYSQL object;
	// mysql_close later frees it. Nothing crosses the CRT heap boundary.
	handle = mysql_init(nullptr);
	if (!handle)
	{
		std::fprintf(stderr, "DbWriter::connect(): mysql_init() failed.\n");
		return false;
	}

	if (!mysql_real_connect(
		    handle,
		    MonitorServerConfig::DB_HOST.data(),
		    MonitorServerConfig::DB_USER.data(),
		    MonitorServerConfig::DB_PASSWORD.data(),
		    MonitorServerConfig::DB_NAME.data(),
		    MonitorServerConfig::DB_PORT,
		    nullptr,
		    0))
	{
		std::fprintf(stderr, "DbWriter::connect(): mysql_real_connect() failed: %s\n", mysql_error(handle));
		mysql_close(handle);
		return false;
	}

	this->connection = handle;
	return true;
}

void DbWriter::disconnect(void) noexcept
{
	if (this->connection)
	{
		mysql_close(this->connection);
		this->connection = nullptr;
	}
}

void DbWriter::writer_thread_proc(DbWriter *writer) noexcept
{
	bool connected;

	// The connection is opened and used only on this thread. On failure we log
	// and keep running: batches are still drained (and discarded), so the rest
	// of the server is unaffected by a missing DB.
	connected = writer->connect();
	if (!connected)
		std::fprintf(stderr, "DbWriter: running without a database connection; batches will be dropped.\n");

	while (true)
	{
		std::deque<std::vector<AggregateRow>> pending;
		bool stop_requested;

		{
			std::unique_lock<std::mutex> lock(writer->mutex);
			writer->cv.wait(
				lock,
				[writer]
				{
					return writer->wake;
				});
			writer->wake = false;
			pending.swap(writer->queue);
			stop_requested = !writer->running.load(std::memory_order_relaxed);
		}

		// Content thread is stopped before the writer during shutdown, so a
		// single swap captures every remaining batch; nothing is lost.
		for (const std::vector<AggregateRow> &batch : pending)
			if (connected)
				writer->write_batch(batch);

		if (stop_requested)
			break;
	}

	if (connected)
		writer->disconnect();
}

void DbWriter::write_batch(const std::vector<AggregateRow> &batch) noexcept
{
	char table_name[32];
	std::time_t now;
	std::tm tm;

	// Every row in a batch comes from the same flush (same minute), so the
	// monthly table name is resolved once per batch.
	now = std::time(nullptr);
	::localtime_s(&tm, &now);
	std::snprintf(table_name, sizeof(table_name), "monitorlog_%04d%02d", tm.tm_year + 1900, tm.tm_mon + 1);

	for (const AggregateRow &row : batch)
		this->write_row(table_name, row);
}

void DbWriter::write_row(const char *table_name, const AggregateRow &row) noexcept
{
	char query[256];
	char create[128];

	// All values are integers, so string assembly carries no injection risk.
	std::snprintf(
		query,
		sizeof(query),
		"INSERT INTO %s (logtime, serverno, type, avr, `min`, `max`) VALUES (NOW(), %d, %u, %d, %d, %d)",
		table_name,
		row.server_no,
		static_cast<unsigned int>(row.data_type),
		row.avg,
		row.min,
		row.max);

	if (mysql_query(this->connection, query) == 0)
		return;

	// Missing table (first row of a new month): create it from the template,
	// then retry the insert once.
	if (mysql_errno(this->connection) != ER_NO_SUCH_TABLE)
	{
		std::fprintf(stderr, "DbWriter::write_row(): insert failed: %s\n", mysql_error(this->connection));
		return;
	}

	std::snprintf(create, sizeof(create), "CREATE TABLE %s LIKE monitorlog_template", table_name);

	if (mysql_query(this->connection, create) != 0)
	{
		std::fprintf(stderr, "DbWriter::write_row(): create table failed: %s\n", mysql_error(this->connection));
		return;
	}

	if (mysql_query(this->connection, query) != 0)
		std::fprintf(stderr, "DbWriter::write_row(): insert after create failed: %s\n", mysql_error(this->connection));
}
