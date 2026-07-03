/**
 * File: console.cpp
 * Path: ajylib/source/utility/console.cpp
 * Description:
 * 	A generic interactive stdin command console definition.
 * Author: ajy-dev
 * Created: 2026-07-03
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/utility/console.hpp>

#include <cstdio>
#include <exception>
#include <iostream>
#include <new>
#include <utility>

namespace ajy::utility
{
	Console::Console(void) noexcept
		: command_table()
	{
	}

	Console::~Console(void) noexcept
	{
	}

	bool Console::register_command(std::string_view subsystem, std::string_view name, std::string_view description, CommandHandler handler) noexcept
	{
		if (subsystem.empty() || name.empty() || this->is_reserved_word(subsystem) || name == this->COMMAND_HELP)
			return false;

		try
		{
			CommandMap &commands = this->command_table[std::string(subsystem)];

			if (commands.find(std::string(name)) != commands.end())
				return false;

			commands.emplace(std::string(name), CommandEntry{std::string(description), std::move(handler)});
		}
		catch (const std::bad_alloc &error)
		{
			std::fprintf(stderr, "ajy::utility::Console::register_command(): allocation failed: %s\n", error.what());
			std::terminate();
		}

		return true;
	}

	void Console::run(void) noexcept
	{
		std::string line;

		try
		{
			while (std::getline(std::cin, line))
			{
				std::istringstream iss(line);
				std::string subsystem_name;
				std::string command_name;
				SubsystemMap::iterator subsystem_it;
				CommandMap::iterator command_it;

				iss >> subsystem_name;

				if (subsystem_name.empty())
					continue;

				if (subsystem_name == this->COMMAND_EXIT)
					break;

				if (subsystem_name == this->COMMAND_HELP)
				{
					this->print_help();
					continue;
				}

				iss >> command_name;

				subsystem_it = this->command_table.find(subsystem_name);
				if (subsystem_it == this->command_table.end())
				{
					std::cout << "Unknown subsystem: " << subsystem_name << ". Type 'help'.\n";
					continue;
				}

				if (command_name == this->COMMAND_HELP)
				{
					this->print_subsystem_help(subsystem_it->first, subsystem_it->second);
					continue;
				}

				command_it = subsystem_it->second.find(command_name);
				if (command_it == subsystem_it->second.end())
				{
					std::cout << "Unknown command: " << subsystem_name << " " << command_name << ". Type 'help'.\n";
					continue;
				}

				try
				{
					command_it->second.handler(iss);
				}
				catch (const std::exception &error)
				{
					std::fprintf(stderr, "ajy::utility::Console::run(): command handler threw: %s\n", error.what());
				}
				catch (...)
				{
					std::fprintf(stderr, "ajy::utility::Console::run(): command handler threw an unknown exception\n");
				}
			}
		}
		catch (const std::bad_alloc &error)
		{
			std::fprintf(stderr, "ajy::utility::Console::run(): allocation failed: %s\n", error.what());
			std::terminate();
		}
	}

	bool Console::is_reserved_word(std::string_view word) noexcept
	{
		return word == COMMAND_EXIT || word == COMMAND_HELP;
	}

	void Console::print_help(void) const noexcept
	{
		for (SubsystemMap::const_iterator subsystem_it = this->command_table.begin(); subsystem_it != this->command_table.end(); ++subsystem_it)
			this->print_subsystem_help(subsystem_it->first, subsystem_it->second);
	}

	void Console::print_subsystem_help(const std::string &subsystem_name, const CommandMap &commands) const noexcept
	{
		std::cout << subsystem_name << ":\n";

		for (CommandMap::const_iterator command_it = commands.begin(); command_it != commands.end(); ++command_it)
			std::cout << "  " << command_it->first << " - " << command_it->second.description << "\n";
	}
}
