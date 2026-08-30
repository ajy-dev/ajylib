/**
 * File: echo_group.hpp
 * Path: ajylib/examples/echo_with_group/echo_group.hpp
 * Description:
 *	The content group of the echo_with_group example. It builds the reply
 *	packet itself and hands it to a SendWorkerPool, so only the send call
 *	leaves the group thread.
 * Author: ajy-dev
 * Created: 2026-08-14
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

	std::size_t get_account_miss_count(void) const noexcept;

protected:
	void on_enter(SessionID id) noexcept override;
	void on_leave(SessionID id) noexcept override;
	void on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept override;
	void on_frame(typename ServerClock::duration elapsed) noexcept override;

private:
	void send_res_login(SessionID id, std::int64_t account_no) noexcept;
	void send_res_echo(SessionID id, std::int64_t account_no, std::int64_t send_time) noexcept;

	AccountStore &accounts;
	SendWorkerPool &senders;
	std::atomic<std::size_t> account_miss_count;
};

#endif
