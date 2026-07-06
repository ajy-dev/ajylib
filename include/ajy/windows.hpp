/**
 * File: windows.hpp
 * Path: ajylib/include/ajy/windows.hpp
 * Description:
 *	A centralized Windows API include header.
 * Author: ajy-dev
 * Created: 2026-07-01
 * Updated: Never
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
#include <Psapi.h>
// clang-format on

#endif
