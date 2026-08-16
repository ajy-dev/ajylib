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
 * Updated: 2026-08-14
 * Version: 0.1.0
 */

#ifndef AJY_CONCURRENCY_GROUP_HPP
#define AJY_CONCURRENCY_GROUP_HPP

#include <ajy/concurrency/ring_queue.hpp>
#include <ajy/utility/logger.hpp>

#include <atomic>
#include <chrono>
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
	template <typename TServer>
	class Group
	{
	public:
		using SessionID = typename TServer::SessionID;
		using Packet = typename TServer::Packet;
		using ServerClock = typename TServer::ServerClock;

		Group(TServer &server, std::uint32_t fps, std::string_view logger_name) noexcept;
		virtual ~Group(void) noexcept;

		Group(const Group &other) = delete;
		Group &operator=(const Group &other) = delete;
		Group(Group &&other) = delete;
		Group &operator=(Group &&other) = delete;

		void post_enter(SessionID id) noexcept;
		void post_leave(SessionID id) noexcept;
		void post_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept;

		std::uint32_t get_frame_tps(void) noexcept;
		std::size_t get_queued_job_count(void) const noexcept;
		std::size_t get_rejected_session_count(void) const noexcept;
		std::size_t get_stale_enter_count(void) const noexcept;
		std::uint64_t get_session_count(void) const noexcept;
		std::uint64_t get_enter_count(void) const noexcept;
		std::uint64_t get_leave_count(void) const noexcept;

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

		static constexpr std::size_t JOB_QUEUE_CAPACITY = 1048576;
		static constexpr std::size_t REJECT_LOG_INTERVAL = 65535;

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

		static std::uint32_t unpack_index(SessionID id) noexcept;
		static std::uint32_t unpack_generation(SessionID id) noexcept;

		static typename ServerClock::duration calculate_frame_interval(std::uint32_t fps) noexcept;

		static std::uint32_t calculate_tps(
			std::atomic<std::uint32_t> &counter,
			std::atomic<typename ServerClock::time_point> &last_query,
			std::atomic<std::uint32_t> &last_tps) noexcept;

		static void thread_proc(Group *group) noexcept;

		void start(std::uint32_t max_sessions) noexcept;
		void stop(void) noexcept;

		void reject_session(const char *function_name, SessionID id) noexcept;

		void drain_jobs(void) noexcept;
		void wake_thread(void) noexcept;

		const typename ServerClock::duration frame_interval;

		std::atomic<std::uint32_t> frame_count;
		std::atomic<std::uint32_t> last_frame_tps;
		std::atomic<typename ServerClock::time_point> last_frame_query;

		RingQueue<Job> jobs;

		std::atomic<std::size_t> rejected_session_count;
		std::atomic<std::size_t> stale_enter_count;
		std::atomic<bool> reject_reported;

		static constexpr std::uint32_t EMPTY_GENERATION = ~static_cast<std::uint32_t>(0);

		std::vector<std::uint32_t> session_generations;
		std::atomic<std::uint64_t> session_count;
		std::atomic<std::uint64_t> enter_count;
		std::atomic<std::uint64_t> leave_count;

		std::mutex mutex;
		std::condition_variable condition;
		bool wake;
		std::atomic<bool> running;
		std::thread thread;
	};
}

#include <ajy/concurrency/group.tpp>

#endif
