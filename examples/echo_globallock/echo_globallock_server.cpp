/**
 * File: echo_globallock_server.cpp
 * Path: ajylib/examples/echo_globallock/echo_globallock_server.cpp
 * Description:
 *	An echo server serialized by one global lock.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#include "echo_globallock_server.hpp"

#include "echo_server_config.hpp"
#include "protocol.hpp"

#include <ajy/utility/logger.hpp>

#include <chrono>
#include <cstdint>

#include <cstddef>
#include <mutex>
#include <utility>

EchoGlobalLockServer::EchoGlobalLockServer(std::string_view logger_name) noexcept
	: ajy::network::windows::iocp::NetServer(
		  logger_name,
		  EchoServerConfig::PROTOCOL_CODE,
		  EchoServerConfig::FIXED_KEY,
		  EchoServerConfig::MAX_PACKET_PAYLOAD)
	, senders(*this, EchoServerConfig::SEND_WORKER_COUNT)
	, send_completion_count(0)
	, send_completion_bytes(0)
	, last_send_batching_query(ServerClock::now())
{
}

EchoGlobalLockServer::~EchoGlobalLockServer(void) noexcept
{
	this->stop();
}

bool EchoGlobalLockServer::start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept
{
	this->senders.start();

	return ajy::network::windows::iocp::NetServer::start(bind_ip, port, worker_thread_count, nagle, max_sessions);
}

void EchoGlobalLockServer::stop(void) noexcept
{
	ajy::network::windows::iocp::NetServer::stop();

	this->senders.stop();
}

std::size_t EchoGlobalLockServer::get_send_worker_count(void) const noexcept
{
	return this->senders.get_worker_count();
}

bool EchoGlobalLockServer::is_send_queue_empty(std::size_t worker) const noexcept
{
	return this->senders.is_queue_empty(worker);
}

bool EchoGlobalLockServer::on_connection_request(const char *ip, std::uint16_t port)
{
	(void)ip;
	(void)port;

	return true;
}

void EchoGlobalLockServer::on_client_join(SessionID id) noexcept
{
	(void)id;
}

void EchoGlobalLockServer::on_client_leave(SessionID id) noexcept
{
	(void)id;
}

void EchoGlobalLockServer::on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
{
	std::lock_guard<std::mutex> guard(this->global_lock);
	PacketType type;

	if (packet->get_data_size() < sizeof(type))
	{
		this->disconnect(id);
		return;
	}

	*packet >> type;

	if (type == PacketType::REQ_ECHO && packet->get_data_size() == EchoServerConfig::REQ_ECHO_SIZE - sizeof(type))
		this->handle_req_echo(id, packet.get());
	else if (type == PacketType::REQ_LOGIN && packet->get_data_size() == EchoServerConfig::REQ_LOGIN_SIZE - sizeof(type))
		this->handle_req_login(id, packet.get());
	else
		this->disconnect(id);
}

void EchoGlobalLockServer::on_send(SessionID id, std::size_t size) noexcept
{
	(void)id;

	this->send_completion_count.fetch_add(1, std::memory_order_relaxed);
	this->send_completion_bytes.fetch_add(size, std::memory_order_relaxed);
}

void EchoGlobalLockServer::query_send_batching(std::uint32_t &completions_per_second, std::size_t &mean_size) noexcept
{
	ServerClock::time_point now;
	ServerClock::time_point previous;
	std::chrono::duration<double> elapsed;
	std::uint32_t count;
	std::uint64_t bytes;

	now = ServerClock::now();
	previous = this->last_send_batching_query.exchange(now, std::memory_order_relaxed);
	count = this->send_completion_count.exchange(0, std::memory_order_relaxed);
	bytes = this->send_completion_bytes.exchange(0, std::memory_order_relaxed);

	elapsed = now - previous;
	completions_per_second = (elapsed.count() > 0.0)
		? static_cast<std::uint32_t>(static_cast<double>(count) / elapsed.count())
		: 0;
	mean_size = count ? static_cast<std::size_t>(bytes / count) : 0;
}

void EchoGlobalLockServer::on_worker_thread_begin(void) noexcept
{
}

void EchoGlobalLockServer::on_worker_thread_end(void) noexcept
{
}

void EchoGlobalLockServer::handle_req_login(SessionID id, Packet *packet) noexcept
{
	std::shared_ptr<Packet> reply;
	std::int64_t account_no;

	*packet >> account_no;

	reply = this->alloc_packet(EchoServerConfig::MAX_PACKET_PAYLOAD);
	if (!reply)
		return;

	*reply << PacketType::RES_LOGIN;
	*reply << LoginStatus::OK;
	*reply << account_no;

	this->senders.post(id, std::move(reply));
}

void EchoGlobalLockServer::handle_req_echo(SessionID id, Packet *packet) noexcept
{
	std::shared_ptr<Packet> reply;
	std::int64_t account_no;
	std::int64_t send_time;

	*packet >> account_no;
	*packet >> send_time;

	reply = this->alloc_packet(EchoServerConfig::MAX_PACKET_PAYLOAD);
	if (!reply)
		return;

	*reply << PacketType::RES_ECHO;
	*reply << account_no;
	*reply << send_time;

	this->senders.post(id, std::move(reply));
}
