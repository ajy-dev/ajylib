/**
 * File: send_worker_pool.hpp
 * Path: ajylib/examples/echo_sendworker/send_worker_pool.hpp
 * Description:
 *	Takes a built packet and issues the send call on its own threads.
 * Note:
 *	Measurement scaffolding. The group thread still allocates and serializes;
 *	only send_packet leaves it. Pairs with echo_sendintent, which also moves
 *	allocation and serialization off the group thread.
 *	A session is pinned to one worker so its packets keep their order.
 * Author: ajy-dev
 * Created: 2026-08-14
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef SEND_WORKER_POOL_HPP
#define SEND_WORKER_POOL_HPP

#include <ajy/container/lockfree/queue.hpp>
#include <ajy/network/windows/iocp/net_server.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

class SendWorkerPool
{
public:
	using SessionID = ajy::network::windows::iocp::NetServer::SessionID;
	using Packet = ajy::network::windows::iocp::NetServer::Packet;

	SendWorkerPool(ajy::network::windows::iocp::NetServer &server, std::size_t worker_count) noexcept;
	~SendWorkerPool(void) noexcept;

	SendWorkerPool(const SendWorkerPool &other) = delete;
	SendWorkerPool &operator=(const SendWorkerPool &other) = delete;
	SendWorkerPool(SendWorkerPool &&other) = delete;
	SendWorkerPool &operator=(SendWorkerPool &&other) = delete;

	void start(void) noexcept;
	void stop(void) noexcept;

	void post(SessionID id, std::shared_ptr<Packet> packet) noexcept;

	std::size_t get_worker_count(void) const noexcept;
	bool is_queue_empty(std::size_t worker) const noexcept;

private:
	struct Job
	{
		SessionID session_id;
		std::shared_ptr<Packet> packet;
	};

	struct Worker
	{
		ajy::container::lockfree::Queue<Job> jobs;
		std::atomic<bool> wake;
		std::thread thread;
	};

	static void thread_proc(SendWorkerPool *pool, Worker *worker) noexcept;

	void dispatch(Job &job) noexcept;

	ajy::network::windows::iocp::NetServer &server;
	std::vector<std::unique_ptr<Worker>> workers;
	std::atomic<bool> running;
};

#endif
