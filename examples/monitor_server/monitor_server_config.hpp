/**
 * File: monitor_server_config.hpp
 * Path: ajylib/examples/monitor_server/monitor_server_config.hpp
 * Description:
 *	Transport constants (code, fixed key, tool login key), this monitor's
 *	identity, and the aggregation timing for the monitor server example.
 *	Wire packet formats live in protocol.hpp, not here.
 * Note:
 *	One NetServer serves both the SS (reporter) and CS (tool) roles, so a
 *	single code/fixed_key pair is shared; the tool's client config must be
 *	set to match.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef MONITOR_SERVER_CONFIG_HPP
#define MONITOR_SERVER_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace MonitorServerConfig
{
	// --- Transport constants (our choice; tool client config must match) ---
	// Distinct from the chat server's 0x77/0x32 so a client wired to the
	// wrong port fails at the framing/checksum gate rather than the app layer.
	inline constexpr std::uint8_t PROTOCOL_CODE = 0x88;
	inline constexpr std::uint8_t FIXED_KEY = 0x64;

	// Expected tool login key. ASCII, null-terminated within the 32-byte
	// login_session_key field on CS_MONITOR_TOOL_REQ_LOGIN; compared as a
	// C string. Mismatch -> ToolLoginStatus::ERR_SESSION_KEY.
	inline constexpr std::string_view TOOL_LOGIN_KEY = "ajy-monitor-tool-key";
	inline constexpr std::size_t TOOL_LOGIN_KEY_FIELD_SIZE = 32;

	// --- Identity ---
	// This monitor's own host, reported under the 40-44 local-sample metrics.
	// Each reporting server registers its own distinct ServerNo on its side
	// (e.g. the chat server picks another number).
	inline constexpr std::int32_t SELF_SERVER_NO = 1;

	// --- Timing ---
	// Local host sampling cadence for 40-44 (the SS reporters also send at 1 s).
	inline constexpr std::int64_t SAMPLE_INTERVAL_MS = 1'000;
	// DB aggregation flush: roll each (serverno, datatype) accumulator into
	// one avg/min/max row every 60 s.
	inline constexpr std::int64_t DB_FLUSH_INTERVAL_MS = 60'000;
	// Content-loop wake cadence: how often the content thread checks the
	// flush and timeout deadlines when no job wakes it.
	inline constexpr std::int64_t CONTENT_CHECK_INTERVAL_MS = 1'000;

	// Role-aware session timeout thresholds (MONITORING_TOOL is exempt).
	inline constexpr std::int64_t UNASSIGNED_TIMEOUT_MS = 5'000;
	inline constexpr std::int64_t REPORTER_TIMEOUT_MS = 20'000;

	// --- Database (MySQL) ---
	// Local, offline DB. USER/PASSWORD are intentionally left blank in the
	// committed source; fill them in locally to run. A .gitignore'd credentials
	// file is the planned refactor.
	inline constexpr std::string_view DB_HOST = "127.0.0.1";
	inline constexpr std::uint16_t DB_PORT = 3306;
	inline constexpr std::string_view DB_USER = "";
	inline constexpr std::string_view DB_PASSWORD = "";
	inline constexpr std::string_view DB_NAME = "logdb";
}

#endif
