/**
 * File: network_probe.hpp
 * Path: ajylib/include/ajy/utility/monitor/windows/network_probe.hpp
 * Description:
 *	Windows network-throughput probe declarations.
 * Author: ajy-dev
 * Created: 2026-07-18
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_UTILITY_MONITOR_WINDOWS_NETWORK_PROBE_HPP
#define AJY_UTILITY_MONITOR_WINDOWS_NETWORK_PROBE_HPP

#include <ajy/utility/monitor/probe.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string_view>

namespace ajy::utility::monitor::windows
{
	class SystemNetworkRecvProbe : public Probe
	{
	public:
		explicit SystemNetworkRecvProbe(std::string_view name);
		~SystemNetworkRecvProbe(void) noexcept override;

		SystemNetworkRecvProbe(const SystemNetworkRecvProbe &other) = delete;
		SystemNetworkRecvProbe &operator=(const SystemNetworkRecvProbe &other) = delete;
		SystemNetworkRecvProbe(SystemNetworkRecvProbe &&other) = delete;
		SystemNetworkRecvProbe &operator=(SystemNetworkRecvProbe &&other) = delete;

		void update(void) noexcept override;
		double get_value(void) const noexcept override;

	private:
		std::atomic<double> last_value;
		std::uint64_t prev_octets;
		MonitorClock::time_point prev_sample_point;
	};

	class SystemNetworkSendProbe : public Probe
	{
	public:
		explicit SystemNetworkSendProbe(std::string_view name);
		~SystemNetworkSendProbe(void) noexcept override;

		SystemNetworkSendProbe(const SystemNetworkSendProbe &other) = delete;
		SystemNetworkSendProbe &operator=(const SystemNetworkSendProbe &other) = delete;
		SystemNetworkSendProbe(SystemNetworkSendProbe &&other) = delete;
		SystemNetworkSendProbe &operator=(SystemNetworkSendProbe &&other) = delete;

		void update(void) noexcept override;
		double get_value(void) const noexcept override;

	private:
		std::atomic<double> last_value;
		std::uint64_t prev_octets;
		MonitorClock::time_point prev_sample_point;
	};
}

#endif