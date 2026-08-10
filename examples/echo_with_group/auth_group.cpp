/**
 * File: auth_group.cpp
 * Path: ajylib/examples/echo_with_group/auth_group.cpp
 * Description:
 *	The authentication group of the echo_with_group example.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#include "auth_group.hpp"

#include "echo_server_config.hpp"
#include "protocol.hpp"

#include <ajy/utility/logger.hpp>

AuthGroup::AuthGroup(
	ajy::network::windows::iocp::NetServer &server,
	std::vector<std::unique_ptr<EchoGroup>> &echoes,
	AccountStore &accounts,
	std::uint32_t fps) noexcept
	: ajy::concurrency::Group<ajy::network::windows::iocp::NetServer>(server, fps)
	, echoes(echoes)
	, accounts(accounts)
	, session_count(0)
{
}

AuthGroup::~AuthGroup(void) noexcept
{
}

std::uint32_t AuthGroup::get_session_count(void) const noexcept
{
	return this->session_count.load(std::memory_order_relaxed);
}

void AuthGroup::on_enter(SessionID id) noexcept
{
	(void)id;

	this->session_count.fetch_add(1, std::memory_order_relaxed);
}

void AuthGroup::on_leave(SessionID id) noexcept
{
	(void)id;

	this->session_count.fetch_sub(1, std::memory_order_relaxed);
}

void AuthGroup::on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
{
	PacketType type;

	if (packet->get_data_size() != EchoServerConfig::REQ_LOGIN_SIZE)
	{
		this->server.disconnect(id);
		return;
	}

	*packet >> type;
	if (type != PacketType::REQ_LOGIN)
	{
		this->server.disconnect(id);
		return;
	}

	this->handle_req_login(id, packet.get());
}

void AuthGroup::on_frame(typename ServerClock::duration elapsed) noexcept
{
	(void)elapsed;
}

void AuthGroup::handle_req_login(SessionID id, Packet *packet) noexcept
{
	std::int64_t account_no;
	std::size_t shard;

	*packet >> account_no;

	this->accounts.put(id, account_no);

	shard = static_cast<std::size_t>(id) % this->echoes.size();
	this->move_session(id, *this->echoes[shard]);
}
