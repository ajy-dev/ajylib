/**
 * File: login_link.cpp
 * Path: ajylib/examples/login_gated_chat/chat_server/login_link.cpp
 * Description:
 * 	Registers this chat instance with the login server and receives
 * 	duplicate-login notifications
 * Author: ajy-dev
 * Created: 2026-08-08
 * Updated: Never
 * Version: 0.1.0
 */

#include <login_gated_chat/chat_server/login_link.hpp>

#include <login_gated_chat/chat_server/chat_server.hpp>
#include <login_gated_chat/chat_server/chat_server_config.hpp>
#include <login_gated_chat/protocol.hpp>

#include <ajy/network/protocol/packet_buffer.hpp>
#include <ajy/utility/logger.hpp>
#include <ajy/windows.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <string_view>
#include <system_error>

LoginLink::LoginLink(ChatServer &server, std::string_view logger_name) noexcept
	: server(server)
	, logger(ajy::utility::Logger::get(logger_name))
	, running(false)
	, wake(false)
	, login_socket(INVALID_SOCKET)
	, listen_port(0)
	, wsa_ready(false)
	, connect_failure_logged(false)
{
}

LoginLink::~LoginLink(void) noexcept
{
}

void LoginLink::start(std::uint16_t listen_port) noexcept
{
	int wsa_result;
	WSADATA wsa;

	if (this->thread.joinable())
		return;

	if ((wsa_result = ::WSAStartup(MAKEWORD(2, 2), &wsa)))
	{
		ajy::log_winapi_error("WSAStartup()", static_cast<DWORD>(wsa_result), this->logger);
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "Login link disabled.");
		this->wsa_ready = false;
		return;
	}

	if (wsa.wVersion != MAKEWORD(2, 2))
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Error,
			"WSAStartup(): requested 2.2 but got %u.%u",
			static_cast<unsigned int>(LOBYTE(wsa.wVersion)),
			static_cast<unsigned int>(HIBYTE(wsa.wVersion)));
		::WSACleanup();
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "Login link disabled.");
		this->wsa_ready = false;
		return;
	}

	this->wsa_ready = true;
	this->listen_port = listen_port;
	this->running.store(true, std::memory_order_relaxed);

	try
	{
		this->thread = std::thread(thread_proc, this);
	}
	catch (const std::system_error &error)
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Fatal,
			"LoginLink::start(): std::thread(thread_proc) failed: [Code: %d] %s",
			error.code().value(),
			error.what());
		std::terminate();
	}
}

void LoginLink::stop(void) noexcept
{
	this->running.store(false, std::memory_order_relaxed);

	try
	{
		std::lock_guard<std::mutex> guard(this->mutex);

		if (this->login_socket != INVALID_SOCKET)
			::shutdown(this->login_socket, SD_RECEIVE);
		this->wake = true;
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "LoginLink::stop(): std::lock_guard(this->mutex) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}

	this->cv.notify_one();

	if (this->thread.joinable())
	{
		try
		{
			this->thread.join();
		}
		catch (const std::system_error &error)
		{
			this->logger->log(
				ajy::utility::Logger::LogLevel::Fatal,
				"LoginLink::stop(): std::thread::join(thread_proc) failed: [Code: %d] %s",
				error.code().value(),
				error.what());
			std::terminate();
		}
	}

	this->disconnect_from_login();

	if (this->wsa_ready)
	{
		::WSACleanup();
		this->wsa_ready = false;
	}
}

void LoginLink::thread_proc(LoginLink *link) noexcept
{
	while (link->running.load(std::memory_order_relaxed))
	{
		if (!link->connect_to_login())
		{
			if (!link->wait_for_reconnect())
				break;
			continue;
		}

		if (!link->send_register())
		{
			link->disconnect_from_login();
			if (!link->wait_for_reconnect())
				break;
			continue;
		}

		link->recv_loop();

		link->disconnect_from_login();

		if (!link->wait_for_reconnect())
			break;
	}
}

bool LoginLink::connect_to_login(void) noexcept
{
	SOCKET login_socket;
	sockaddr_in address;
	int pton_result;

	login_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (login_socket == INVALID_SOCKET)
	{
		ajy::log_winapi_error("socket()", ::WSAGetLastError(), this->logger, ajy::utility::Logger::LogLevel::Warning);
		return false;
	}

	std::memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = ::htons(ChatServerConfig::LOGIN_SERVER_SS_PORT);

	pton_result = ::inet_pton(AF_INET, ChatServerConfig::LOGIN_SERVER_IP.data(), &address.sin_addr);
	if (pton_result != 1)
	{
		if (pton_result == 0)
			this->logger->log(ajy::utility::Logger::LogLevel::Error, "connect_to_login(): invalid login server host \"%s\"", ChatServerConfig::LOGIN_SERVER_IP.data());
		else
			ajy::log_winapi_error("inet_pton()", ::WSAGetLastError(), this->logger);
		::closesocket(login_socket);
		return false;
	}

	if (::connect(login_socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR)
	{
		if (!this->connect_failure_logged)
		{
			ajy::log_winapi_error("connect()", ::WSAGetLastError(), this->logger, ajy::utility::Logger::LogLevel::Warning);
			this->connect_failure_logged = true;
		}
		::closesocket(login_socket);
		return false;
	}

	try
	{
		std::lock_guard<std::mutex> guard(this->mutex);

		this->login_socket = login_socket;
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "LoginLink::connect_to_login(): std::lock_guard(this->mutex) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}

	if (this->connect_failure_logged)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Info, "connect_to_login(): login server connection restored.");
		this->connect_failure_logged = false;
	}

	return true;
}

void LoginLink::disconnect_from_login(void) noexcept
{
	SOCKET login_socket;

	try
	{
		std::lock_guard<std::mutex> guard(this->mutex);

		login_socket = this->login_socket;
		this->login_socket = INVALID_SOCKET;
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "LoginLink::disconnect_from_login(): std::lock_guard(this->mutex) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}

	if (login_socket != INVALID_SOCKET)
		::closesocket(login_socket);
}

bool LoginLink::send_register(void) noexcept
{
	ajy::network::protocol::PacketBuffer packet(ChatServerConfig::SS_MAX_PACKET_PAYLOAD);
	std::uint16_t type;
	char server_name[16];
	char server_ip[16];

	std::memset(server_name, 0, sizeof(server_name));
	std::memset(server_ip, 0, sizeof(server_ip));
	static_assert(ChatServerConfig::INSTANCE_NAME.size() < sizeof(server_name), "INSTANCE_NAME does not fit SS_REGISTER::server_name");
	static_assert(ChatServerConfig::ADVERTISED_IP.size() < sizeof(server_ip), "ADVERTISED_IP does not fit SS_REGISTER::server_ip");

	std::memcpy(server_name, ChatServerConfig::INSTANCE_NAME.data(), ChatServerConfig::INSTANCE_NAME.size());
	std::memcpy(server_ip, ChatServerConfig::ADVERTISED_IP.data(), ChatServerConfig::ADVERTISED_IP.size());

	type = static_cast<std::uint16_t>(PacketType::SS_REGISTER);
	packet << type << server_name << server_ip << this->listen_port;
	packet.build_header();

	if (!this->send_all(packet.get_buffer_ptr(), packet.get_packet_size()))
		return false;

	this->logger->log(
		ajy::utility::Logger::LogLevel::Info,
		"send_register(): registered as %s at %s:%hu",
		server_name,
		server_ip,
		this->listen_port);

	return true;
}

bool LoginLink::send_all(const void *data, std::size_t size) noexcept
{
	std::size_t sent_bytes;

	sent_bytes = 0;
	while (sent_bytes < size)
	{
		int result;

		result = ::send(this->login_socket, static_cast<const char *>(data) + sent_bytes, static_cast<int>(size - sent_bytes), 0);
		if (result == SOCKET_ERROR)
		{
			if (this->running.load(std::memory_order_relaxed))
				ajy::log_winapi_error("send()", ::WSAGetLastError(), this->logger, ajy::utility::Logger::LogLevel::Warning);
			return false;
		}
		if (!result)
			return false;

		sent_bytes += static_cast<std::size_t>(result);
	}

	return true;
}

bool LoginLink::recv_all(void *data, std::size_t size) noexcept
{
	std::size_t received_bytes;

	received_bytes = 0;
	while (received_bytes < size)
	{
		int result;

		result = ::recv(this->login_socket, static_cast<char *>(data) + received_bytes, static_cast<int>(size - received_bytes), 0);
		if (result == SOCKET_ERROR)
		{
			if (this->running.load(std::memory_order_relaxed))
				ajy::log_winapi_error("recv()", ::WSAGetLastError(), this->logger, ajy::utility::Logger::LogLevel::Warning);
			return false;
		}
		if (!result)
			return false;

		received_bytes += static_cast<std::size_t>(result);
	}

	return true;
}

void LoginLink::recv_loop(void) noexcept
{
	while (this->running.load(std::memory_order_relaxed))
	{
		std::uint16_t payload_length;
		std::uint8_t payload[ChatServerConfig::SS_MAX_PACKET_PAYLOAD];
		std::uint16_t type;

		if (!this->recv_all(&payload_length, sizeof(payload_length)))
			return;

		if (payload_length < sizeof(type) || payload_length > sizeof(payload))
		{
			this->logger->log(ajy::utility::Logger::LogLevel::Warning, "recv_loop(): bad payload length %hu, dropping connection.", payload_length);
			return;
		}

		if (!this->recv_all(payload, payload_length))
			return;

		std::memcpy(&type, payload, sizeof(type));

		switch (static_cast<PacketType>(type))
		{
		case PacketType::SS_NOTIFY_DISCONNECT:
			this->handle_notify_disconnect(payload + sizeof(type), payload_length - sizeof(type));
			break;

		default:
			this->logger->log(ajy::utility::Logger::LogLevel::Warning, "recv_loop(): unknown type %hu, dropping connection.", type);
			return;
		}
	}
}

void LoginLink::handle_notify_disconnect(const void *payload, std::size_t size) noexcept
{
	std::int64_t account_no;

	if (size < sizeof(account_no))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_notify_disconnect(): payload too small.");
		return;
	}

	std::memcpy(&account_no, payload, sizeof(account_no));

	this->server.kick_account(account_no);
}

bool LoginLink::wait_for_reconnect(void) noexcept
{
	try
	{
		std::unique_lock<std::mutex> guard(this->mutex);

		this->cv.wait_for(
			guard,
			std::chrono::milliseconds(ChatServerConfig::LOGIN_LINK_RECONNECT_INTERVAL_MS),
			[this]
			{
				return this->wake || !this->running.load(std::memory_order_relaxed);
			});
		this->wake = false;
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "LoginLink::wait_for_reconnect(): std::unique_lock(this->mutex) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}

	return this->running.load(std::memory_order_relaxed);
}
