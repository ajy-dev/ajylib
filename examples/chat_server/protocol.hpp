/**
 * File: protocol.hpp
 * Path: ajylib/examples/chat_server/protocol.hpp
 * Description:
 *	Application-level chat protocol shared by the chat server examples.
 *	Packet type identifiers; each value carries its payload layout as a
 *	struct-shaped comment. The transport code/fixed_key are injected by
 *	the server class, not defined here.
 * Note:
 *	Each payload begins with a PacketType Type field, followed by the
 *	fields shown. All integers little-endian, packed (no padding). The
 *	original protocol type of each field is noted in a trailing comment
 *	(WORD, INT64, BYTE, WCHAR, char). WCHAR is UTF-16LE. Servers relay
 *	ID/Nickname/Message as opaque byte blocks; only numeric fields are read.
 * Author: ajy-dev
 * Created: 2026-07-06
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef CHAT_SERVER_PROTOCOL_HPP
#define CHAT_SERVER_PROTOCOL_HPP

#include <cstdint>

enum class PacketType : std::uint16_t
{
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
	REQ_LOGIN = 1,

	/*
	 * S -> C : login response (11 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::uint8_t status;		// (BYTE, 0: fail / 1: success)
	 *	std::int64_t account_no;	// (INT64)
	 * }
	 */
	RES_LOGIN = 2,

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
	REQ_SECTOR_MOVE = 3,

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
	RES_SECTOR_MOVE = 4,

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
	REQ_MESSAGE = 5,

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
	RES_MESSAGE = 6,

	/*
	 * C -> S : heartbeat (2 Byte) — no server response
	 * struct
	 * {
	 *	PacketType type;
	 * }
	 */
	REQ_HEARTBEAT = 7,
};

#endif
