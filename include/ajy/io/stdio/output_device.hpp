/**
 * File: output_device.hpp
 * Path: ajylib/include/ajy/io/stdio/output_device.hpp
 * Description:
 * 	An OutputDevice implementation using the C standard I/O library.
 * Author: ajy-dev
 * Created: 2026-06-22
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_IO_STDIO_OUTPUT_DEVICE_HPP
#define AJY_IO_STDIO_OUTPUT_DEVICE_HPP

#include <ajy/io/output_device.hpp>

#include <cstddef>
#include <cstdio>
#include <filesystem>

namespace ajy::io::stdio
{
	class OutputDevice : public io::OutputDevice
	{
	public:
		enum class Target
		{
			STDOUT,
			STDERR,
			FILE
		};
		
		explicit OutputDevice(Target target = Target::STDERR) noexcept;
		explicit OutputDevice(const std::filesystem::path &filepath) noexcept;
		~OutputDevice(void) noexcept override;

		OutputDevice(const OutputDevice &other) = delete;
		OutputDevice &operator=(const OutputDevice &other) = delete;
		OutputDevice(OutputDevice &&other) = delete;
		OutputDevice &operator=(OutputDevice &&other) = delete;

		bool open(void) noexcept override;
		void close(void) noexcept override;
		bool flush(void) noexcept override;
		std::size_t write(const char *buffer, std::size_t count) noexcept override;

	private:
		Target target;
		std::filesystem::path filepath;
		std::FILE *fp;
	};
}

#endif
