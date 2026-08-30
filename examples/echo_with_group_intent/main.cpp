/**
 * File: main.cpp
 * Path: ajylib/examples/echo_with_group_intent/main.cpp
 * Description:
 *	Entry point for the echo_with_group_intent example. Drives the server through
 *	an interactive management console; type 'help' for available commands,
 *	'exit' to quit.
 * Note:
 *	LOG_LEVEL below is hardcoded for quick editing.
 * Author: ajy-dev
 * Created: 2026-08-14
 * Updated: Never
 * Version: 0.1.0
 */

#include "echo_with_group_intent_server.hpp"
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

// Timer resolution, declared here so the library headers stay untouched.
extern "C" __declspec(dllimport) unsigned int __stdcall timeBeginPeriod(unsigned int period);
extern "C" __declspec(dllimport) unsigned int __stdcall timeEndPeriod(unsigned int period);
#pragma comment(lib, "winmm.lib")

namespace
{
	constexpr ajy::utility::Logger::LogLevel LOG_LEVEL = ajy::utility::Logger::LogLevel::Warning;
}

int main(void)
{
	std::size_t logger_index;
	ajy::utility::Logger *logger;

	logger_index = ajy::utility::Logger::create("echo_with_group_intent");
	if (logger_index >= ajy::utility::Logger::INVALID_INDEX)
	{
		std::fprintf(stderr, "Logger::create() failed.\n");
		return EXIT_FAILURE;
	}

	logger = ajy::utility::Logger::get(logger_index);
	logger->set_threshold(LOG_LEVEL);

	// 1 ms timer resolution: the group frame loop waits on a condition
	// variable with a 33 ms timeout, which the 15.6 ms default quantizes.
	::timeBeginPeriod(1);

	ajy::utility::monitor::Monitor monitor;
	monitor.add(std::make_unique<ajy::utility::monitor::windows::ProcessCpuProbe>("process_cpu"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::ProcessPrivateMemoryProbe>("process_mem_mb"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemCpuProbe>("system_cpu"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemAvailableMemoryProbe>("system_available_mem_mb"));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemNonpagedMemoryProbe>("system_nonpaged_mem_mb"));

	EchoWithGroupIntentServer server("echo_with_group_intent");
	ajy::utility::Console console;
	MonitorReporter reporter(monitor, server, "monitor_reporter");

	ajy::network::register_server_commands(&console, &server);
	ajy::utility::monitor::register_monitor_commands(&console, &monitor);

	console.register_command(
		"group",
		"fps",
		"Shows each group's frame rate (frames/sec) since the last call.",
		[&server](std::istringstream &args)
		{
			(void)args;

			std::printf("Auth FPS: %u / Echo FPS: %u\n", server.get_auth_frame_tps(), server.get_echo_frame_tps());
		});

	console.register_command(
		"group",
		"session_count",
		"Shows every group's live session count.\n"
		"  gap = group total - server total. A negative gap is normal (a session is\n"
		"  unaffiliated between accept and enter, and while moving between groups).\n"
		"  A positive gap means a membership outlived its session.",
		[&server](std::istringstream &args)
		{
			std::uint64_t total_live;
			std::uint32_t server_live;
			std::int64_t gap;

			(void)args;

			total_live = server.get_auth_session_count();

			std::printf("Auth:    live %llu\n", static_cast<unsigned long long>(total_live));

			{
				std::uint64_t live;

				live = server.get_echo_session_count();

				std::printf("Echo:    live %llu\n", static_cast<unsigned long long>(live));

				total_live += live;
			}

			server_live = server.get_session_count();
			gap = static_cast<std::int64_t>(total_live) - static_cast<std::int64_t>(server_live);

			std::printf("Groups:  live %llu\n", static_cast<unsigned long long>(total_live));
			std::printf("Server:  live %u, gap %+lld\n", server_live, static_cast<long long>(gap));
		});

	console.register_command(
		"group",
		"job_pool",
		"Shows jobs currently queued in each group (backlog).",
		[&server](std::istringstream &args)
		{
			std::size_t i;

			(void)args;

			std::printf("Echo: %zu\n", server.get_echo_job_pool_in_use());

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
			(void)args;

			std::printf("orphan_recv: %zu, account_store: %zu\n",
				server.get_orphan_recv_count(), server.get_account_store_size());

			std::printf("Echo: account_miss %zu\n", server.get_echo_account_miss_count());
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

	std::printf("EchoWithGroupIntentServer management console. Type 'help' for commands, 'exit' to quit.\n");
	console.run();

	reporter.stop();

	::timeEndPeriod(1);

	return EXIT_SUCCESS;
}
