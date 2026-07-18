/**
 * File: network_probe.cpp
 * Path: ajylib/source/utility/monitor/windows/network_probe.cpp
 * Description:
 *	Windows network-throughput probe definitions.
 * Author: ajy-dev
 * Created: 2026-07-18
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/utility/monitor/windows/network_probe.hpp>
#include <ajy/windows.hpp>

#include <chrono>
#include <limits>
#include <optional>

namespace ajy::utility::monitor::windows
{
	namespace
	{
		constexpr double BYTES_PER_KB = 1024.0;

		enum class Direction
		{
			RECV,
			SEND,
		};

		std::optional<std::uint64_t> sum_octets(Direction direction) noexcept
		{
			PMIB_IF_TABLE2 table;
			std::uint64_t sum;

			table = nullptr;
			if (::GetIfTable2(&table) != NO_ERROR)
				return std::nullopt;

			sum = 0;
			for (ULONG i = 0; i < table->NumEntries; ++i)
			{
				const MIB_IF_ROW2 &row = table->Table[i];

				if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK)
					continue;

				sum += (direction == Direction::RECV) ? row.InOctets : row.OutOctets;
			}
			::FreeMibTable(table);

			return sum;
		}

		void update_rate(Direction direction, std::atomic<double> &last_value, std::uint64_t &prev_octets, MonitorClock::time_point &prev_sample_point) noexcept
		{
			std::optional<std::uint64_t> octets;
			MonitorClock::time_point sample_point;
			std::chrono::duration<double> elapsed;
			std::uint64_t delta;

			octets = sum_octets(direction);
			if (!octets.has_value())
			{
				last_value.store(std::numeric_limits<double>::quiet_NaN(), std::memory_order_relaxed);

				return;
			}

			sample_point = MonitorClock::now();
			elapsed = sample_point - prev_sample_point;

			delta = octets >= prev_octets ? octets.value() - prev_octets : 0;

			prev_octets = *octets;
			prev_sample_point = sample_point;

			if (elapsed.count() <= 0.0)
				return;

			last_value.store(static_cast<double>(delta) / BYTES_PER_KB / elapsed.count(), std::memory_order_relaxed);
		}

		void seed_baseline(Direction direction, std::uint64_t &prev_octets, MonitorClock::time_point &prev_sample_point) noexcept
		{
			std::optional<std::uint64_t> octets;

			prev_octets = 0;
			prev_sample_point = MonitorClock::time_point();

			octets = sum_octets(direction);
			if (octets.has_value())
			{
				prev_octets = octets.value();
				prev_sample_point = MonitorClock::now();
			}
		}
	}

	SystemNetworkRecvProbe::SystemNetworkRecvProbe(std::string_view name)
		: Probe(name)
		, last_value(0.0)
		, prev_octets(0)
		, prev_sample_point()
	{
		seed_baseline(Direction::RECV, this->prev_octets, this->prev_sample_point);
	}

	SystemNetworkRecvProbe::~SystemNetworkRecvProbe(void) noexcept
	{
	}

	void SystemNetworkRecvProbe::update(void) noexcept
	{
		update_rate(Direction::RECV, this->last_value, this->prev_octets, this->prev_sample_point);
	}

	double SystemNetworkRecvProbe::get_value(void) const noexcept
	{
		return this->last_value.load(std::memory_order_relaxed);
	}

	SystemNetworkSendProbe::SystemNetworkSendProbe(std::string_view name)
		: Probe(name)
		, last_value(0.0)
		, prev_octets(0)
		, prev_sample_point()
	{
		seed_baseline(Direction::SEND, this->prev_octets, this->prev_sample_point);
	}

	SystemNetworkSendProbe::~SystemNetworkSendProbe(void) noexcept
	{
	}

	void SystemNetworkSendProbe::update(void) noexcept
	{
		update_rate(Direction::SEND, this->last_value, this->prev_octets, this->prev_sample_point);
	}

	double SystemNetworkSendProbe::get_value(void) const noexcept
	{
		return this->last_value.load(std::memory_order_relaxed);
	}
}
