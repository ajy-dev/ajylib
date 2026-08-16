/**
 * File: monitor_report_config.hpp
 * Path: ajylib/examples/echo_with_group_intent/monitor_report_config.hpp
 * Description:
 *	Constants for the game server's monitoring report path (SS reporter):
 *	transport code/fixed_key (must match the monitor server), the monitor
 *	endpoint, this server's ServerNo, the report cadence, and the mirrored
 *	SS packet-type and game DataType enums.
 * Note:
 *	examples are independent, so the SS-side wire values are mirrored here
 *	rather than shared with the monitor server's protocol.hpp. The transport
 *	code/fixed_key must equal the monitor server's PROTOCOL_CODE/FIXED_KEY,
 *	or every packet fails the framing/checksum gate.
 * Author: ajy-dev
 * Created: 2026-08-17
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_WITH_GROUP_INTENT_MONITOR_REPORT_CONFIG_HPP
#define ECHO_WITH_GROUP_INTENT_MONITOR_REPORT_CONFIG_HPP

#include <cstdint>
#include <string_view>

namespace MonitorReportConfig
{
	// --- Transport (must match the monitor server's NetServer) ---
	inline constexpr std::uint8_t MONITOR_CODE = 0x88;
	inline constexpr std::uint8_t MONITOR_FIXED_KEY = 0x64;

	// --- Monitor server endpoint this game server reports to ---
	inline constexpr std::string_view MONITOR_HOST = "127.0.0.1";
	inline constexpr std::uint16_t MONITOR_PORT = 10442;

	// --- Identity ---
	// This game server's ServerNo on the SS login, distinct from the monitor's
	// own SELF_SERVER_NO (1) and the chat server's (2).
	inline constexpr std::int32_t GAME_SERVER_NO = 3;

	// --- Timing ---
	// Report cadence; also the Monitor::update() cadence of the reporter loop.
	inline constexpr std::int64_t REPORT_INTERVAL_MS = 1'000;

	// --- SS packet family (mirror of the monitor server's protocol.hpp) ---
	enum class MonitorPacketType : std::uint16_t
	{
		SS_MONITOR_LOGIN = 20001,  // { u16 type; int32 server_no; }
		SS_MONITOR_DATA_UPDATE = 20002,	// { u16 type; u8 data_type; int32 data_value; int32 time_stamp; }
	};

	// --- Game DataType (10-23), values identical to the monitor server enum ---
	enum class MonitorDataType : std::uint8_t
	{
		GAME_SERVER_RUN = 10,        // ON/OFF, constant 1
		GAME_SERVER_CPU = 11,        // own-process CPU %
		GAME_SERVER_MEM = 12,        // own-process memory, MByte
		GAME_SESSION = 13,           // session (connection) count
		GAME_AUTH_PLAYER = 14,       // AUTH-group session count
		GAME_GAME_PLAYER = 15,       // ECHO-group session count
		GAME_ACCEPT_TPS = 16,        // accept TPS
		GAME_PACKET_RECV_TPS = 17,   // packet-recv TPS
		GAME_PACKET_SEND_TPS = 18,   // packet-send TPS
		GAME_DB_WRITE_TPS = 19,      // DB-write TPS, always 0 (no DB here)
		GAME_DB_WRITE_MSG = 20,      // DB-write queue depth, always 0 (no DB here)
		GAME_AUTH_THREAD_FPS = 21,   // AUTH-group loop FPS
		GAME_GAME_THREAD_FPS = 22,   // ECHO-group loop FPS
		GAME_PACKET_POOL = 23,       // packet-pool in-use count
	};
}

#endif
