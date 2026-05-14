/**
 * File: serialization_buffer.tpp
 * Path: library/include/ajy/container/serialization_buffer.tpp
 * Description: Template definitions for SerializationBuffer operators.
 * Author: ajy-dev
 * Created: 2026-05-14
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_SERIALIZATION_BUFFER_TPP
# define AJY_CONTAINER_SERIALIZATION_BUFFER_TPP

# include <ajy/container/serialization_buffer.hpp>

namespace ajy::container
{
	template <typename T>
	requires std::is_trivially_copyable<T>::value
	SerializationBuffer &operator<<(SerializationBuffer &buffer, const T &value) noexcept
	{
		buffer.serialize(&value, sizeof(T));

		return buffer;
	}

	template <typename T>
	requires std::is_trivially_copyable<T>::value
	SerializationBuffer &operator>>(SerializationBuffer &buffer, T &value) noexcept
	{
		buffer.deserialize(&value, sizeof(T));
		
		return buffer;
	}
}

#endif