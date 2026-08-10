/**
 * File: echo_group.hpp
 * Path: ajylib/examples/echo_with_group/echo_group.hpp
 * Description:
 *	The content group of the echo_with_group example. Sends the login
 *	response on entry, then echoes every request back to its sender.
 * Note:
 *	RES_LOGIN is sent here rather than by AuthGroup, so the client is told
 *	the move is complete only after this group has registered the session.
 *	A client that follows the protocol sends nothing between REQ_LOGIN and
 *	RES_LOGIN, so no echo request can arrive before this group is ready.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_GROUP_HPP
#define ECHO_GROUP_HPP

#include "account_store.hpp"

#include <ajy/concurrency/group.hpp>
#include <ajy/network/windows/iocp/net_server.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

class EchoGroup : public ajy::concurrency::Group<ajy::network::windows::iocp::NetServer>
{
public:
	EchoGroup(ajy::network::windows::iocp::NetServer &server, AccountStore &accounts, std::uint32_t fps) noexcept;
	~EchoGroup(void) noexcept override;

	EchoGroup(const EchoGroup &other) = delete;
	EchoGroup &operator=(const EchoGroup &other) = delete;
	EchoGroup(EchoGroup &&other) = delete;
	EchoGroup &operator=(EchoGroup &&other) = delete;

	std::uint32_t get_session_count(void) const noexcept;

protected:
	void on_enter(SessionID id) noexcept override;
	void on_leave(SessionID id) noexcept override;
	void on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept override;
	void on_frame(typename ServerClock::duration elapsed) noexcept override;

private:
	void send_res_login(SessionID id, std::int64_t account_no) noexcept;
	void send_res_echo(SessionID id, std::int64_t account_no, std::int64_t send_time) noexcept;

	AccountStore &accounts;
	std::atomic<std::uint32_t> session_count;
};

#endif
