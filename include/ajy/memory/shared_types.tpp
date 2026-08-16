/**
 * File: shared_types.tpp
 * Path: ajylib/include/ajy/memory/shared_types.tpp
 * Description:
 *	Shared types for ajy::memory allocators.
 * Note:
 *	ObjectSlot's default constructor is user-provided so that value
 *	initialization does not zero the whole storage array.
 * Author: ajy-dev
 * Created: 2026-08-16
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_SHARED_TYPES_TPP
#define AJY_MEMORY_SHARED_TYPES_TPP

#include <ajy/memory/shared_types.hpp>

namespace ajy::memory
{
	template <typename T>
	ObjectSlot<T>::ObjectSlot(void) noexcept
		: pool_next(nullptr)
	{
	}
}

#endif
