/**
 * File: echo_baseline_sw_server.hpp
 * Path: ajylib/examples/echo_baseline_sw/echo_baseline_sw_server.hpp
 * Description:
 *	echo_baseline with a SendWorkerPool bolted on: no content group, but the
 *	send call is handed to a worker instead of being made in place.
 * Note:
 *	Isolates one variable. echo_baseline batches sends well (~28 responses
 *	per packet on the wire) while echo_sendworker does not (~2), and those
 *	two differ in both grouping and send handling. This one differs from
 *	echo_baseline in send handling alone, so its packets-per-response ratio
 *	says which of the two is responsible.
 *	No idle timeout, matching echo_with_group.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_BASELINE_SW_SERVER_HPP
#define ECHO_BASELINE_SW_SERVER_HPP

#include "send_worker_pool.hpp"

#include <ajy/network/windows/iocp/net_server.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

class EchoBaselineSwServer : public ajy::network::windows::iocp::NetServer
{
public:
	explicit EchoBaselineSwServer(std::string_view logger_name) noexcept;
	~EchoBaselineSwServer(void) noexcept override;

	EchoBaselineSwServer(const EchoBaselineSwServer &other) = delete;
	EchoBaselineSwServer &operator=(const EchoBaselineSwServer &other) = delete;
	EchoBaselineSwServer(EchoBaselineSwServer &&other) = delete;
	EchoBaselineSwServer &operator=(EchoBaselineSwServer &&other) = delete;

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

	std::atomic<std::uint32_t> send_completion_count;
	std::atomic<std::uint64_t> send_completion_bytes;
	std::atomic<ServerClock::time_point> last_send_batching_query;
};

#endif
