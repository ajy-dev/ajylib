/**
 * File: content_link.cpp
 * Path: ajylib/examples/login_gated_chat/login_server/content_link.cpp
 * Description:
 * 	SS listener that tracks content server registrations
 * Author: ajy-dev
 * Created: 2026-08-07
 * Updated: Never
 * Version: 0.1.0
 */

#include <login_gated_chat/login_server/content_link.hpp>
#include <login_gated_chat/login_server/login_server_config.hpp>
#include <login_gated_chat/protocol.hpp>

#include <ajy/utility/logger.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

ContentLink::ContentLink(std::string_view logger_name) noexcept
	: ajy::network::windows::iocp::Server(logger_name, LoginServerConfig::SS_MAX_PACKET_PAYLOAD)
{
}

ContentLink::~ContentLink(void) noexcept
{
}

const ContentLink::ServerInfo *ContentLink::find(const std::vector<ServerInfo> &servers, std::string_view name) noexcept
{
	for (const ServerInfo &server : servers)
	{
		if (!name.compare(server.name))
			return &server;
	}

	return nullptr;
}

std::vector<ContentLink::ServerInfo> ContentLink::get_servers(void) const noexcept
{
	std::vector<ServerInfo> result;

	try
	{
		std::shared_lock<std::shared_mutex> guard(this->lock);

		result.reserve(this->servers.size());
		for (const std::pair<const SessionID, ServerInfo> &entry : this->servers)
			result.push_back(entry.second);
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "ContentLink::get_servers(): std::shared_lock(this->lock) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "ContentLink::get_servers(): result allocation failed: %s\n", error.what());
		std::terminate();
	}

	return result;
}

void ContentLink::broadcast_disconnect(std::int64_t account_no) noexcept
{
	constexpr std::size_t PAYLOAD_SIZE = sizeof(std::uint16_t) + sizeof(std::int64_t);

	std::shared_ptr<Packet> packet;
	std::uint16_t type;
	std::vector<SessionID> targets;

	packet = this->alloc_packet(PAYLOAD_SIZE);
	if (!packet)
	{
		std::fprintf(stderr, "ContentLink::broadcast_disconnect(): alloc_packet() failed (out of memory).\n");
		std::terminate();
	}

	type = static_cast<std::uint16_t>(PacketType::SS_NOTIFY_DISCONNECT);
	*packet << type << account_no;

	try
	{
		std::shared_lock<std::shared_mutex> guard(this->lock);

		targets.reserve(this->servers.size());
		for (const std::pair<const SessionID, ServerInfo> &entry : this->servers)
			targets.push_back(entry.first);
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "ContentLink::broadcast_disconnect(): std::shared_lock(this->lock) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "ContentLink::broadcast_disconnect(): targets allocation failed: %s\n", error.what());
		std::terminate();
	}

	for (SessionID id : targets)
		this->send_packet(id, packet);
}

bool ContentLink::on_connection_request(const char *ip, std::uint16_t port)
{
	static_cast<void>(ip);
	static_cast<void>(port);

	return true;
}

void ContentLink::on_client_join(SessionID id) noexcept
{
	static_cast<void>(id);
}

void ContentLink::on_client_leave(SessionID id) noexcept
{
	try
	{
		std::lock_guard<std::shared_mutex> guard(this->lock);

		this->servers.erase(id);
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "ContentLink::on_client_leave(): std::lock_guard(this->lock) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
}

void ContentLink::on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
{
	std::uint16_t type;

	if (packet->get_data_size() < sizeof(type))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "on_recv(): payload too small. id: %llu", id);
		this->disconnect(id);
		return;
	}

	*packet >> type;

	switch (static_cast<PacketType>(type))
	{
	case PacketType::SS_REGISTER:
		this->handle_ss_register(id, packet.get());
		break;

	default:
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "on_recv(): unknown type %hu. id: %llu", type, id);
		this->disconnect(id);
		break;
	}
}

void ContentLink::on_send(SessionID id, std::size_t size) noexcept
{
	static_cast<void>(id);
	static_cast<void>(size);
}

void ContentLink::on_worker_thread_begin(void) noexcept
{
}

void ContentLink::on_worker_thread_end(void) noexcept
{
}

void ContentLink::handle_ss_register(SessionID id, Packet *packet) noexcept
{
	ServerInfo info;
	ServerInfo duplicate;
	bool duplicate_found = false;

	if (packet->get_data_size() < sizeof(info.name) + sizeof(info.ip) + sizeof(info.port))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_ss_register(): payload too small. id: %llu", id);
		this->disconnect(id);
		return;
	}

	*packet >> info.name >> info.ip >> info.port;

	info.name[sizeof(info.name) - 1] = '\0';
	info.ip[sizeof(info.ip) - 1] = '\0';

	try
	{
		std::lock_guard<std::shared_mutex> guard(this->lock);

		for (const std::pair<const SessionID, ServerInfo> &entry : this->servers)
		{
			if (!std::strcmp(entry.second.name, info.name))
			{
				duplicate = entry.second;
				duplicate_found = true;
				break;
			}
		}

		this->servers.insert_or_assign(id, info);
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "ContentLink::handle_ss_register(): std::lock_guard(this->lock) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "ContentLink::handle_ss_register(): servers.insert_or_assign() failed: %s\n", error.what());
		std::terminate();
	}

	if (duplicate_found)
		this->logger->log(
			ajy::utility::Logger::LogLevel::Warning,
			"handle_ss_register(): instance name %s already registered at %s:%hu. id: %llu",
			info.name,
			duplicate.ip,
			duplicate.port,
			id);

	this->logger->log(
		ajy::utility::Logger::LogLevel::Info,
		"handle_ss_register(): %s registered at %s:%hu. id: %llu",
		info.name,
		info.ip,
		info.port,
		id);
}
