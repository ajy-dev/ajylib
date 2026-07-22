/**
 * File: content_link.hpp
 * Path: ajylib/examples/login_gated_chat/login_server/content_link.hpp
 * Description:
 * 	SS listener that tracks content server registrations
 * Note:
 * 	Plaintext listener on its own port, separate from the client-facing
 * 	NetServer. A registration lives exactly as long as its connection, so the
 * 	table doubles as the liveness view the login server judges against.
 * Author: ajy-dev
 * Created: 2026-08-07
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef CONTENT_LINK_HPP
#define CONTENT_LINK_HPP

#include <ajy/network/windows/iocp/server.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

class ContentLink : public ajy::network::windows::iocp::Server
{
public:
	struct ServerInfo
	{
		char name[16];
		char ip[16];
		std::uint16_t port;
	};

	explicit ContentLink(std::string_view logger_name) noexcept;
	~ContentLink(void) noexcept override;

	ContentLink(const ContentLink &other) = delete;
	ContentLink &operator=(const ContentLink &other) = delete;
	ContentLink(ContentLink &&other) = delete;
	ContentLink &operator=(ContentLink &&other) = delete;

	static const ServerInfo *find(const std::vector<ServerInfo> &servers, std::string_view name) noexcept;

	std::vector<ServerInfo> get_servers(void) const noexcept;
	void broadcast_disconnect(std::int64_t account_no) noexcept;

protected:
	bool on_connection_request(const char *ip, std::uint16_t port) override;
	void on_client_join(SessionID id) noexcept override;
	void on_client_leave(SessionID id) noexcept override;
	void on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept override;
	void on_send(SessionID id, std::size_t size) noexcept override;
	void on_worker_thread_begin(void) noexcept override;
	void on_worker_thread_end(void) noexcept override;

private:
	void handle_ss_register(SessionID id, Packet *packet) noexcept;

	mutable std::shared_mutex lock;
	std::unordered_map<SessionID, ServerInfo> servers;
};

#endif
