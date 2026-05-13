/**
 * File: shared_types.hpp
 * Path: library/include/ajy/memory/shared_types.hpp
 * Description: Shared types for ajy::memory allocators.
 * Author: ajy-dev
 * Created: 2026-05-13
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_SHARED_TYPES_HPP
# define AJY_MEMORY_SHARED_TYPES_HPP

# include <cstddef>

namespace ajy::memory
{
	struct FreeNode
	{
		FreeNode *next;
	};

	template <typename T>
	union Slot
	{
		FreeNode free_node;
		alignas(T) std::byte storage[sizeof(T)];
	};

	template <typename T>
	struct Chunk
	{
		Slot<T> *slots;
		std::size_t capacity;
		Chunk *next;
	};
}

#endif