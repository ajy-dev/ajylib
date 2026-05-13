/**
 * File: ring_buffer.hpp
 * Path: library/include/ajy/container/ring_buffer.hpp
 * Description:
 * 	A ring buffer declaration.
 * 	Thread-safe for SPSC on x86-64 architecture only.
 * 	Relies on x86-64 TSO memory model and volatile to prevent compiler reordering.
 * Author: ajy-dev
 * Created: 2026-05-13
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_RING_BUFFER_HPP
# define AJY_RING_BUFFER_HPP

# include <cstddef>
# include <cstdint>

namespace ajy::container
{
	class RingBuffer
	{
	public:
		static constexpr std::size_t DEFAULT_CAPACITY = 1024;

		explicit RingBuffer(std::size_t buffer_capacity = DEFAULT_CAPACITY) noexcept;
		~RingBuffer(void) noexcept;

		RingBuffer(const RingBuffer &other) = delete;
		RingBuffer &operator=(const RingBuffer &other) = delete;
		RingBuffer(RingBuffer &&other) noexcept;
		RingBuffer &operator=(RingBuffer &&other) noexcept;

		std::size_t get_capacity(void) const noexcept;
		std::size_t get_used_size(void) const noexcept;
		std::size_t get_free_size(void) const noexcept;

		bool peek(void *dst, std::size_t size) const noexcept;
		bool read(void *dst, std::size_t size) noexcept;
		bool write(const void *src, std::size_t size) noexcept;
		void clear(void) noexcept;

		std::size_t get_direct_read_size(void) const noexcept;
		std::size_t get_direct_write_size(void) const noexcept;
		const void *get_direct_read_ptr(void) const noexcept;
		void *get_direct_read_ptr(void) noexcept;
		const void *get_direct_write_ptr(void) const noexcept;
		void *get_direct_write_ptr(void) noexcept;
		void commit_direct_read(std::size_t size) noexcept;
		void commit_direct_write(std::size_t size) noexcept;
	private:
		std::byte *buffer = nullptr;
		std::size_t capacity;
		std::size_t mask;
		volatile std::uint64_t read_index = 0;
		volatile std::uint64_t write_index = 0;
	};
}


#endif