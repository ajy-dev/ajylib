/**
 * File: echo_sendintent_server.hpp
 * Path: ajylib/examples/echo_sendintent/echo_sendintent_server.hpp
 * Description:
 *	A variant of echo_with_group whose group threads never issue a send
 *	syscall: finished packets are handed to a SendWorkerPool instead.
 * Note:
 *	Measurement scaffolding. Run it against echo_with_group under the same
 *	dummy settings to size how much of a group thread's per-packet cost is
 *	the send path.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_SENDINTENT_SERVER_HPP
#define ECHO_SENDINTENT_SERVER_HPP

#include "account_store.hpp"
#include "auth_group.hpp"
#include "echo_group.hpp"
#include "send_worker_pool.hpp"

#include <ajy/network/windows/iocp/net_server.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

class EchoSendIntentServer : public ajy::network::windows::iocp::NetServer
{
public:
	explicit EchoSendIntentServer(std::string_view logger_name) noexcept;
	~EchoSendIntentServer(void) noexcept override;

	EchoSendIntentServer(const EchoSendIntentServer &other) = delete;
	EchoSendIntentServer &operator=(const EchoSendIntentServer &other) = delete;
	EchoSendIntentServer(EchoSendIntentServer &&other) = delete;
	EchoSendIntentServer &operator=(EchoSendIntentServer &&other) = delete;

	bool start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept override;
	void stop(void) noexcept override;

	std::uint32_t get_auth_frame_tps(void) noexcept;
	std::uint32_t get_echo_frame_tps(std::size_t shard) noexcept;
	std::uint32_t get_echo_session_count(std::size_t shard) const noexcept;
	std::size_t get_echo_group_count(void) const noexcept;
	std::size_t get_echo_job_pool_in_use(std::size_t shard) const noexcept;
	std::size_t get_send_worker_count(void) const noexcept;
	bool is_send_queue_empty(std::size_t worker) const noexcept;

	void query_send_batching(std::uint32_t &completions_per_second, std::size_t &mean_size) noexcept;

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
	SendWorkerPool senders;
	std::vector<std::unique_ptr<EchoGroup>> echoes;
	AuthGroup auth;

	std::atomic<std::uint32_t> send_completion_count;
	std::atomic<std::uint64_t> send_completion_bytes;
	std::atomic<ServerClock::time_point> last_send_batching_query;
};

#endif
