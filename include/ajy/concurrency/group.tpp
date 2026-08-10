/**
 * File: group.tpp
 * Path: ajylib/include/ajy/concurrency/group.tpp
 * Description:
 * 	A serial processing unit definition.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
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
		Job *job;

		job = this->job_pool.create(JobType::Enter, id);
		if (!job)
		{
			std::fprintf(stderr, "Group::post_enter(): job_pool.create() failed.\n");
			std::terminate();
		}

		if (!this->jobs.enqueue(job))
		{
			std::fprintf(stderr, "Group::post_enter(): jobs.enqueue() failed.\n");
			std::terminate();
		}

		this->wake_thread();
	}

	template <typename TServer>
	void Group<TServer>::post_leave(SessionID id) noexcept
	{
		Job *job;

		job = this->job_pool.create(JobType::Leave, id);
		if (!job)
		{
			std::fprintf(stderr, "Group::post_leave(): job_pool.create() failed.\n");
			std::terminate();
		}

		if (!this->jobs.enqueue(job))
		{
			std::fprintf(stderr, "Group::post_leave(): jobs.enqueue() failed.\n");
			std::terminate();
		}

		this->wake_thread();
	}

	template <typename TServer>
	void Group<TServer>::post_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
	{
		Job *job;

		job = this->job_pool.create(JobType::Recv, id, std::move(packet));
		if (!job)
		{
			std::fprintf(stderr, "Group::post_recv(): job_pool.create() failed.\n");
			std::terminate();
		}

		if (!this->jobs.enqueue(job))
		{
			std::fprintf(stderr, "Group::post_recv(): jobs.enqueue() failed.\n");
			std::terminate();
		}

		this->wake_thread();
	}
	template <typename TServer>
	std::uint32_t Group<TServer>::get_frame_tps(void) noexcept
	{
		return calculate_tps(this->frame_count, this->last_frame_query, this->last_frame_tps);
	}

	template <typename TServer>
	std::size_t Group<TServer>::get_job_pool_in_use(void) const noexcept
	{
		return this->job_pool.get_in_use_count();
	}

	template <typename TServer>
	void Group<TServer>::move_session(SessionID id, Group &destination) noexcept
	{
		if (!this->sessions.erase(id))
			return;

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
	void Group<TServer>::start(void) noexcept
	{
		if (this->thread.joinable())
			return;

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
		std::optional<Job *> job;

		while ((job = this->jobs.dequeue()).has_value())
		{
			SessionID id;

			id = job.value()->session_id;

			switch (job.value()->type)
			{
			case JobType::Enter:
				if (!this->server.set_session_group(id, this))
					break;

				try
				{
					this->sessions.insert(id);
				}
				catch (const std::bad_alloc &error)
				{
					std::fprintf(stderr, "Group::drain_jobs(): std::unordered_set::insert failed: %s\n", error.what());
					std::terminate();
				}

				this->on_enter(id);
				break;

			case JobType::Leave:
				if (this->sessions.erase(id))
					this->on_leave(id);
				break;

			case JobType::Recv:
				if (this->sessions.count(id))
					this->on_recv(id, std::move(job.value()->packet));
				break;
			}

			this->job_pool.destroy(job.value());
		}
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
