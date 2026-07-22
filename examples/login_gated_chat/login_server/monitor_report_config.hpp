/**
 * File: monitor_report_config.hpp
 * Path: ajylib/examples/login_gated_chat/login_server/monitor_report_config.hpp
 * Description:
 *	Constants for the login server's monitoring report path (SS reporter):
 *	transport code/fixed_key (must match the monitor server), the monitor
 *	endpoint, this server's ServerNo, the report cadence, and the mirrored
 *	SS packet-type and login DataType enums.
 * Note:
 *	examples are independent, so the SS-side wire values are mirrored here
 *	rather than shared with the monitor server's protocol.hpp. The transport
 *	code/fixed_key must equal the monitor server's PROTOCOL_CODE/FIXED_KEY,
 *	or every packet fails the framing/checksum gate.
 * Author: ajy-dev
 * Created: 2026-07-23
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef LOGIN_SERVER_MONITOR_REPORT_CONFIG_HPP
#define LOGIN_SERVER_MONITOR_REPORT_CONFIG_HPP

#include <cstdint>
#include <string_view>

namespace MonitorReportConfig
{
	// --- Transport (must match the monitor server's NetServer) ---
	inline constexpr std::uint8_t MONITOR_CODE = 0x88;
	inline constexpr std::uint8_t MONITOR_FIXED_KEY = 0x64;

	// --- Monitor server endpoint this login server reports to ---
	inline constexpr std::string_view MONITOR_HOST = "127.0.0.1";
	inline constexpr std::uint16_t MONITOR_PORT = 10442;

	// --- Identity ---
	// This login server's ServerNo on the SS login, distinct from the monitor's
	// own SELF_SERVER_NO (1) and the chat server's (2).
	inline constexpr std::int32_t LOGIN_SERVER_NO = 3;

	// --- Timing ---
	// Report cadence; also the Monitor::update() cadence of the reporter loop.
	inline constexpr std::int64_t REPORT_INTERVAL_MS = 1'000;

	// --- SS packet family (mirror of the monitor server's protocol.hpp) ---
	enum class MonitorPacketType : std::uint16_t
	{
		SS_MONITOR_LOGIN = 20001,  // { u16 type; int32 server_no; }
		SS_MONITOR_DATA_UPDATE = 20002, // { u16 type; u8 data_type; int32 data_value; int32 time_stamp; }
	};

	// --- Login DataType (1-6), values identical to the monitor server enum ---
	enum class MonitorDataType : std::uint8_t
	{
		LOGIN_SERVER_RUN = 1,  // ON/OFF, constant 1
		LOGIN_SERVER_CPU = 2,  // own-process CPU %
		LOGIN_SERVER_MEM = 3,  // own-process memory, MByte
		LOGIN_SESSION = 4,  // session (connection) count
		LOGIN_AUTH_TPS = 5,  // auth TPS (all processed logins)
		LOGIN_PACKET_POOL = 6,  // packet-pool in-use count
	};
}

#endif
