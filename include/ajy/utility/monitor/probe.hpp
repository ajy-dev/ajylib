/**
 * File: probe.hpp
 * Path: ajylib/include/ajy/utility/monitor/probe.hpp
 * Description:
 *	An abstract monitored-metric probe declaration.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_UTILITY_MONITOR_PROBE_HPP
#define AJY_UTILITY_MONITOR_PROBE_HPP

#include <chrono>
#include <string>
#include <string_view>
#include <type_traits>

namespace ajy::utility::monitor
{
	using MonitorClock = std::conditional<
		std::chrono::high_resolution_clock::is_steady,
		std::chrono::high_resolution_clock,
		std::chrono::steady_clock>::type;

	class Probe
	{
	public:
		explicit Probe(std::string_view name);
		virtual ~Probe(void) noexcept;

		Probe(const Probe &other) = delete;
		Probe &operator=(const Probe &other) = delete;
		Probe(Probe &&other) = delete;
		Probe &operator=(Probe &&other) = delete;

		virtual void update(void) noexcept = 0;
		virtual double get_value(void) const noexcept = 0;

		const std::string &get_name(void) const noexcept;

	protected:
		std::string name;
	};
}

#endif
