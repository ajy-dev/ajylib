/**
 * File: concepts.hpp
 * Path: ajylib/include/ajy/concurrency/concepts.hpp
 * Description:
 *	Type concepts for ajy::concurrency components.
 * Author: ajy-dev
 * Created: 2026-08-30
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONCURRENCY_CONCEPTS_HPP
#define AJY_CONCURRENCY_CONCEPTS_HPP

#include <chrono>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace ajy::concurrency
{
	template <typename T>
	concept HasPacketType =
		requires {
			typename T::Packet;
		};

	template <typename T>
	concept HasServerClockType =
		std::chrono::is_clock<typename T::ServerClock>::value;

	template <typename T>
	concept GroupServerHasNestedTypes =
		std::integral<typename T::SessionID>
		&& HasPacketType<T>
		&& HasServerClockType<T>;

	template <GroupServerHasNestedTypes TServer>
	class Group;

	template <typename T>
	concept HasUnpackSessionIdFunction =
		requires(typename T::SessionID id) {
			{
				T::unpack_session_id(id)
			} -> std::same_as<std::pair<std::uint32_t, std::uint32_t>>;
		};

	template <typename T>
	concept HasSetSessionGroupFunction =
		requires(T &server, typename T::SessionID id, Group<T> *group) {
			{
				server.set_session_group(id, group)
			} -> std::same_as<bool>;
		};

	template <typename T>
	concept HasDisconnectFunction =
		requires(T &server, typename T::SessionID id) {
			server.disconnect(id);
		};

	template <typename T>
	concept GroupServerHasFunctions =
		std::is_class<T>::value
		&& HasUnpackSessionIdFunction<T>
		&& HasSetSessionGroupFunction<T>
		&& HasDisconnectFunction<T>;
}

#endif
