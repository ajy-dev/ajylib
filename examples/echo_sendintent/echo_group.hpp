/**
 * File: echo_group.hpp
 * Path: ajylib/examples/echo_sendintent/echo_group.hpp
 * Description:
 *	The content group of the echo_sendintent example. It decides what to
 *	send and hands those fields to a SendWorkerPool; it never allocates or
 *	serializes a packet itself.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_GROUP_HPP
#define ECHO_GROUP_HPP

#include "account_store.hpp"
#include "send_worker_pool.hpp"

#include <ajy/concurrency/group.hpp>
#include <ajy/network/windows/iocp/net_server.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

class EchoGroup : public ajy::concurrency::Group<ajy::network::windows::iocp::NetServer>
{
public:
	EchoGroup(
		ajy::network::windows::iocp::NetServer &server,
		AccountStore &accounts,
		SendWorkerPool &senders,
		std::uint32_t fps) noexcept;
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
	AccountStore &accounts;
	SendWorkerPool &senders;
	std::atomic<std::uint32_t> session_count;
};

#endif
