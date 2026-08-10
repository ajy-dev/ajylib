/**
 * File: main.cpp
 * Path: ajylib/examples/echo_baseline_sw/main.cpp
 * Description:
 *	Entry point for the echo_baseline_sw example. Drives the server through an
 *	interactive management console; type 'help' for available commands,
 *	'exit' to quit.
 * Note:
 *	LOG_LEVEL below is hardcoded for quick editing.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#include "echo_baseline_sw_server.hpp"
#include "monitor_reporter.hpp"

#include <ajy/network/server_console_commands.hpp>
#include <ajy/utility/console.hpp>
#include <ajy/utility/logger.hpp>
#include <ajy/utility/monitor/monitor.hpp>
#include <ajy/utility/monitor/monitor_console_commands.hpp>
#include <ajy/utility/monitor/windows/cpu_probe.hpp>
#include <ajy/utility/monitor/windows/memory_probe.hpp>

#include <cstdint>
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

	logger_index = ajy::utility::Logger::create("echo_baseline_sw");
	if (logger_index >= ajy::utility::Logger::INVALID_INDEX)
	{
		std::fprintf(stderr, "Logger::create() failed.\n");
		return EXIT_FAILURE;
	}

	logger = ajy::utility::Logger::get(logger_index);
	logger->set_threshold(LOG_LEVEL);

	ajy::utility::monitor::Monitor monitor;
	monitor.add(std::make_unique<ajy::utility::monitor::windows::ProcessCpuProbe>("process_cpu"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::ProcessPrivateMemoryProbe>("process_mem_mb"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemCpuProbe>("system_cpu"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemAvailableMemoryProbe>("system_available_mem_mb"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemNonpagedMemoryProbe>("system_nonpaged_mem_mb"));

	EchoBaselineSwServer server("echo_baseline_sw");
	ajy::utility::Console console;
	MonitorReporter reporter(monitor);

	ajy::network::register_server_commands(&console, &server);
	ajy::utility::monitor::register_monitor_commands(&console, &monitor);

	console.register_command(
		"echo",
		"send_queue",
		"Shows whether each send worker's queue is drained.",
		[&server](std::istringstream &args)
		{
			std::size_t i;

			(void)args;

			for (i = 0; i < server.get_send_worker_count(); ++i)
				std::printf("Send[%zu]: %s ", i, server.is_send_queue_empty(i) ? "empty" : "BUSY");
			std::printf("\n");
		});

	console.register_command(
		"echo",
		"send_batch",
		"Shows completed WSASend calls per second and their mean size.",
		[&server](std::istringstream &args)
		{
			std::uint32_t completions_per_second;
			std::size_t mean_size;

			(void)args;

			server.query_send_batching(completions_per_second, mean_size);
			std::printf("Send completions/sec: %u, mean bytes: %zu\n", completions_per_second, mean_size);
		});

	console.register_command(
		"echo",
		"packet_pool",
		"Shows packets currently checked out from the packet pool.",
		[&server](std::istringstream &args)
		{
			(void)args;
			std::printf("Packet pool in-use: %zu\n", server.get_packet_pool_in_use());
		});

	reporter.start();

	std::printf("EchoBaselineSwServer management console. Type 'help' for commands, 'exit' to quit.\n");
	console.run();

	reporter.stop();

	return EXIT_SUCCESS;
}
