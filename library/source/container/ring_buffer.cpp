/**
 * File: ring_buffer.cpp
 * Path: library/source/container/ring_buffer.cpp
 * Description: A ring buffer definition.
 * Author: ajy-dev
 * Created: 2026-05-13
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/container/ring_buffer.hpp>
#include <bit>
#include <new>
#include <cstring>

namespace ajy::container
{
	RingBuffer::RingBuffer(std::size_t buffer_capacity) noexcept
	{
		this->capacity = std::bit_ceil(buffer_capacity);
		this->mask = this->capacity - 1;
		this->buffer = new (std::nothrow) std::byte[this->capacity];
		if (!this->buffer)
			this->capacity = 0;
	}

	RingBuffer::~RingBuffer(void) noexcept
	{
		delete[] this->buffer;
	}

	RingBuffer::RingBuffer(RingBuffer &&other) noexcept
		: buffer(other.buffer)
		, capacity(other.capacity)
		, mask(other.mask)
		, read_index(other.read_index)
		, write_index(other.write_index)
	{
		other.buffer = nullptr;
		other.capacity = 0;
		other.clear();
	}

	RingBuffer &RingBuffer::operator=(RingBuffer &&other) noexcept
	{
		if (this == &other)
			return *this;

		delete[] this->buffer;

		this->buffer = other.buffer;
		this->capacity = other.capacity;
		this->mask = other.mask;
		this->read_index = other.read_index;
		this->write_index = other.write_index;

		other.buffer = nullptr;
		other.capacity = 0;
		other.clear();

		return *this;
	}

	std::size_t RingBuffer::get_capacity(void) const noexcept
	{
		return this->capacity;
	}

	std::size_t RingBuffer::get_used_size(void) const noexcept
	{
		return this->write_index - this->read_index;
	}

	std::size_t RingBuffer::get_free_size(void) const noexcept
	{
		return this->capacity - this->get_used_size();
	}

	bool RingBuffer::peek(void *dst, std::size_t size) const noexcept
	{
		std::byte *dst_ptr;
		std::size_t space_to_end;
		std::size_t first_size;
		std::size_t second_size;

		if (!size)
			return true;
		if (!this->buffer || !dst || this->get_used_size() < size)
			return false;

		dst_ptr = static_cast<std::byte *>(dst);
		space_to_end = this->capacity - (this->read_index & this->mask);
		first_size = space_to_end >= size ? size : space_to_end;
		second_size = size - first_size;

		std::memcpy(dst_ptr, &this->buffer[this->read_index & this->mask], first_size);
		if (second_size)
			std::memcpy(dst_ptr + first_size, this->buffer, second_size);

		return true;
	}
	
	bool RingBuffer::read(void *dst, std::size_t size) noexcept
	{
		if (!this->peek(dst, size))
			return false;

		this->read_index += size;

		return true;
	}

	bool RingBuffer::write(const void *src, std::size_t size) noexcept
	{
		const std::byte *src_ptr;
		std::size_t space_to_end;
		std::size_t first_size;
		std::size_t second_size;

		if (!size)
			return true;
		if (!this->buffer || !src || this->get_free_size() < size)
			return false;

		src_ptr = static_cast<const std::byte *>(src);
		space_to_end = this->capacity - (this->write_index & this->mask);
		first_size = space_to_end >= size ? size : space_to_end;
		second_size = size - first_size;

		std::memcpy(&this->buffer[this->write_index & this->mask], src_ptr, first_size);
		if (second_size)
			std::memcpy(this->buffer, src_ptr + first_size, second_size);
		
		this->write_index += size;

		return true;
	}

	void RingBuffer::clear(void) noexcept
	{
		this->read_index = 0;
		this->write_index = 0;
	}

	std::size_t RingBuffer::get_direct_read_size(void) const noexcept
	{
		std::size_t used_size = this->get_used_size();
		std::size_t space_to_end = this->capacity - (this->read_index & this->mask);

		return (space_to_end >= used_size) ? used_size : space_to_end;
	}

	std::size_t RingBuffer::get_direct_write_size(void) const noexcept
	{
		std::size_t free_size = this->get_free_size();
		std::size_t space_to_end = this->capacity - (this->write_index & this->mask);

		return (space_to_end >= free_size) ? free_size : space_to_end;
	}

	const void *RingBuffer::get_direct_read_ptr(void) const noexcept
	{
		if (!this->buffer)
			return nullptr;

		return &this->buffer[this->read_index & this->mask];
	}

	void *RingBuffer::get_direct_read_ptr(void) noexcept
	{
		return const_cast<void *>(static_cast<const RingBuffer *>(this)->get_direct_read_ptr());
	}

	const void *RingBuffer::get_direct_write_ptr(void) const noexcept
	{
		if (!this->buffer)
			return nullptr;

		return &this->buffer[this->write_index & this->mask];
	}

	void *RingBuffer::get_direct_write_ptr(void) noexcept
	{
		return const_cast<void *>(static_cast<const RingBuffer *>(this)->get_direct_write_ptr());
	}

	void RingBuffer::commit_direct_read(std::size_t size) noexcept
	{
		this->read_index += size;
	}

	void RingBuffer::commit_direct_write(std::size_t size) noexcept
	{
		this->write_index += size;
	}
}