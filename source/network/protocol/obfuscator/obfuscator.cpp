/**
 * File: obfuscator.cpp
 * Path: ajylib/source/network/protocol/obfuscator/obfuscator.cpp
 * Description:
 * 	A two-pass rolling XOR byte obfuscator definition.
 * Note:
 * 	obfuscate runs a forward pass keyed by the random key, then a forward
 * 	pass keyed by the fixed key. deobfuscate undoes them in reverse order:
 * 	a reverse pass keyed by the fixed key, then a reverse pass keyed by the
 * 	random key. A forward pass chains each mask on the byte it just wrote;
 * 	a reverse pass chains on the byte it just read. That single difference
 * 	is what makes the two passes exact inverses.
 * Author: ajy-dev
 * Created: 2026-07-06
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/network/protocol/obfuscator/obfuscator.hpp>

#include <cstddef>
#include <cstdint>

namespace ajy::network::protocol::obfuscator
{
	namespace
	{
		void forward_pass(std::uint8_t *bytes, std::size_t length, std::uint8_t key) noexcept
		{
			std::uint8_t previous;

			previous = 0;
			for (std::size_t i = 0; i < length; ++i)
			{
				std::uint8_t position;
				std::uint8_t current;
				std::uint8_t result;

				position = static_cast<std::uint8_t>(i + 1);
				current = bytes[i];
				result = static_cast<std::uint8_t>(current ^ static_cast<std::uint8_t>(previous + key + position));

				bytes[i] = result;
				previous = result;
			}
		}

		void reverse_pass(std::uint8_t *bytes, std::size_t length, std::uint8_t key) noexcept
		{
			std::uint8_t previous;

			previous = 0;
			for (std::size_t i = 0; i < length; ++i)
			{
				std::uint8_t position;
				std::uint8_t current;
				std::uint8_t result;

				position = static_cast<std::uint8_t>(i + 1);
				current = bytes[i];
				result = static_cast<std::uint8_t>(current ^ static_cast<std::uint8_t>(previous + key + position));

				bytes[i] = result;
				previous = current;
			}
		}
	}

	void obfuscate(void *data, std::size_t length, std::uint8_t fixed_key, std::uint8_t random_key) noexcept
	{
		std::uint8_t *bytes;

		bytes = static_cast<std::uint8_t *>(data);
		forward_pass(bytes, length, random_key);
		forward_pass(bytes, length, fixed_key);
	}

	void deobfuscate(void *data, std::size_t length, std::uint8_t fixed_key, std::uint8_t random_key) noexcept
	{
		std::uint8_t *bytes;

		bytes = static_cast<std::uint8_t *>(data);
		reverse_pass(bytes, length, fixed_key);
		reverse_pass(bytes, length, random_key);
	}
}
