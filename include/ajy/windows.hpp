/**
 * File: windows.hpp
 * Path: ajylib/include/ajy/windows.hpp
 * Description:
 *	A centralized Windows API include header with Windows/Winsock helpers.
 * Author: ajy-dev
 * Created: 2026-07-01
 * Updated: 2026-07-21
 * Version: 0.1.0
 */

#ifndef AJY_WINDOWS_HPP
#define AJY_WINDOWS_HPP

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
# define NOMINMAX
#endif

// clang-format off
#include <Winsock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <Psapi.h>
// clang-format on

#include <ajy/utility/logger.hpp>

namespace ajy
{
	void log_winapi_error(
		const char *func_name,
		DWORD error_code,
		utility::Logger *logger,
		utility::Logger::LogLevel level = utility::Logger::LogLevel::Error) noexcept;
	void log_winapi_error(const char *func_name, DWORD error_code) noexcept;
}

#endif
