/**
 * File: echo_group.cpp
 * Path: ajylib/examples/echo_sendworker/echo_group.cpp
 * Description:
 *	The content group of the echo_sendworker example.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#include "echo_group.hpp"

#include "echo_server_config.hpp"
#include "protocol.hpp"

#include <ajy/utility/logger.hpp>

EchoGroup::EchoGroup(
	ajy::network::windows::iocp::NetServer &server,
	AccountStore &accounts,
	SendWorkerPool &senders,
	std::uint32_t fps) noexcept
	: ajy::concurrency::Group<ajy::network::windows::iocp::NetServer>(server, fps)
	, accounts(accounts)
	, senders(senders)
	, session_count(0)
{
}

EchoGroup::~EchoGroup(void) noexcept
{
}

std::uint32_t EchoGroup::get_session_count(void) const noexcept
{
	return this->session_count.load(std::memory_order_relaxed);
}

void EchoGroup::on_enter(SessionID id) noexcept
{
	std::int64_t account_no;

	this->session_count.fetch_add(1, std::memory_order_relaxed);

	if (!this->accounts.take(id, account_no))
	{
		this->server.disconnect(id);
		return;
	}

	this->send_res_login(id, account_no);
}

void EchoGroup::on_leave(SessionID id) noexcept
{
	(void)id;

	this->session_count.fetch_sub(1, std::memory_order_relaxed);
}

void EchoGroup::on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
{
	PacketType type;
	std::int64_t account_no;
	std::int64_t send_time;

	if (packet->get_data_size() != EchoServerConfig::REQ_ECHO_SIZE)
	{
		this->server.disconnect(id);
		return;
	}

	*packet >> type;
	if (type != PacketType::REQ_ECHO)
	{
		this->server.disconnect(id);
		return;
	}

	*packet >> account_no;
	*packet >> send_time;

	this->send_res_echo(id, account_no, send_time);
}

void EchoGroup::on_frame(typename ServerClock::duration elapsed) noexcept
{
	(void)elapsed;
}

void EchoGroup::send_res_login(SessionID id, std::int64_t account_no) noexcept
{
	std::shared_ptr<Packet> packet;

	packet = this->server.alloc_packet(EchoServerConfig::MAX_PACKET_PAYLOAD);
	if (!packet)
		return;

	*packet << PacketType::RES_LOGIN;
	*packet << LoginStatus::OK;
	*packet << account_no;

	this->senders.post(id, std::move(packet));
}

void EchoGroup::send_res_echo(SessionID id, std::int64_t account_no, std::int64_t send_time) noexcept
{
	std::shared_ptr<Packet> packet;

	packet = this->server.alloc_packet(EchoServerConfig::MAX_PACKET_PAYLOAD);
	if (!packet)
		return;

	*packet << PacketType::RES_ECHO;
	*packet << account_no;
	*packet << send_time;

	this->senders.post(id, std::move(packet));
}
