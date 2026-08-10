/**
 * File: echo_baseline_server.hpp
 * Path: ajylib/examples/echo_baseline/echo_baseline_server.hpp
 * Description:
 *	A baseline echo server that handles every packet directly on the IOCP
 *	worker thread, with no content group in between.
 * Note:
 *	Exists to measure the ceiling of the transport path itself, so that
 *	echo_with_group's throughput can be read against it. It keeps no
 *	per-session state at all: REQ_LOGIN and REQ_ECHO are answered from the
 *	request payload alone, so no synchronization is needed.
 *	No idle timeout, matching echo_with_group.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ECHO_BASELINE_SERVER_HPP
#define ECHO_BASELINE_SERVER_HPP

#include <ajy/network/windows/iocp/net_server.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

class EchoBaselineServer : public ajy::network::windows::iocp::NetServer
{
public:
	explicit EchoBaselineServer(std::string_view logger_name) noexcept;
	~EchoBaselineServer(void) noexcept override;

	EchoBaselineServer(const EchoBaselineServer &other) = delete;
	EchoBaselineServer &operator=(const EchoBaselineServer &other) = delete;
	EchoBaselineServer(EchoBaselineServer &&other) = delete;
	EchoBaselineServer &operator=(EchoBaselineServer &&other) = delete;

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
};

#endif
