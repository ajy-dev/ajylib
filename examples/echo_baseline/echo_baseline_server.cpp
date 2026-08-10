/**
 * File: echo_baseline_server.cpp
 * Path: ajylib/examples/echo_baseline/echo_baseline_server.cpp
 * Description:
 *	A baseline echo server that handles every packet directly on the IOCP
 *	worker thread.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#include "echo_baseline_server.hpp"

#include "echo_server_config.hpp"
#include "protocol.hpp"

#include <ajy/utility/logger.hpp>

EchoBaselineServer::EchoBaselineServer(std::string_view logger_name) noexcept
	: ajy::network::windows::iocp::NetServer(
		  logger_name,
		  EchoServerConfig::PROTOCOL_CODE,
		  EchoServerConfig::FIXED_KEY,
		  EchoServerConfig::MAX_PACKET_PAYLOAD)
{
}

EchoBaselineServer::~EchoBaselineServer(void) noexcept
{
	this->stop();
}

bool EchoBaselineServer::on_connection_request(const char *ip, std::uint16_t port)
{
	(void)ip;
	(void)port;

	return true;
}

void EchoBaselineServer::on_client_join(SessionID id) noexcept
{
	(void)id;
}

void EchoBaselineServer::on_client_leave(SessionID id) noexcept
{
	(void)id;
}

void EchoBaselineServer::on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
{
	PacketType type;

	if (packet->get_data_size() < sizeof(type))
	{
		this->disconnect(id);
		return;
	}

	*packet >> type;

	if (type == PacketType::REQ_ECHO && packet->get_data_size() == EchoServerConfig::REQ_ECHO_SIZE - sizeof(type))
		this->handle_req_echo(id, packet.get());
	else if (type == PacketType::REQ_LOGIN && packet->get_data_size() == EchoServerConfig::REQ_LOGIN_SIZE - sizeof(type))
		this->handle_req_login(id, packet.get());
	else
		this->disconnect(id);
}

void EchoBaselineServer::on_send(SessionID id, std::size_t size) noexcept
{
	(void)id;
	(void)size;
}

void EchoBaselineServer::on_worker_thread_begin(void) noexcept
{
}

void EchoBaselineServer::on_worker_thread_end(void) noexcept
{
}

void EchoBaselineServer::handle_req_login(SessionID id, Packet *packet) noexcept
{
	std::shared_ptr<Packet> reply;
	std::int64_t account_no;

	*packet >> account_no;

	reply = this->alloc_packet(EchoServerConfig::MAX_PACKET_PAYLOAD);
	if (!reply)
		return;

	*reply << PacketType::RES_LOGIN;
	*reply << LoginStatus::OK;
	*reply << account_no;

	this->send_packet(id, reply);
}

void EchoBaselineServer::handle_req_echo(SessionID id, Packet *packet) noexcept
{
	std::shared_ptr<Packet> reply;
	std::int64_t account_no;
	std::int64_t send_time;

	*packet >> account_no;
	*packet >> send_time;

	reply = this->alloc_packet(EchoServerConfig::MAX_PACKET_PAYLOAD);
	if (!reply)
		return;

	*reply << PacketType::RES_ECHO;
	*reply << account_no;
	*reply << send_time;

	this->send_packet(id, reply);
}
