/**
 * File: monitor_server.hpp
 * Path: ajylib/examples/monitor_server/monitor_server.hpp
 * Description:
 *	A monitoring server built on ajy::network::windows::iocp::NetServer.
 *	A single content thread owns all shared state; a sampler thread feeds
 *	local-host metrics as jobs, and a DB writer thread persists the
 *	per-minute aggregates.
 * Note:
 *	One NetServer serves both the reporter (SS) and tool (CS) roles; each
 *	session is role-tagged, and every data point is relayed to all tools
 *	and folded into the (serverno, datatype) aggregator.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef MONITOR_SERVER_HPP
#define MONITOR_SERVER_HPP

#include <ajy/container/lockfree/queue.hpp>
#include <ajy/memory/lockfree/memory_pool.hpp>
#include <ajy/memory/threadlocal/memory_pool.hpp>
#include <ajy/network/windows/iocp/net_server.hpp>

#include <monitor_server/db_writer.hpp>
#include <monitor_server/protocol.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

class MonitorServer : public ajy::network::windows::iocp::NetServer
{
public:
	explicit MonitorServer(std::string_view logger_name) noexcept;
	~MonitorServer(void) noexcept override;

	MonitorServer(const MonitorServer &other) = delete;
	MonitorServer &operator=(const MonitorServer &other) = delete;
	MonitorServer(MonitorServer &&other) = delete;
	MonitorServer &operator=(MonitorServer &&other) = delete;

	bool start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept override;
	void stop(void) noexcept override;

	void submit_local_sample(std::uint8_t data_type, std::int32_t data_value, std::int32_t time_stamp) noexcept;

protected:
	bool on_connection_request(const char *ip, std::uint16_t port) override;
	void on_client_join(SessionID id) noexcept override;
	void on_client_leave(SessionID id) noexcept override;
	void on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept override;
	void on_send(SessionID id, std::size_t size) noexcept override;
	void on_worker_thread_begin(void) noexcept override;
	void on_worker_thread_end(void) noexcept override;

private:
	struct MonitorSession
	{
		enum class Role : std::uint8_t
		{
			UNASSIGNED,
			REPORTER,
			MONITORING_TOOL,
		};

		SessionID session_id;
		Role role;
		std::int32_t server_no; // valid only when role == REPORTER
		ServerClock::time_point last_recv_time;
	};

	struct SampleData
	{
		std::uint8_t data_type;
		std::int32_t data_value;
		std::int32_t time_stamp;
	};

	struct Job
	{
		enum class Type : std::uint8_t
		{
			ClientJoin,
			ClientLeave,
			Recv,
			LocalSample,
		};

		Type type;
		SessionID session_id; // Recv/Join/Leave; unused for LocalSample
		std::shared_ptr<Packet> packet; // Recv only
		SampleData sample; // LocalSample only

		Job(Type type, SessionID session_id, std::shared_ptr<Packet> packet = nullptr) noexcept;
		Job(SampleData sample) noexcept;
	};

	struct Accumulator
	{
		std::uint32_t count = 0;
		std::int64_t sum = 0; // int32 values summed over a window; int64 avoids overflow
		std::int32_t min = std::numeric_limits<std::int32_t>::max(); // sentinel: first value narrows it down
		std::int32_t max = std::numeric_limits<std::int32_t>::min(); // sentinel: first value narrows it up
	};

	static void content_thread_proc(MonitorServer *server) noexcept;
	void drain_jobs(void) noexcept;
	void wake_content_thread(void) noexcept;
	void shutdown_threads(void) noexcept;

	void handle_client_join(SessionID id) noexcept;
	void handle_client_leave(SessionID id) noexcept;
	void handle_recv(SessionID id, Packet *packet) noexcept;
	void handle_local_sample(const SampleData &sample) noexcept;

	void handle_ss_login(MonitorSession *session, Packet *packet) noexcept;
	void handle_cs_tool_login(MonitorSession *session, Packet *packet) noexcept;

	void handle_ss_data_update(MonitorSession *session, Packet *packet) noexcept;
	void ingest(std::int32_t server_no, std::uint8_t data_type, std::int32_t data_value, std::int32_t time_stamp) noexcept;
	void relay_to_tools(std::int32_t server_no, std::uint8_t data_type, std::int32_t data_value, std::int32_t time_stamp) noexcept;

	void flush_aggregator(void) noexcept;
	void check_timeout(void) noexcept;

	std::shared_ptr<Packet> make_res_tool_login(ToolLoginStatus status) noexcept;
	std::shared_ptr<Packet> make_data_update(std::int32_t server_no, std::uint8_t data_type, std::int32_t data_value, std::int32_t time_stamp) noexcept;

	void send_res_tool_login(SessionID id, ToolLoginStatus status) noexcept;

	ajy::memory::threadlocal::MemoryPool<MonitorSession> session_pool;
	std::unordered_map<SessionID, MonitorSession *> session_map;
	std::vector<SessionID> relay_targets; // authenticated tool sessions, fanned out on each data point

	std::unordered_map<std::int32_t, std::unordered_map<std::uint8_t, Accumulator>> aggregator;

	DbWriter db_writer;

	ajy::memory::lockfree::MemoryPool<Job> job_pool;
	ajy::container::lockfree::Queue<Job *> job_queue;

	std::mutex content_mutex;
	std::condition_variable content_cv;
	bool content_wake; // guarded by content_mutex
	std::atomic<bool> content_running; // content thread shutdown signal

	std::thread content_thread;
};

#endif
