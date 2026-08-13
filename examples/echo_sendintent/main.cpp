/**
 * File: main.cpp
 * Path: ajylib/examples/echo_with_group/main.cpp
 * Description:
 *	Entry point for the echo_with_group example. Drives the server through
 *	an interactive management console; type 'help' for available commands,
 *	'exit' to quit.
 * Note:
 *	LOG_LEVEL below is hardcoded for quick editing.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#include "echo_sendintent_server.hpp"
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

	logger_index = ajy::utility::Logger::create("echo_sendintent");
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

	EchoSendIntentServer server("echo_sendintent");
	ajy::utility::Console console;
	MonitorReporter reporter(monitor);

	ajy::network::register_server_commands(&console, &server);
	ajy::utility::monitor::register_monitor_commands(&console, &monitor);

	console.register_command(
		"group",
		"fps",
		"Shows each group's frame rate (frames/sec) since the last call.",
		[&server](std::istringstream &args)
		{
			std::size_t i;

			(void)args;

			std::printf("Auth FPS: %u", server.get_auth_frame_tps());
			for (i = 0; i < server.get_echo_group_count(); ++i)
				std::printf(" / Echo[%zu] FPS: %u", i, server.get_echo_frame_tps(i));
			std::printf("\n");
		});

	console.register_command(
		"group",
		"session_count",
		"Shows the session count of each group.",
		[&server](std::istringstream &args)
		{
			std::size_t i;

			(void)args;

			for (i = 0; i < server.get_echo_group_count(); ++i)
				std::printf("Echo[%zu]: %u ", i, server.get_echo_session_count(i));
			std::printf("\n");
		});

	console.register_command(
		"group",
		"job_pool",
		"Shows jobs currently queued in each group (backlog).",
		[&server](std::istringstream &args)
		{
			std::size_t i;

			(void)args;

			for (i = 0; i < server.get_echo_group_count(); ++i)
				std::printf("Echo[%zu]: %zu (rejected %zu) ", i,
					server.get_echo_job_pool_in_use(i), server.get_echo_rejected_session_count(i));
			std::printf("\n");

			for (i = 0; i < server.get_send_worker_count(); ++i)
				std::printf("Send[%zu]: %s ", i, server.is_send_queue_empty(i) ? "empty" : "BUSY");
			std::printf("\n");
		});

	console.register_command(
		"group",
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
		"group",
		"losses",
		"Shows where sessions or packets went missing.",
		[&server](std::istringstream &args)
		{
			std::size_t i;

			(void)args;

			std::printf("orphan_recv: %zu, account_store: %zu\n",
				server.get_orphan_recv_count(), server.get_account_store_size());

			for (i = 0; i < server.get_echo_group_count(); ++i)
				std::printf("Echo[%zu]: account_miss %zu, stale_enter %zu, rejected %zu\n", i,
					server.get_echo_account_miss_count(i),
					server.get_echo_stale_enter_count(i),
					server.get_echo_rejected_session_count(i));
		});

	console.register_command(
		"group",
		"packet_pool",
		"Shows packets currently checked out from the packet pool.",
		[&server](std::istringstream &args)
		{
			(void)args;
			std::printf("Packet pool in-use: %zu\n", server.get_packet_pool_in_use());
		});

	reporter.start();

	std::printf("EchoSendIntentServer management console. Type 'help' for commands, 'exit' to quit.\n");
	console.run();

	reporter.stop();

	return EXIT_SUCCESS;
}
