/**
 * File: protocol.hpp
 * Path: ajylib/examples/echo_sendworker/protocol.hpp
 * Description:
 *	Shared packet types for the echo_sendworker example
 * Note:
 *	All integers little-endian, packed. The wire format follows an external
 *	dummy client, so each field's original type is noted in a trailing
 *	comment.
 * Author: ajy-dev
 * Created: 2026-08-14
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_SENDWORKER_PROTOCOL_HPP
#define ECHO_SENDWORKER_PROTOCOL_HPP

#include <cstdint>

enum class PacketType : std::uint16_t
{
	/*
	 * C -> S : login request (78 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::int64_t account_no;	// (INT64)
	 *	std::uint8_t session_key[64];	// (char[64], auth token)
	 *	std::int32_t version;		// (INT, unused by the server)
	 * }
	 */
	REQ_LOGIN = 1001,

	/*
	 * S -> C : login response (11 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::uint8_t status;		// (BYTE, 0: fail / 1: success)
	 *	std::int64_t account_no;	// (INT64)
	 * }
	 */
	RES_LOGIN = 1002,

	/*
	 * C -> S : echo request (18 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::int64_t account_no;	// (INT64)
	 *	std::int64_t send_time;		// (LONGLONG, client tick; echoed back as-is)
	 * }
	 */
	REQ_ECHO = 5000,

	/*
	 * S -> C : echo response (18 Byte)
	 * struct
	 * {
	 *	PacketType type;
	 *	std::int64_t account_no;	// (INT64)
	 *	std::int64_t send_time;		// (LONGLONG, copied from the request)
	 * }
	 */
	RES_ECHO = 5001,
};

/*
 * RES_LOGIN Status field (BYTE). A separate enum from PacketType.
 */
enum class LoginStatus : std::uint8_t
{
	FAIL = 0,
	OK = 1,
};

#endif
