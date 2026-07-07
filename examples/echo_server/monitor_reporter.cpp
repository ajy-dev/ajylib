/**
 * File: monitor_reporter.cpp
 * Path: ajylib/examples/echo_server/monitor_reporter.cpp
 * Description:
 *	Drives a Monitor on a 1-second thread.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#include "monitor_reporter.hpp"

#include <chrono>
#include <cstdio>
#include <exception>
#include <system_error>

namespace
{
	constexpr std::chrono::seconds UPDATE_INTERVAL(1);
}

MonitorReporter::MonitorReporter(ajy::utility::monitor::Monitor &monitor) noexcept
	: monitor(monitor)
	, running(false)
{
}

MonitorReporter::~MonitorReporter(void) noexcept
{
	this->stop();
}

void MonitorReporter::start(void) noexcept
{
	if (this->thread.joinable())
		return;

	this->running.store(true, std::memory_order_relaxed);

	try
	{
		this->thread = std::thread(thread_proc, this);
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "MonitorReporter::start(): std::thread failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
}

void MonitorReporter::stop(void) noexcept
{
	this->running.store(false, std::memory_order_relaxed);

	if (this->thread.joinable())
	{
		try
		{
			this->thread.join();
		}
		catch (const std::system_error &error)
		{
			std::fprintf(stderr, "MonitorReporter::stop(): std::thread::join failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}
	}
}

void MonitorReporter::thread_proc(MonitorReporter *reporter) noexcept
{
	while (reporter->running.load(std::memory_order_relaxed))
	{
		reporter->monitor.update();
		std::this_thread::sleep_for(UPDATE_INTERVAL);
	}
}
