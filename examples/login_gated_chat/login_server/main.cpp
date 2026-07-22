/**
 * File: main.cpp
 * Path: ajylib/examples/login_gated_chat/login_server/main.cpp
 * Description:
 *	Entry point for the login_gated_chat example's login server. Drives the
 *	server through an interactive management console; type 'help' for
 *	available commands, 'exit' to quit. Start with 'server start <port>
 *	<max_sessions>'.
 * Note:
 *	LOG_LEVEL below is hardcoded for quick editing.
 * Author: ajy-dev
 * Created: 2026-07-22
 * Updated: 2026-07-23
 * Version: 0.1.0
 */

#include <login_gated_chat/login_server/login_server.hpp>
#include <login_gated_chat/login_server/login_server_config.hpp>
#include <login_gated_chat/login_server/monitor_reporter.hpp>

#include <ajy/network/server_console_commands.hpp>
#include <ajy/utility/console.hpp>
#include <ajy/utility/logger.hpp>
#include <ajy/utility/monitor/monitor.hpp>
#include <ajy/utility/monitor/monitor_console_commands.hpp>
#include <ajy/utility/monitor/windows/cpu_probe.hpp>
#include <ajy/utility/monitor/windows/memory_probe.hpp>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>

namespace
{
	constexpr ajy::utility::Logger::LogLevel LOG_LEVEL = ajy::utility::Logger::LogLevel::Warning;
}

int main(void)
{
	std::size_t logger_index;
	ajy::utility::Logger *logger;
	std::size_t reporter_logger_index;
	ajy::utility::Logger *reporter_logger;

	logger_index = ajy::utility::Logger::create("login_server");
	if (logger_index >= ajy::utility::Logger::INVALID_INDEX)
	{
		std::fprintf(stderr, "Logger::create() failed.\n");
		return EXIT_FAILURE;
	}

	logger = ajy::utility::Logger::get(logger_index);
	logger->set_threshold(LOG_LEVEL);

	reporter_logger_index = ajy::utility::Logger::create("monitor_reporter");
	if (reporter_logger_index >= ajy::utility::Logger::INVALID_INDEX)
	{
		std::fprintf(stderr, "Logger::create() failed.\n");
		return EXIT_FAILURE;
	}

	reporter_logger = ajy::utility::Logger::get(reporter_logger_index);
	reporter_logger->set_threshold(LOG_LEVEL);

	ajy::utility::monitor::Monitor monitor;
	monitor.add(std::make_unique<ajy::utility::monitor::windows::ProcessCpuProbe>("process_cpu"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::ProcessPrivateMemoryProbe>("process_mem_mb"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemCpuProbe>("system_cpu"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemAvailableMemoryProbe>("system_available_mem_mb"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemNonpagedMemoryProbe>("system_nonpaged_mem_mb"));

	LoginServer server("login_server", LoginServerConfig::WORKER_COUNT);
	ajy::utility::Console console;
	MonitorReporter reporter(monitor, server, "monitor_reporter");

	ajy::network::register_server_commands(&console, &server);
	ajy::utility::monitor::register_monitor_commands(&console, &monitor);

	console.register_command(
		"login",
		"auth_tps",
		"Shows login processing throughput (logins/sec) since the last call.",
		[&server](std::istringstream &args)
		{
			(void)args;
			std::printf("Auth TPS: %u\n", server.get_auth_tps());
		});

	console.register_command(
		"login",
		"packet_pool",
		"Shows packets currently checked out from the packet pool.",
		[&server](std::istringstream &args)
		{
			(void)args;
			std::printf("Packet pool in-use: %zu\n", server.get_packet_pool_in_use());
		});

	console.register_command(
		"login",
		"job_pool",
		"Shows jobs currently checked out from the job pool.",
		[&server](std::istringstream &args)
		{
			(void)args;
			std::printf("Job pool in-use: %zu\n", server.get_job_pool_in_use());
		});

	reporter.start();

	std::printf("LoginServer (%u workers) management console. Type 'help' for commands, 'exit' to quit.\n", LoginServerConfig::WORKER_COUNT);
	console.run();

	reporter.stop();

	return EXIT_SUCCESS;
}
