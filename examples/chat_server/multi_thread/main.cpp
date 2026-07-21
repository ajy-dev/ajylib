/**
 * File: main.cpp
 * Path: ajylib/examples/chat_server/multi_thread/main.cpp
 * Description:
 *	Entry point for the chat_server example. Drives the server through an
 *	interactive management console; type 'help' for available commands,
 *	'exit' to quit.
 * Note:
 *	LOG_LEVEL below is hardcoded for quick editing. SEND_WORKERS sets the
 *	send-worker thread count for A/B throughput sweeps.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: 2026-07-21
 * Version: 0.1.0
 */

#include <chat_server/monitor_reporter.hpp>
#include <chat_server/multi_thread/chat_server.hpp>

#include <ajy/network/server_console_commands.hpp>
#include <ajy/utility/console.hpp>
#include <ajy/utility/logger.hpp>
#include <ajy/utility/monitor/monitor.hpp>
#include <ajy/utility/monitor/monitor_console_commands.hpp>
#include <ajy/utility/monitor/windows/cpu_probe.hpp>
#include <ajy/utility/monitor/windows/memory_probe.hpp>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>

namespace
{
	constexpr ajy::utility::Logger::LogLevel LOG_LEVEL = ajy::utility::Logger::LogLevel::Warning;
	constexpr unsigned int SEND_WORKERS = 4;
}

int main(void)
{
	std::size_t logger_index;
	ajy::utility::Logger *logger;
	std::size_t reporter_logger_index;
	ajy::utility::Logger *reporter_logger;

	logger_index = ajy::utility::Logger::create("chat_server");
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

	ChatServer server("chat_server", SEND_WORKERS); // [MT] send-worker count
	ajy::utility::Console console;
	MonitorReporter reporter(monitor, server, "monitor_reporter");

	ajy::network::register_server_commands(&console, &server);
	ajy::utility::monitor::register_monitor_commands(&console, &monitor);

	console.register_command(
		"chat",
		"content_tps",
		"Shows content-thread job throughput (jobs/sec) since the last call.",
		[&server](std::istringstream &args)
		{
			(void)args;
			std::printf("Content Job TPS: %u\n", server.get_content_job_tps());
		});

	console.register_command(
		"chat",
		"player_count",
		"Shows the current logged-in player count.",
		[&server](std::istringstream &args)
		{
			(void)args;
			std::printf("Player count: %u\n", server.get_player_count());
		});

	console.register_command(
		"chat",
		"packet_pool",
		"Shows packets currently checked out from the packet pool.",
		[&server](std::istringstream &args)
		{
			(void)args;
			std::printf("Packet pool in-use: %zu\n", server.get_packet_pool_in_use());
		});

	console.register_command(
		"chat",
		"job_pool",
		"Shows jobs currently checked out from the job pool.",
		[&server](std::istringstream &args)
		{
			(void)args;
			std::printf("Job pool in-use: %zu\n", server.get_job_pool_in_use());
		});

	reporter.start();

	std::printf("ChatServer (multi_thread, %u send workers) management console. Type 'help' for commands, 'exit' to quit.\n", SEND_WORKERS);
	console.run();

	reporter.stop();

	return EXIT_SUCCESS;
}