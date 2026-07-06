/**
 * File: cpu_probe.hpp
 * Path: ajylib/include/ajy/utility/monitor/windows/cpu_probe.hpp
 * Description:
 *	Windows CPU-usage probe declarations.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_UTILITY_MONITOR_WINDOWS_CPU_PROBE_HPP
#define AJY_UTILITY_MONITOR_WINDOWS_CPU_PROBE_HPP

#include <ajy/utility/monitor/probe.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string_view>

namespace ajy::utility::monitor::windows
{
	class SystemCpuProbe : public Probe
	{
	public:
		explicit SystemCpuProbe(std::string_view name);
		~SystemCpuProbe(void) noexcept override;

		SystemCpuProbe(const SystemCpuProbe &other) = delete;
		SystemCpuProbe &operator=(const SystemCpuProbe &other) = delete;
		SystemCpuProbe(SystemCpuProbe &&other) = delete;
		SystemCpuProbe &operator=(SystemCpuProbe &&other) = delete;

		void update(void) noexcept override;
		double get_value(void) const noexcept override;

	private:
		std::atomic<double> last_value;
		std::uint64_t prev_idle_cpu_time;
		std::uint64_t prev_kernel_cpu_time;
		std::uint64_t prev_user_cpu_time;
	};

	class ProcessCpuProbe : public Probe
	{
	public:
		explicit ProcessCpuProbe(std::string_view name, void *process_handle = nullptr);
		~ProcessCpuProbe(void) noexcept override;

		ProcessCpuProbe(const ProcessCpuProbe &other) = delete;
		ProcessCpuProbe &operator=(const ProcessCpuProbe &other) = delete;
		ProcessCpuProbe(ProcessCpuProbe &&other) = delete;
		ProcessCpuProbe &operator=(ProcessCpuProbe &&other) = delete;

		void update(void) noexcept override;
		double get_value(void) const noexcept override;

	private:
		void *process;
		int processor_count;
		std::atomic<double> last_value;
		std::uint64_t prev_kernel_cpu_time;
		std::uint64_t prev_user_cpu_time;
		MonitorClock::time_point prev_sample_point;
	};
}

#endif
