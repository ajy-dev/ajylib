/**
 * File: login_server_config.hpp
 * Path: ajylib/examples/login_gated_chat/login_server/login_server_config.hpp
 * Description:
 *	Configuration constants for the login_gated_chat example's login server
 * Author: ajy-dev
 * Created: 2026-07-22
 * Updated: 2026-08-07
 * Version: 0.1.0
 */

#ifndef LOGIN_SERVER_CONFIG_HPP
#define LOGIN_SERVER_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace LoginServerConfig
{
	// --- Transport constants (must match the dummy client; from CommonProtocol.h) ---
	// Framing magic byte and obfuscation fixed key injected into NetServer.
	// Client-facing only; the SS listener is a plaintext Server on its own port.
	inline constexpr std::uint8_t PROTOCOL_CODE = 0x77; // TODO: confirm vs original spec/dummy
	inline constexpr std::uint8_t FIXED_KEY = 0x32; // TODO: confirm vs original spec/dummy

	// --- Shard worker pool ---
	// hash(AccountNo) % WORKER_COUNT -> worker. Tuning knob (design notes 2.3/3).
	inline constexpr unsigned int WORKER_COUNT = 4;

	// --- Packet-pool slot size ---
	// Largest packet the login server allocates is LOGIN_RES_LOGIN:
	// 2 + 8 + 1 + 40 + 40 + 32 + 2 + 32 + 2 = 159 Byte.
	inline constexpr std::size_t MAX_PACKET_PAYLOAD = 159;

	// --- MySQL (accountdb) ---
	inline constexpr std::string_view DB_HOST = "127.0.0.1";
	inline constexpr std::uint16_t DB_PORT = 3306;
	inline constexpr std::string_view DB_USER = ""; // filled locally; blank-committed
	inline constexpr std::string_view DB_PASSWORD = ""; // filled locally; blank-committed
	inline constexpr std::string_view DB_NAME = "accountdb";

	// --- Redis (Memurai) ---
	inline constexpr std::string_view REDIS_HOST = "127.0.0.1";
	inline constexpr std::uint16_t REDIS_PORT = 6379;

	// --- SS listener (ContentLink) ---
	// Largest SS payload is SS_REGISTER (36 Byte).
	inline constexpr std::uint16_t SS_PORT = 10501;
	inline constexpr std::size_t SS_MAX_PACKET_PAYLOAD = 36;
	inline constexpr std::uint32_t SS_MAX_SESSIONS = 8;

	// --- Serviced content instances ---
	inline constexpr std::string_view CHAT_INSTANCE_NAME = "chat";
	inline constexpr std::string_view GAME_INSTANCE_NAME = "game";

	// --- Redis key sections ---
	// Key format: <section>:<instance>:<account_no>
	inline constexpr std::string_view TICKET_SECTION = "ticket";
	inline constexpr std::string_view SESSION_SECTION = "session";

	// --- Auth ticket lifetime ---
	inline constexpr std::uint32_t TICKET_TTL_SEC = 10;

	// --- Client connection timeout ---
	// A client connects, sends LOGIN_REQ_LOGIN, gets its response, and leaves.
	// Absolute lifetime from connect (not idle-based): any client still around
	// after this is stale and gets swept. SS connections live on a separate
	// server and are never swept.
	inline constexpr std::int64_t CLIENT_TIMEOUT_MS = 10'000;
	inline constexpr std::int64_t TIMEOUT_CHECK_INTERVAL_MS = 1'000;
}

#endif
