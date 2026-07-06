/**
 * File: memory_probe.cpp
 * Path: ajylib/source/utility/monitor/windows/memory_probe.cpp
 * Description:
 *	Windows memory probe definitions.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/utility/monitor/windows/memory_probe.hpp>
#include <ajy/windows.hpp>

#include <limits>

namespace ajy::utility::monitor::windows
{
	namespace
	{
		constexpr double BYTES_PER_MB = 1024.0 * 1024.0;
	}

	ProcessPrivateMemoryProbe::ProcessPrivateMemoryProbe(std::string_view name, void *process_handle)
		: Probe(name)
		, process(process_handle ? process_handle : ::GetCurrentProcess())
		, last_value(0.0)
	{
	}

	ProcessPrivateMemoryProbe::~ProcessPrivateMemoryProbe(void) noexcept
	{
	}

	void ProcessPrivateMemoryProbe::update(void) noexcept
	{
		PROCESS_MEMORY_COUNTERS_EX counters;

		counters.cb = sizeof(counters);

		if (!::GetProcessMemoryInfo(reinterpret_cast<HANDLE>(this->process), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters), sizeof(counters)))
		{
			this->last_value.store(std::numeric_limits<double>::quiet_NaN(), std::memory_order_relaxed);
			return;
		}

		this->last_value.store(static_cast<double>(counters.PrivateUsage) / BYTES_PER_MB, std::memory_order_relaxed);
	}

	double ProcessPrivateMemoryProbe::get_value(void) const noexcept
	{
		return this->last_value.load(std::memory_order_relaxed);
	}

	SystemAvailableMemoryProbe::SystemAvailableMemoryProbe(std::string_view name)
		: Probe(name)
		, last_value(0.0)
	{
	}

	SystemAvailableMemoryProbe::~SystemAvailableMemoryProbe(void) noexcept
	{
	}

	void SystemAvailableMemoryProbe::update(void) noexcept
	{
		MEMORYSTATUSEX status;

		status.dwLength = sizeof(status);

		if (!::GlobalMemoryStatusEx(&status))
		{
			this->last_value.store(std::numeric_limits<double>::quiet_NaN(), std::memory_order_relaxed);
			return;
		}

		this->last_value.store(static_cast<double>(status.ullAvailPhys) / BYTES_PER_MB, std::memory_order_relaxed);
	}

	double SystemAvailableMemoryProbe::get_value(void) const noexcept
	{
		return this->last_value.load(std::memory_order_relaxed);
	}

	SystemNonpagedMemoryProbe::SystemNonpagedMemoryProbe(std::string_view name)
		: Probe(name)
		, last_value(0.0)
	{
	}

	SystemNonpagedMemoryProbe::~SystemNonpagedMemoryProbe(void) noexcept
	{
	}

	void SystemNonpagedMemoryProbe::update(void) noexcept
	{
		PERFORMANCE_INFORMATION performance_info;

		performance_info.cb = sizeof(performance_info);

		if (!::GetPerformanceInfo(&performance_info, sizeof(performance_info)))
		{
			this->last_value.store(std::numeric_limits<double>::quiet_NaN(), std::memory_order_relaxed);
			return;
		}

		this->last_value.store(
			static_cast<double>(performance_info.KernelNonpaged) * static_cast<double>(performance_info.PageSize) / BYTES_PER_MB,
			std::memory_order_relaxed);
	}

	double SystemNonpagedMemoryProbe::get_value(void) const noexcept
	{
		return this->last_value.load(std::memory_order_relaxed);
	}
}
