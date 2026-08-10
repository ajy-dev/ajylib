/**
 * File: echo_server_config.hpp
 * Path: ajylib/examples/echo_baseline_sw/echo_server_config.hpp
 * Description:
 *	Configuration constants for the echo_baseline_sw example
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_BASELINE_SW_SERVER_CONFIG_HPP
#define ECHO_BASELINE_SW_SERVER_CONFIG_HPP

#include <cstddef>
#include <cstdint>

namespace EchoServerConfig
{
	// --- Transport constants (must match the client; do not change) ---
	inline constexpr std::uint8_t PROTOCOL_CODE = 119;
	inline constexpr std::uint8_t FIXED_KEY = 50;

	// Packet-pool slot size. The largest payload is REQ_LOGIN (protocol.hpp).
	inline constexpr std::size_t MAX_PACKET_PAYLOAD = 78;

	// --- Send workers ---
	inline constexpr std::size_t SEND_WORKER_COUNT = 8;

	// --- Wire payload sizes (protocol.hpp) ---
	inline constexpr std::size_t REQ_LOGIN_SIZE = 78;
	inline constexpr std::size_t REQ_ECHO_SIZE = 18;
}

#endif
