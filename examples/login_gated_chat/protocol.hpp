/**
 * File: protocol.hpp
 * Path: ajylib/examples/login_gated_chat/protocol.hpp
 * Description:
 *	Shared packet types for the login_gated_chat example
 * Note:
 *	All integers little-endian, packed. CS packets follow an external spec and
 *	note each field's original type in a trailing comment; WCHAR is UTF-16LE.
 *	SS packets are defined by this example and use fixed-width types.
 * Author: ajy-dev
 * Created: 2026-07-22
 * Updated: 2026-08-07
 * Version: 0.1.0
 */

#ifndef LOGIN_GATED_CHAT_PROTOCOL_HPP
#define LOGIN_GATED_CHAT_PROTOCOL_HPP

#include <cstdint>

enum class PacketType : std::uint16_t
{
	// ---- Chat CS (base en_PACKET_CS_CHAT_SERVER = 0) ----

	/*
	 * C -> S : login request (154 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::int64_t account_no;	// (INT64)
	 *	std::uint16_t id[20];		// (WCHAR[20], 40 Byte, null-terminated)
	 *	std::uint16_t nickname[20];	// (WCHAR[20], 40 Byte, null-terminated)
	 *	std::uint8_t session_key[64];	// (char[64], auth token)
	 * }
	 */
	CHAT_REQ_LOGIN = 1,

	/*
	 * S -> C : login response (11 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::uint8_t status;		// (BYTE, 0: fail / 1: success)
	 *	std::int64_t account_no;	// (INT64)
	 * }
	 */
	CHAT_RES_LOGIN = 2,

	/*
	 * C -> S : sector move request (14 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::int64_t account_no;	// (INT64)
	 *	std::uint16_t sector_x;		// (WORD)
	 *	std::uint16_t sector_y;		// (WORD)
	 * }
	 */
	CHAT_REQ_SECTOR_MOVE = 3,

	/*
	 * S -> C : sector move response (14 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::int64_t account_no;	// (INT64)
	 *	std::uint16_t sector_x;		// (WORD)
	 *	std::uint16_t sector_y;		// (WORD)
	 * }
	 */
	CHAT_RES_SECTOR_MOVE = 4,

	/*
	 * C -> S : chat message request (12 + MessageLen Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::int64_t account_no;	// (INT64)
	 *	std::uint16_t message_len;	// (WORD, byte length of Message)
	 *	std::uint16_t message[message_len / 2];	// (WCHAR[], not null-terminated)
	 * }
	 */
	CHAT_REQ_MESSAGE = 5,

	/*
	 * S -> C : chat message, broadcast (92 + MessageLen Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::int64_t account_no;	// (INT64)
	 *	std::uint16_t id[20];		// (WCHAR[20], 40 Byte, null-terminated)
	 *	std::uint16_t nickname[20];	// (WCHAR[20], 40 Byte, null-terminated)
	 *	std::uint16_t message_len;	// (WORD, byte length of Message)
	 *	std::uint16_t message[message_len / 2];	// (WCHAR[], not null-terminated)
	 * }
	 */
	CHAT_RES_MESSAGE = 6,

	/*
	 * C -> S : heartbeat (2 Byte) — no server response
	 * struct
	 * {
	 *	PacketType type;
	 * }
	 */
	CHAT_REQ_HEARTBEAT = 7,

	// ---- Login CS (base en_PACKET_CS_LOGIN_SERVER = 100) ----

	/*
	 * C -> S : login request (74 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::int64_t account_no;	// (INT64)
	 *	std::uint8_t session_key[64];	// (char[64], token issued by an external API)
	 * }
	 */
	LOGIN_REQ_LOGIN = 101,

	/*
	 * S -> C : login response (159 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::int64_t account_no;	// (INT64)
	 *	std::int8_t status;		// (BYTE, LoginStatus)
	 *	std::uint16_t id[20];		// (WCHAR[20], 40 Byte, from DB, null-terminated)
	 *	std::uint16_t nickname[20];	// (WCHAR[20], 40 Byte, from DB, null-terminated)
	 *	std::uint16_t game_server_ip[16];	// (WCHAR[16], 32 Byte, from SS_REGISTER)
	 *	std::uint16_t game_server_port;	// (USHORT)
	 *	std::uint16_t chat_server_ip[16];	// (WCHAR[16], 32 Byte, from SS_REGISTER)
	 *	std::uint16_t chat_server_port;	// (USHORT)
	 * }
	 */
	LOGIN_RES_LOGIN = 102,

	// ---- SS: content server <-> login server (base 200) ----

	/*
	 * Content -> Login : registration (36 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	char server_name[16];		// instance name, null-terminated
	 *	char server_ip[16];		// listen IP, dotted IPv4, null-terminated
	 *	std::uint16_t server_port;	// listen port
	 * }
	 */
	SS_REGISTER = 201,

	/*
	 * Login -> Content : duplicate-login notification (10 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::int64_t account_no;	// disconnect this account if present
	 * }
	 */
	SS_NOTIFY_DISCONNECT = 202,
};

/*
 * LOGIN_RES_LOGIN Status field (BYTE). A separate enum from PacketType.
 * Only OK(1) / GAME(2) / ACCOUNT_MISS(3) / NOSERVER(6) are ever sent in this example;
 * the others are defensive (unreachable — see design notes 1.3).
 */
enum class LoginStatus : std::int8_t
{
	NONE = -1,  // unauthenticated
	FAIL = 0,  // session error
	OK = 1, // success
	GAME = 2,  // already in game (duplicate-login rejection)
	ACCOUNT_MISS = 3, // AccountNo absent from the account table
	SESSION_MISS = 4, // AccountNo absent from the sessionkey table (defensive)
	STATUS_MISS = 5, // AccountNo absent from the status table (defensive)
	NOSERVER = 6,  // no serviceable server
};

#endif
