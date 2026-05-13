/**
 * File: concepts.hpp
 * Path: library/include/ajy/memory/concepts.hpp
 * Description: Concepts for ajy::memory allocators.
 * Author: ajy-dev
 * Created: 2026-05-13
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_MEMORY_CONCEPTS_HPP
# define AJY_MEMORY_CONCEPTS_HPP

# include <type_traits>

namespace ajy::memory
{
	template <typename T>
	concept PoolableType =
		std::is_object<T>::value 
		&& !std::is_const<T>::value
		&& !std::is_volatile<T>::value;
}

#endif