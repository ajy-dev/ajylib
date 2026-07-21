/**
 * File: chat_server.hpp
 * Path: ajylib/examples/chat_server/multi_thread/chat_server.hpp
 * Description:
 *	A sector-based chat server built on ajy::network::windows::iocp::NetServer.
 *	Multi-threaded send model: IOCP workers enqueue jobs; one content thread
 *	owns all shared state and processes them, but offloads response/broadcast
 *	fan-out to a session-sharded pool of send worker threads.
 * Note:
 *	Windows-only: Player stores ID/Nickname as wchar_t[20], relying on
 *	wchar_t being 2 bytes (UTF-16) so sizeof matches the 40-byte wire field.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: 2026-07-21
 * Version: 0.1.0
 */

#ifndef CHAT_SERVER_HPP
#define CHAT_SERVER_HPP

#include <ajy/container/lockfree/queue.hpp>
#include <ajy/memory/lockfree/memory_pool.hpp>
#include <ajy/memory/threadlocal/memory_pool.hpp>
#include <ajy/network/windows/iocp/net_server.hpp>

#include <chat_server/chat_server_config.hpp>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

class ChatServer : public ajy::network::windows::iocp::NetServer
{
public:
	explicit ChatServer(std::string_view logger_name, unsigned int send_worker_count) noexcept;
	~ChatServer(void) noexcept override;

	ChatServer(const ChatServer &other) = delete;
	ChatServer &operator=(const ChatServer &other) = delete;
	ChatServer(ChatServer &&other) = delete;
	ChatServer &operator=(ChatServer &&other) = delete;

	bool start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept override;
	void stop(void) noexcept override;

	std::uint32_t get_content_job_tps(void) noexcept;
	std::uint32_t get_player_count(void) const noexcept;
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
	enum class JobType
	{
		ClientJoin,
		ClientLeave,
		Recv,
	};

	struct Job
	{
		JobType type;
		SessionID session_id;
		std::shared_ptr<Packet> packet; // Recv only; null for Join/Leave

		Job(JobType type, SessionID session_id, std::shared_ptr<Packet> packet = nullptr) noexcept;
	};

	struct Player
	{
		SessionID session_id;
		std::int64_t account_no;

		wchar_t id[20]; // WCHAR[20], null-terminated (Windows: 2-byte)
		wchar_t nickname[20]; // WCHAR[20], null-terminated

		std::uint16_t sector; // 1D index, or ChatServerConfig::INVALID_SECTOR

		bool logged_in;
		ServerClock::time_point last_recv_time;
	};

	struct Sector
	{
		std::vector<Player *> players;
	};

	struct SendTask
	{
		SessionID session_id;
		std::shared_ptr<Packet> packet;
	};

	struct SendWorker
	{
		ajy::container::lockfree::Queue<SendTask> queue;
		std::atomic<bool> wake;
		std::thread thread;
	};

	static void content_thread_proc(ChatServer *server) noexcept;
	void drain_jobs(void) noexcept;
	void wake_content_thread(void) noexcept;

	void handle_client_join(SessionID id) noexcept;
	void handle_client_leave(SessionID id) noexcept;
	void handle_recv(SessionID id, Packet *packet) noexcept;
	void check_heartbeat_timeout(void) noexcept;

	void handle_req_login(Player *player, Packet *packet) noexcept;
	void handle_req_sector_move(Player *player, Packet *packet) noexcept;
	void handle_req_message(Player *player, Packet *packet) noexcept;
	void handle_req_heartbeat(Player *player) noexcept;

	static std::uint16_t to_sector_index(std::uint16_t x, std::uint16_t y) noexcept;
	static void get_around_sectors(std::uint16_t sector, std::uint16_t *out) noexcept;
	void sector_add_player(Player *player) noexcept;
	void sector_remove_player(Player *player) noexcept;

	std::shared_ptr<Packet> make_res_login(std::uint8_t status, std::int64_t account_no) noexcept;
	std::shared_ptr<Packet> make_res_sector_move(std::int64_t account_no, std::uint16_t x, std::uint16_t y) noexcept;
	std::shared_ptr<Packet> make_res_message(const Player *sender, const wchar_t *msg, std::uint16_t msg_len) noexcept;

	void send_res_login(SessionID id, std::uint8_t status, std::int64_t account_no) noexcept;
	void send_res_sector_move(SessionID id, std::int64_t account_no, std::uint16_t x, std::uint16_t y) noexcept;

	void broadcast_res_message(const Player *sender, const wchar_t *msg, std::uint16_t msg_len) noexcept;

	static void finalize_packet(Packet *packet) noexcept;
	
	void enqueue_send(SessionID id, std::shared_ptr<Packet> packet) noexcept;
	void start_send_pool(unsigned count) noexcept;
	void stop_send_pool(void) noexcept;

	static void send_thread_proc(ChatServer *server, unsigned int worker_index) noexcept;

	ajy::memory::threadlocal::MemoryPool<Player> player_pool;
	std::unordered_map<SessionID, Player *> player_map;
	std::array<Sector, ChatServerConfig::TOTAL_SECTORS> sectors;
	std::atomic<std::uint32_t> player_count;

	ajy::memory::lockfree::MemoryPool<Job> job_pool;
	ajy::container::lockfree::Queue<Job *> job_queue;

	std::mutex content_mutex;
	std::condition_variable content_cv;
	bool content_wake; // guarded by content_mutex
	std::atomic<bool> content_running;

	std::atomic<std::uint32_t> content_job_count;
	std::atomic<ServerClock::time_point> last_content_query;
	std::atomic<std::uint32_t> last_content_tps;

	std::thread content_thread;

	unsigned send_worker_count;
	std::vector<std::unique_ptr<SendWorker>> send_workers;
};

#endif
