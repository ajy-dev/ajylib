/**
 * File: login_server.hpp
 * Path: ajylib/examples/login_gated_chat/login_server/login_server.hpp
 * Description:
 * 	A login server built on ajy::network::windows::iocp::NetServer. Processes
 * 	each client login through an AccountNo-sharded worker pool (DB -> Redis ->
 * 	response, serial per worker).
 * 	Content server registrations arrive on a separate server (ContentLink).
 * Author: ajy-dev
 * Created: 2026-07-22
 * Updated: 2026-08-09
 * Version: 0.1.0
 */

#ifndef LOGIN_SERVER_HPP
#define LOGIN_SERVER_HPP

#include <ajy/container/lockfree/queue.hpp>
#include <ajy/memory/lockfree/memory_pool.hpp>
#include <ajy/network/windows/iocp/net_server.hpp>

#include <login_gated_chat/login_server/content_link.hpp>
#include <login_gated_chat/protocol.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

struct MYSQL;
struct redisContext;

class LoginServer : public ajy::network::windows::iocp::NetServer
{
public:
	explicit LoginServer(std::string_view logger_name, unsigned int worker_count) noexcept;
	~LoginServer(void) noexcept override;

	LoginServer(const LoginServer &other) = delete;
	LoginServer &operator=(const LoginServer &other) = delete;
	LoginServer(LoginServer &&other) = delete;
	LoginServer &operator=(LoginServer &&other) = delete;

	bool start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept override;
	void stop(void) noexcept override;

	std::uint32_t get_auth_tps(void) noexcept;
	std::size_t get_job_pool_in_use(void) const noexcept;

protected:
	bool on_connection_request(const char *ip, std::uint16_t port) override;
	void on_client_join(SessionID id) noexcept override;
	void on_client_leave(SessionID id) noexcept override;
	void on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept override;
	void on_send(SessionID id, std::size_t size) noexcept override;
	void on_worker_thread_begin(void) noexcept override;
	void on_worker_thread_end(void) noexcept override;

private:
	struct Job
	{
		SessionID session_id;
		std::int64_t account_no;
		std::uint8_t session_key[64];
	};

	struct Worker
	{
		ajy::container::lockfree::Queue<Job *> queue;
		std::atomic<bool> wake;
		std::thread thread;
		MYSQL *db;
		redisContext *redis;
		std::atomic<std::uint32_t> auth_count;
	};

	enum class QueryResult
	{
		DB_FAIL,
		MISS,
		FOUND
	};

	struct AccountRecord
	{
		QueryResult result;
		std::uint16_t userid[20]; // WCHAR[20], UTF-16, null-terminated
		std::uint16_t usernick[20]; // WCHAR[20], UTF-16, null-terminated
	};

	static constexpr Job *STOP_SENTINEL = nullptr;

	static void worker_thread_proc(LoginServer *server, unsigned int index) noexcept;
	static void timeout_thread_proc(LoginServer *server) noexcept;

	void start_worker_pool(void) noexcept;
	void stop_worker_pool(void) noexcept;
	void start_timeout_thread(void) noexcept;
	void stop_timeout_thread(void) noexcept;

	void dispatch_login(SessionID id, Packet *packet) noexcept;
	void sweep_timeouts(void) noexcept;

	void process_login(Worker *worker, Job *job) noexcept;
	AccountRecord query_account(Worker *worker, std::int64_t account_no) noexcept;
	LoginStatus issue_ticket(Worker *worker, const Job *job, const std::vector<ContentLink::ServerInfo> &servers) noexcept;
	void to_wide_field(std::uint16_t *dst, int dst_count, const char *src) noexcept;

	std::shared_ptr<Packet> make_res_login(
		std::int64_t account_no,
		LoginStatus status,
		const AccountRecord &record,
		const std::vector<ContentLink::ServerInfo> &servers) noexcept;
	void send_res_login(
		SessionID id,
		std::int64_t account_no,
		LoginStatus status,
		const AccountRecord &record,
		const std::vector<ContentLink::ServerInfo> &servers) noexcept;

	static MYSQL *connect_db(void) noexcept;
	static redisContext *connect_redis(void) noexcept;

	ajy::memory::lockfree::MemoryPool<Job> job_pool;

	unsigned int worker_count;
	std::vector<std::unique_ptr<Worker>> workers;

	std::atomic<ServerClock::time_point> last_auth_query;
	std::atomic<std::uint32_t> last_auth_tps;

	std::atomic<bool> timeout_running;
	std::thread timeout_thread;

	std::mutex client_sessions_mutex;
	std::unordered_map<SessionID, ServerClock::time_point> client_sessions;

	ContentLink content_link;
};

#endif
