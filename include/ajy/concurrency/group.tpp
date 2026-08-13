/**
 * File: group.tpp
 * Path: ajylib/include/ajy/concurrency/group.tpp
 * Description:
 * 	A serial processing unit definition.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: 2026-08-14
 * Version: 0.1.0
 */

#ifndef AJY_CONCURRENCY_GROUP_TPP
#define AJY_CONCURRENCY_GROUP_TPP

#include <ajy/concurrency/group.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>

namespace ajy::concurrency
{
	template <typename TServer>
	Group<TServer>::Group(TServer &server, std::uint32_t fps) noexcept
		: server(server)
		, frame_interval(calculate_frame_interval(fps))
		, frame_count(0)
		, last_frame_tps(0)
		, last_frame_query(ServerClock::now())
		, jobs(JOB_QUEUE_CAPACITY)
		, rejected_session_count(0)
		, stale_enter_count(0)
		, reject_reported(false)
		, backlog_reported(false)
		, session_count(0)
		, enter_count(0)
		, leave_count(0)
		, wake(false)
		, running(false)
	{
	}

	template <typename TServer>
	Group<TServer>::~Group(void) noexcept
	{
	}

	template <typename TServer>
	void Group<TServer>::post_enter(SessionID id) noexcept
	{
		std::size_t size;

		if (!this->jobs.enqueue(Job(JobType::Enter, id)))
		{
			this->reject_session("post_enter", id);
			return;
		}

		this->server.set_session_group(id, this);

		size = this->jobs.get_size();
		if (size > QUEUE_WARNING_THRESHOLD)
			this->report_backlog("post_enter", size);

		this->wake_thread();
	}

	template <typename TServer>
	void Group<TServer>::post_leave(SessionID id) noexcept
	{
		std::size_t size;

		if (!this->jobs.enqueue(Job(JobType::Leave, id)))
		{
			this->reject_session("post_leave", id);
			return;
		}

		size = this->jobs.get_size();
		if (size > QUEUE_WARNING_THRESHOLD)
			this->report_backlog("post_leave", size);

		this->wake_thread();
	}

	template <typename TServer>
	void Group<TServer>::post_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
	{
		std::size_t size;

		if (!this->jobs.enqueue(Job(JobType::Recv, id, std::move(packet))))
		{
			this->reject_session("post_recv", id);
			return;
		}

		size = this->jobs.get_size();
		if (size > QUEUE_WARNING_THRESHOLD)
			this->report_backlog("post_recv", size);

		this->wake_thread();
	}
	template <typename TServer>
	std::uint32_t Group<TServer>::get_frame_tps(void) noexcept
	{
		return calculate_tps(this->frame_count, this->last_frame_query, this->last_frame_tps);
	}

	template <typename TServer>
	std::size_t Group<TServer>::get_queued_job_count(void) const noexcept
	{
		return this->jobs.get_size();
	}

	template <typename TServer>
	std::size_t Group<TServer>::get_rejected_session_count(void) const noexcept
	{
		return this->rejected_session_count.load(std::memory_order_relaxed);
	}

	template <typename TServer>
	std::size_t Group<TServer>::get_stale_enter_count(void) const noexcept
	{
		return this->stale_enter_count.load(std::memory_order_relaxed);
	}

	template <typename TServer>
	std::uint64_t Group<TServer>::get_session_count(void) const noexcept
	{
		return this->session_count.load(std::memory_order_relaxed);
	}

	template <typename TServer>
	std::uint64_t Group<TServer>::get_enter_count(void) const noexcept
	{
		return this->enter_count.load(std::memory_order_relaxed);
	}

	template <typename TServer>
	std::uint64_t Group<TServer>::get_leave_count(void) const noexcept
	{
		return this->leave_count.load(std::memory_order_relaxed);
	}

	template <typename TServer>
	void Group<TServer>::move_session(SessionID id, Group &destination) noexcept
	{
		std::uint32_t index;

		index = unpack_index(id);
		if (index >= this->session_generations.size())
			return;

		if (this->session_generations[index] != unpack_generation(id))
			return;

		this->session_generations[index] = EMPTY_GENERATION;
		this->session_count.fetch_sub(1, std::memory_order_relaxed);
		this->leave_count.fetch_add(1, std::memory_order_relaxed);
		this->on_leave(id);
		this->server.set_session_group(id, nullptr);
		destination.post_enter(id);
	}

	template <typename TServer>
	Group<TServer>::Job::Job(JobType type, SessionID session_id, std::unique_ptr<Packet> packet) noexcept
		: type(type)
		, session_id(session_id)
		, packet(std::move(packet))
	{
	}

	template <typename TServer>
	std::uint32_t Group<TServer>::unpack_index(SessionID id) noexcept
	{
		return static_cast<std::uint32_t>(id);
	}

	template <typename TServer>
	std::uint32_t Group<TServer>::unpack_generation(SessionID id) noexcept
	{
		static constexpr unsigned int GENERATION_SHIFT = 32;

		return static_cast<std::uint32_t>(id >> GENERATION_SHIFT);
	}

	template <typename TServer>
	typename Group<TServer>::ServerClock::duration Group<TServer>::calculate_frame_interval(std::uint32_t fps) noexcept
	{
		if (!fps)
			return ServerClock::duration::zero();

		return std::chrono::duration_cast<typename ServerClock::duration>(std::chrono::seconds(1)) / fps;
	}

	template <typename TServer>
	std::uint32_t Group<TServer>::calculate_tps(
		std::atomic<std::uint32_t> &counter,
		std::atomic<typename ServerClock::time_point> &last_query,
		std::atomic<std::uint32_t> &last_tps) noexcept
	{
		typename ServerClock::time_point now;
		typename ServerClock::time_point previous;
		std::uint32_t count;
		std::chrono::duration<double> elapsed;
		double tps;
		std::uint32_t result;

		now = ServerClock::now();
		previous = last_query.exchange(now, std::memory_order_relaxed);

		elapsed = now - previous;
		if (elapsed.count() <= 0.0)
			return last_tps.load(std::memory_order_relaxed);

		count = counter.exchange(0, std::memory_order_relaxed);

		tps = static_cast<double>(count) / elapsed.count();
		result = (tps > static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
			? std::numeric_limits<std::uint32_t>::max()
			: static_cast<std::uint32_t>(tps);

		last_tps.store(result, std::memory_order_relaxed);
		return result;
	}

	template <typename TServer>
	void Group<TServer>::thread_proc(Group *group) noexcept
	{
		typename ServerClock::time_point next_frame;
		typename ServerClock::time_point last_frame;

		next_frame = ServerClock::now();
		last_frame = next_frame;

		while (group->running.load(std::memory_order_relaxed))
		{
			try
			{
				std::unique_lock<std::mutex> lock(group->mutex);

				if (group->frame_interval == ServerClock::duration::zero())
				{
					group->condition.wait(
						lock,
						[group]
						{
							return group->wake || !group->running.load(std::memory_order_relaxed);
						});
				}
				else
				{
					group->condition.wait_until(
						lock,
						next_frame,
						[group]
						{
							return group->wake || !group->running.load(std::memory_order_relaxed);
						});
				}

				group->wake = false;
			}
			catch (const std::system_error &error)
			{
				std::fprintf(stderr, "Group::thread_proc(): std::unique_lock(mutex) failed: [Code: %d] %s\n", error.code().value(), error.what());
				std::terminate();
			}

			group->drain_jobs();

			if (group->frame_interval != ServerClock::duration::zero())
			{
				typename ServerClock::time_point now;

				now = ServerClock::now();
				if (now >= next_frame)
				{
					group->frame_count.fetch_add(1, std::memory_order_relaxed);
					group->on_frame(now - last_frame);
					last_frame = now;

					while (next_frame <= now)
						next_frame += group->frame_interval;
				}
			}
		}

		group->drain_jobs();
	}

	template <typename TServer>
	void Group<TServer>::start(std::uint32_t max_sessions) noexcept
	{
		if (this->thread.joinable())
			return;

		try
		{
			this->session_generations.assign(max_sessions, EMPTY_GENERATION);
		}
		catch (const std::bad_alloc &error)
		{
			std::fprintf(stderr, "Group::start(): std::vector::assign(session_generations) failed: %s\n", error.what());
			std::terminate();
		}

		this->running.store(true, std::memory_order_relaxed);

		try
		{
			this->thread = std::thread(thread_proc, this);
		}
		catch (const std::system_error &error)
		{
			std::fprintf(stderr, "Group::start(): std::thread failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}
	}

	template <typename TServer>
	void Group<TServer>::stop(void) noexcept
	{
		if (!this->thread.joinable())
			return;

		this->running.store(false, std::memory_order_relaxed);
		this->wake_thread();

		try
		{
			this->thread.join();
		}
		catch (const std::system_error &error)
		{
			std::fprintf(stderr, "Group::stop(): std::thread::join failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}
	}


	template <typename TServer>
	void Group<TServer>::drain_jobs(void) noexcept
	{
		std::optional<Job> dequeued;

		while ((dequeued = this->jobs.dequeue()).has_value())
		{
			Job *job;
			SessionID id;
			std::uint32_t index;
			std::uint32_t generation;

			job = &*dequeued;

			id = job->session_id;
			index = unpack_index(id);
			generation = unpack_generation(id);

			switch (job->type)
			{
			case JobType::Enter:
				// The session died between post_enter and here, so neither the
				// membership record nor on_enter should be created. Counting it
				// separates "the move raced a disconnect" from other losses.
				if (!this->server.set_session_group(id, this))
				{
					this->stale_enter_count.fetch_add(1, std::memory_order_relaxed);
					break;
				}

				this->session_generations[index] = generation;
				this->session_count.fetch_add(1, std::memory_order_relaxed);
				this->enter_count.fetch_add(1, std::memory_order_relaxed);
				this->on_enter(id);
				break;

			case JobType::Leave:
				if (this->session_generations[index] != generation)
					break;

				this->session_generations[index] = EMPTY_GENERATION;
				this->session_count.fetch_sub(1, std::memory_order_relaxed);
				this->leave_count.fetch_add(1, std::memory_order_relaxed);
				this->on_leave(id);
				break;

			case JobType::Recv:
				if (this->session_generations[index] != generation)
					break;

				this->on_recv(id, std::move(job->packet));
				break;
			}
		}

		this->reject_reported.store(false, std::memory_order_relaxed);
		this->backlog_reported.store(false, std::memory_order_relaxed);
	}

	template <typename TServer>
	void Group<TServer>::reject_session(const char *function_name, SessionID id) noexcept
	{
		utility::Logger *logger;
		std::size_t previous;

		this->server.disconnect(id);

		previous = this->rejected_session_count.fetch_add(1, std::memory_order_relaxed);

		if (previous / REJECT_LOG_INTERVAL == (previous + 1) / REJECT_LOG_INTERVAL)
			return;

		if (this->reject_reported.exchange(true, std::memory_order_relaxed))
			return;

		logger = utility::Logger::get(LOGGER_NAME);
		if (!logger)
			return;

		logger->log(
			utility::Logger::LogLevel::Error,
			"%s(): job queue full, disconnecting. rejected: %zu, capacity: %zu, id: %llu",
			function_name,
			previous + 1,
			this->jobs.get_capacity(),
			static_cast<unsigned long long>(id));
	}

	template <typename TServer>
	void Group<TServer>::report_backlog(const char *function_name, std::size_t size) noexcept
	{
		utility::Logger *logger;

		if (this->backlog_reported.exchange(true, std::memory_order_relaxed))
			return;

		logger = utility::Logger::get(LOGGER_NAME);
		if (!logger)
			return;

		logger->log(
			utility::Logger::LogLevel::Warning,
			"%s(): job queue backlog over threshold. size: %zu, capacity: %zu",
			function_name,
			size,
			this->jobs.get_capacity());
	}

	template <typename TServer>
	void Group<TServer>::wake_thread(void) noexcept
	{
		try
		{
			std::lock_guard<std::mutex> guard(this->mutex);

			this->wake = true;
		}
		catch (const std::system_error &error)
		{
			std::fprintf(stderr, "Group::wake_thread(): std::lock_guard(mutex) failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}

		this->condition.notify_one();
	}
}

#endif
