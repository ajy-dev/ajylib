/**
 * File: server_console_commands.cpp
 * Path: ajylib/source/network/server_console_commands.cpp
 * Description:
 * 	Registers ajy::network::Server management commands into a utility::Console instance.
 * Author: ajy-dev
 * Created: 2026-07-03
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/network/server_console_commands.hpp>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

namespace ajy::network
{
	namespace
	{
		constexpr std::string_view SUBSYSTEM_NAME = "server";

		constexpr std::string_view COMMAND_START = "start";
		constexpr std::string_view COMMAND_STOP = "stop";
		constexpr std::string_view COMMAND_DISCONNECT = "disconnect";
		constexpr std::string_view COMMAND_SESSION_COUNT = "session_count";
		constexpr std::string_view COMMAND_TPS = "tps";

		constexpr std::string_view FLAG_IP = "--ip";
		constexpr std::string_view FLAG_THREADS = "--threads";
		constexpr std::string_view FLAG_NAGLE = "--nagle";

		constexpr std::string_view START_USAGE = "Usage: server start <port> <max_sessions> [--ip <ip>] [--threads <n>] [--nagle]";
		constexpr std::string_view DISCONNECT_USAGE = "Usage: server disconnect <id>";

		constexpr std::string_view START_DESCRIPTION =
			"Starts the server and begins accepting client connections.\n"
			"    Usage: server start <port> <max_sessions> [--ip <ip>] [--threads <n>] [--nagle]\n"
			"      <port>          Port number to listen on.\n"
			"      <max_sessions>  Maximum number of concurrent sessions.\n"
			"      --ip <ip>       Address to bind to. Default: 0.0.0.0 (all interfaces).\n"
			"      --threads <n>   Number of worker threads. Default: CPU core count x2.\n"
			"      --nagle         Enable Nagle's algorithm. Default: disabled.";
		constexpr std::string_view STOP_DESCRIPTION =
			"Stops the server, disconnecting all active sessions and releasing all resources.\n"
			"    Usage: server stop\n"
			"    Safe to call even if the server is not currently running.";
		constexpr std::string_view DISCONNECT_DESCRIPTION =
			"Forcibly disconnects a single session by its id.\n"
			"    Usage: server disconnect <id>\n"
			"      <id>  Session id to disconnect.";
		constexpr std::string_view SESSION_COUNT_DESCRIPTION =
			"Shows the number of currently connected sessions.\n"
			"    Usage: server session_count";
		constexpr std::string_view TPS_DESCRIPTION =
			"Shows accept/recv/send throughput.\n"
			"    Usage: server tps\n"
			"    Note: each call resets the underlying counters, so the figures reflect\n"
			"    the interval since the previous call, not a rolling average.";

		void handle_start(Server *server, std::istringstream &args)
		{
			std::uint16_t port;
			std::uint32_t max_sessions;
			std::string ip;
			int threads;
			bool nagle;
			std::string token;

			if (!(args >> port >> max_sessions))
				goto PRINT_USAGE;

			ip.clear();
			threads = 0;
			nagle = false;

			while (args >> token)
			{
				if (token == FLAG_IP)
				{
					if (!(args >> ip))
						goto PRINT_USAGE;
				}
				else if (token == FLAG_THREADS)
				{
					if (!(args >> threads))
						goto PRINT_USAGE;
				}
				else if (token == FLAG_NAGLE)
					nagle = true;
				else
					goto PRINT_USAGE;
			}

			if (server->start(ip.empty() ? nullptr : ip.c_str(), port, threads, nagle, max_sessions))
			{
				std::cout
					<< "Server started on " << (ip.empty() ? "0.0.0.0" : ip) << ":" << port
					<< " (threads=" << threads << ", nagle=" << (nagle ? "true" : "false") << ", max_sessions=" << max_sessions << ")\n";
			}
			else
				std::cout << "Server start failed (already running, or see log).\n";

			return;

PRINT_USAGE:
			std::cout << START_USAGE << "\n";
		}

		void handle_stop(Server *server, std::istringstream &args)
		{
			(void)args;

			server->stop();
			std::cout << "Server stopped.\n";
		}

		void handle_disconnect(Server *server, std::istringstream &args)
		{
			Server::SessionID id;

			if (!(args >> id))
			{
				std::cout << DISCONNECT_USAGE << "\n";
				return;
			}

			if (server->disconnect(id))
				std::cout << "Session " << id << " disconnected.\n";
			else
				std::cout << "Session " << id << " not found.\n";
		}

		void handle_session_count(Server *server, std::istringstream &args)
		{
			(void)args;

			std::cout << "Session count: " << server->get_session_count() << "\n";
		}

		void handle_tps(Server *server, std::istringstream &args)
		{
			(void)args;

			std::cout
				<< "Accept TPS: " << server->get_accept_tps()
				<< ", Recv TPS: " << server->get_recv_message_tps()
				<< ", Send TPS: " << server->get_send_message_tps() << "\n";
		}
	}

	void register_server_commands(utility::Console *console, Server *server) noexcept
	{
		console->register_command(
			SUBSYSTEM_NAME,
			COMMAND_START,
			START_DESCRIPTION,
			[server](std::istringstream &args)
			{
				handle_start(server, args);
			});
		console->register_command(
			SUBSYSTEM_NAME,
			COMMAND_STOP,
			STOP_DESCRIPTION,
			[server](std::istringstream &args)
			{
				handle_stop(server, args);
			});
		console->register_command(
			SUBSYSTEM_NAME,
			COMMAND_DISCONNECT,
			DISCONNECT_DESCRIPTION,
			[server](std::istringstream &args)
			{
				handle_disconnect(server, args);
			});
		console->register_command(
			SUBSYSTEM_NAME,
			COMMAND_SESSION_COUNT,
			SESSION_COUNT_DESCRIPTION,
			[server](std::istringstream &args)
			{
				handle_session_count(server, args);
			});
		console->register_command(
			SUBSYSTEM_NAME,
			COMMAND_TPS,
			TPS_DESCRIPTION,
			[server](std::istringstream &args)
			{
				handle_tps(server, args);
			});
	}
}
