/**
 * File: main.cpp
 * Path: ajylib/examples/monitor_server/main.cpp
 * Description:
 *	Entry point for the monitor_server example. Drives the server through an
 *	interactive management console; type 'help' for available commands,
 *	'exit' to quit. Start the server with 'server start <port> <max_sessions>'.
 * Note:
 *	LOG_LEVEL below is hardcoded for quick editing.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#include <monitor_server/local_sampler.hpp>
#include <monitor_server/monitor_server.hpp>

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

namespace
{
	constexpr ajy::utility::Logger::LogLevel LOG_LEVEL = ajy::utility::Logger::LogLevel::Warning;
}

int main(void)
{
	std::size_t logger_index;
	ajy::utility::Logger *logger;

	logger_index = ajy::utility::Logger::create("monitor_server");
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
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemCpuProbe>(LocalSampler::PROBE_CPU));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemAvailableMemoryProbe>(LocalSampler::PROBE_AVAILABLE));
	monitor.add(std::make_unique<ajy::utility::monitor::windows::SystemNonpagedMemoryProbe>(LocalSampler::PROBE_NONPAGED));

	MonitorServer server("monitor_server");
	ajy::utility::Console console;
	LocalSampler sampler(monitor, server);

	ajy::network::register_server_commands(&console, &server);
	ajy::utility::monitor::register_monitor_commands(&console, &monitor);

	sampler.start();

	std::printf("MonitorServer management console. Type 'help' for commands, 'exit' to quit.\n");
	console.run();

	sampler.stop();

	return EXIT_SUCCESS;
}
