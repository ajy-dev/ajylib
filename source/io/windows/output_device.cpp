/**
 * File: output_device.cpp
 * Path: ajylib/source/io/windows/output_device.cpp
 * Description:
 * 	An OutputDevice implementation using the Windows API definition.
 * Author: ajy-dev
 * Created: 2026-06-22
 * Updated: Never
 * Version: 0.1.0
 */

#define NOMINMAX
#include <Windows.h>

#include <ajy/io/windows/output_device.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>

namespace ajy::io::windows
{
	OutputDevice::OutputDevice(Target target) noexcept
		: target(target)
		, handle(INVALID_HANDLE_VALUE)
	{
	}

	OutputDevice::OutputDevice(const std::filesystem::path &filepath) noexcept
		: target(Target::FILE)
		, filepath(filepath)
		, handle(INVALID_HANDLE_VALUE)
	{
	}

	OutputDevice::~OutputDevice(void) noexcept
	{
		this->close();
	}

	bool OutputDevice::open(void) noexcept
	{
		switch (this->target)
		{
		case Target::STDOUT:
			this->handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
			return this->handle != INVALID_HANDLE_VALUE;
		case Target::STDERR:
			this->handle = ::GetStdHandle(STD_ERROR_HANDLE);
			return this->handle != INVALID_HANDLE_VALUE;
		case Target::FILE:
			if (this->filepath.empty())
				return false;
			this->handle = ::CreateFileW(
				this->filepath.wstring().c_str(),
				FILE_APPEND_DATA,
				FILE_SHARE_READ,
				NULL,
				OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
			return this->handle != INVALID_HANDLE_VALUE;
		}
		return false;
	}

	void OutputDevice::close(void) noexcept
	{
		if (this->target != Target::FILE)
			return;

		HANDLE handle = static_cast<HANDLE>(this->handle);
		if (handle && handle != INVALID_HANDLE_VALUE)
			::CloseHandle(handle);

		this->handle = INVALID_HANDLE_VALUE;
	}

	bool OutputDevice::flush(void) noexcept
	{
		HANDLE handle;

		handle = static_cast<HANDLE>(this->handle);
		if (!handle || handle == INVALID_HANDLE_VALUE)
			return false;

		return ::FlushFileBuffers(handle) != 0;
	}

	std::size_t OutputDevice::write(const char *buffer, std::size_t count) noexcept
	{
		HANDLE handle;
		std::size_t bytes_done;

		if (!buffer || !count)
			return 0;

		handle = static_cast<HANDLE>(this->handle);
		if (!handle || handle == INVALID_HANDLE_VALUE)
			return 0;

		bytes_done = 0;
		while (count)
		{
			DWORD chunk;
			DWORD bytes_written;

			chunk = static_cast<DWORD>(std::min<std::size_t>(count, std::numeric_limits<DWORD>::max()));
			if (!::WriteFile(handle, buffer + bytes_done, chunk, &bytes_written, NULL))
				break;
			if (!bytes_written)
				break;
			count -= static_cast<std::size_t>(bytes_written);
			bytes_done += static_cast<std::size_t>(bytes_written);
		}
		return bytes_done;
	}
}
