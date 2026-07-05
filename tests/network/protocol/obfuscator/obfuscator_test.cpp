/**
 * File: obfuscator_test.cpp
 * Path: ajylib/tests/network/protocol/obfuscator/obfuscator_test.cpp
 * Description:
 *	Unit tests for ajy::network::protocol::obfuscator.
 * Author: ajy-dev
 * Created: 2026-07-06
 * Updated: Never
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/network/protocol/obfuscator/obfuscator.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
	// Sample vector: the sentence "Never trust the client." (ASCII, no null
	// terminator) obfuscated with fixed_key 0x2a, random_key 0x42.
	constexpr std::uint8_t SAMPLE_FIXED_KEY = 0x2a;
	constexpr std::uint8_t SAMPLE_RANDOM_KEY = 0x42;

	const std::vector<std::uint8_t> SAMPLE_PLAINTEXT = {
		0x4e, 0x65, 0x76, 0x65, 0x72, 0x20, 0x74, 0x72, 0x75, 0x73, 0x74, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x2e};

	const std::vector<std::uint8_t> SAMPLE_CIPHERTEXT = {
		0x26, 0x66, 0x9c, 0xfa, 0x2c, 0x31, 0xa0, 0xac, 0x63, 0xec, 0x9d, 0xf9, 0x3d, 0x40, 0x9a, 0xc1, 0xf7, 0x00, 0xdc, 0x48, 0x40, 0xeb, 0xc6};
}

// ----------------------------------------------------------------
// Specification sample vector
// ----------------------------------------------------------------

TEST(ObfuscatorUnitTest, ObfuscateMatchesSampleVector)
{
	std::vector<std::uint8_t> buffer = SAMPLE_PLAINTEXT;

	ajy::network::protocol::obfuscator::obfuscate(
		buffer.data(), buffer.size(), SAMPLE_FIXED_KEY, SAMPLE_RANDOM_KEY);

	EXPECT_EQ(buffer, SAMPLE_CIPHERTEXT);
}

TEST(ObfuscatorUnitTest, DeobfuscateMatchesSampleVector)
{
	std::vector<std::uint8_t> buffer = SAMPLE_CIPHERTEXT;

	ajy::network::protocol::obfuscator::deobfuscate(
		buffer.data(), buffer.size(), SAMPLE_FIXED_KEY, SAMPLE_RANDOM_KEY);

	EXPECT_EQ(buffer, SAMPLE_PLAINTEXT);
}

// ----------------------------------------------------------------
// Round-trip
// ----------------------------------------------------------------

TEST(ObfuscatorUnitTest, RoundTripRestoresOriginal)
{
	const std::vector<std::uint8_t> original = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
	std::vector<std::uint8_t> buffer = original;

	ajy::network::protocol::obfuscator::obfuscate(buffer.data(), buffer.size(), 0x5a, 0xc3);
	EXPECT_NE(buffer, original);

	ajy::network::protocol::obfuscator::deobfuscate(buffer.data(), buffer.size(), 0x5a, 0xc3);
	EXPECT_EQ(buffer, original);
}

TEST(ObfuscatorUnitTest, RoundTripSingleByte)
{
	const std::vector<std::uint8_t> original = {0x7e};
	std::vector<std::uint8_t> buffer = original;

	ajy::network::protocol::obfuscator::obfuscate(buffer.data(), buffer.size(), 0x5a, 0xc3);
	ajy::network::protocol::obfuscator::deobfuscate(buffer.data(), buffer.size(), 0x5a, 0xc3);

	EXPECT_EQ(buffer, original);
}

// ----------------------------------------------------------------
// Wrong key detection surface
// ----------------------------------------------------------------

TEST(ObfuscatorUnitTest, WrongFixedKeyFailsToRestore)
{
	const std::vector<std::uint8_t> original = {
		0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
	std::vector<std::uint8_t> buffer = original;

	ajy::network::protocol::obfuscator::obfuscate(buffer.data(), buffer.size(), 0x5a, 0xc3);
	ajy::network::protocol::obfuscator::deobfuscate(buffer.data(), buffer.size(), 0x5b, 0xc3);

	EXPECT_NE(buffer, original);
}

TEST(ObfuscatorUnitTest, WrongRandomKeyFailsToRestore)
{
	const std::vector<std::uint8_t> original = {
		0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
	std::vector<std::uint8_t> buffer = original;

	ajy::network::protocol::obfuscator::obfuscate(buffer.data(), buffer.size(), 0x5a, 0xc3);
	ajy::network::protocol::obfuscator::deobfuscate(buffer.data(), buffer.size(), 0x5a, 0xc4);

	EXPECT_NE(buffer, original);
}

// ----------------------------------------------------------------
// Per-packet variation (random key changes the output)
// ----------------------------------------------------------------

TEST(ObfuscatorUnitTest, DifferentRandomKeyProducesDifferentOutput)
{
	const std::vector<std::uint8_t> original = {
		0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
	std::vector<std::uint8_t> first = original;
	std::vector<std::uint8_t> second = original;

	ajy::network::protocol::obfuscator::obfuscate(first.data(), first.size(), 0x5a, 0x01);
	ajy::network::protocol::obfuscator::obfuscate(second.data(), second.size(), 0x5a, 0x02);

	EXPECT_NE(first, second);
}

// ----------------------------------------------------------------
// Pattern suppression (repeated plaintext leaves no repeated output)
// ----------------------------------------------------------------

TEST(ObfuscatorUnitTest, RepeatedPlaintextHasNoRepeatedOutput)
{
	std::vector<std::uint8_t> buffer(32, 0x00);
	bool all_identical;

	ajy::network::protocol::obfuscator::obfuscate(buffer.data(), buffer.size(), 0x5a, 0xc3);

	all_identical = true;
	for (std::size_t i = 1; i < buffer.size(); ++i)
		if (buffer[i] != buffer[0])
			all_identical = false;

	EXPECT_FALSE(all_identical);
}

// ----------------------------------------------------------------
// Empty range
// ----------------------------------------------------------------

TEST(ObfuscatorUnitTest, ZeroLengthIsNoOp)
{
	ajy::network::protocol::obfuscator::obfuscate(nullptr, 0, 0x5a, 0xc3);
	ajy::network::protocol::obfuscator::deobfuscate(nullptr, 0, 0x5a, 0xc3);

	SUCCEED();
}
