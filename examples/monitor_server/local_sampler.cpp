/**
 * File: local_sampler.cpp
 * Path: ajylib/examples/monitor_server/local_sampler.cpp
 * Description:
 *	Definition of LocalSampler (declared in local_sampler.hpp): binds the
 *	self-host probes to their DataTypes, then drives them on a fixed 1-second
 *	grid and injects each value into the MonitorServer.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: 2026-07-18
 * Version: 0.1.0
 */

#include <monitor_server/local_sampler.hpp>
#include <monitor_server/monitor_server_config.hpp>
#include <monitor_server/protocol.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <exception>
#include <system_error>

LocalSampler::LocalSampler(ajy::utility::monitor::Monitor &monitor, MonitorServer &server) noexcept
	: monitor(monitor)
	, server(server)
	, running(false)
{
	this->bind_probe(PROBE_CPU, static_cast<std::uint8_t>(MonitorDataType::MONITOR_CPU_TOTAL));
	this->bind_probe(PROBE_NONPAGED, static_cast<std::uint8_t>(MonitorDataType::MONITOR_NONPAGED_MEMORY));
	this->bind_probe(PROBE_AVAILABLE, static_cast<std::uint8_t>(MonitorDataType::MONITOR_AVAILABLE_MEMORY));
	this->bind_probe(PROBE_NETWORK_RECV, static_cast<std::uint8_t>(MonitorDataType::MONITOR_NETWORK_RECV));
	this->bind_probe(PROBE_NETWORK_SEND, static_cast<std::uint8_t>(MonitorDataType::MONITOR_NETWORK_SEND));
}

LocalSampler::~LocalSampler(void) noexcept
{
	this->stop();
}

void LocalSampler::bind_probe(std::string_view probe_name, std::uint8_t data_type) noexcept
{
	ajy::utility::monitor::Probe *probe;

	probe = this->monitor.find(probe_name);
	if (!probe)
	{
		std::fprintf(stderr, "LocalSampler::bind_probe(): probe \"%.*s\" not registered.\n", static_cast<int>(probe_name.size()), probe_name.data());
		std::terminate();
	}

	try
	{
		this->bindings.push_back(ProbeBinding{probe, data_type});
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "LocalSampler::bind_probe(): bindings.push_back() failed: %s\n", error.what());
		std::terminate();
	}
}

void LocalSampler::start(void) noexcept
{
	if (this->thread.joinable())
		return;

	this->running.store(true, std::memory_order_relaxed);

	try
	{
		this->thread = std::thread(thread_proc, this);
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "LocalSampler::start(): std::thread failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
}

void LocalSampler::stop(void) noexcept
{
	this->running.store(false, std::memory_order_relaxed);

	if (this->thread.joinable())
	{
		try
		{
			this->thread.join();
		}
		catch (const std::system_error &error)
		{
			std::fprintf(stderr, "LocalSampler::stop(): std::thread::join failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}
	}
}

void LocalSampler::thread_proc(LocalSampler *sampler) noexcept
{
	ajy::utility::monitor::MonitorClock::time_point next_sample;

	next_sample = ajy::utility::monitor::MonitorClock::now();

	while (sampler->running.load(std::memory_order_relaxed))
	{
		std::int32_t time_stamp;

		next_sample += std::chrono::milliseconds(MonitorServerConfig::SAMPLE_INTERVAL_MS);

		time_stamp = static_cast<std::int32_t>(std::time(nullptr));

		for (const ProbeBinding &binding : sampler->bindings)
		{
			double value;

			binding.probe->update();

			value = binding.probe->get_value();
			if (std::isnan(value))
				continue; // syscall failure this tick; skip this DataType

			sampler->server.submit_local_sample(binding.data_type, static_cast<std::int32_t>(std::lround(value)), time_stamp);
		}

		std::this_thread::sleep_until(next_sample);
	}
}
