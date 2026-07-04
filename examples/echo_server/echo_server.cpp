/**
 * File: echo_server.cpp
 * Path: ajylib/examples/echo_server/echo_server.cpp
 * Description:
 *	A minimal echo server built on ajy::network::windows::iocp::Server.
 * Author: ajy-dev
 * Created: 2026-07-02
 * Updated: 2026-07-04
 * Version: 0.1.0
 */

#include "echo_server.hpp"

#include <ajy/utility/logger.hpp>

bool EchoServer::on_connection_request(const char *ip, std::uint16_t port)
{
	this->logger->log(ajy::utility::Logger::LogLevel::Info, "connection request from %s:%u", ip, static_cast<unsigned int>(port));
	return true;
}

void EchoServer::on_client_join(SessionID id) noexcept
{
	this->logger->log(ajy::utility::Logger::LogLevel::Info, "client joined. id: %llu", id);
}

void EchoServer::on_client_leave(SessionID id) noexcept
{
	this->logger->log(ajy::utility::Logger::LogLevel::Info, "client left. id: %llu", id);
}

void EchoServer::on_recv(SessionID id, Packet *packet) noexcept
{
	this->send_packet(id, packet);
}

void EchoServer::on_send(SessionID id, std::size_t size) noexcept
{
	this->logger->log(ajy::utility::Logger::LogLevel::Info, "sent %zu bytes. id: %llu", size, id);
}

void EchoServer::on_worker_thread_begin(void) noexcept
{
}

void EchoServer::on_worker_thread_end(void) noexcept
{
}
