/**
 * File: monitor.hpp
 * Path: ajylib/include/ajy/utility/monitor/monitor.hpp
 * Description:
 *	A driven, open set of Probes declaration.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_UTILITY_MONITOR_MONITOR_HPP
#define AJY_UTILITY_MONITOR_MONITOR_HPP

#include <ajy/utility/monitor/probe.hpp>

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace ajy::utility::monitor
{
	class Monitor
	{
	public:
		Monitor(void) noexcept;
		~Monitor(void) noexcept;

		Monitor(const Monitor &other) = delete;
		Monitor &operator=(const Monitor &other) = delete;
		Monitor(Monitor &&other) = delete;
		Monitor &operator=(Monitor &&other) = delete;

		bool add(std::unique_ptr<Probe> probe) noexcept;
		void update(void) noexcept;

		std::size_t get_count(void) const noexcept;

		Probe *get(std::size_t index) noexcept;
		Probe *find(std::string_view name) noexcept;

	private:
		std::vector<std::unique_ptr<Probe>> probes;
	};
}

#endif