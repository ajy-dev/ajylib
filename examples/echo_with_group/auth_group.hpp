/**
 * File: auth_group.hpp
 * Path: ajylib/examples/echo_with_group/auth_group.hpp
 * Description:
 *	The authentication group of the echo_with_group example. Every session
 *	enters here on connect and is moved to one of the echo groups once it
 *	presents a login request.
 * Note:
 *	The session key carried by REQ_LOGIN is not validated against anything;
 *	this example gates on packet shape only. The destination echo group is
 *	picked by SessionID so a session always lands on the same shard.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AUTH_GROUP_HPP
#define AUTH_GROUP_HPP

#include "account_store.hpp"
#include "echo_group.hpp"

#include <ajy/concurrency/group.hpp>
#include <ajy/network/windows/iocp/net_server.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class AuthGroup : public ajy::concurrency::Group<ajy::network::windows::iocp::NetServer>
{
public:
	AuthGroup(
		ajy::network::windows::iocp::NetServer &server,
		std::vector<std::unique_ptr<EchoGroup>> &echoes,
		AccountStore &accounts,
		std::uint32_t fps) noexcept;
	~AuthGroup(void) noexcept override;

	AuthGroup(const AuthGroup &other) = delete;
	AuthGroup &operator=(const AuthGroup &other) = delete;
	AuthGroup(AuthGroup &&other) = delete;
	AuthGroup &operator=(AuthGroup &&other) = delete;

	std::uint32_t get_session_count(void) const noexcept;

protected:
	void on_enter(SessionID id) noexcept override;
	void on_leave(SessionID id) noexcept override;
	void on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept override;
	void on_frame(typename ServerClock::duration elapsed) noexcept override;

private:
	void handle_req_login(SessionID id, Packet *packet) noexcept;

	std::vector<std::unique_ptr<EchoGroup>> &echoes;
	AccountStore &accounts;
	std::atomic<std::uint32_t> session_count;
};

#endif
