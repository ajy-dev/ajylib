/**
 * File: output_device.hpp
 * Path: ajylib/include/ajy/io/windows/output_device.hpp
 * Description:
 * 	An OutputDevice implementation using the Windows API.
 * Note:
 * 	Windows.h is not included in this header.
 * 	HANDLE is stored as void * and cast in the implementation.
 * Author: ajy-dev
 * Created: 2026-06-22
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_IO_WINDOWS_OUTPUT_DEVICE_HPP
#define AJY_IO_WINDOWS_OUTPUT_DEVICE_HPP

#include <ajy/io/output_device.hpp>

#include <cstddef>
#include <filesystem>

namespace ajy::io::windows
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
		void *handle;
	};
}

#endif
