/**
 * File: echo_globallock_server.hpp
 * Path: ajylib/examples/echo_globallock/echo_globallock_server.hpp
 * Description:
 *	An echo server whose packet handling runs on the IOCP worker threads but
 *	is serialized by one global lock.
 * Note:
 *	Measurement scaffolding. It stands in for "IOCP workers as the group's
 *	executor, serialized by a per-group lock" and, since the whole handler
 *	including the send call sits inside the lock, its throughput is directly
 *	comparable to a single group thread's -- but with no job queue and no
 *	hand-off between threads. Sends go to a SendWorkerPool, matching
 *	echo_sendworker, so the locked section covers the same work a group
 *	thread does there.
 *	No idle timeout, matching echo_with_group.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_GLOBALLOCK_SERVER_HPP
#define ECHO_GLOBALLOCK_SERVER_HPP

#include "send_worker_pool.hpp"

#include <ajy/network/windows/iocp/net_server.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>

class EchoGlobalLockServer : public ajy::network::windows::iocp::NetServer
{
public:
	explicit EchoGlobalLockServer(std::string_view logger_name) noexcept;
	~EchoGlobalLockServer(void) noexcept override;

	EchoGlobalLockServer(const EchoGlobalLockServer &other) = delete;
	EchoGlobalLockServer &operator=(const EchoGlobalLockServer &other) = delete;
	EchoGlobalLockServer(EchoGlobalLockServer &&other) = delete;
	EchoGlobalLockServer &operator=(EchoGlobalLockServer &&other) = delete;

	bool start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept override;
	void stop(void) noexcept override;

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
	void handle_req_login(SessionID id, Packet *packet) noexcept;
	void handle_req_echo(SessionID id, Packet *packet) noexcept;

	SendWorkerPool senders;
	std::mutex global_lock;

	std::atomic<std::uint32_t> send_completion_count;
	std::atomic<std::uint64_t> send_completion_bytes;
	std::atomic<ServerClock::time_point> last_send_batching_query;
};

#endif
