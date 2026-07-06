/**
 * File: probe.cpp
 * Path: ajylib/source/utility/monitor/probe.cpp
 * Description:
 *	An abstract monitored-metric probe definition.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/utility/monitor/probe.hpp>

namespace ajy::utility::monitor
{
	Probe::Probe(std::string_view name)
		: name(name)
	{
	}

	Probe::~Probe(void) noexcept
	{
	}

	const std::string &Probe::get_name(void) const noexcept
	{
		return this->name;
	}
}
