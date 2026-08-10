/**
 * File: group.hpp
 * Path: ajylib/include/ajy/concurrency/group.hpp
 * Description:
 * 	A serial processing unit declaration.
 * Note:
 * 	Sessions belonging to one group are processed serially by a dedicated
 * 	thread, so a handler needs no synchronization against other sessions
 * 	of the same group. The server type is a template parameter; the group
 * 	itself carries no network dependency.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_CONCURRENCY_GROUP_HPP
#define AJY_CONCURRENCY_GROUP_HPP

#include <ajy/container/lockfree/queue.hpp>
#include <ajy/memory/lockfree/memory_pool.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace ajy::concurrency
{
	template <typename TServer>
	class Group
	{
	public:
		using SessionID = typename TServer::SessionID;
		using Packet = typename TServer::Packet;
		using ServerClock = typename TServer::ServerClock;

		explicit Group(TServer &server, std::uint32_t fps) noexcept;
		virtual ~Group(void) noexcept;

		Group(const Group &other) = delete;
		Group &operator=(const Group &other) = delete;
		Group(Group &&other) = delete;
		Group &operator=(Group &&other) = delete;

		void post_enter(SessionID id) noexcept;
		void post_leave(SessionID id) noexcept;
		void post_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept;

		std::uint32_t get_frame_tps(void) noexcept;
		std::size_t get_job_pool_in_use(void) const noexcept;

	protected:
		virtual void on_enter(SessionID id) noexcept = 0;
		virtual void on_leave(SessionID id) noexcept = 0;
		virtual void on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept = 0;
		virtual void on_frame(typename ServerClock::duration elapsed) noexcept = 0;

		void move_session(SessionID id, Group &destination) noexcept;

		TServer &server;

	private:
		friend TServer;

		enum class JobType
		{
			Enter,
			Leave,
			Recv,
		};

		struct Job
		{
			JobType type;
			SessionID session_id;
			std::unique_ptr<Packet> packet;

			Job(JobType type, SessionID session_id, std::unique_ptr<Packet> packet = nullptr) noexcept;
		};

		static typename ServerClock::duration calculate_frame_interval(std::uint32_t fps) noexcept;

		static std::uint32_t calculate_tps(
			std::atomic<std::uint32_t> &counter,
			std::atomic<typename ServerClock::time_point> &last_query,
			std::atomic<std::uint32_t> &last_tps) noexcept;

		static void thread_proc(Group *group) noexcept;

		void start(void) noexcept;
		void stop(void) noexcept;

		void drain_jobs(void) noexcept;
		void wake_thread(void) noexcept;

		const typename ServerClock::duration frame_interval;

		std::atomic<std::uint32_t> frame_count;
		std::atomic<std::uint32_t> last_frame_tps;
		std::atomic<typename ServerClock::time_point> last_frame_query;

		container::lockfree::Queue<Job *> jobs;
		memory::lockfree::MemoryPool<Job> job_pool;

		std::unordered_set<SessionID> sessions;

		std::mutex mutex;
		std::condition_variable condition;
		bool wake;
		std::atomic<bool> running;
		std::thread thread;
	};
}

#include <ajy/concurrency/group.tpp>

#endif
