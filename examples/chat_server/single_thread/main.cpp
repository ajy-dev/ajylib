/**
 * File: main.cpp
 * Path: ajylib/examples/chat_server/single_thread/main.cpp
 * Description:
 *	Entry point for the chat_server example. Drives the server through an
 *	interactive management console; type 'help' for available commands,
 *	'exit' to quit.
 * Note:
 *	LOG_LEVEL below is hardcoded for quick editing.
 * Author: ajy-dev
 * Created: 2026-07-06
 * Updated: 2026-07-07
 * Version: 0.1.0
 */

#include <chat_server/monitor_reporter.hpp>
#include <chat_server/single_thread/chat_server.hpp>

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
}

int main(void)
{
	std::size_t logger_index;
	ajy::utility::Logger *logger;

	logger_index = ajy::utility::Logger::create("chat_server");
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

	ChatServer server("chat_server");
	ajy::utility::Console console;
	MonitorReporter reporter(monitor);

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

	reporter.start();

	std::printf("ChatServer management console. Type 'help' for commands, 'exit' to quit.\n");
	console.run();

	reporter.stop();

	return EXIT_SUCCESS;
}
