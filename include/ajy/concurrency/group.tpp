/**
 * File: group.tpp
 * Path: ajylib/include/ajy/concurrency/group.tpp
 * Description:
 *	A serial processing unit definition.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: 2026-08-30
 * Version: 0.1.0
 */

#ifndef AJY_CONCURRENCY_GROUP_TPP
#define AJY_CONCURRENCY_GROUP_TPP

#include <ajy/concurrency/group.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

namespace ajy::concurrency
{
	template <GroupServerHasNestedTypes TServer>
	Group<TServer>::Group(TServer &server, std::uint32_t fps, std::string_view logger_name, std::size_t job_queue_capacity) noexcept
	requires GroupServerHasFunctions<TServer>
		: server(server)
		, logger(utility::Logger::get(logger_name))
		, frame_interval(calculate_frame_interval(fps))
		, last_fps_query(ServerClock::now())
		, jobs(job_queue_capacity)
		, wake(false)
	{
		if (!this->logger)
		{
			std::size_t index;

			index = utility::Logger::create(logger_name);

			if (index == utility::Logger::DUPLICATE_NAME)
				this->logger = utility::Logger::get(logger_name);
			else if (index < utility::Logger::INVALID_INDEX)
				this->logger = utility::Logger::get(index);
			else
			{
				std::fprintf(
					stderr,
					"ajy::concurrency::Group::Group() failed: could not acquire logger \"%.*s\"\n",
					static_cast<int>(logger_name.size()),
					logger_name.data());
				std::terminate();
			}
		}
	}

	template <GroupServerHasNestedTypes TServer>
	Group<TServer>::~Group(void) noexcept
	{
		if (!this->thread.joinable())
			return;

		std::fprintf(stderr, "ajy::concurrency::Group::~Group() failed: destroyed while running.\n");
		std::terminate();
	}

	template <GroupServerHasNestedTypes TServer>
	void Group<TServer>::post_enter(SessionID id) noexcept
	{
		if (!this->running.load(std::memory_order_relaxed))
			return;

		if (this->jobs.enqueue(Job(JobType::Enter, id)))
		{
			this->server.set_session_group(id, this);
			this->wake_thread();
		}
		else
			this->handle_job_queue_full("post_enter", id);
	}

	template <GroupServerHasNestedTypes TServer>
	void Group<TServer>::post_leave(SessionID id) noexcept
	{
		if (!this->running.load(std::memory_order_acquire))
			return;

		if (!this->jobs.enqueue(Job(JobType::Leave, id)))
			this->reserved_jobs->enqueue(Job(JobType::Leave, id));

		this->wake_thread();
	}

	template <GroupServerHasNestedTypes TServer>
	void Group<TServer>::post_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
	{
		if (!this->running.load(std::memory_order_relaxed))
			return;

		if (this->jobs.enqueue(Job(JobType::Recv, id, std::move(packet))))
			this->wake_thread();
		else
			this->handle_job_queue_full("post_recv", id);
	}

	template <GroupServerHasNestedTypes TServer>
	std::uint32_t Group<TServer>::get_current_fps(void) noexcept
	{
		typename ServerClock::time_point now;
		typename ServerClock::time_point previous;
		std::uint32_t count;
		std::chrono::duration<double> elapsed;
		double fps;
		std::uint32_t result;

		now = ServerClock::now();
		previous = this->last_fps_query.exchange(now, std::memory_order_relaxed);

		elapsed = now - previous;
		if (elapsed.count() <= 0.0)
			return this->last_fps.load(std::memory_order_relaxed);

		count = this->frame_count.exchange(0, std::memory_order_relaxed);

		fps = static_cast<double>(count) / elapsed.count();
		result = (fps > static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
			? std::numeric_limits<std::uint32_t>::max()
			: static_cast<std::uint32_t>(fps);

		this->last_fps.store(result, std::memory_order_relaxed);

		return result;
	}

	template <GroupServerHasNestedTypes TServer>
	std::size_t Group<TServer>::get_queued_job_count(void) const noexcept
	{
		return this->jobs.get_size();
	}

	template <GroupServerHasNestedTypes TServer>
	std::size_t Group<TServer>::get_reserved_job_count(void) const noexcept
	{
		if (!this->reserved_jobs)
			return 0;

		return this->reserved_jobs->get_size();
	}

	template <GroupServerHasNestedTypes TServer>
	std::uint64_t Group<TServer>::get_session_count(void) const noexcept
	{
		return this->session_count.load(std::memory_order_relaxed);
	}

	template <GroupServerHasNestedTypes TServer>
	void Group<TServer>::move_session(SessionID id, Group &destination) noexcept
	{
		std::pair<std::uint32_t, std::uint32_t> decoded;
		std::uint32_t index;
		std::uint32_t generation;

		decoded = TServer::unpack_session_id(id);
		index = decoded.first;
		generation = decoded.second;

		if (index >= this->session_joined.size())
			return;

		if (!this->session_joined[index] || this->session_generations[index] != generation)
			return;

		this->session_joined[index] = false;
		this->session_count.fetch_sub(1, std::memory_order_relaxed);
		this->on_leave(id);
		this->server.set_session_group(id, nullptr);
		destination.post_enter(id);
	}

	template <GroupServerHasNestedTypes TServer>
	Group<TServer>::Job::Job(JobType type, SessionID session_id, std::unique_ptr<Packet> packet) noexcept
		: type(type)
		, session_id(session_id)
		, packet(std::move(packet))
	{
	}

	template <GroupServerHasNestedTypes TServer>
	typename Group<TServer>::ServerClock::duration Group<TServer>::calculate_frame_interval(std::uint32_t fps) noexcept
	{
		if (!fps)
			return ServerClock::duration::zero();

		return std::chrono::duration_cast<typename ServerClock::duration>(std::chrono::seconds(1)) / fps;
	}

	template <GroupServerHasNestedTypes TServer>
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
				std::fprintf(stderr, "ajy::concurrency::Group::thread_proc() failed: [Code: %d] %s\n", error.code().value(), error.what());
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

	template <GroupServerHasNestedTypes TServer>
	void Group<TServer>::start(std::uint32_t max_sessions) noexcept
	{
		if (this->thread.joinable())
			return;

		try
		{
			this->session_joined.assign(max_sessions, false);
			this->session_generations.assign(max_sessions, 0);
			this->reserved_jobs = std::make_unique<container::mpsc::Queue<Job>>(max_sessions);
		}
		catch (const std::bad_alloc &error)
		{
			std::fprintf(stderr, "ajy::concurrency::Group::start() failed: %s\n", error.what());
			std::terminate();
		}

		if (!this->reserved_jobs->get_capacity())
		{
			std::fprintf(stderr, "ajy::concurrency::Group::start() failed: could not allocate the reserved job queue.\n");
			std::terminate();
		}

		this->running.store(true, std::memory_order_release);

		try
		{
			this->thread = std::thread(thread_proc, this);
		}
		catch (const std::system_error &error)
		{
			std::fprintf(stderr, "ajy::concurrency::Group::start() failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}
	}

	template <GroupServerHasNestedTypes TServer>
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
			std::fprintf(stderr, "ajy::concurrency::Group::stop() failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}
	}

	template <GroupServerHasNestedTypes TServer>
	void Group<TServer>::drain_jobs(void) noexcept
	{
		while (true)
		{
			std::optional<Job> job;
			SessionID id;
			std::pair<std::uint32_t, std::uint32_t> decoded;
			std::uint32_t index;
			std::uint32_t generation;

			job = this->jobs.dequeue();
			if (!job.has_value())
				break;

			id = job->session_id;
			decoded = TServer::unpack_session_id(id);
			index = decoded.first;
			generation = decoded.second;

			switch (job->type)
			{
			case JobType::Enter:
				if (!this->server.set_session_group(id, this))
					break;

				this->session_joined[index] = true;
				this->session_generations[index] = generation;
				this->session_count.fetch_add(1, std::memory_order_relaxed);
				this->on_enter(id);
				break;

			case JobType::Leave:
				if (!this->session_joined[index] || this->session_generations[index] != generation)
					break;

				this->session_joined[index] = false;
				this->session_count.fetch_sub(1, std::memory_order_relaxed);
				this->on_leave(id);
				break;

			case JobType::Recv:
				if (!this->session_joined[index] || this->session_generations[index] != generation)
					break;

				this->on_recv(id, std::move(job->packet));
				break;

			default:
				break;
			}
		}

		while (true)
		{
			std::optional<Job> job;
			SessionID id;
			std::pair<std::uint32_t, std::uint32_t> decoded;
			std::uint32_t index;
			std::uint32_t generation;

			job = this->reserved_jobs->dequeue();
			if (!job.has_value())
				break;

			id = job->session_id;
			decoded = TServer::unpack_session_id(id);
			index = decoded.first;
			generation = decoded.second;

			switch (job->type)
			{
			case JobType::Leave:
				if (!this->session_joined[index] || this->session_generations[index] != generation)
					break;

				this->session_joined[index] = false;
				this->session_count.fetch_sub(1, std::memory_order_relaxed);
				this->on_leave(id);
				break;

			default:
				break;
			}
		}
	}

	template <GroupServerHasNestedTypes TServer>
	void Group<TServer>::wake_thread(void) noexcept
	{
		try
		{
			std::lock_guard<std::mutex> guard(this->mutex);

			this->wake = true;
		}
		catch (const std::system_error &error)
		{
			std::fprintf(stderr, "ajy::concurrency::Group::wake_thread() failed: [Code: %d] %s\n", error.code().value(), error.what());
			std::terminate();
		}

		this->condition.notify_one();
	}

	template <GroupServerHasNestedTypes TServer>
	void Group<TServer>::handle_job_queue_full(const char *function_name, SessionID id) noexcept
	{
		this->server.disconnect(id);

		this->logger->log(
			utility::Logger::LogLevel::Error,
			"%s(): job queue full, disconnecting. id: %llu",
			function_name,
			static_cast<unsigned long long>(id));
	}
}

#endif
