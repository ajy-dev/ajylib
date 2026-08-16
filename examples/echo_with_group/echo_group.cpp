/**
 * File: echo_group.cpp
 * Path: ajylib/examples/echo_with_group/echo_group.cpp
 * Description:
 *	The content group of the echo_with_group example.
 * Author: ajy-dev
 * Created: 2026-08-14
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
	: ajy::concurrency::Group<ajy::network::windows::iocp::NetServer>(server, fps, "echo_with_group")
	, accounts(accounts)
	, senders(senders)
	, account_miss_count(0)
{
}

EchoGroup::~EchoGroup(void) noexcept
{
}

std::size_t EchoGroup::get_account_miss_count(void) const noexcept
{
	return this->account_miss_count.load(std::memory_order_relaxed);
}

void EchoGroup::on_enter(SessionID id) noexcept
{
	std::int64_t account_no;

	// A miss means AuthGroup's entry went missing between put() and take() --
	// the session died mid-move and on_client_leave removed it first. The
	// session cannot be served without its account, so it is dropped, but the
	// count says how often that race actually fires.
	if (!this->accounts.take(id, account_no))
	{
		this->account_miss_count.fetch_add(1, std::memory_order_relaxed);
		this->server.disconnect(id);
		return;
	}

	this->send_res_login(id, account_no);
}

void EchoGroup::on_leave(SessionID id) noexcept
{
	(void)id;
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

