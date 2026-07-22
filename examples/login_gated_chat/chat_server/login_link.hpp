/**
 * File: login_link.hpp
 * Path: ajylib/examples/login_gated_chat/chat_server/login_link.hpp
 * Description:
 * 	Registers this chat instance with the login server and receives
 * 	duplicate-login notifications
 * Note:
 * 	Plaintext SS channel (2-byte length header) over a blocking socket on a
 * 	dedicated thread. The connection is the liveness signal, so a drop shows
 * 	up only as a recv/send failure and is followed by a reconnect and a fresh
 * 	SS_REGISTER. Shutdown wakes a blocked recv through shutdown() and a
 * 	pending reconnect wait through the condition variable.
 * Author: ajy-dev
 * Created: 2026-08-07
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef LOGIN_LINK_HPP
#define LOGIN_LINK_HPP

#include <ajy/utility/logger.hpp>
#include <ajy/windows.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <thread>

class ChatServer;

class LoginLink
{
public:
	LoginLink(ChatServer &server, std::string_view logger_name) noexcept;
	~LoginLink(void) noexcept;

	LoginLink(const LoginLink &other) = delete;
	LoginLink &operator=(const LoginLink &other) = delete;
	LoginLink(LoginLink &&other) = delete;
	LoginLink &operator=(LoginLink &&other) = delete;

	void start(std::uint16_t listen_port) noexcept;
	void stop(void) noexcept;

private:
	static void thread_proc(LoginLink *link) noexcept;

	bool connect_to_login(void) noexcept;
	void disconnect_from_login(void) noexcept;

	bool send_register(void) noexcept;
	bool send_all(const void *data, std::size_t size) noexcept;
	bool recv_all(void *data, std::size_t size) noexcept;

	void recv_loop(void) noexcept;
	void handle_notify_disconnect(const void *payload, std::size_t size) noexcept;

	bool wait_for_reconnect(void) noexcept;

	ChatServer &server;
	ajy::utility::Logger *logger;

	std::atomic<bool> running;
	std::thread thread;

	std::mutex mutex;
	std::condition_variable cv;
	bool wake; // guarded by mutex

	SOCKET login_socket; // guarded by mutex
	std::uint16_t listen_port;

	bool wsa_ready;
	bool connect_failure_logged;
};

#endif
