/**
 * File: chat_server_config.hpp
 * Path: ajylib/examples/login_gated_chat/chat_server/chat_server_config.hpp
 * Description:
 *	Configuration constants for the login_gated_chat example's chat server
 * Author: ajy-dev
 * Created: 2026-07-22
 * Updated: 2026-08-07
 * Version: 0.1.0
 */

#ifndef CHAT_SERVER_CONFIG_HPP
#define CHAT_SERVER_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace ChatServerConfig
{
	// --- Transport constants (must match the client; do not change) ---
	// Framing magic byte and obfuscation fixed key injected into NetServer.
	inline constexpr std::uint8_t PROTOCOL_CODE = 0x77;
	inline constexpr std::uint8_t FIXED_KEY = 0x32;

	// --- World / server settings ---
	// Sector grid: 50 x 50 = 2500 sectors.
	inline constexpr std::uint16_t SECTOR_MAX_X = 50;
	inline constexpr std::uint16_t SECTOR_MAX_Y = 50;
	inline constexpr std::size_t TOTAL_SECTORS = static_cast<std::size_t>(SECTOR_MAX_X) * SECTOR_MAX_Y;

	// Area of interest: self + adjacent +/-1 on each axis -> 3 x 3.
	inline constexpr int SECTOR_AOI_RANGE = 1;

	// Sentinel for a player not yet placed in any sector.
	inline constexpr std::uint16_t INVALID_SECTOR = std::numeric_limits<std::uint16_t>::max();

	// Heartbeat: client sends every 30 s; server disconnects a session after
	// 40 s of silence, sweeping for timeouts every 5 s.
	inline constexpr std::int64_t HEARTBEAT_TIMEOUT_MS = 40'000;
	inline constexpr std::int64_t HEARTBEAT_CHECK_INTERVAL_MS = 5'000;

	// Maximum chat message length.
	inline constexpr std::uint16_t MAX_MESSAGE_CHARS = 200;

	// Packet-pool slot size. The largest outbound payload is RES_MESSAGE
	// (protocol.hpp): 92-byte fixed part + MAX_MESSAGE_CHARS WCHARs = 492.
	inline constexpr std::size_t MAX_PACKET_PAYLOAD = 492;

	// --- Instance identity ---
	inline constexpr std::string_view INSTANCE_NAME = "chat";
	inline constexpr std::string_view ADVERTISED_IP = "10.0.2.1";

	// --- Login server SS endpoint (LoginLink) ---
	inline constexpr std::string_view LOGIN_SERVER_IP = "127.0.0.1";
	inline constexpr std::uint16_t LOGIN_SERVER_SS_PORT = 10501;
	inline constexpr std::int64_t LOGIN_LINK_RECONNECT_INTERVAL_MS = 5'000;
	inline constexpr std::size_t SS_MAX_PACKET_PAYLOAD = 36;

	// --- Redis (Memurai) ---
	// Shared with the login server: the login gate consumes the auth ticket
	// and records this account's session here.
	inline constexpr std::string_view REDIS_HOST = "127.0.0.1";
	inline constexpr std::uint16_t REDIS_PORT = 6379;

	// --- Redis key sections ---
	// Key format: <section>:<instance>:<account_no>
	inline constexpr std::string_view TICKET_SECTION = "ticket";
	inline constexpr std::string_view SESSION_SECTION = "session";
}

#endif
