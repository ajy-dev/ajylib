/**
 * File: send_worker_pool.cpp
 * Path: ajylib/examples/echo_with_group_intent/send_worker_pool.cpp
 * Description:
 *	Moves send_packet off the group threads.
 * Author: ajy-dev
 * Created: 2026-08-14
 * Updated: Never
 * Version: 0.1.0
 */

#include "send_worker_pool.hpp"

#include "echo_server_config.hpp"

#include <cstdio>
#include <exception>
#include <new>
#include <optional>
#include <system_error>
#include <utility>

SendWorkerPool::SendWorkerPool(ajy::network::windows::iocp::NetServer &server, std::size_t worker_count) noexcept
	: server(server)
	, running(false)
{
	std::size_t i;

	try
	{
		this->workers.reserve(worker_count);

		for (i = 0; i < worker_count; ++i)
			this->workers.push_back(std::make_unique<Worker>());
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "SendWorkerPool::SendWorkerPool(): worker allocation failed: %s\n", error.what());
		std::terminate();
	}
}

SendWorkerPool::~SendWorkerPool(void) noexcept
{
	this->stop();
}

void SendWorkerPool::start(void) noexcept
{
	std::size_t i;

	this->running.store(true, std::memory_order_relaxed);

	try
	{
		for (i = 0; i < this->workers.size(); ++i)
			this->workers[i]->thread = std::thread(thread_proc, this, this->workers[i].get());
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "SendWorkerPool::start(): std::thread failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
}

void SendWorkerPool::stop(void) noexcept
{
	std::size_t i;

	this->running.store(false, std::memory_order_relaxed);

	for (i = 0; i < this->workers.size(); ++i)
	{
		this->workers[i]->wake.store(true, std::memory_order_release);
		this->workers[i]->wake.notify_one();
	}

	for (i = 0; i < this->workers.size(); ++i)
	{
		if (!this->workers[i]->thread.joinable())
			continue;

		try
		{
			this->workers[i]->thread.join();
		}
		catch (const std::system_error &error)
		{
			std::fprintf(stderr, "SendWorkerPool::stop(): std::thread::join failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}
	}
}

void SendWorkerPool::post(SessionID id, PacketType type, std::int64_t account_no, std::int64_t send_time) noexcept
{
	Worker *worker;

	worker = this->workers[static_cast<std::size_t>(id) % this->workers.size()].get();

	if (!worker->jobs.enqueue(Job{id, type, account_no, send_time}))
	{
		std::fprintf(stderr, "SendWorkerPool::post(): jobs.enqueue() failed.\n");
		std::terminate();
	}

	if (!worker->wake.exchange(true, std::memory_order_release))
		worker->wake.notify_one();
}

void SendWorkerPool::dispatch(const Job &job) noexcept
{
	std::shared_ptr<Packet> packet;

	packet = this->server.alloc_packet(EchoServerConfig::MAX_PACKET_PAYLOAD);
	if (!packet)
		return;

	switch (job.type)
	{
	case PacketType::RES_LOGIN:
		*packet << PacketType::RES_LOGIN;
		*packet << LoginStatus::OK;
		*packet << job.account_no;
		break;

	case PacketType::RES_ECHO:
		*packet << PacketType::RES_ECHO;
		*packet << job.account_no;
		*packet << job.send_time;
		break;

	default:
		return;
	}

	this->server.send_packet(job.session_id, std::move(packet));
}

std::size_t SendWorkerPool::get_worker_count(void) const noexcept
{
	return this->workers.size();
}

bool SendWorkerPool::is_queue_empty(std::size_t worker) const noexcept
{
	return this->workers[worker]->jobs.is_empty();
}

void SendWorkerPool::thread_proc(SendWorkerPool *pool, Worker *worker) noexcept
{
	while (pool->running.load(std::memory_order_relaxed))
	{
		std::optional<Job> job;

		worker->wake.wait(false, std::memory_order_acquire);

		while ((job = worker->jobs.dequeue()).has_value())
			pool->dispatch(job.value());

		worker->wake.store(false, std::memory_order_release);

		while ((job = worker->jobs.dequeue()).has_value())
			pool->dispatch(job.value());
	}

	while (worker->jobs.dequeue().has_value())
		;
}
