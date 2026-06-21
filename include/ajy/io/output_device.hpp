/**
 * File: output_device.hpp
 * Path: ajylib/include/ajy/io/output_device.hpp
 * Description:
 * 	A pure abstract byte-stream output interface declaration.
 * Author: ajy-dev
 * Created: 2026-06-22
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_IO_OUTPUT_DEVICE_HPP
#define AJY_IO_OUTPUT_DEVICE_HPP

#include <cstddef>

namespace ajy::io
{
	class OutputDevice
	{
	public:
		OutputDevice(void) = default;
		virtual ~OutputDevice(void) = default;

		OutputDevice(const OutputDevice &other) = delete;
		OutputDevice &operator=(const OutputDevice &other) = delete;
		OutputDevice(OutputDevice &&other) = delete;
		OutputDevice &operator=(OutputDevice &&other) = delete;

		virtual bool open(void) noexcept = 0;
		virtual void close(void) noexcept = 0;
		virtual bool flush(void) noexcept = 0;
		virtual std::size_t write(const char *buffer, std::size_t count) noexcept = 0;
	};
}

#endif
