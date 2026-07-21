/**
 * File: monitor_reporter.cpp
 * Path: ajylib/examples/chat_server/monitor_reporter.cpp
 * Description:
 *	Drives a Monitor on a 1-second thread and reports the chat server's
 *	metrics (DataType 30-37) to the monitoring server as an SS reporter.
 * Note:
 *	The outbound connect is blocking; against a localhost monitor it fails
 *	fast (connection refused) rather than stalling, so Monitor::update() -
 *	run first each tick - is not held up. A remote monitor would want a
 *	non-blocking connect with a select timeout.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: 2026-07-21
 * Version: 0.1.0
 */

#include <chat_server/monitor_reporter.hpp>

#include <chat_server/monitor_report_config.hpp>
#ifdef CHAT_SERVER_MULTI_THREAD
# include <chat_server/multi_thread/chat_server.hpp>
#else
# include <chat_server/single_thread/chat_server.hpp>
#endif

#include <ajy/network/protocol/net_packet_buffer.hpp>
#include <ajy/utility/logger.hpp>
#include <ajy/windows.hpp>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <exception>
#include <random>
#include <string_view>
#include <system_error>
#include <thread>

namespace
{
	constexpr std::size_t LOGIN_PAYLOAD_SIZE = sizeof(std::uint16_t) + sizeof(std::int32_t);
	constexpr std::size_t DATA_UPDATE_PAYLOAD_SIZE = sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::int32_t) + sizeof(std::int32_t);
}

MonitorReporter::MonitorReporter(ajy::utility::monitor::Monitor &monitor, ChatServer &server, std::string_view logger_name) noexcept
	: monitor(monitor)
	, server(server)
	, logger(ajy::utility::Logger::get(logger_name))
	, running(false)
	, monitor_socket(INVALID_SOCKET)
	, connected(false)
	, wsa_ready(false)
	, connect_failure_logged(false)
{
	if (!this->logger)
	{
		std::size_t index;

		index = ajy::utility::Logger::create(logger_name);

		if (index == ajy::utility::Logger::DUPLICATE_NAME)
			this->logger = ajy::utility::Logger::get(logger_name);
		else if (index < ajy::utility::Logger::INVALID_INDEX)
			this->logger = ajy::utility::Logger::get(index);
		else
		{
			std::fprintf(
				stderr,
				"MonitorReporter::MonitorReporter() failed: could not acquire logger \"%.*s\"\n",
				static_cast<int>(logger_name.size()),
				logger_name.data());
			std::terminate();
		}
	}
}

MonitorReporter::~MonitorReporter(void) noexcept
{
	this->stop();
}

void MonitorReporter::start(void) noexcept
{
	int wsa_result;
	WSADATA wsa;

	if (this->thread.joinable())
		return;

	if ((wsa_result = ::WSAStartup(MAKEWORD(2, 2), &wsa)))
	{
		ajy::log_winapi_error("WSAStartup()", static_cast<DWORD>(wsa_result), this->logger);
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "Monitor reporting disabled.");
		this->wsa_ready = false;
	}
	else if (wsa.wVersion != MAKEWORD(2, 2))
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Error,
			"WSAStartup(): requested 2.2 but got %u.%u",
			static_cast<unsigned int>(LOBYTE(wsa.wVersion)),
			static_cast<unsigned int>(HIBYTE(wsa.wVersion)));
		::WSACleanup();
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "Monitor reporting disabled.");
		this->wsa_ready = false;
	}
	else
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Info, "Winsock initialized.");
		this->wsa_ready = true;
	}

	this->running.store(true, std::memory_order_relaxed);

	try
	{
		this->thread = std::thread(thread_proc, this);
	}
	catch (const std::system_error &error)
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Fatal,
			"MonitorReporter::start(): std::thread(thread_proc) failed: [Code: %d] %s",
			error.code().value(),
			error.what());
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
			this->logger->log(
				ajy::utility::Logger::LogLevel::Fatal,
				"MonitorReporter::stop(): std::thread::join(thread_proc) failed: [Code: %d] %s",
				error.code().value(),
				error.what());
			std::terminate();
		}
	}

	this->disconnect_from_monitor();

	if (this->wsa_ready)
	{
		::WSACleanup();
		this->wsa_ready = false;
	}
}

void MonitorReporter::thread_proc(MonitorReporter *reporter) noexcept
{
	ajy::utility::monitor::MonitorClock::time_point next_report;

	next_report = ajy::utility::monitor::MonitorClock::now();

	while (reporter->running.load(std::memory_order_relaxed))
	{
		next_report += std::chrono::milliseconds(MonitorReportConfig::REPORT_INTERVAL_MS);

		if (next_report > ajy::utility::monitor::MonitorClock::now())
		{
			reporter->monitor.update();

			if (reporter->wsa_ready)
			{
				if (!reporter->connected)
					reporter->connect_to_monitor();
				if (reporter->connected)
					reporter->report_metrics();
			}
		}

		std::this_thread::sleep_until(next_report);
	}
}

void MonitorReporter::connect_to_monitor(void) noexcept
{
	SOCKET monitor_socket;
	sockaddr_in address;
	int pton_result;

	monitor_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (monitor_socket == INVALID_SOCKET)
	{
		ajy::log_winapi_error("socket()", ::WSAGetLastError(), this->logger, ajy::utility::Logger::LogLevel::Warning);
		return;
	}

	std::memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = ::htons(MonitorReportConfig::MONITOR_PORT);

	pton_result = ::inet_pton(AF_INET, MonitorReportConfig::MONITOR_HOST.data(), &address.sin_addr);
	if (pton_result != 1)
	{
		if (pton_result == 0)
			this->logger->log(ajy::utility::Logger::LogLevel::Error, "connect_to_monitor(): invalid monitor host \"%s\"", MonitorReportConfig::MONITOR_HOST.data());
		else
			ajy::log_winapi_error("inet_pton()", ::WSAGetLastError(), this->logger);
		::closesocket(monitor_socket);
		return;
	}

	if (::connect(monitor_socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR)
	{
		if (!this->connect_failure_logged)
		{
			ajy::log_winapi_error("connect()", ::WSAGetLastError(), this->logger, ajy::utility::Logger::LogLevel::Warning);
			this->connect_failure_logged = true;
		}
		::closesocket(monitor_socket);
		return;
	}

	this->monitor_socket = monitor_socket;
	this->connected = true;

	if (this->connect_failure_logged)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Info, "connect_to_monitor(): monitor connection restored.");
		this->connect_failure_logged = false;
	}

	if (!this->send_login())
		this->disconnect_from_monitor();
}

void MonitorReporter::disconnect_from_monitor(void) noexcept
{
	if (!this->connected)
		return;

	::closesocket(this->monitor_socket);
	this->monitor_socket = INVALID_SOCKET;
	this->connected = false;
}

bool MonitorReporter::send_login(void) noexcept
{
	ajy::network::protocol::NetPacketBuffer packet(LOGIN_PAYLOAD_SIZE);
	std::uint16_t type;

	if (!packet.get_capacity())
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Error, "send_login(): packet buffer allocation failed.");
		return false;
	}

	type = static_cast<std::uint16_t>(MonitorReportConfig::MonitorPacketType::SS_MONITOR_LOGIN);

	packet << type << MonitorReportConfig::CHAT_SERVER_NO;

	return this->finalize_and_send(packet);
}

void MonitorReporter::report_metrics(void) noexcept
{
	struct Metric
	{
		MonitorReportConfig::MonitorDataType data_type;
		std::int32_t value;
	};

	const Metric metrics[] = {
		{MonitorReportConfig::MonitorDataType::CHAT_SERVER_RUN, 1},
		{MonitorReportConfig::MonitorDataType::CHAT_SERVER_CPU, this->probe_value("process_cpu")},
		{MonitorReportConfig::MonitorDataType::CHAT_SERVER_MEM, this->probe_value("process_mem_mb")},
		{MonitorReportConfig::MonitorDataType::CHAT_SESSION, static_cast<std::int32_t>(this->server.get_session_count())},
		{MonitorReportConfig::MonitorDataType::CHAT_PLAYER, static_cast<std::int32_t>(this->server.get_player_count())},
		{MonitorReportConfig::MonitorDataType::CHAT_UPDATE_TPS, static_cast<std::int32_t>(this->server.get_content_job_tps())},
		{MonitorReportConfig::MonitorDataType::CHAT_PACKET_POOL, static_cast<std::int32_t>(this->server.get_packet_pool_in_use())},
		{MonitorReportConfig::MonitorDataType::CHAT_UPDATEMSG_POOL, static_cast<std::int32_t>(this->server.get_job_pool_in_use())},
	};

	for (const Metric &metric : metrics)
	{
		if (!this->send_data_update(static_cast<std::uint8_t>(metric.data_type), metric.value))
		{
			this->disconnect_from_monitor();
			return;
		}
	}
}

bool MonitorReporter::send_data_update(std::uint8_t data_type, std::int32_t value) noexcept
{
	ajy::network::protocol::NetPacketBuffer packet(DATA_UPDATE_PAYLOAD_SIZE);
	std::uint16_t type;
	std::int32_t time_stamp;

	if (!packet.get_capacity())
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Error, "send_data_update(): packet buffer allocation failed.");
		return false;
	}

	type = static_cast<std::uint16_t>(MonitorReportConfig::MonitorPacketType::SS_MONITOR_DATA_UPDATE);
	time_stamp = static_cast<std::int32_t>(std::time(nullptr));

	packet << type << data_type << value << time_stamp;

	return this->finalize_and_send(packet);
}

bool MonitorReporter::finalize_and_send(ajy::network::protocol::NetPacketBuffer &packet) noexcept
{
	packet.build_header(MonitorReportConfig::MONITOR_CODE, generate_random_key());
	packet.encode(MonitorReportConfig::MONITOR_FIXED_KEY);

	return this->send_all(packet.get_buffer_ptr(), packet.get_packet_size());
}

bool MonitorReporter::send_all(const void *data, std::size_t size) noexcept
{
	const char *bytes;
	std::size_t sent;

	bytes = static_cast<const char *>(data);
	sent = 0;
	while (sent < size)
	{
		int result;

		result = ::send(this->monitor_socket, bytes + sent, static_cast<int>(size - sent), 0);
		if (result == SOCKET_ERROR)
		{
			ajy::log_winapi_error("send()", ::WSAGetLastError(), this->logger, ajy::utility::Logger::LogLevel::Warning);
			return false;
		}
		if (result == 0)
			return false;

		sent += static_cast<std::size_t>(result);
	}

	return true;
}

std::int32_t MonitorReporter::probe_value(std::string_view name) noexcept
{
	ajy::utility::monitor::Probe *probe;
	double value;

	probe = this->monitor.find(name);
	if (!probe)
		return 0;

	value = probe->get_value();
	if (std::isnan(value)) // NaN = probe syscall error; report 0 rather than a garbage cast
		return 0;

	return static_cast<std::int32_t>(std::llround(value));
}

std::uint8_t MonitorReporter::generate_random_key(void) noexcept
{
	thread_local std::random_device random_device;
	thread_local std::mt19937 engine(random_device());
	thread_local std::uniform_int_distribution<unsigned int> distribution(0, 255);

	return static_cast<std::uint8_t>(distribution(engine));
}
