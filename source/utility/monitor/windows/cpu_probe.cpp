/**
 * File: cpu_probe.cpp
 * Path: ajylib/source/utility/monitor/windows/cpu_probe.cpp
 * Description:
 *	Windows CPU-usage probe definitions.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/utility/monitor/windows/cpu_probe.hpp>
#include <ajy/windows.hpp>

#include <chrono>
#include <limits>

namespace ajy::utility::monitor::windows
{
	namespace
	{
		constexpr double HUNDRED_NANOSECONDS_PER_SECOND = 10'000'000.0;

		std::uint64_t to_uint64(const FILETIME &file_time) noexcept
		{
			ULARGE_INTEGER large;

			large.LowPart = file_time.dwLowDateTime;
			large.HighPart = file_time.dwHighDateTime;

			return large.QuadPart;
		}
	}

	SystemCpuProbe::SystemCpuProbe(std::string_view name)
		: Probe(name)
		, last_value(0.0)
		, prev_idle_cpu_time(0)
		, prev_kernel_cpu_time(0)
		, prev_user_cpu_time(0)
	{
		FILETIME idle_file_time;
		FILETIME kernel_file_time;
		FILETIME user_file_time;

		if (::GetSystemTimes(&idle_file_time, &kernel_file_time, &user_file_time))
		{
			this->prev_idle_cpu_time = to_uint64(idle_file_time);
			this->prev_kernel_cpu_time = to_uint64(kernel_file_time);
			this->prev_user_cpu_time = to_uint64(user_file_time);
		}
	}

	SystemCpuProbe::~SystemCpuProbe(void) noexcept
	{
	}

	void SystemCpuProbe::update(void) noexcept
	{
		FILETIME idle_file_time;
		FILETIME kernel_file_time;
		FILETIME user_file_time;
		std::uint64_t idle_cpu_time;
		std::uint64_t kernel_cpu_time;
		std::uint64_t user_cpu_time;
		std::uint64_t idle_delta;
		std::uint64_t kernel_delta;
		std::uint64_t user_delta;
		std::uint64_t total_delta;

		if (!::GetSystemTimes(&idle_file_time, &kernel_file_time, &user_file_time))
		{
			this->last_value.store(std::numeric_limits<double>::quiet_NaN(), std::memory_order_relaxed);

			return;
		}

		idle_cpu_time = to_uint64(idle_file_time);
		kernel_cpu_time = to_uint64(kernel_file_time);
		user_cpu_time = to_uint64(user_file_time);

		idle_delta = idle_cpu_time - this->prev_idle_cpu_time;
		kernel_delta = kernel_cpu_time - this->prev_kernel_cpu_time;
		user_delta = user_cpu_time - this->prev_user_cpu_time;
		total_delta = kernel_delta + user_delta;

		this->prev_idle_cpu_time = idle_cpu_time;
		this->prev_kernel_cpu_time = kernel_cpu_time;
		this->prev_user_cpu_time = user_cpu_time;

		if (total_delta == 0)
			return;

		this->last_value.store(static_cast<double>(total_delta - idle_delta) / static_cast<double>(total_delta) * 100.0, std::memory_order_relaxed);
	}

	double SystemCpuProbe::get_value(void) const noexcept
	{
		return this->last_value.load(std::memory_order_relaxed);
	}

	ProcessCpuProbe::ProcessCpuProbe(std::string_view name, void *process_handle)
		: Probe(name)
		, process(process_handle ? process_handle : ::GetCurrentProcess())
		, processor_count(0)
		, last_value(0.0)
		, prev_kernel_cpu_time(0)
		, prev_user_cpu_time(0)
		, prev_sample_point()
	{
		SYSTEM_INFO system_info;
		FILETIME creation_file_time;
		FILETIME exit_file_time;
		FILETIME kernel_file_time;
		FILETIME user_file_time;

		::GetSystemInfo(&system_info);
		this->processor_count = static_cast<int>(system_info.dwNumberOfProcessors);
		if (this->processor_count <= 0)
			this->processor_count = 1;

		if (::GetProcessTimes(reinterpret_cast<HANDLE>(this->process), &creation_file_time, &exit_file_time, &kernel_file_time, &user_file_time))
		{
			this->prev_kernel_cpu_time = to_uint64(kernel_file_time);
			this->prev_user_cpu_time = to_uint64(user_file_time);
			this->prev_sample_point = MonitorClock::now();
		}
	}

	ProcessCpuProbe::~ProcessCpuProbe(void) noexcept
	{
	}

	void ProcessCpuProbe::update(void) noexcept
	{
		FILETIME creation_file_time;
		FILETIME exit_file_time;
		FILETIME kernel_file_time;
		FILETIME user_file_time;
		MonitorClock::time_point sample_point;
		std::chrono::duration<double> elapsed;
		std::uint64_t kernel_cpu_time;
		std::uint64_t user_cpu_time;
		std::uint64_t kernel_delta;
		std::uint64_t user_delta;
		std::uint64_t busy_delta;
		double busy_seconds;

		if (!::GetProcessTimes(reinterpret_cast<HANDLE>(this->process), &creation_file_time, &exit_file_time, &kernel_file_time, &user_file_time))
		{
			this->last_value.store(std::numeric_limits<double>::quiet_NaN(), std::memory_order_relaxed);

			return;
		}

		sample_point = MonitorClock::now();

		kernel_cpu_time = to_uint64(kernel_file_time);
		user_cpu_time = to_uint64(user_file_time);

		kernel_delta = kernel_cpu_time - this->prev_kernel_cpu_time;
		user_delta = user_cpu_time - this->prev_user_cpu_time;
		busy_delta = kernel_delta + user_delta;
		elapsed = sample_point - this->prev_sample_point;

		this->prev_kernel_cpu_time = kernel_cpu_time;
		this->prev_user_cpu_time = user_cpu_time;
		this->prev_sample_point = sample_point;

		if (elapsed.count() <= 0.0)
			return;

		busy_seconds = static_cast<double>(busy_delta) / HUNDRED_NANOSECONDS_PER_SECOND;

		this->last_value.store(busy_seconds / static_cast<double>(this->processor_count) / elapsed.count() * 100.0, std::memory_order_relaxed);
	}

	double ProcessCpuProbe::get_value(void) const noexcept
	{
		return this->last_value.load(std::memory_order_relaxed);
	}
}
