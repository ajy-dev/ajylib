/**
 * File: group.hpp
 * Path: ajylib/include/ajy/concurrency/group.hpp
 * Description:
 *	A serial processing unit declaration.
 *	One dedicated thread drains a job queue and runs the handlers.
 * Note:
 *	A handler needs no synchronization against other sessions of the same
 *	group. The server type is a template parameter; the group itself
 *	carries no network dependency.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: 2026-08-30
 * Version: 0.1.0
 */

#ifndef AJY_CONCURRENCY_GROUP_HPP
#define AJY_CONCURRENCY_GROUP_HPP

#include <ajy/concurrency/concepts.hpp>
#include <ajy/container/mpsc/queue.hpp>
#include <ajy/utility/logger.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

namespace ajy::concurrency
{
	template <GroupServerHasNestedTypes TServer>
	class Group
	{
	public:
		using SessionID = typename TServer::SessionID;
		using Packet = typename TServer::Packet;
		using ServerClock = typename TServer::ServerClock;

		static constexpr std::size_t DEFAULT_JOB_QUEUE_CAPACITY = 65536;

		Group(TServer &server, std::uint32_t fps, std::string_view logger_name, std::size_t job_queue_capacity = DEFAULT_JOB_QUEUE_CAPACITY) noexcept
		requires GroupServerHasFunctions<TServer>;
		virtual ~Group(void) noexcept;

		Group(const Group &other) = delete;
		Group &operator=(const Group &other) = delete;
		Group(Group &&other) = delete;
		Group &operator=(Group &&other) = delete;

		void post_enter(SessionID id) noexcept;
		void post_leave(SessionID id) noexcept;
		void post_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept;

		std::uint32_t get_current_fps(void) noexcept;
		std::size_t get_queued_job_count(void) const noexcept;
		std::size_t get_reserved_job_count(void) const noexcept;
		std::uint64_t get_session_count(void) const noexcept;

	protected:
		virtual void on_enter(SessionID id) noexcept = 0;
		virtual void on_leave(SessionID id) noexcept = 0;
		virtual void on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept = 0;
		virtual void on_frame(typename ServerClock::duration elapsed) noexcept = 0;

		void move_session(SessionID id, Group &destination) noexcept;

		TServer &server;
		utility::Logger *logger;

	private:
		friend TServer;

		enum class JobType
		{
			Enter,
			Leave,
			Recv
		};

		struct Job
		{
			JobType type;
			SessionID session_id;
			std::unique_ptr<Packet> packet;

			Job(JobType type, SessionID session_id, std::unique_ptr<Packet> packet = nullptr) noexcept;
		};

		static typename ServerClock::duration calculate_frame_interval(std::uint32_t fps) noexcept;

		static void thread_proc(Group *group) noexcept;

		void start(std::uint32_t max_sessions) noexcept;
		void stop(void) noexcept;

		void drain_jobs(void) noexcept;
		void wake_thread(void) noexcept;

		void handle_job_queue_full(const char *function_name, SessionID id) noexcept;

		const typename ServerClock::duration frame_interval;
		std::atomic<std::uint32_t> frame_count;
		std::atomic<std::uint32_t> last_fps;
		std::atomic<typename ServerClock::time_point> last_fps_query;

		container::mpsc::Queue<Job> jobs;
		std::unique_ptr<container::mpsc::Queue<Job>> reserved_jobs;

		std::vector<bool> session_joined;
		std::vector<std::uint32_t> session_generations;
		std::atomic<std::uint64_t> session_count;

		std::mutex mutex;
		std::condition_variable condition;
		bool wake;
		std::atomic<bool> running;
		std::thread thread;
	};
}

#include <ajy/concurrency/group.tpp>

#endif
