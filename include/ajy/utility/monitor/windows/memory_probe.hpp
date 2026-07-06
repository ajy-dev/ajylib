/**
 * File: memory_probe.hpp
 * Path: ajylib/include/ajy/utility/monitor/windows/memory_probe.hpp
 * Description:
 *	Windows memory-usage probe declarations.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_UTILITY_MONITOR_WINDOWS_MEMORY_PROBE_HPP
#define AJY_UTILITY_MONITOR_WINDOWS_MEMORY_PROBE_HPP

#include <ajy/utility/monitor/probe.hpp>

#include <atomic>
#include <string_view>

namespace ajy::utility::monitor::windows
{
	class ProcessPrivateMemoryProbe : public Probe
	{
	public:
		explicit ProcessPrivateMemoryProbe(std::string_view name, void *process_handle = nullptr);
		~ProcessPrivateMemoryProbe(void) noexcept override;

		ProcessPrivateMemoryProbe(const ProcessPrivateMemoryProbe &other) = delete;
		ProcessPrivateMemoryProbe &operator=(const ProcessPrivateMemoryProbe &other) = delete;
		ProcessPrivateMemoryProbe(ProcessPrivateMemoryProbe &&other) = delete;
		ProcessPrivateMemoryProbe &operator=(ProcessPrivateMemoryProbe &&other) = delete;

		void update(void) noexcept override;
		double get_value(void) const noexcept override;

	private:
		void *process;
		std::atomic<double> last_value;
	};

	class SystemAvailableMemoryProbe : public Probe
	{
	public:
		explicit SystemAvailableMemoryProbe(std::string_view name);
		~SystemAvailableMemoryProbe(void) noexcept override;

		SystemAvailableMemoryProbe(const SystemAvailableMemoryProbe &other) = delete;
		SystemAvailableMemoryProbe &operator=(const SystemAvailableMemoryProbe &other) = delete;
		SystemAvailableMemoryProbe(SystemAvailableMemoryProbe &&other) = delete;
		SystemAvailableMemoryProbe &operator=(SystemAvailableMemoryProbe &&other) = delete;

		void update(void) noexcept override;
		double get_value(void) const noexcept override;

	private:
		std::atomic<double> last_value;
	};

	class SystemNonpagedMemoryProbe : public Probe
	{
	public:
		explicit SystemNonpagedMemoryProbe(std::string_view name);
		~SystemNonpagedMemoryProbe(void) noexcept override;

		SystemNonpagedMemoryProbe(const SystemNonpagedMemoryProbe &other) = delete;
		SystemNonpagedMemoryProbe &operator=(const SystemNonpagedMemoryProbe &other) = delete;
		SystemNonpagedMemoryProbe(SystemNonpagedMemoryProbe &&other) = delete;
		SystemNonpagedMemoryProbe &operator=(SystemNonpagedMemoryProbe &&other) = delete;

		void update(void) noexcept override;
		double get_value(void) const noexcept override;

	private:
		std::atomic<double> last_value;
	};
}

#endif
