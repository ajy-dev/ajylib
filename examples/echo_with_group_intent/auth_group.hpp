/**
 * File: auth_group.hpp
 * Path: ajylib/examples/echo_with_group_intent/auth_group.hpp
 * Description:
 *	The authentication group of the echo_with_group_intent example. Every session
 *	enters here on connect and is moved to the echo group once it presents a
 *	login request.
 * Note:
 *	The session key carried by REQ_LOGIN is not validated against anything;
 *	this example gates on packet shape only.
 * Author: ajy-dev
 * Created: 2026-08-14
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AUTH_GROUP_HPP
#define AUTH_GROUP_HPP

#include "account_store.hpp"
#include "echo_group.hpp"

#include <ajy/concurrency/group.hpp>
#include <ajy/network/windows/iocp/net_server.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

class AuthGroup : public ajy::concurrency::Group<ajy::network::windows::iocp::NetServer>
{
public:
	AuthGroup(
		ajy::network::windows::iocp::NetServer &server,
		EchoGroup &echo,
		AccountStore &accounts,
		std::uint32_t fps) noexcept;
	~AuthGroup(void) noexcept override;

	AuthGroup(const AuthGroup &other) = delete;
	AuthGroup &operator=(const AuthGroup &other) = delete;
	AuthGroup(AuthGroup &&other) = delete;
	AuthGroup &operator=(AuthGroup &&other) = delete;

protected:
	void on_enter(SessionID id) noexcept override;
	void on_leave(SessionID id) noexcept override;
	void on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept override;
	void on_frame(typename ServerClock::duration elapsed) noexcept override;

private:
	void handle_req_login(SessionID id, Packet *packet) noexcept;

	EchoGroup &echo;
	AccountStore &accounts;
};

#endif
