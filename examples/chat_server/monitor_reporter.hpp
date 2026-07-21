/**
 * File: monitor_reporter.hpp
 * Path: ajylib/examples/chat_server/monitor_reporter.hpp
 * Description:
 *	Drives a Monitor on a 1-second thread and, on the same cadence, reports
 *	the chat server's metrics (DataType 30-37) to the monitoring server as an
 *	SS reporter over a single outbound connection.
 * Note:
 *	Reporting is best-effort: Monitor::update() always runs; a missing or
 *	dropped monitor connection is retried on the next tick. The report path is
 *	fire-and-forget (no server response), so no recv loop is needed.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: 2026-07-21
 * Version: 0.1.0
 */

#ifndef CHAT_SERVER_MONITOR_REPORTER_HPP
#define CHAT_SERVER_MONITOR_REPORTER_HPP

#include <ajy/network/protocol/net_packet_buffer.hpp>
#include <ajy/utility/logger.hpp>
#include <ajy/utility/monitor/monitor.hpp>
#include <ajy/windows.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <thread>

class ChatServer;

class MonitorReporter
{
public:
	MonitorReporter(ajy::utility::monitor::Monitor &monitor, ChatServer &server, std::string_view logger_name) noexcept;
	~MonitorReporter(void) noexcept;

	MonitorReporter(const MonitorReporter &other) = delete;
	MonitorReporter &operator=(const MonitorReporter &other) = delete;
	MonitorReporter(MonitorReporter &&other) = delete;
	MonitorReporter &operator=(MonitorReporter &&other) = delete;

	void start(void) noexcept;
	void stop(void) noexcept;

private:
	static void thread_proc(MonitorReporter *reporter) noexcept;
	static std::uint8_t generate_random_key(void) noexcept;

	void connect_to_monitor(void) noexcept;
	void disconnect_from_monitor(void) noexcept;

	bool send_login(void) noexcept;
	void report_metrics(void) noexcept;
	bool send_data_update(std::uint8_t data_type, std::int32_t value) noexcept;
	bool finalize_and_send(ajy::network::protocol::NetPacketBuffer &packet) noexcept;
	bool send_all(const void *data, std::size_t size) noexcept;
	std::int32_t probe_value(std::string_view name) noexcept;

	ajy::utility::monitor::Monitor &monitor;
	ChatServer &server;
	ajy::utility::Logger *logger;

	std::atomic<bool> running;
	std::thread thread;

	SOCKET monitor_socket;

	bool connected;
	bool wsa_ready;
	bool connect_failure_logged;
};

#endif