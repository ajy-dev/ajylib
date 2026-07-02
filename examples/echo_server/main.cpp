/**
 * File: main.cpp
 * Path: ajylib/examples/echo_server/main.cpp
 * Description:
 *	Entry point for the echo_server example. Runs until Ctrl+C is pressed.
 * Note:
 *	LOG_LEVEL and PORT below are hardcoded for quick editing.
 * Author: ajy-dev
 * Created: 2026-07-02
 * Updated: Never
 * Version: 0.1.0
 */

#include "echo_server.hpp"

#include <ajy/utility/logger.hpp>
#include <ajy/windows.hpp>

#include <cstdint>
#include <cstdio>

namespace
{
	constexpr ajy::utility::Logger::LogLevel LOG_LEVEL = ajy::utility::Logger::LogLevel::Info;
	constexpr std::uint16_t PORT = 6000;

	HANDLE shutdown_event = nullptr;

	BOOL WINAPI console_ctrl_handler(DWORD ctrl_type)
	{
		(void)ctrl_type;

		if (shutdown_event)
			::SetEvent(shutdown_event);

		return TRUE;
	}
}

int main(void)
{
	std::size_t logger_index;
	ajy::utility::Logger *logger;

	logger_index = ajy::utility::Logger::create("echo_server");
	if (logger_index >= ajy::utility::Logger::INVALID_INDEX)
	{
		std::fprintf(stderr, "Logger::create() failed.\n");
		return EXIT_FAILURE;
	}

	logger = ajy::utility::Logger::get(logger_index);
	logger->set_threshold(LOG_LEVEL);

	EchoServer server("echo_server");

	shutdown_event = ::CreateEventA(nullptr, TRUE, FALSE, nullptr);
	if (!shutdown_event)
	{
		std::fprintf(stderr, "CreateEventA() failed.\n");
		return EXIT_FAILURE;
	}

	if (!::SetConsoleCtrlHandler(console_ctrl_handler, TRUE))
	{
		std::fprintf(stderr, "SetConsoleCtrlHandler() failed.\n");
		::CloseHandle(shutdown_event);
		return EXIT_FAILURE;
	}

	if (!server.start(nullptr, PORT, 0, false, 1024))
	{
		std::fprintf(stderr, "EchoServer::start() failed.\n");
		::CloseHandle(shutdown_event);
		return EXIT_FAILURE;
	}

	std::printf("EchoServer started on port %u. Press Ctrl+C to stop.\n", static_cast<unsigned int>(PORT));

	::WaitForSingleObject(shutdown_event, INFINITE);

	std::printf("Shutting down...\n");
	server.stop();

	::CloseHandle(shutdown_event);
	return EXIT_SUCCESS;
}
