/**
 * File: monitor_reporter.hpp
 * Path: ajylib/examples/echo_server/monitor_reporter.hpp
 * Description:
 *	Drives a Monitor on a 1-second thread.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_SERVER_MONITOR_REPORTER_HPP
#define ECHO_SERVER_MONITOR_REPORTER_HPP

#include <ajy/utility/monitor/monitor.hpp>

#include <atomic>
#include <thread>

class MonitorReporter
{
public:
	explicit MonitorReporter(ajy::utility::monitor::Monitor &monitor) noexcept;
	~MonitorReporter(void) noexcept;

	MonitorReporter(const MonitorReporter &other) = delete;
	MonitorReporter &operator=(const MonitorReporter &other) = delete;
	MonitorReporter(MonitorReporter &&other) = delete;
	MonitorReporter &operator=(MonitorReporter &&other) = delete;

	void start(void) noexcept;
	void stop(void) noexcept;

private:
	static void thread_proc(MonitorReporter *reporter) noexcept;

	ajy::utility::monitor::Monitor &monitor;
	std::atomic<bool> running;
	std::thread thread;
};

#endif
