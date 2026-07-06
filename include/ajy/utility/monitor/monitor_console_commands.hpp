/**
 * File: monitor_console_commands.hpp
 * Path: ajylib/include/ajy/utility/monitor/monitor_console_commands.hpp
 * Description:
 * 	Registers ajy::utility::monitor::Monitor query commands into a utility::Console instance.
 * Note:
 * 	The caller must ensure monitor outlives console.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_UTILITY_MONITOR_MONITOR_CONSOLE_COMMANDS_HPP
#define AJY_UTILITY_MONITOR_MONITOR_CONSOLE_COMMANDS_HPP

#include <ajy/utility/console.hpp>
#include <ajy/utility/monitor/monitor.hpp>

namespace ajy::utility::monitor
{
	void register_monitor_commands(utility::Console *console, Monitor *monitor) noexcept;
}

#endif
