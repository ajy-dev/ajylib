/**
 * File: monitor_console_commands.cpp
 * Path: ajylib/source/utility/monitor/monitor_console_commands.cpp
 * Description:
 * 	Registers ajy::utility::monitor::Monitor query commands into a utility::Console instance.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/utility/monitor/monitor_console_commands.hpp>

#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>

namespace ajy::utility::monitor
{
	namespace
	{
		constexpr std::string_view SUBSYSTEM_NAME = "monitor";

		constexpr std::string_view COMMAND_LIST = "list";
		constexpr std::string_view COMMAND_GET = "get";

		constexpr std::string_view GET_USAGE = "Usage: monitor get <name>";

		constexpr std::string_view LIST_DESCRIPTION =
			"Lists every registered probe and its most recent value.\n"
			"    Usage: monitor list";
		constexpr std::string_view GET_DESCRIPTION =
			"Shows the most recent value of a single probe by name.\n"
			"    Usage: monitor get <name>\n"
			"      <name>  Probe name to query.";

		void handle_list(Monitor *monitor, std::istringstream &args)
		{
			(void)args;

			std::size_t count;

			count = monitor->get_count();
			if (count == 0)
			{
				std::cout << "No probes registered.\n";
				return;
			}

			for (std::size_t i = 0; i < count; ++i)
			{
				Probe *probe;

				probe = monitor->get(i);
				if (!probe)
					continue;

				std::cout << probe->get_name() << ": " << probe->get_value() << "\n";
			}
		}

		void handle_get(Monitor *monitor, std::istringstream &args)
		{
			std::string name;
			Probe *probe;

			if (!(args >> name))
			{
				std::cout << GET_USAGE << "\n";
				return;
			}

			probe = monitor->find(name);
			if (!probe)
			{
				std::cout << "Probe \"" << name << "\" not found.\n";
				return;
			}

			std::cout << probe->get_name() << ": " << probe->get_value() << "\n";
		}
	}

	void register_monitor_commands(utility::Console *console, Monitor *monitor) noexcept
	{
		console->register_command(
			SUBSYSTEM_NAME,
			COMMAND_LIST,
			LIST_DESCRIPTION,
			[monitor](std::istringstream &args)
			{
				handle_list(monitor, args);
			});
		console->register_command(
			SUBSYSTEM_NAME,
			COMMAND_GET,
			GET_DESCRIPTION,
			[monitor](std::istringstream &args)
			{
				handle_get(monitor, args);
			});
	}
}
