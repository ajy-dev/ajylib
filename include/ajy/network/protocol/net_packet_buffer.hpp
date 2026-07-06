/**
 * File: net_packet_buffer.hpp
 * Path: ajylib/include/ajy/network/protocol/net_packet_buffer.hpp
 * Description:
 * 	A 5-byte-header obfuscated packet buffer built on top of
 * 	container::SerializationBuffer.
 * Note:
 * 	Header layout: Code(1) | Len(2) | RandKey(1) | CheckSum(1) | Payload(Len).
 * 	Code, Len and RandKey are sent in cleartext; the CheckSum and Payload
 * 	bytes form one contiguous range that is obfuscated together, keyed by a
 * 	shared fixed key and the per-packet random key. The packet holds no key
 * 	state: the fixed key is injected by the owning server at encode/decode
 * 	time, and the random key travels in the header. CheckSum is the low byte
 * 	of the payload byte sum and is the decode-success test, since the cipher
 * 	gives no intrinsic failure signal.
 * 	finalized is a plain (non-atomic) flag that makes build_header/encode
 * 	idempotent: the first call finalizes the packet, later calls are no-ops,
 * 	so one packet can be broadcast to many sessions without re-encoding. A
 * 	packet destined to be shared across threads must be finalized by a single
 * 	thread before it is shared; afterwards it is immutable and may be passed
 * 	between threads freely. Finalizing concurrently from multiple threads is a
 * 	data race. If a packet is pooled and reused, finalized must be reset.
 * Author: ajy-dev
 * Created: 2026-07-06
 * Updated: 2026-07-06
 * Version: 0.1.0
 */

#ifndef AJY_NETWORK_PROTOCOL_NET_PACKET_BUFFER_HPP
#define AJY_NETWORK_PROTOCOL_NET_PACKET_BUFFER_HPP

#include <ajy/container/serialization_buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace ajy::network::protocol
{
	class NetPacketBuffer : public container::SerializationBuffer
	{
	public:
		static constexpr std::size_t CODE_SIZE = sizeof(std::uint8_t);
		static constexpr std::size_t LEN_SIZE = sizeof(std::uint16_t);
		static constexpr std::size_t RANDKEY_SIZE = sizeof(std::uint8_t);
		static constexpr std::size_t CHECKSUM_SIZE = sizeof(std::uint8_t);
		static constexpr std::size_t HEADER_SIZE = CODE_SIZE + LEN_SIZE + RANDKEY_SIZE + CHECKSUM_SIZE;

		static constexpr std::size_t CODE_OFFSET = 0;
		static constexpr std::size_t LEN_OFFSET = CODE_OFFSET + CODE_SIZE;
		static constexpr std::size_t RANDKEY_OFFSET = LEN_OFFSET + LEN_SIZE;
		static constexpr std::size_t CHECKSUM_OFFSET = RANDKEY_OFFSET + RANDKEY_SIZE;

		static constexpr std::size_t MAX_PAYLOAD_SIZE = std::numeric_limits<std::uint16_t>::max();
		static constexpr std::size_t DEFAULT_PAYLOAD_CAPACITY = container::SerializationBuffer::DEFAULT_CAPACITY - HEADER_SIZE;

		explicit NetPacketBuffer(std::size_t payload_capacity = DEFAULT_PAYLOAD_CAPACITY) noexcept;

		void build_header(std::uint8_t code, std::uint8_t random_key) noexcept;
		void encode(std::uint8_t fixed_key) noexcept;
		bool decode(std::uint8_t fixed_key) noexcept;
		void set_header(const void *header_bytes) noexcept;

		std::size_t get_packet_size(void) const noexcept;

		const void *get_payload_ptr(void) const noexcept;
		void *get_payload_ptr(void) noexcept;

	private:
		bool finalized;

		static std::uint8_t compute_checksum(const void *payload, std::size_t length) noexcept;
	};
}

#endif