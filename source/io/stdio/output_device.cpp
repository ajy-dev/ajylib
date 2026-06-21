/**
 * File: output_device.cpp
 * Path: ajylib/source/io/stdio/output_device.cpp
 * Description:
 * 	An OutputDevice implementation using the C standard I/O library definition.
 * Author: ajy-dev
 * Created: 2026-06-22
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/io/stdio/output_device.hpp>

#include <cstdio>

namespace ajy::io::stdio
{
	OutputDevice::OutputDevice(Target target) noexcept
		: target(target)
		, fp(nullptr)
	{
	}

	OutputDevice::OutputDevice(const std::filesystem::path &filepath) noexcept
		: target(Target::FILE)
		, filepath(filepath)
		, fp(nullptr)
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
			this->fp = stdout;
			return true;
		case Target::STDERR:
			this->fp = stderr;
			return true;
		case Target::FILE:
			if (this->filepath.empty())
				return false;
			this->fp = std::fopen(this->filepath.string().c_str(), "ab");
			return this->fp != nullptr;
		}
		return false;
	}

	void OutputDevice::close(void) noexcept
	{
		if (this->target != Target::FILE || !this->fp)
			return;

		std::fclose(this->fp);
		this->fp = nullptr;
	}

	bool OutputDevice::flush(void) noexcept
	{
		if (!this->fp)
			return false;

		return std::fflush(this->fp) == 0;
	}

	std::size_t OutputDevice::write(const char *buffer, std::size_t count) noexcept
	{
		if (!buffer || !count || !this->fp)
			return 0;

		return std::fwrite(buffer, sizeof(char), count, this->fp);
	}
}
