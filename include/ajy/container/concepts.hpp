/**
 * File: concepts.hpp
 * Path: ajylib/include/ajy/container/concepts.hpp
 * Description:
 * 	Type concepts for ajy::container containers.
 * Author: ajy-dev
 * Created: 2026-08-28
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONTAINER_CONCEPTS_HPP
#define AJY_CONTAINER_CONCEPTS_HPP

#include <concepts>
#include <type_traits>

namespace ajy::container
{
	template <typename T>
	concept MpscQueueableType =
		std::move_constructible<T>
		&& std::is_nothrow_move_constructible<T>::value;
}

#endif
