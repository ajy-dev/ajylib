/**
 * File: chat_server_config.hpp
 * Path: ajylib/examples/chat_server/chat_server_config.hpp
 * Description:
 *	Configuration constants shared by the chat server examples: the
 *	transport constants (framing code, fixed key) that must match the
 *	client, plus world/server settings (sector grid, area-of-interest
 *	range, heartbeat timing). Application packet formats live in
 *	protocol.hpp, not here.
 * Author: ajy-dev
 * Created: 2026-07-06
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef CHAT_SERVER_CONFIG_HPP
#define CHAT_SERVER_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <limits>

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
}

#endif
