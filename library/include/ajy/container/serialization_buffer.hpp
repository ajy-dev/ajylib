/**
 * File: serialization_buffer.hpp
 * Path: library/include/ajy/container/serialization_buffer.hpp
 * Description:
 * 	A serialization buffer declaration.
 * 	This buffer enforces a strict serialize-then-deserialize contract:
 * 	all serialization must precede any deserialization.
 * 	Interleaving serialization and deserialization results in undefined behavior.
 * Author: ajy-dev
 * Created: 2026-05-14
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_SERIALIZATION_BUFFER_HPP
# define AJY_CONTAINER_SERIALIZATION_BUFFER_HPP

# include <cstddef>
# include <cstdint>
# include <type_traits>

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
		SerializationBuffer (SerializationBuffer &&other) noexcept;
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

# include <ajy/container/serialization_buffer.tpp>

#endif