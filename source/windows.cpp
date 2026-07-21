/**
 * File: windows.cpp
 * Path: ajylib/source/windows.cpp
 * Description:
 *	Definition of the Windows/Winsock helpers declared in windows.hpp.
 * Author: ajy-dev
 * Created: 2026-07-21
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/windows.hpp>

#include <cstdio>
#include <cstring>

namespace ajy
{
	namespace
	{
		// Allocates the message buffer via FormatMessageA (nullptr on failure).
		// The caller owns it and must LocalFree it.
		LPSTR format_winapi_error(DWORD error_code) noexcept
		{
			LPSTR buffer;

			buffer = nullptr;
			::FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				error_code,
				MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
				reinterpret_cast<LPSTR>(&buffer),
				0,
				NULL);

			if (buffer)
				buffer[std::strcspn(buffer, "\r\n")] = '\0';

			return buffer;
		}
	}

	void log_winapi_error(const char *func_name, DWORD error_code, utility::Logger *logger, utility::Logger::LogLevel level) noexcept
	{
		LPSTR buffer;

		buffer = format_winapi_error(error_code);
		if (buffer)
			logger->log(level, "%s: %s", func_name, buffer);
		else
			logger->log(level, "%s: Unknown WinAPI error (Code: %lu)", func_name, error_code);

		::LocalFree(buffer);
	}

	void log_winapi_error(const char *func_name, DWORD error_code) noexcept
	{
		LPSTR buffer;

		buffer = format_winapi_error(error_code);
		if (buffer)
			std::fprintf(stderr, "%s: %s\n", func_name, buffer);
		else
			std::fprintf(stderr, "%s: Unknown WinAPI error (Code: %lu)\n", func_name, error_code);

		::LocalFree(buffer);
	}
}
