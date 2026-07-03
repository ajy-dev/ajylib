/**
 * File: server_console_commands.hpp
 * Path: ajylib/include/ajy/network/server_console_commands.hpp
 * Description:
 * 	Registers ajy::network::Server management commands into a utility::Console instance.
 * Note:
 * 	The caller must ensure server outlives console.
 * Author: ajy-dev
 * Created: 2026-07-03
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_NETWORK_SERVER_CONSOLE_COMMANDS_HPP
#define AJY_NETWORK_SERVER_CONSOLE_COMMANDS_HPP

#include <ajy/network/server.hpp>
#include <ajy/utility/console.hpp>

namespace ajy::network
{
	void register_server_commands(utility::Console *console, Server *server) noexcept;
}

#endif
