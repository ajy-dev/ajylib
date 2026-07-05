/**
 * File: obfuscator.hpp
 * Path: ajylib/include/ajy/network/protocol/obfuscator/obfuscator.hpp
 * Description:
 * 	A two-pass rolling XOR byte obfuscator.
 * Note:
 * 	This is lightweight obfuscation, not secure encryption. It only hides
 * 	byte patterns cheaply and is weak against brute force by design.
 * 	Operates in place on an arbitrary byte range. Pass 1 chains each byte
 * 	against the per-packet random key, pass 2 against the shared fixed key,
 * 	each mixing in the running result plus the 1-based byte position so that
 * 	repeated plaintext leaves no repeated output. The transform carries no
 * 	intrinsic success signal: a wrong key yields wrong output silently, so
 * 	deobfuscation success must be judged externally (via a checksum).
 * Author: ajy-dev
 * Created: 2026-07-06
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_NETWORK_PROTOCOL_OBFUSCATOR_OBFUSCATOR_HPP
#define AJY_NETWORK_PROTOCOL_OBFUSCATOR_OBFUSCATOR_HPP

#include <cstddef>
#include <cstdint>

namespace ajy::network::protocol::obfuscator
{
	void obfuscate(void *data, std::size_t length, std::uint8_t fixed_key, std::uint8_t random_key) noexcept;
	void deobfuscate(void *data, std::size_t length, std::uint8_t fixed_key, std::uint8_t random_key) noexcept;
}

#endif
