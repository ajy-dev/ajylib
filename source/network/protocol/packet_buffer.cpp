/**
 * File: packet_buffer.cpp
 * Path: ajylib/source/network/protocol/packet_buffer.cpp
 * Description:
 *	A fixed-header packet buffer definition.
 * Author: ajy-dev
 * Created: 2026-07-04
 * Updated: 2026-07-21
 * Version: 0.1.0
 */

#include <ajy/network/protocol/packet_buffer.hpp>

#include <algorithm>
#include <cstring>

namespace ajy::network::protocol
{
	PacketBuffer::PacketBuffer(std::size_t payload_capacity) noexcept
		: SerializationBuffer(HEADER_SIZE + std::min(payload_capacity, MAX_PAYLOAD_SIZE))
	{
		this->commit_direct_serialize(HEADER_SIZE);
		this->commit_direct_deserialize(HEADER_SIZE);
	}

	void PacketBuffer::build_header(void) noexcept
	{
		std::uint16_t payload_length;

		payload_length = static_cast<std::uint16_t>(this->get_data_size());
		std::memcpy(this->get_buffer_ptr(), &payload_length, HEADER_SIZE);
	}

	void PacketBuffer::set_header(const void *header_bytes) noexcept
	{
		std::memcpy(this->get_buffer_ptr(), header_bytes, HEADER_SIZE);
	}

	void PacketBuffer::clear(void) noexcept
	{
		SerializationBuffer::clear();
		this->commit_direct_serialize(HEADER_SIZE);
		this->commit_direct_deserialize(HEADER_SIZE);
	}

	std::size_t PacketBuffer::get_packet_size(void) const noexcept
	{
		return HEADER_SIZE + this->get_data_size();
	}

	const void *PacketBuffer::get_payload_ptr(void) const noexcept
	{
		return static_cast<const std::byte *>(this->get_buffer_ptr()) + HEADER_SIZE;
	}

	void *PacketBuffer::get_payload_ptr(void) noexcept
	{
		return const_cast<void *>(static_cast<const PacketBuffer *>(this)->get_payload_ptr());
	}
}
