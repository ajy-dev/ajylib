/**
 * File: console.hpp
 * Path: ajylib/include/ajy/utility/console.hpp
 * Description:
 * 	A generic interactive stdin command console declaration.
 * Note:
 * 	Console itself has no knowledge of any subsystem.
 * 	Other modules register commands into it via register_command(),
 * 	grouped under a subsystem name (e.g. "server start", "pool stats").
 * 	"help" and "exit" are reserved subsystem names handled internally by run().
 * Author: ajy-dev
 * Created: 2026-07-03
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_UTILITY_CONSOLE_HPP
#define AJY_UTILITY_CONSOLE_HPP

#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace ajy::utility
{
	class Console
	{
	public:
		using CommandHandler = std::function<void(std::istringstream &args)>;

		Console(void) noexcept;
		~Console(void) noexcept;

		Console(const Console &other) = delete;
		Console &operator=(const Console &other) = delete;
		Console(Console &&other) = delete;
		Console &operator=(Console &&other) = delete;

		bool register_command(std::string_view subsystem, std::string_view name, std::string_view description, CommandHandler handler) noexcept;
		void run(void) noexcept;

	private:
		struct CommandEntry
		{
			std::string description;
			CommandHandler handler;
		};

		using CommandMap = std::map<std::string, CommandEntry>;
		using SubsystemMap = std::map<std::string, CommandMap>;

		static constexpr std::string_view COMMAND_EXIT = "exit";
		static constexpr std::string_view COMMAND_HELP = "help";

		static bool is_reserved_word(std::string_view word) noexcept;

		void print_help(void) const noexcept;
		void print_subsystem_help(const std::string &subsystem_name, const CommandMap &commands) const noexcept;

		SubsystemMap command_table;
	};
}

#endif
