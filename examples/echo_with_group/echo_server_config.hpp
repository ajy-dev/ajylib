/**
 * File: echo_server_config.hpp
 * Path: ajylib/examples/echo_with_group/echo_server_config.hpp
 * Description:
 *	Configuration constants for the echo_with_group example
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_SERVER_CONFIG_HPP
#define ECHO_SERVER_CONFIG_HPP

#include <cstddef>
#include <cstdint>

namespace EchoServerConfig
{
	// --- Transport constants (must match the client; do not change) ---
	// Framing magic byte and obfuscation fixed key injected into NetServer.
	inline constexpr std::uint8_t PROTOCOL_CODE = 119;
	inline constexpr std::uint8_t FIXED_KEY = 50;

	// Packet-pool slot size. The largest payload is REQ_LOGIN (protocol.hpp).
	inline constexpr std::size_t MAX_PACKET_PAYLOAD = 78;

	// --- Groups ---
	// Echo work is sharded across this many groups, each with its own thread;
	// echo sessions share no state, so the split is free.
	inline constexpr std::size_t ECHO_GROUP_COUNT = 1;

	inline constexpr std::uint32_t AUTH_GROUP_FPS = 30;
	inline constexpr std::uint32_t ECHO_GROUP_FPS = 30;

	// --- Wire payload sizes (protocol.hpp) ---
	inline constexpr std::size_t REQ_LOGIN_SIZE = 78;
	inline constexpr std::size_t REQ_ECHO_SIZE = 18;
	inline constexpr std::size_t SESSION_KEY_SIZE = 64;
}

#endif
