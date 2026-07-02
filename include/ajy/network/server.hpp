/**
 * File: server.hpp
 * Path: ajylib/include/ajy/network/server.hpp
 * Description:
 * 	A pure abstract TCP server interface declaration.
 * Author: ajy-dev
 * Created: 2026-06-30
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_NETWORK_SERVER_HPP
#define AJY_NETWORK_SERVER_HPP

#include <ajy/container/serialization_buffer.hpp>

#include <cstddef>
#include <cstdint>

namespace ajy::network
{
	class Server
	{
	public:
		using SessionID = std::uint64_t;

		Server(void) = default;
		virtual ~Server(void) = default;

		Server(const Server &other) = delete;
		Server &operator=(const Server &other) = delete;
		Server(Server &&other) = delete;
		Server &operator=(Server &&other) = delete;

		virtual bool start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept = 0;
		virtual void stop(void) noexcept = 0;

		virtual bool disconnect(SessionID id) noexcept = 0;
		virtual bool send_packet(SessionID id, container::SerializationBuffer *packet) noexcept = 0;

		virtual std::uint32_t get_session_count(void) const noexcept = 0;
		virtual std::uint32_t get_accept_tps(void) noexcept = 0;
		virtual std::uint32_t get_recv_message_tps(void) noexcept = 0;
		virtual std::uint32_t get_send_message_tps(void) noexcept = 0;

	protected:
		virtual bool on_connection_request(const char *ip, std::uint16_t port) = 0;
		virtual void on_client_join(SessionID id) noexcept = 0;
		virtual void on_client_leave(SessionID id) noexcept = 0;
		virtual void on_recv(SessionID id, container::SerializationBuffer *packet) noexcept = 0;
		virtual void on_send(SessionID id, std::size_t size) noexcept = 0;
		virtual void on_worker_thread_begin(void) noexcept = 0;
		virtual void on_worker_thread_end(void) noexcept = 0;
	};
}

#endif
