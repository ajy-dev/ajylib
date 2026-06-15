/**
 * File: concepts.hpp
 * Path: ajylib/include/ajy/memory/concepts.hpp
 * Description:
 * 	Type concepts for ajy::memory allocators.
 * Author: ajy-dev
 * Created: 2026-06-16
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_CONCEPTS_HPP
#define AJY_MEMORY_CONCEPTS_HPP

#include <type_traits>

namespace ajy::memory
{
	template <typename T>
	concept CompleteType =
		requires {
			sizeof(T);
			alignof(T);
		};

	template <typename T>
	concept PoolableType =
		std::is_object<T>::value
		&& !std::is_const<T>::value
		&& !std::is_volatile<T>::value
		&& !std::is_array<T>::value
		&& CompleteType<T>;
}

#endif
