/**
 * File: net_packet_buffer.cpp
 * Path: ajylib/source/network/protocol/net_packet_buffer.cpp
 * Description:
 *	A 5-byte-header obfuscated packet buffer definition.
 * Author: ajy-dev
 * Created: 2026-07-06
 * Updated: 2026-07-21
 * Version: 0.1.0
 */

#include <ajy/network/protocol/net_packet_buffer.hpp>
#include <ajy/network/protocol/obfuscator/obfuscator.hpp>

#include <algorithm>
#include <cstring>

namespace ajy::network::protocol
{
	NetPacketBuffer::NetPacketBuffer(std::size_t payload_capacity) noexcept
		: SerializationBuffer(HEADER_SIZE + std::min(payload_capacity, MAX_PAYLOAD_SIZE))
		, finalized(false)
	{
		this->commit_direct_serialize(HEADER_SIZE);
		this->commit_direct_deserialize(HEADER_SIZE);
	}

	void NetPacketBuffer::build_header(std::uint8_t code, std::uint8_t random_key) noexcept
	{
		std::byte *header;
		std::uint16_t payload_length;
		std::uint8_t checksum;

		if (this->finalized)
			return;

		header = static_cast<std::byte *>(this->get_buffer_ptr());
		payload_length = static_cast<std::uint16_t>(this->get_data_size());
		checksum = this->compute_checksum(this->get_payload_ptr(), payload_length);

		std::memcpy(header + CODE_OFFSET, &code, CODE_SIZE);
		std::memcpy(header + LEN_OFFSET, &payload_length, LEN_SIZE);
		std::memcpy(header + RANDKEY_OFFSET, &random_key, RANDKEY_SIZE);
		std::memcpy(header + CHECKSUM_OFFSET, &checksum, CHECKSUM_SIZE);
	}

	void NetPacketBuffer::encode(std::uint8_t fixed_key) noexcept
	{
		std::byte *header;
		std::uint8_t random_key;
		std::size_t block_length;

		if (this->finalized)
			return;

		header = static_cast<std::byte *>(this->get_buffer_ptr());
		std::memcpy(&random_key, header + RANDKEY_OFFSET, RANDKEY_SIZE);
		block_length = CHECKSUM_SIZE + this->get_data_size();

		obfuscator::obfuscate(header + CHECKSUM_OFFSET, block_length, fixed_key, random_key);

		this->finalized = true;
	}

	bool NetPacketBuffer::decode(std::uint8_t fixed_key) noexcept
	{
		std::byte *header;
		std::uint8_t random_key;
		std::size_t payload_length;
		std::size_t block_length;
		std::uint8_t stored_checksum;
		std::uint8_t computed_checksum;

		header = static_cast<std::byte *>(this->get_buffer_ptr());
		std::memcpy(&random_key, header + RANDKEY_OFFSET, RANDKEY_SIZE);
		payload_length = this->get_data_size();
		block_length = CHECKSUM_SIZE + payload_length;

		obfuscator::deobfuscate(header + CHECKSUM_OFFSET, block_length, fixed_key, random_key);

		std::memcpy(&stored_checksum, header + CHECKSUM_OFFSET, CHECKSUM_SIZE);
		computed_checksum = this->compute_checksum(this->get_payload_ptr(), payload_length);

		return stored_checksum == computed_checksum;
	}

	void NetPacketBuffer::set_header(const void *header_bytes) noexcept
	{
		std::memcpy(this->get_buffer_ptr(), header_bytes, HEADER_SIZE);
	}

	void NetPacketBuffer::clear(void) noexcept
	{
		SerializationBuffer::clear();
		this->commit_direct_serialize(HEADER_SIZE);
		this->commit_direct_deserialize(HEADER_SIZE);
		this->finalized = false;
	}

	std::size_t NetPacketBuffer::get_packet_size(void) const noexcept
	{
		return HEADER_SIZE + this->get_data_size();
	}

	const void *NetPacketBuffer::get_payload_ptr(void) const noexcept
	{
		return static_cast<const std::byte *>(this->get_buffer_ptr()) + HEADER_SIZE;
	}

	void *NetPacketBuffer::get_payload_ptr(void) noexcept
	{
		return const_cast<void *>(static_cast<const NetPacketBuffer *>(this)->get_payload_ptr());
	}

	std::uint8_t NetPacketBuffer::compute_checksum(const void *payload, std::size_t length) noexcept
	{
		const std::uint8_t *bytes;
		std::uint8_t sum;

		bytes = static_cast<const std::uint8_t *>(payload);
		sum = 0;
		for (std::size_t i = 0; i < length; ++i)
			sum = static_cast<std::uint8_t>(sum + bytes[i]);

		return sum;
	}
}