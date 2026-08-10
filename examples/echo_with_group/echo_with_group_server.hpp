/**
 * File: echo_with_group_server.hpp
 * Path: ajylib/examples/echo_with_group/echo_with_group_server.hpp
 * Description:
 *	An echo server split into content groups (one auth group and a shard of
 *	echo groups) built on ajy::network::windows::iocp::NetServer.
 * Note:
 *	Every session enters the auth group on connect, so the server's own
 *	on_recv only sees packets that arrive while a session belongs to no
 *	group (i.e. mid-move); those are dropped.
 *	No idle timeout: the dummy client's loop may stall, and a timeout would
 *	disconnect it spuriously.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_WITH_GROUP_SERVER_HPP
#define ECHO_WITH_GROUP_SERVER_HPP

#include "account_store.hpp"
#include "auth_group.hpp"
#include "echo_group.hpp"

#include <ajy/network/windows/iocp/net_server.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

class EchoWithGroupServer : public ajy::network::windows::iocp::NetServer
{
public:
	explicit EchoWithGroupServer(std::string_view logger_name) noexcept;
	~EchoWithGroupServer(void) noexcept override;

	EchoWithGroupServer(const EchoWithGroupServer &other) = delete;
	EchoWithGroupServer &operator=(const EchoWithGroupServer &other) = delete;
	EchoWithGroupServer(EchoWithGroupServer &&other) = delete;
	EchoWithGroupServer &operator=(EchoWithGroupServer &&other) = delete;

	std::uint32_t get_auth_frame_tps(void) noexcept;
	std::uint32_t get_echo_frame_tps(std::size_t shard) noexcept;
	std::uint32_t get_auth_session_count(void) const noexcept;
	std::uint32_t get_echo_session_count(std::size_t shard) const noexcept;
	std::size_t get_echo_group_count(void) const noexcept;
	std::size_t get_echo_job_pool_in_use(std::size_t shard) const noexcept;
	std::size_t get_auth_job_pool_in_use(void) const noexcept;

protected:
	bool on_connection_request(const char *ip, std::uint16_t port) override;
	void on_client_join(SessionID id) noexcept override;
	void on_client_leave(SessionID id) noexcept override;
	void on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept override;
	void on_send(SessionID id, std::size_t size) noexcept override;
	void on_worker_thread_begin(void) noexcept override;
	void on_worker_thread_end(void) noexcept override;

private:
	AccountStore accounts;
	std::vector<std::unique_ptr<EchoGroup>> echoes;
	AuthGroup auth;
};

#endif
