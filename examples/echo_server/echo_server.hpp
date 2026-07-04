/**
 * File: echo_server.hpp
 * Path: ajylib/examples/echo_server/echo_server.hpp
 * Description:
 *	A minimal echo server built on ajy::network::windows::iocp::Server.
 * Author: ajy-dev
 * Created: 2026-07-02
 * Updated: 2026-07-04
 * Version: 0.1.0
 */

#ifndef ECHO_SERVER_HPP
#define ECHO_SERVER_HPP

#include <ajy/network/windows/iocp/server.hpp>

#include <cstddef>
#include <cstdint>

class EchoServer : public ajy::network::windows::iocp::Server
{
public:
	using ajy::network::windows::iocp::Server::Server;

protected:
	bool on_connection_request(const char *ip, std::uint16_t port) override;
	void on_client_join(SessionID id) noexcept override;
	void on_client_leave(SessionID id) noexcept override;
	void on_recv(SessionID id, Packet *packet) noexcept override;
	void on_send(SessionID id, std::size_t size) noexcept override;
	void on_worker_thread_begin(void) noexcept override;
	void on_worker_thread_end(void) noexcept override;
};

#endif
