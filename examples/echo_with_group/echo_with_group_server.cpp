/**
 * File: echo_with_group_server.cpp
 * Path: ajylib/examples/echo_with_group/echo_with_group_server.cpp
 * Description:
 *	An echo server split into content groups (one auth group and a shard of
 *	echo groups).
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#include "echo_with_group_server.hpp"

#include "echo_server_config.hpp"

#include <ajy/utility/logger.hpp>

#include <chrono>
#include <cstdint>

#include <cstdio>
#include <exception>
#include <new>

EchoWithGroupServer::EchoWithGroupServer(std::string_view logger_name) noexcept
	: ajy::network::windows::iocp::NetServer(
		  logger_name,
		  EchoServerConfig::PROTOCOL_CODE,
		  EchoServerConfig::FIXED_KEY,
		  EchoServerConfig::MAX_PACKET_PAYLOAD)
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
			this->echoes.push_back(std::make_unique<EchoGroup>(*this, this->accounts, EchoServerConfig::ECHO_GROUP_FPS));
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "EchoWithGroupServer::EchoWithGroupServer(): echo group allocation failed: %s\n", error.what());
		std::terminate();
	}

	this->add_group(this->auth);

	for (i = 0; i < this->echoes.size(); ++i)
		this->add_group(*this->echoes[i]);
}

EchoWithGroupServer::~EchoWithGroupServer(void) noexcept
{
	this->stop();
}

std::uint32_t EchoWithGroupServer::get_auth_frame_tps(void) noexcept
{
	return this->auth.get_frame_tps();
}

std::uint32_t EchoWithGroupServer::get_echo_frame_tps(std::size_t shard) noexcept
{
	return this->echoes[shard]->get_frame_tps();
}

std::uint32_t EchoWithGroupServer::get_auth_session_count(void) const noexcept
{
	return this->auth.get_session_count();
}

std::uint32_t EchoWithGroupServer::get_echo_session_count(std::size_t shard) const noexcept
{
	return this->echoes[shard]->get_session_count();
}

std::size_t EchoWithGroupServer::get_echo_group_count(void) const noexcept
{
	return this->echoes.size();
}

std::size_t EchoWithGroupServer::get_echo_job_pool_in_use(std::size_t shard) const noexcept
{
	return this->echoes[shard]->get_job_pool_in_use();
}

std::size_t EchoWithGroupServer::get_auth_job_pool_in_use(void) const noexcept
{
	return this->auth.get_job_pool_in_use();
}

bool EchoWithGroupServer::on_connection_request(const char *ip, std::uint16_t port)
{
	(void)ip;
	(void)port;

	return true;
}

void EchoWithGroupServer::on_client_join(SessionID id) noexcept
{
	this->auth.post_enter(id);
}

void EchoWithGroupServer::on_client_leave(SessionID id) noexcept
{
	this->accounts.remove(id);
}

void EchoWithGroupServer::on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
{
	(void)id;
	(void)packet;
}

void EchoWithGroupServer::on_send(SessionID id, std::size_t size) noexcept
{
	(void)id;

	this->send_completion_count.fetch_add(1, std::memory_order_relaxed);
	this->send_completion_bytes.fetch_add(size, std::memory_order_relaxed);
}

void EchoWithGroupServer::query_send_batching(std::uint32_t &completions_per_second, std::size_t &mean_size) noexcept
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

void EchoWithGroupServer::on_worker_thread_begin(void) noexcept
{
}

void EchoWithGroupServer::on_worker_thread_end(void) noexcept
{
}
