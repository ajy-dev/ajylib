/**
 * File: packet_buffer.hpp
 * Path: ajylib/include/ajy/network/protocol/packet_buffer.hpp
 * Description:
 *	A fixed-header packet buffer built on top of container::SerializationBuffer.
 * Note:
 *	Embeds a 2-byte length-prefixed header directly in the underlying buffer,
 *	so a single instance maps to exactly one WSABUF entry on the send path.
 *	payload_capacity is clamped to the maximum value representable by the
 *	2-byte header, since a larger payload could never be described by it.
 * Author: ajy-dev
 * Created: 2026-07-04
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_NETWORK_PROTOCOL_PACKET_BUFFER_HPP
#define AJY_NETWORK_PROTOCOL_PACKET_BUFFER_HPP

#include <ajy/container/serialization_buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace ajy::network::protocol
{
	class PacketBuffer : public container::SerializationBuffer
	{
	public:
		static constexpr std::size_t HEADER_SIZE = sizeof(std::uint16_t);
		static constexpr std::size_t MAX_PAYLOAD_SIZE = std::numeric_limits<std::uint16_t>::max();
		static constexpr std::size_t DEFAULT_PAYLOAD_CAPACITY = container::SerializationBuffer::DEFAULT_CAPACITY - HEADER_SIZE;

		explicit PacketBuffer(std::size_t payload_capacity = DEFAULT_PAYLOAD_CAPACITY) noexcept;

		void build_header(void) noexcept;
		void set_header(const void *header_bytes) noexcept;

		std::size_t get_packet_size(void) const noexcept;

		const void *get_payload_ptr(void) const noexcept;
		void *get_payload_ptr(void) noexcept;
	};
}

#endif
