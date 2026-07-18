/**
 * File: local_sampler.hpp
 * Path: ajylib/examples/monitor_server/local_sampler.hpp
 * Description:
 *	Drives a Monitor on a fixed 1-second grid and injects each probe value
 *	into a MonitorServer as a local sample (self-host metrics 40-44).
 * Note:
 *	The Monitor owns the probes; this sampler holds non-owning Probe* plus the
 *	DataType each maps to. main registers the probes under the PROBE_* names
 *	below before constructing the sampler.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: 2026-07-18
 * Version: 0.1.0
 */

#ifndef MONITOR_SERVER_LOCAL_SAMPLER_HPP
#define MONITOR_SERVER_LOCAL_SAMPLER_HPP

#include <monitor_server/monitor_server.hpp>

#include <ajy/utility/monitor/monitor.hpp>
#include <ajy/utility/monitor/probe.hpp>

#include <atomic>
#include <cstdint>
#include <string_view>
#include <thread>
#include <vector>

class LocalSampler
{
public:
	// Probe names: main registers under these, the sampler resolves them.
	static constexpr std::string_view PROBE_CPU = "system_cpu";
	static constexpr std::string_view PROBE_NONPAGED = "system_nonpaged_mem_mb";
	static constexpr std::string_view PROBE_AVAILABLE = "system_available_mem_mb";
	static constexpr std::string_view PROBE_NETWORK_RECV = "system_network_recv_kbyte_per_sec";
	static constexpr std::string_view PROBE_NETWORK_SEND = "system_network_send_kbyte_per_sec";

	LocalSampler(ajy::utility::monitor::Monitor &monitor, MonitorServer &server) noexcept;
	~LocalSampler(void) noexcept;

	LocalSampler(const LocalSampler &other) = delete;
	LocalSampler &operator=(const LocalSampler &other) = delete;
	LocalSampler(LocalSampler &&other) = delete;
	LocalSampler &operator=(LocalSampler &&other) = delete;

	void start(void) noexcept;
	void stop(void) noexcept;

private:
	struct ProbeBinding
	{
		ajy::utility::monitor::Probe *probe; // owned by monitor, not by this
		std::uint8_t data_type;
	};

	static void thread_proc(LocalSampler *sampler) noexcept;

	void bind_probe(std::string_view probe_name, std::uint8_t data_type) noexcept;

	ajy::utility::monitor::Monitor &monitor;
	MonitorServer &server;
	std::vector<ProbeBinding> bindings;
	std::atomic<bool> running;
	std::thread thread;
};

#endif
