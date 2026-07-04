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

#include <cstddef>
#include <cstdint>
#include <vector>

bool EchoServer::on_connection_request(const char *ip, std::uint16_t port)
{
	this->logger->log(ajy::utility::Logger::LogLevel::Info, "connection request from %s:%u", ip, static_cast<unsigned int>(port));
	return true;
}

void EchoServer::on_client_join(SessionID id) noexcept
{
	constexpr std::int64_t WELCOME_MAGIC = 0x7fffffffffffffff;
	Packet welcome(sizeof(WELCOME_MAGIC));

	this->logger->log(ajy::utility::Logger::LogLevel::Info, "client joined. id: %llu", id);

	welcome << WELCOME_MAGIC;
	this->send_packet(id, &welcome);
}

void EchoServer::on_client_leave(SessionID id) noexcept
{
	this->logger->log(ajy::utility::Logger::LogLevel::Info, "client left. id: %llu", id);
}

void EchoServer::on_recv(SessionID id, Packet *packet) noexcept
{
	std::vector<std::byte> payload;
	std::size_t payload_size;

	payload_size = packet->get_data_size();

	try
	{
		payload.resize(payload_size);
	}
	catch (const std::bad_alloc &error)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Fatal, "std::vector::resize(payload): %s", error.what());
		std::terminate();
	}

	if (!packet->deserialize(payload.data(), payload_size))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Error, "on_recv(): deserialize failed. id: %llu", id);
		return;
	}
	else
	{
		Packet reply(payload_size);

		if (!reply.serialize(payload.data(), payload_size))
		{
			this->logger->log(ajy::utility::Logger::LogLevel::Error, "on_recv(): serialize failed. id: %llu", id);
			return;
		}

		this->send_packet(id, &reply);
	}
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