/**
 * File: monitor.cpp
 * Path: ajylib/source/utility/monitor/monitor.cpp
 * Description:
 *	A driven, open set of Probes definition.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/utility/monitor/monitor.hpp>

#include <cstdio>
#include <exception>
#include <new>
#include <utility>

namespace ajy::utility::monitor
{
	Monitor::Monitor(void) noexcept
	{
	}

	Monitor::~Monitor(void) noexcept
	{
	}

	bool Monitor::add(std::unique_ptr<Probe> probe) noexcept
	{
		if (!probe)
			return false;

		if (this->find(probe->get_name()) != nullptr)
			return false;

		try
		{
			this->probes.push_back(std::move(probe));
		}
		catch (const std::bad_alloc &error)
		{
			std::fprintf(stderr, "Monitor::add(): probes.push_back() failed: %s\n", error.what());
			std::terminate();
		}

		return true;
	}

	void Monitor::update(void) noexcept
	{
		for (std::unique_ptr<Probe> &probe : this->probes)
			probe->update();
	}

	std::size_t Monitor::get_count(void) const noexcept
	{
		return this->probes.size();
	}

	Probe *Monitor::get(std::size_t index) noexcept
	{
		if (index >= this->probes.size())
			return nullptr;

		return this->probes[index].get();
	}

	Probe *Monitor::find(std::string_view name) noexcept
	{
		for (std::unique_ptr<Probe> &probe : this->probes)
			if (probe->get_name() == name)
				return probe.get();

		return nullptr;
	}
}
