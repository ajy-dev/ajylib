/**
 * File: serialization_buffer.cpp
 * Path: library/source/container/serialization_buffer.cpp
 * Description: A serialization buffer definition.
 * Author: ajy-dev
 * Created: 2026-05-14
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/container/serialization_buffer.hpp>
#include <new>
#include <cstring>

namespace ajy::container
{
	SerializationBuffer::SerializationBuffer(std::size_t buffer_capacity) noexcept
		: capacity(buffer_capacity)
	{
		this->buffer = new (std::nothrow) std::byte[this->capacity];

		if (!this->buffer)
			this->capacity = 0;
	}

	SerializationBuffer::~SerializationBuffer(void) noexcept
	{
		delete[] this->buffer;
	}

	SerializationBuffer::SerializationBuffer(SerializationBuffer &&other) noexcept
		: buffer(other.buffer)
		, capacity(other.capacity)
		, read_index(other.read_index)
		, write_index(other.write_index)
	{
		other.buffer = nullptr;
		other.capacity = 0;
		other.clear();
	}

	SerializationBuffer &SerializationBuffer::operator=(SerializationBuffer &&other) noexcept
	{
		if (this == &other)
			return *this;
		
		delete[] this->buffer;

		this->buffer = other.buffer;
		this->capacity = other.capacity;
		this->read_index = other.read_index;
		this->write_index = other.write_index;

		other.buffer = nullptr;
		other.capacity = 0;
		other.read_index = 0;
		other.write_index = 0;

		return *this;
	}

	std::size_t SerializationBuffer::get_capacity(void) const noexcept
	{
		return this->capacity;
	}

	std::size_t SerializationBuffer::get_data_size(void) const noexcept
	{
		return this->write_index - this->read_index;
	}

	std::size_t SerializationBuffer::get_free_size(void) const noexcept
	{
		return this->capacity - this->write_index;
	}

	bool SerializationBuffer::serialize(const void *src, std::size_t size) noexcept
	{
		if (!size)
			return true;
		if (!this->buffer || !src || size > this->get_free_size())
			return false;

		std::memcpy(this->buffer + this->write_index, src, size);
		this->write_index += size;

		return true;
	}

	bool SerializationBuffer::deserialize(void *dst,  std::size_t size) noexcept
	{
		if (!size)
			return true;
		if (!this->buffer || !dst || size > this->get_data_size())
			return false;
		
		std::memcpy(dst, this->buffer + this->read_index, size);
		this->read_index += size;

		return true;
	}

	void SerializationBuffer::clear(void) noexcept
	{
		this->read_index = 0;
		this->write_index = 0;
	}

	const void *SerializationBuffer::get_buffer_ptr(void) const noexcept
	{
		return this->buffer;
	}
	
	void *SerializationBuffer::get_buffer_ptr(void) noexcept
	{
		return const_cast<void *>(static_cast<const SerializationBuffer *>(this)->get_buffer_ptr());
	}

	void SerializationBuffer::commit_direct_serialize(std::size_t size) noexcept
	{
		this->write_index += size;

		if (this->write_index > this->capacity)
			this->write_index = this->capacity;
	}

	void SerializationBuffer::commit_direct_deserialize(std::size_t size) noexcept
	{
		this->read_index += size;

		if (this->read_index > this->write_index)
			this->read_index = this->write_index;
	}
}