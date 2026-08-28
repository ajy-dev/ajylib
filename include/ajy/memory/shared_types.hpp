/**
 * File: shared_types.hpp
 * Path: ajylib/include/ajy/memory/shared_types.hpp
 * Description:
 *	Shared types for ajy::memory allocators.
 * Note:
 *	ObjectSlot's default constructor is user-provided so that value
 *	initialization does not zero the whole storage array.
 * Author: ajy-dev
 * Created: 2026-06-16
 * Updated: 2026-08-16
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_SHARED_TYPES_HPP
#define AJY_MEMORY_SHARED_TYPES_HPP

#include <cstddef>

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
	struct ObjectSlot
	{
		ObjectSlot(void) noexcept;
		~ObjectSlot(void) noexcept = default;

		ObjectSlot(const ObjectSlot &other) = delete;
		ObjectSlot &operator=(const ObjectSlot &other) = delete;
		ObjectSlot(ObjectSlot &&other) = delete;
		ObjectSlot &operator=(ObjectSlot &&other) = delete;

		void *pool_next;
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

#include <ajy/memory/shared_types.tpp>

#endif
