/**
 * File: protocol.hpp
 * Path: ajylib/examples/monitor_server/protocol.hpp
 * Description:
 *	Application-level monitoring protocol for the monitor server example:
 *	the SS (reporter -> monitor) and CS (tool <-> monitor) packet families,
 *	plus the wire DataType and login-status enums. The transport
 *	code/fixed_key are injected by the server class, not defined here.
 * Note:
 *	Each payload begins with a Type field; all integers little-endian,
 *	packed. ServerNo is INT on the SS login but narrows to BYTE on the tool
 *	update. DataValue and TimeStamp are INT, scaled and stamped by the
 *	reporter. DataType spans several server families; this project only
 *	emits the CHAT (30-37) and MONITOR host (40-44) values, the rest are
 *	here for wire completeness.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: 2026-07-18
 * Version: 0.1.0
 */

#ifndef MONITOR_SERVER_PROTOCOL_HPP
#define MONITOR_SERVER_PROTOCOL_HPP

#include <cstdint>

enum class MonitorPacketType : std::uint16_t
{
	// --- SS: reporting server -> monitor (family base 20000) ---

	/*
	 * SS : reporting server -> monitor, login/register (6 Byte)
	 * struct
	 * {
	 *	MonitorPacketType type;
	 *	std::int32_t server_no;			// (INT, per-server unique id)
	 * }
	 */
	SS_MONITOR_LOGIN = 20001,

	/*
	 * SS : reporting server -> monitor, one data point (11 Byte)
	 *	Sent every 1 s. No server response.
	 * struct
	 * {
	 *	MonitorPacketType type;
	 *	std::uint8_t data_type;			// (BYTE, MonitorDataType)
	 *	std::int32_t data_value;		// (INT)
	 *	std::int32_t time_stamp;		// (INT, time() cast to int, valid until 2038)
	 * }
	 */
	SS_MONITOR_DATA_UPDATE = 20002,

	// --- CS: monitor <-> tool (family base 25000) ---

	/*
	 * CS : tool -> monitor, login request (34 Byte)
	 * struct
	 * {
	 *	MonitorPacketType type;
	 *	std::uint8_t login_session_key[32];	// (char[32], fixed key held by the monitor)
	 * }
	 */
	CS_MONITOR_TOOL_REQ_LOGIN = 25001,

	/*
	 * CS : monitor -> tool, login response (3 Byte)
	 * struct
	 * {
	 *	MonitorPacketType type;
	 *	std::uint8_t status;			// (BYTE, ToolLoginStatus)
	 * }
	 */
	CS_MONITOR_TOOL_RES_LOGIN = 25002,

	/*
	 * CS : monitor -> tool, one data point, real-time relay (12 Byte)
	 *	Every collected point is relayed to every tool, individually.
	 * struct
	 * {
	 *	MonitorPacketType type;
	 *	std::uint8_t server_no;			// (BYTE, narrowed from SS INT)
	 *	std::uint8_t data_type;			// (BYTE, MonitorDataType)
	 *	std::int32_t data_value;		// (INT)
	 *	std::int32_t time_stamp;		// (INT, time() cast to int, valid until 2038)
	 * }
	 */
	CS_MONITOR_TOOL_DATA_UPDATE = 25003,
};

enum class ToolLoginStatus : std::uint8_t
{
	OK = 1,	// login success
	ERR_NO_SERVER = 2, // server-name mismatch
	ERR_SESSION_KEY = 3, // login session key mismatch
};

enum class MonitorDataType : std::uint8_t
{
	// --- Login server (1-6) --- (not emitted by this project; wire completeness)
	LOGIN_SERVER_RUN = 1,  // ON/OFF
	LOGIN_SERVER_CPU = 2,  // CPU %
	LOGIN_SERVER_MEM = 3,  // memory, MByte
	LOGIN_SESSION = 4,  // session (connection) count
	LOGIN_AUTH_TPS = 5,  // auth TPS
	LOGIN_PACKET_POOL = 6,  // packet-pool usage

	// --- Game server (10-23) --- (not emitted by this project; wire completeness)
	GAME_SERVER_RUN = 10,  // ON/OFF
	GAME_SERVER_CPU = 11,  // CPU %
	GAME_SERVER_MEM = 12,  // memory, MByte
	GAME_SESSION = 13,  // session (connection) count
	GAME_AUTH_PLAYER = 14,  // AUTH-mode player count
	GAME_GAME_PLAYER = 15,  // GAME-mode player count
	GAME_ACCEPT_TPS = 16,  // accept TPS
	GAME_PACKET_RECV_TPS = 17, // packet-recv TPS
	GAME_PACKET_SEND_TPS = 18, // packet-send TPS
	GAME_DB_WRITE_TPS = 19,	// DB-write TPS
	GAME_DB_WRITE_MSG = 20,	// DB-write queue depth (remaining)
	GAME_AUTH_THREAD_FPS = 21, // AUTH-thread loop FPS
	GAME_GAME_THREAD_FPS = 22, // GAME-thread loop FPS
	GAME_PACKET_POOL = 23,  // packet-pool usage

	// --- Chat server (30-37) --- IN SCOPE: chat server reports these
	CHAT_SERVER_RUN = 30,  // ON/OFF, constant 1 (report itself is the ON signal)
	CHAT_SERVER_CPU = 31,  // own-process CPU %
	CHAT_SERVER_MEM = 32,  // own-process memory, MByte
	CHAT_SESSION = 33,  // session (connection) count
	CHAT_PLAYER = 34,  // logged-in (authenticated) count
	CHAT_UPDATE_TPS = 35,  // content(UPDATE)-thread TPS
	CHAT_PACKET_POOL = 36,  // packet-pool usage
	CHAT_UPDATEMSG_POOL = 37, // UPDATE-MSG(Job)-pool usage

	// --- Server computer (40-44) --- IN SCOPE: monitor samples its own host
	MONITOR_CPU_TOTAL = 40,	// whole-processor CPU %
	MONITOR_NONPAGED_MEMORY = 41, // nonpaged pool, MByte
	MONITOR_NETWORK_RECV = 42, // network recv, KByte
	MONITOR_NETWORK_SEND = 43, // network send, KByte
	MONITOR_AVAILABLE_MEMORY = 44, // available memory, MByte
};

#endif
