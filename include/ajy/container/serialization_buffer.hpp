/**
 * File: serialization_buffer.hpp
 * Path: ajylib/include/ajy/container/serialization_buffer.hpp
 * Description:
 * 	A fixed-size linear buffer for network packet serialization declaration.
 * 	Serialization and deserialization must not be interleaved;
 * 	all writes must complete before any reads begin.
 * Note:
 * 	DEFAULT_CAPACITY is set to 1400 to fit within a typical Ethernet
 * 	MTU(1500 bytes) after IP and TCP/UDP header overhead.
 * 	Intended as a base class for packet buffers with headers.
 * Author: ajy-dev
 * Created: 2026-06-15
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_SERIALIZATION_BUFFER_HPP
#define AJY_CONTAINER_SERIALIZATION_BUFFER_HPP

#include <cstddef>
#include <type_traits>

namespace ajy::container
{
	class SerializationBuffer
	{
	public:
		static constexpr std::size_t DEFAULT_CAPACITY = 1400;

		explicit SerializationBuffer(std::size_t buffer_capacity = DEFAULT_CAPACITY) noexcept;
		virtual ~SerializationBuffer(void) noexcept;

		SerializationBuffer(const SerializationBuffer &other) = delete;
		SerializationBuffer &operator=(const SerializationBuffer &other) = delete;
		SerializationBuffer(SerializationBuffer &&other) noexcept;
		SerializationBuffer &operator=(SerializationBuffer &&other) noexcept;

		std::size_t get_capacity(void) const noexcept;
		std::size_t get_data_size(void) const noexcept;
		std::size_t get_free_size(void) const noexcept;

		bool serialize(const void *src, std::size_t size) noexcept;
		bool deserialize(void *dst, std::size_t size) noexcept;
		void clear(void) noexcept;

		const void *get_buffer_ptr(void) const noexcept;
		void *get_buffer_ptr(void) noexcept;
		void commit_direct_serialize(std::size_t size) noexcept;
		void commit_direct_deserialize(std::size_t size) noexcept;

	private:
		std::byte *buffer = nullptr;
		std::size_t capacity;
		std::size_t read_index = 0;
		std::size_t write_index = 0;
	};

	template <typename T>
	requires std::is_trivially_copyable<T>::value
	SerializationBuffer &operator<<(SerializationBuffer &buffer, const T &value) noexcept;

	template <typename T>
	requires std::is_trivially_copyable<T>::value
	SerializationBuffer &operator>>(SerializationBuffer &buffer, T &value) noexcept;
}

#include <ajy/container/serialization_buffer.tpp>

#endif
