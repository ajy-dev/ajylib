/**
 * File: echo_sendintent_server.cpp
 * Path: ajylib/examples/echo_sendintent/echo_sendintent_server.cpp
 * Description:
 *	A variant of echo_with_group whose group threads never issue a send
 *	syscall.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: 2026-08-14
 * Version: 0.1.0
 */

#include "echo_sendintent_server.hpp"

#include "echo_server_config.hpp"
#include "protocol.hpp"

#include <ajy/utility/logger.hpp>

#include <chrono>
#include <cstdint>

#include <cstdio>
#include <exception>
#include <new>

EchoSendIntentServer::EchoSendIntentServer(std::string_view logger_name) noexcept
	: ajy::network::windows::iocp::NetServer(
		  logger_name,
		  EchoServerConfig::PROTOCOL_CODE,
		  EchoServerConfig::FIXED_KEY,
		  EchoServerConfig::MAX_PACKET_PAYLOAD)
	, orphan_recv_count(0)
	, orphan_join_count(0)
	, senders(*this, EchoServerConfig::SEND_WORKER_COUNT)
	, auth(*this, this->echoes, this->accounts, EchoServerConfig::AUTH_GROUP_FPS)
	, send_completion_count(0)
	, send_completion_bytes(0)
	, last_send_batching_query(ServerClock::now())
{
	std::size_t i;

	try
	{
		this->echoes.reserve(EchoServerConfig::ECHO_GROUP_COUNT);

		for (i = 0; i < EchoServerConfig::ECHO_GROUP_COUNT; ++i)
			this->echoes.push_back(
				std::make_unique<EchoGroup>(*this, this->accounts, this->senders, EchoServerConfig::ECHO_GROUP_FPS));
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "EchoSendIntentServer::EchoSendIntentServer(): echo group allocation failed: %s\n", error.what());
		std::terminate();
	}

	this->add_group(this->auth);

	for (i = 0; i < this->echoes.size(); ++i)
		this->add_group(*this->echoes[i]);
}

EchoSendIntentServer::~EchoSendIntentServer(void) noexcept
{
	this->stop();
}

bool EchoSendIntentServer::start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept
{
	this->senders.start();

	return ajy::network::windows::iocp::NetServer::start(bind_ip, port, worker_thread_count, nagle, max_sessions);
}

void EchoSendIntentServer::stop(void) noexcept
{
	ajy::network::windows::iocp::NetServer::stop();

	this->senders.stop();
}

std::uint32_t EchoSendIntentServer::get_auth_frame_tps(void) noexcept
{
	return this->auth.get_frame_tps();
}

std::uint32_t EchoSendIntentServer::get_echo_frame_tps(std::size_t shard) noexcept
{
	return this->echoes[shard]->get_frame_tps();
}

std::uint64_t EchoSendIntentServer::get_auth_session_count(void) const noexcept
{
	return this->auth.get_session_count();
}

std::uint64_t EchoSendIntentServer::get_auth_enter_count(void) const noexcept
{
	return this->auth.get_enter_count();
}

std::uint64_t EchoSendIntentServer::get_auth_leave_count(void) const noexcept
{
	return this->auth.get_leave_count();
}

std::uint64_t EchoSendIntentServer::get_echo_session_count(std::size_t shard) const noexcept
{
	return this->echoes[shard]->get_session_count();
}

std::uint64_t EchoSendIntentServer::get_echo_enter_count(std::size_t shard) const noexcept
{
	return this->echoes[shard]->get_enter_count();
}

std::uint64_t EchoSendIntentServer::get_echo_leave_count(std::size_t shard) const noexcept
{
	return this->echoes[shard]->get_leave_count();
}

std::size_t EchoSendIntentServer::get_echo_group_count(void) const noexcept
{
	return this->echoes.size();
}

std::size_t EchoSendIntentServer::get_echo_job_pool_in_use(std::size_t shard) const noexcept
{
	return this->echoes[shard]->get_queued_job_count();
}

std::size_t EchoSendIntentServer::get_echo_rejected_session_count(std::size_t shard) const noexcept
{
	return this->echoes[shard]->get_rejected_session_count();
}

std::size_t EchoSendIntentServer::get_send_worker_count(void) const noexcept
{
	return this->senders.get_worker_count();
}

bool EchoSendIntentServer::is_send_queue_empty(std::size_t worker) const noexcept
{
	return this->senders.is_queue_empty(worker);
}

bool EchoSendIntentServer::on_connection_request(const char *ip, std::uint16_t port)
{
	(void)ip;
	(void)port;

	return true;
}

void EchoSendIntentServer::on_client_join(SessionID id) noexcept
{
	this->auth.post_enter(id);
}

void EchoSendIntentServer::on_client_leave(SessionID id) noexcept
{
	this->accounts.remove(id);
}

void EchoSendIntentServer::on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
{
	// A session belongs to no group between on_client_join and the auth group's
	// on_enter, and again between on_leave and the destination's on_enter. A
	// conforming client sends nothing in the second window, but the first one
	// opens before the client has heard anything at all -- so log what actually
	// arrives here rather than guessing.
	PacketType type;
	std::size_t size;

	this->orphan_recv_count.fetch_add(1, std::memory_order_relaxed);

	size = packet->get_data_size();

	type = static_cast<PacketType>(0);
	if (size >= sizeof(type))
		*packet >> type;

	this->logger->log(
		ajy::utility::Logger::LogLevel::Warning,
		"on_recv(): packet arrived while the session belonged to no group. type: %u, size: %zu, id: %llu",
		static_cast<unsigned int>(type),
		size,
		static_cast<unsigned long long>(id));
}

std::size_t EchoSendIntentServer::get_orphan_recv_count(void) const noexcept
{
	return this->orphan_recv_count.load(std::memory_order_relaxed);
}

std::size_t EchoSendIntentServer::get_orphan_join_count(void) const noexcept
{
	return this->orphan_join_count.load(std::memory_order_relaxed);
}

std::size_t EchoSendIntentServer::get_account_store_size(void) noexcept
{
	return this->accounts.get_size();
}

std::size_t EchoSendIntentServer::get_echo_account_miss_count(std::size_t shard) const noexcept
{
	return this->echoes[shard]->get_account_miss_count();
}

std::size_t EchoSendIntentServer::get_echo_stale_enter_count(std::size_t shard) const noexcept
{
	return this->echoes[shard]->get_stale_enter_count();
}

void EchoSendIntentServer::on_send(SessionID id, std::size_t size) noexcept
{
	(void)id;

	this->send_completion_count.fetch_add(1, std::memory_order_relaxed);
	this->send_completion_bytes.fetch_add(size, std::memory_order_relaxed);
}

void EchoSendIntentServer::query_send_batching(std::uint32_t &completions_per_second, std::size_t &mean_size) noexcept
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

void EchoSendIntentServer::on_worker_thread_begin(void) noexcept
{
}

void EchoSendIntentServer::on_worker_thread_end(void) noexcept
{
}