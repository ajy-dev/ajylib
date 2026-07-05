/**
 * File: net_packet_buffer_test.cpp
 * Path: ajylib/tests/network/protocol/net_packet_buffer_test.cpp
 * Description:
 *	Unit tests for ajy::network::protocol::NetPacketBuffer.
 * Author: ajy-dev
 * Created: 2026-07-06
 * Updated: Never
 * Version: 0.1.0
 */

#include <gtest/gtest.h>

#include <ajy/network/protocol/net_packet_buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

using ajy::network::protocol::NetPacketBuffer;

namespace
{
	constexpr std::uint8_t TEST_CODE = 0x77;
	constexpr std::uint8_t TEST_FIXED_KEY = 0x2a;
	constexpr std::uint8_t TEST_RANDOM_KEY = 0x42;

	// Build a wire-ready packet: serialize payload, build header, encode.
	NetPacketBuffer make_packet(const std::vector<std::uint8_t> &payload, std::uint8_t code, std::uint8_t fixed_key, std::uint8_t random_key)
	{
		NetPacketBuffer packet;

		packet.serialize(payload.data(), payload.size());
		packet.build_header(code, random_key);
		packet.encode(fixed_key);

		return packet;
	}

	// Reconstruct a received packet from wire bytes, mirroring the server's
	// recv path: read Len from the cleartext header, set_header, copy payload.
	NetPacketBuffer reconstruct(const NetPacketBuffer &sent)
	{
		const std::byte *wire;
		std::uint16_t payload_length;
		NetPacketBuffer packet;

		wire = static_cast<const std::byte *>(sent.get_buffer_ptr());
		std::memcpy(&payload_length, wire + NetPacketBuffer::LEN_OFFSET, NetPacketBuffer::LEN_SIZE);

		packet.set_header(wire);
		std::memcpy(packet.get_payload_ptr(), wire + NetPacketBuffer::HEADER_SIZE, payload_length);
		packet.commit_direct_serialize(payload_length);

		return packet;
	}

	std::vector<std::uint8_t> payload_bytes(const NetPacketBuffer &packet)
	{
		const std::uint8_t *base;

		base = static_cast<const std::uint8_t *>(packet.get_payload_ptr());
		return std::vector<std::uint8_t>(base, base + packet.get_data_size());
	}
}

// ----------------------------------------------------------------
// Construction / State
// ----------------------------------------------------------------

TEST(NetPacketBufferUnitTest, InitialStateIsEmptyPayload)
{
	NetPacketBuffer packet;
	EXPECT_EQ(packet.get_data_size(), 0u);
	EXPECT_EQ(packet.get_packet_size(), NetPacketBuffer::HEADER_SIZE);
}

TEST(NetPacketBufferUnitTest, HeaderSizeIsFiveBytes)
{
	EXPECT_EQ(NetPacketBuffer::HEADER_SIZE, 5u);
}

// ----------------------------------------------------------------
// Cleartext header fields (Code, Len, RandKey stay in the clear)
// ----------------------------------------------------------------

TEST(NetPacketBufferUnitTest, HeaderExposesCleartextCodeLenRandKey)
{
	const std::vector<std::uint8_t> payload = {0x10, 0x20, 0x30, 0x40};
	NetPacketBuffer packet = make_packet(payload, TEST_CODE, TEST_FIXED_KEY, TEST_RANDOM_KEY);
	const std::byte *wire;
	std::uint8_t code;
	std::uint16_t length;
	std::uint8_t random_key;

	wire = static_cast<const std::byte *>(packet.get_buffer_ptr());
	std::memcpy(&code, wire + NetPacketBuffer::CODE_OFFSET, NetPacketBuffer::CODE_SIZE);
	std::memcpy(&length, wire + NetPacketBuffer::LEN_OFFSET, NetPacketBuffer::LEN_SIZE);
	std::memcpy(&random_key, wire + NetPacketBuffer::RANDKEY_OFFSET, NetPacketBuffer::RANDKEY_SIZE);

	EXPECT_EQ(code, TEST_CODE);
	EXPECT_EQ(length, payload.size());
	EXPECT_EQ(random_key, TEST_RANDOM_KEY);
}

TEST(NetPacketBufferUnitTest, EncodeAltersPayloadBytes)
{
	const std::vector<std::uint8_t> payload = {0x10, 0x20, 0x30, 0x40};
	NetPacketBuffer packet;
	const std::byte *wire;

	packet.serialize(payload.data(), payload.size());
	packet.build_header(TEST_CODE, TEST_RANDOM_KEY);
	packet.encode(TEST_FIXED_KEY);

	wire = static_cast<const std::byte *>(packet.get_payload_ptr());
	EXPECT_NE(std::memcmp(wire, payload.data(), payload.size()), 0);
}

// ----------------------------------------------------------------
// Send -> recv reconstruction round-trip
// ----------------------------------------------------------------

TEST(NetPacketBufferUnitTest, RoundTripRestoresPayload)
{
	const std::vector<std::uint8_t> payload = {
		0x4e, 0x65, 0x76, 0x65, 0x72, 0x20, 0x74, 0x72, 0x75, 0x73, 0x74, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x2e};
	NetPacketBuffer sent = make_packet(payload, TEST_CODE, TEST_FIXED_KEY, TEST_RANDOM_KEY);
	NetPacketBuffer received = reconstruct(sent);

	EXPECT_TRUE(received.decode(TEST_FIXED_KEY));
	EXPECT_EQ(payload_bytes(received), payload);
}

TEST(NetPacketBufferUnitTest, RoundTripEmptyPayload)
{
	const std::vector<std::uint8_t> payload = {};
	NetPacketBuffer sent = make_packet(payload, TEST_CODE, TEST_FIXED_KEY, TEST_RANDOM_KEY);
	NetPacketBuffer received = reconstruct(sent);

	EXPECT_TRUE(received.decode(TEST_FIXED_KEY));
	EXPECT_EQ(received.get_data_size(), 0u);
}

TEST(NetPacketBufferUnitTest, RoundTripSingleByte)
{
	const std::vector<std::uint8_t> payload = {0xa5};
	NetPacketBuffer sent = make_packet(payload, TEST_CODE, TEST_FIXED_KEY, TEST_RANDOM_KEY);
	NetPacketBuffer received = reconstruct(sent);

	EXPECT_TRUE(received.decode(TEST_FIXED_KEY));
	EXPECT_EQ(payload_bytes(received), payload);
}

// ----------------------------------------------------------------
// Decode failure surface (checksum mismatch)
// ----------------------------------------------------------------

TEST(NetPacketBufferUnitTest, WrongFixedKeyFailsDecode)
{
	const std::vector<std::uint8_t> payload = {0x10, 0x20, 0x30, 0x40, 0x50};
	NetPacketBuffer sent = make_packet(payload, TEST_CODE, TEST_FIXED_KEY, TEST_RANDOM_KEY);
	NetPacketBuffer received = reconstruct(sent);

	EXPECT_FALSE(received.decode(static_cast<std::uint8_t>(TEST_FIXED_KEY ^ 0xff)));
}

TEST(NetPacketBufferUnitTest, CorruptedPayloadFailsDecode)
{
	const std::vector<std::uint8_t> payload = {0x10, 0x20, 0x30, 0x40, 0x50};
	NetPacketBuffer sent = make_packet(payload, TEST_CODE, TEST_FIXED_KEY, TEST_RANDOM_KEY);
	NetPacketBuffer received = reconstruct(sent);
	std::byte *payload_area;
	std::uint8_t corrupted;

	payload_area = static_cast<std::byte *>(received.get_payload_ptr());
	std::memcpy(&corrupted, payload_area, 1);
	corrupted = static_cast<std::uint8_t>(corrupted ^ 0x01);
	std::memcpy(payload_area, &corrupted, 1);

	EXPECT_FALSE(received.decode(TEST_FIXED_KEY));
}

// ----------------------------------------------------------------
// Per-packet variation
// ----------------------------------------------------------------

TEST(NetPacketBufferUnitTest, DifferentRandomKeyProducesDifferentCiphertext)
{
	const std::vector<std::uint8_t> payload = {0x10, 0x20, 0x30, 0x40, 0x50};
	NetPacketBuffer first = make_packet(payload, TEST_CODE, TEST_FIXED_KEY, 0x01);
	NetPacketBuffer second = make_packet(payload, TEST_CODE, TEST_FIXED_KEY, 0x02);

	EXPECT_NE(std::memcmp(first.get_payload_ptr(), second.get_payload_ptr(), payload.size()), 0);
}
