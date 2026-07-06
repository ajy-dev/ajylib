/**
 * File: net_server.hpp
 * Path: ajylib/include/ajy/network/windows/iocp/net_server.hpp
 * Description:
 *	A Windows IOCP obfuscated-protocol TCP server declaration.
 * Note:
 *	Shares the IOCP engine with iocp::Server, differing only in the packet
 *	type (NetPacketBuffer), the send-path header build/encode, and the
 *	recv-path framing/decode. code (framing magic) and fixed_key
 *	(obfuscation secret) are injected by the deriving class via the
 *	constructor; random_key is drawn per packet by the server.
 * Author: ajy-dev
 * Created: 2026-07-06
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef AJY_NETWORK_WINDOWS_IOCP_NET_SERVER_HPP
# define AJY_NETWORK_WINDOWS_IOCP_NET_SERVER_HPP

# include <ajy/container/lockfree/queue.hpp>
# include <ajy/container/lockfree/stack.hpp>
# include <ajy/container/ring_buffer.hpp>
# include <ajy/network/protocol/net_packet_buffer.hpp>
# include <ajy/network/server.hpp>
# include <ajy/utility/logger.hpp>
# include <ajy/windows.hpp>

# include <atomic>
# include <chrono>
# include <cstdint>
# include <deque>
# include <memory>
# include <mutex>
# include <thread>
# include <utility>
# include <vector>

namespace ajy::network::windows::iocp
{
	class NetServer : public ajy::network::Server
	{
	public:
		explicit NetServer(std::string_view logger_name, std::uint8_t code, std::uint8_t fixed_key) noexcept;
		~NetServer(void) noexcept override;

		NetServer(const NetServer &other) = delete;
		NetServer &operator=(const NetServer &other) = delete;
		NetServer(NetServer &&other) = delete;
		NetServer &operator=(NetServer &&other) = delete;

		bool start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept override;
		void stop(void) noexcept override;

		bool disconnect(SessionID id) noexcept override;

		std::uint32_t get_session_count(void) const noexcept override;
		std::uint32_t get_accept_tps(void) noexcept override;
		std::uint32_t get_recv_message_tps(void) noexcept override;
		std::uint32_t get_send_message_tps(void) noexcept override;

	protected:
		using ServerClock = std::conditional<
			std::chrono::high_resolution_clock::is_steady,
			std::chrono::high_resolution_clock,
			std::chrono::steady_clock>::type;
		using Packet = protocol::NetPacketBuffer;

		virtual bool on_connection_request(const char *ip, std::uint16_t port) = 0;
		virtual void on_client_join(SessionID id) noexcept = 0;
		virtual void on_client_leave(SessionID id) noexcept = 0;
		virtual void on_recv(SessionID id, Packet *packet) noexcept = 0;
		virtual void on_send(SessionID id, std::size_t size) noexcept = 0;
		virtual void on_worker_thread_begin(void) noexcept = 0;
		virtual void on_worker_thread_end(void) noexcept = 0;

		std::shared_ptr<Packet> alloc_packet(std::size_t payload_capacity = Packet::DEFAULT_PAYLOAD_CAPACITY) noexcept;
		bool send_packet(SessionID id, std::shared_ptr<Packet> packet) noexcept;

		utility::Logger *logger;

	private:
		static constexpr std::uint16_t DEFAULT_MAX_PENDING_SENDS = 1024;
		static constexpr std::size_t PENDING_SENDS_INITIAL_CAPACITY = 128;
		static constexpr std::size_t SEND_BATCH_SIZE = 128;

		struct Session
		{
			std::uint32_t index;

			SOCKET socket;

			OVERLAPPED recv_overlapped;
			container::RingBuffer recv_buffer = container::RingBuffer(65535);

			OVERLAPPED send_overlapped;
			container::lockfree::Queue<std::shared_ptr<const Packet>> pending_sends =
				container::lockfree::Queue<std::shared_ptr<const Packet>>(PENDING_SENDS_INITIAL_CAPACITY);
			std::atomic<std::uint16_t> pending_send_count;
			std::deque<std::shared_ptr<const Packet>> in_flight_packets;
			std::size_t in_flight_offset;

			std::atomic<bool> send_flag;
			std::atomic<bool> disconnect_flag;
			std::atomic<int> ref_count;
			std::atomic<std::uint32_t> generation;

			std::mutex lock;
		};

		static void accept_thread_proc(NetServer *server) noexcept;
		static void worker_thread_proc(NetServer *server) noexcept;

		static SessionID pack_session_id(std::uint32_t index, std::uint32_t generation) noexcept;
		static std::pair<std::uint32_t, std::uint32_t> unpack_session_id(SessionID id) noexcept;

		static std::uint32_t calculate_tps(
			std::atomic<std::uint32_t> &counter,
			std::atomic<ServerClock::time_point> &last_query,
			std::atomic<std::uint32_t> &last_tps) noexcept;

		static std::uint8_t generate_random_key(void) noexcept;

		Session *acquire_session(SessionID id) noexcept;
		void release_session(Session *session) noexcept;

		bool recv_post(Session *session) noexcept;
		void send_post(Session *session) noexcept;

		void log_winapi_error(const char *func_name, DWORD error_code, utility::Logger::LogLevel level = utility::Logger::LogLevel::Error) noexcept;

		const std::uint8_t code;
		const std::uint8_t fixed_key;

		HANDLE iocp_handle;
		SOCKET listen_socket;
		std::atomic<bool> running;
		std::uint32_t max_sessions;
		std::atomic<std::uint16_t> max_pending_sends;

		std::thread accept_thread;
		std::vector<std::thread> worker_threads;

		std::unique_ptr<Session[]> sessions;
		container::lockfree::Stack<std::uint32_t> free_indices;
		std::atomic<std::uint32_t> active_session_count;

		std::atomic<std::uint32_t> accept_count;
		std::atomic<std::uint32_t> recv_count;
		std::atomic<std::uint32_t> send_count;

		std::atomic<std::uint32_t> last_accept_tps;
		std::atomic<std::uint32_t> last_recv_tps;
		std::atomic<std::uint32_t> last_send_tps;

		std::atomic<ServerClock::time_point> last_accept_query;
		std::atomic<ServerClock::time_point> last_recv_query;
		std::atomic<ServerClock::time_point> last_send_query;
	};
}

#endif
