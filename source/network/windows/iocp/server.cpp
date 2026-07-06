/**
 * File: server.cpp
 * Path: ajylib/source/network/windows/iocp/server.cpp
 * Description:
 *	A Windows IOCP TCP server definition.
 * Author: ajy-dev
 * Created: 2026-06-30
 * Updated: 2026-07-06
 * Version: 0.1.0
 */

#include <ajy/network/windows/iocp/server.hpp>
#include <ajy/network/protocol/packet_buffer.hpp>
#include <ajy/windows.hpp>

#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <system_error>
#include <thread>
#include <vector>

namespace ajy::network::windows::iocp
{
	Server::Server(std::string_view logger_name) noexcept
		: logger(utility::Logger::get(logger_name))
		, iocp_handle(nullptr)
		, listen_socket(INVALID_SOCKET)
		, max_sessions(0)
	{
		if (!this->logger)
		{
			std::size_t index;

			index = utility::Logger::create(logger_name);

			if (index == utility::Logger::DUPLICATE_NAME)
				this->logger = utility::Logger::get(logger_name);
			else if (index < utility::Logger::INVALID_INDEX)
				this->logger = utility::Logger::get(index);
			else
			{
				std::fprintf(
					stderr,
					"ajy::network::windows::iocp::Server::Server() failed: could not acquire logger \"%.*s\"\n",
					static_cast<int>(logger_name.size()),
					logger_name.data());
				std::terminate();
			}
		}
	}

	Server::~Server(void) noexcept
	{
		this->stop();
	}

	bool Server::start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept
	{
		WSADATA wsa;
		SOCKADDR_IN addr;
		int thread_count;

		if (this->running.load(std::memory_order_acquire))
			return false;

		if (::WSAStartup(MAKEWORD(2, 2), &wsa))
		{
			this->log_winapi_error("WSAStartup()", ::WSAGetLastError());
			return false;
		}
		this->logger->log(utility::Logger::LogLevel::Info, "Winsock initialized.");

		if (wsa.wVersion != MAKEWORD(2, 2))
		{
			this->logger->log(
				utility::Logger::LogLevel::Error,
				"WSAStartup(): requested 2.2 but got %u.%u",
				static_cast<unsigned int>(LOBYTE(wsa.wVersion)),
				static_cast<unsigned int>(HIBYTE(wsa.wVersion)));
			goto clean_wsa;
		}
		this->logger->log(utility::Logger::LogLevel::Info, "Winsock version check passed.");

		this->iocp_handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
		if (!this->iocp_handle)
		{
			this->log_winapi_error("CreateIoCompletionPort()", ::GetLastError());
			goto clean_wsa;
		}
		this->logger->log(utility::Logger::LogLevel::Info, "IO Completion Port created.");

		this->listen_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (this->listen_socket == INVALID_SOCKET)
		{
			this->log_winapi_error("socket()", ::WSAGetLastError());
			goto clean_iocp;
		}
		this->logger->log(utility::Logger::LogLevel::Info, "Listen socket created.");

		if (!nagle)
		{
			BOOL opt;

			opt = TRUE;
			::setsockopt(this->listen_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&opt), sizeof(opt));
			this->logger->log(utility::Logger::LogLevel::Info, "TCP Nagle option turned off.");
		}

		std::memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = ::htons(port);
		if (bind_ip)
			::inet_pton(AF_INET, bind_ip, &addr.sin_addr);
		else
			addr.sin_addr.s_addr = ::htonl(INADDR_ANY);
		if (::bind(this->listen_socket, reinterpret_cast<const SOCKADDR *>(&addr), sizeof(addr)))
		{
			this->log_winapi_error("bind()", ::WSAGetLastError());
			goto clean_socket;
		}
		this->logger->log(utility::Logger::LogLevel::Info, "IP address bound to listen socket.");

		if (::listen(this->listen_socket, SOMAXCONN) == SOCKET_ERROR)
		{
			this->log_winapi_error("listen()", ::WSAGetLastError());
			goto clean_socket;
		}
		this->logger->log(utility::Logger::LogLevel::Info, "Listen socket is listening.");

		this->max_sessions = max_sessions;
		try
		{
			this->sessions = std::make_unique<Session[]>(max_sessions);
		}
		catch (const std::bad_alloc &error)
		{
			this->logger->log(
				utility::Logger::LogLevel::Error,
				"std::make_unique<Session[]>(): %s",
				error.what());
			goto clean_socket;
		}
		this->logger->log(utility::Logger::LogLevel::Info, "Session array allocated.");

		for (std::uint32_t i = 0; i < max_sessions; ++i)
		{
			this->sessions[i].index = i;
			this->sessions[i].socket = INVALID_SOCKET;
			this->sessions[i].pending_send_count.store(0, std::memory_order_relaxed);
			this->sessions[i].in_flight_offset = 0;
			if (!this->free_indices.push(i))
			{
				this->logger->log(utility::Logger::LogLevel::Error, "free_indices.push() failed at index %u", i);
				goto clean_sessions;
			}
		}
		this->logger->log(utility::Logger::LogLevel::Info, "Free index stack populated.");

		this->active_session_count.store(0, std::memory_order_relaxed);
		this->accept_count.store(0, std::memory_order_relaxed);
		this->recv_count.store(0, std::memory_order_relaxed);
		this->send_count.store(0, std::memory_order_relaxed);
		this->last_accept_tps.store(0, std::memory_order_relaxed);
		this->last_recv_tps.store(0, std::memory_order_relaxed);
		this->last_send_tps.store(0, std::memory_order_relaxed);
		this->last_accept_query.store(ServerClock::now(), std::memory_order_relaxed);
		this->last_recv_query.store(ServerClock::now(), std::memory_order_relaxed);
		this->last_send_query.store(ServerClock::now(), std::memory_order_relaxed);
		this->max_pending_sends.store(DEFAULT_MAX_PENDING_SENDS, std::memory_order_relaxed);
		this->running.store(true, std::memory_order_release);

		try
		{
			this->accept_thread = std::thread(accept_thread_proc, this);
		}
		catch (const std::system_error &error)
		{
			this->logger->log(
				utility::Logger::LogLevel::Error,
				"std::thread(accept_thread_proc): [Code: %d] %s",
				error.code().value(),
				error.what());
			goto clean_running;
		}
		this->logger->log(utility::Logger::LogLevel::Info, "Accept thread initialized.");

		if (worker_thread_count <= 0)
		{
			SYSTEM_INFO si;

			::GetSystemInfo(&si);
			thread_count = static_cast<int>(si.dwNumberOfProcessors) * 2;
		}
		else
			thread_count = worker_thread_count;

		for (int i = 0; i < thread_count; ++i)
		{
			try
			{
				this->worker_threads.emplace_back(worker_thread_proc, this);
			}
			catch (const std::bad_alloc &error)
			{
				this->logger->log(
					utility::Logger::LogLevel::Error,
					"std::vector::emplace_back(worker_thread_proc): %s",
					error.what());
				goto clean_threads;
			}
			catch (const std::system_error &error)
			{
				this->logger->log(
					utility::Logger::LogLevel::Error,
					"std::thread(worker_thread_proc): [Code: %d] %s",
					error.code().value(),
					error.what());
				goto clean_threads;
			}
		}
		this->logger->log(
			utility::Logger::LogLevel::Info, "%zu worker threads initialized.", this->worker_threads.size());

		return true;

clean_threads:
		this->running.store(false, std::memory_order_release);
		for (std::size_t i = 0; i < this->worker_threads.size(); ++i)
			::PostQueuedCompletionStatus(this->iocp_handle, 0, 0, nullptr);
		for (std::thread &threads : this->worker_threads)
		{
			if (threads.joinable())
			{
				try
				{
					threads.join();
				}
				catch (const std::system_error &error)
				{
					this->logger->log(
						utility::Logger::LogLevel::Fatal,
						"std::thread::join(worker_thread_proc): [Code: %d] %s",
						error.code().value(),
						error.what());
					std::terminate();
				}
			}
		}
		this->worker_threads.clear();
		::closesocket(this->listen_socket);
		this->listen_socket = INVALID_SOCKET;
		if (this->accept_thread.joinable())
		{
			try
			{
				this->accept_thread.join();
			}
			catch (const std::system_error &error)
			{
				this->logger->log(
					utility::Logger::LogLevel::Fatal,
					"std::thread::join(accept_thread_proc): [Code: %d] %s",
					error.code().value(),
					error.what());
				std::terminate();
			}
		}
clean_running:
		this->running.store(false, std::memory_order_release);
clean_sessions:
		while (this->free_indices.pop().has_value())
			;
		this->sessions.reset();
clean_socket:
		::closesocket(this->listen_socket);
		this->listen_socket = INVALID_SOCKET;
clean_iocp:
		::CloseHandle(this->iocp_handle);
		this->iocp_handle = nullptr;
clean_wsa:
		::WSACleanup();
		return false;
	}

	void Server::stop(void) noexcept
	{
		if (!this->running.exchange(false, std::memory_order_acq_rel))
			return;

		if (this->listen_socket != INVALID_SOCKET)
		{
			::closesocket(this->listen_socket);
			this->listen_socket = INVALID_SOCKET;
		}

		if (this->accept_thread.joinable())
		{
			try
			{
				this->accept_thread.join();
			}
			catch (const std::system_error &error)
			{
				this->logger->log(
					utility::Logger::LogLevel::Fatal,
					"std::thread::join(accept_thread_proc): [Code: %d] %s",
					error.code().value(),
					error.what());
				std::terminate();
			}
		}

		for (std::size_t i = 0; i < this->worker_threads.size(); ++i)
			::PostQueuedCompletionStatus(this->iocp_handle, 0, 0, nullptr);
		for (std::thread &threads : this->worker_threads)
		{
			if (threads.joinable())
			{
				try
				{
					threads.join();
				}
				catch (const std::system_error &error)
				{
					this->logger->log(
						utility::Logger::LogLevel::Fatal,
						"std::thread::join(worker_thread_proc): [Code: %d] %s",
						error.code().value(),
						error.what());
					std::terminate();
				}
			}
		}
		this->worker_threads.clear();

		for (std::uint32_t i = 0; i < this->max_sessions; ++i)
		{
			if (this->sessions[i].socket != INVALID_SOCKET)
			{
				::closesocket(this->sessions[i].socket);
				this->on_client_leave(this->pack_session_id(i, this->sessions[i].generation.load(std::memory_order_relaxed)));
			}
		}
		this->sessions.reset();

		while (this->free_indices.pop().has_value())
			;

		this->active_session_count.store(0, std::memory_order_relaxed);

		if (this->iocp_handle)
		{
			::CloseHandle(this->iocp_handle);
			this->iocp_handle = nullptr;
		}

		::WSACleanup();
	}

	bool Server::disconnect(SessionID id) noexcept
	{
		Session *session;

		session = this->acquire_session(id);
		if (!session)
			return false;

		session->disconnect_flag.store(true, std::memory_order_relaxed);
		::CancelIoEx(reinterpret_cast<HANDLE>(session->socket), nullptr);

		this->logger->log(utility::Logger::LogLevel::Info, "Force disconnect id: %llu", id);

		this->release_session(session);
		return true;
	}

	std::uint32_t Server::get_session_count(void) const noexcept
	{
		return this->active_session_count.load(std::memory_order_relaxed);
	}

	std::uint32_t Server::get_accept_tps(void) noexcept
	{
		return this->calculate_tps(this->accept_count, this->last_accept_query, this->last_accept_tps);
	}

	std::uint32_t Server::get_recv_message_tps(void) noexcept
	{
		return this->calculate_tps(this->recv_count, this->last_recv_query, this->last_recv_tps);
	}

	std::uint32_t Server::get_send_message_tps(void) noexcept
	{
		return this->calculate_tps(this->send_count, this->last_send_query, this->last_send_tps);
	}

	std::shared_ptr<Server::Packet> Server::alloc_packet(std::size_t payload_capacity) noexcept
	{
		std::shared_ptr<Packet> packet;

		try
		{
			packet = std::make_shared<Packet>(payload_capacity);
		}
		catch (const std::bad_alloc &error)
		{
			this->logger->log(utility::Logger::LogLevel::Error, "alloc_packet(): %s", error.what());
			return nullptr;
		}

		if (!packet->get_capacity())
		{
			this->logger->log(utility::Logger::LogLevel::Error, "alloc_packet(): internal buffer allocation failed.");
			return nullptr;
		}

		return packet;
	}

	bool Server::send_packet(SessionID id, std::shared_ptr<Packet> packet) noexcept
	{
		Session *session;
		std::shared_ptr<const Packet> finalized_packet;
		std::uint16_t limit;
		std::uint16_t count;

		if (!packet)
		{
			this->logger->log(utility::Logger::LogLevel::Error, "send_packet(): packet is nullptr. id: %llu", id);
			return false;
		}
		if (!packet->get_data_size())
		{
			this->logger->log(utility::Logger::LogLevel::Error, "send_packet(): packet is empty. id: %llu", id);
			return false;
		}

		session = this->acquire_session(id);
		if (!session)
			return false;

		packet->build_header();
		finalized_packet = packet;

		limit = this->max_pending_sends.load(std::memory_order_relaxed);
		count = session->pending_send_count.load(std::memory_order_relaxed);
		while (count < limit)
			if (session->pending_send_count.compare_exchange_weak(count, count + 1, std::memory_order_relaxed))
				break;

		if (count >= limit)
		{
			this->logger->log(utility::Logger::LogLevel::Warning, "send_packet(): pending_sends full, disconnecting. id: %llu", id);
			this->disconnect(id);
			this->release_session(session);
			return false;
		}

		if (!session->pending_sends.enqueue(std::move(finalized_packet)))
		{
			this->logger->log(utility::Logger::LogLevel::Error, "send_packet(): pending_sends.enqueue() failed. id: %llu", id);
			session->pending_send_count.fetch_sub(1, std::memory_order_relaxed);
			this->release_session(session);
			return false;
		}

		this->send_count.fetch_add(1, std::memory_order_relaxed);

		this->send_post(session);

		this->release_session(session);
		return true;
	}

	void Server::accept_thread_proc(Server *server) noexcept
	{
		SOCKADDR_IN addr;
		int addr_len;
		SOCKET client_socket;
		char ip[16];
		std::optional<std::uint32_t> index;
		Session *session;

		while (server->running.load(std::memory_order_acquire))
		{
			addr_len = sizeof(addr);
			client_socket = ::accept(server->listen_socket, reinterpret_cast<SOCKADDR *>(&addr), &addr_len);
			if (client_socket == INVALID_SOCKET)
			{
				if (server->running.load(std::memory_order_acquire))
					server->log_winapi_error("accept()", ::WSAGetLastError());
				continue;
			}

			::inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
			if (!server->on_connection_request(ip, ::ntohs(addr.sin_port)))
			{
				::closesocket(client_socket);
				continue;
			}

			index = server->free_indices.pop();
			if (!index.has_value())
			{
				server->logger->log(utility::Logger::LogLevel::Warning, "Session limit reached. Connection refused.");
				::closesocket(client_socket);
				continue;
			}

			session = &server->sessions[index.value()];
			session->socket = client_socket;

			if (!::CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), server->iocp_handle, reinterpret_cast<ULONG_PTR>(session), 0))
			{
				server->log_winapi_error("CreateIoCompletionPort()", ::GetLastError());
				::closesocket(client_socket);
				session->socket = INVALID_SOCKET;
				if (!server->free_indices.push(index.value()))
					server->logger->log(utility::Logger::LogLevel::Error, "free_indices.push() failed. Slot lost. index: %u", index.value());
				continue;
			}

			server->accept_count.fetch_add(1, std::memory_order_relaxed);
			server->active_session_count.fetch_add(1, std::memory_order_relaxed);
			session->ref_count.fetch_add(1, std::memory_order_relaxed);
			if (server->recv_post(session))
				server->on_client_join(server->pack_session_id(index.value(), session->generation.load(std::memory_order_relaxed)));
			server->release_session(session);
		}
	}

	void Server::worker_thread_proc(Server *server) noexcept
	{
		while (server->running.load(std::memory_order_acquire))
		{
			DWORD bytes_transferred;
			ULONG_PTR completion_key;
			LPOVERLAPPED overlapped;
			BOOL gqcs_ret;
			Session *session;
			SessionID id;

			gqcs_ret = ::GetQueuedCompletionStatus(server->iocp_handle, &bytes_transferred, &completion_key, &overlapped, INFINITE);
			server->on_worker_thread_begin();

			if (!overlapped)
			{
				if (!gqcs_ret)
					server->log_winapi_error("GetQueuedCompletionStatus()", ::GetLastError());
				server->on_worker_thread_end();
				break;
			}

			session = reinterpret_cast<Session *>(completion_key);
			id = server->pack_session_id(session->index, session->generation.load(std::memory_order_relaxed));

			if (!gqcs_ret)
			{
				DWORD error_code;
				utility::Logger::LogLevel level;
				constexpr std::size_t FUNC_NAME_SIZE = sizeof("GetQueuedCompletionStatus(id: )") + std::numeric_limits<SessionID>::digits10 + 1;
				char func_name[FUNC_NAME_SIZE];

				error_code = ::GetLastError();
				switch (error_code)
				{
				case ERROR_NETNAME_DELETED:
					level = utility::Logger::LogLevel::Debug;
					break;
				default:
					level = utility::Logger::LogLevel::Error;
					break;
				}
				std::sprintf(func_name, "GetQueuedCompletionStatus(id: %llu)", id);
				server->log_winapi_error(func_name, error_code, level);
				server->release_session(session);
				server->on_worker_thread_end();
				continue;
			}

			if (!bytes_transferred)
			{
				server->logger->log(utility::Logger::LogLevel::Info, "Session gracefully disconnected. id: %llu", id);
				server->release_session(session);
				server->on_worker_thread_end();
				continue;
			}

			if (overlapped == &session->recv_overlapped)
			{
				std::vector<std::unique_ptr<Packet>> packets;

				try
				{
					std::lock_guard<std::mutex> lock(session->lock);

					session->recv_buffer.commit_direct_write(bytes_transferred);
					server->recv_post(session);

					while (true)
					{
						std::uint16_t payload_length;
						std::size_t total_size;

						if (session->recv_buffer.get_used_size() < Packet::HEADER_SIZE)
							break;
						session->recv_buffer.peek(&payload_length, Packet::HEADER_SIZE);
						total_size = Packet::HEADER_SIZE + payload_length;

						if (session->recv_buffer.get_used_size() < total_size)
							break;

						session->recv_buffer.commit_direct_read(Packet::HEADER_SIZE);

						packets.push_back(std::make_unique<Packet>(payload_length));
						packets.back()->set_header(&payload_length);
						session->recv_buffer.read(packets.back()->get_payload_ptr(), payload_length);
						packets.back()->commit_direct_serialize(payload_length);
					}
				}
				catch (const std::system_error &error)
				{
					server->logger->log(
						utility::Logger::LogLevel::Fatal,
						"std::lock_guard(session->lock): [Code: %d] %s",
						error.code().value(),
						error.what());
					std::terminate();
				}
				catch (const std::bad_alloc &error)
				{
					server->logger->log(
						utility::Logger::LogLevel::Fatal,
						"std::make_unique<Packet>(packets): %s",
						error.what());
					std::terminate();
				}

				for (std::unique_ptr<Packet> &packet : packets)
				{
					server->recv_count.fetch_add(1, std::memory_order_relaxed);
					server->on_recv(id, std::move(packet));
				}
			}
			else if (overlapped == &session->send_overlapped)
			{
				DWORD remaining;

				remaining = bytes_transferred;
				while (remaining && !session->in_flight_packets.empty())
				{
					std::size_t first_size;

					first_size = session->in_flight_packets.front()->get_packet_size() - session->in_flight_offset;
					if (remaining >= first_size)
					{
						remaining -= static_cast<DWORD>(first_size);
						session->in_flight_packets.pop_front();
						session->in_flight_offset = 0;
						session->pending_send_count.fetch_sub(1, std::memory_order_relaxed);
					}
					else
					{
						session->in_flight_offset += remaining;
						remaining = 0;
					}
				}

				session->send_flag.store(false, std::memory_order_release);
				server->send_post(session);

				server->on_send(id, static_cast<std::size_t>(bytes_transferred));
			}

			server->release_session(session);
			server->on_worker_thread_end();
		}
	}

	Server::SessionID Server::pack_session_id(std::uint32_t index, std::uint32_t generation) noexcept
	{
		static constexpr unsigned int GENERATION_SHIFT = 32;

		return (static_cast<SessionID>(generation) << GENERATION_SHIFT) | static_cast<SessionID>(index);
	}

	std::pair<std::uint32_t, std::uint32_t> Server::unpack_session_id(SessionID id) noexcept
	{
		static constexpr unsigned int GENERATION_SHIFT = 32;

		return std::pair<std::uint32_t, std::uint32_t>(static_cast<std::uint32_t>(id), static_cast<std::uint32_t>(id >> GENERATION_SHIFT));
	}

	std::uint32_t Server::calculate_tps(
		std::atomic<std::uint32_t> &counter,
		std::atomic<ServerClock::time_point> &last_query,
		std::atomic<std::uint32_t> &last_tps) noexcept
	{
		ServerClock::time_point now;
		ServerClock::time_point previous;
		std::uint32_t count;
		std::chrono::duration<double> elapsed;
		double tps;
		std::uint32_t result;

		now = ServerClock::now();
		previous = last_query.exchange(now, std::memory_order_relaxed);

		elapsed = now - previous;
		if (elapsed.count() <= 0.0)
			return last_tps.load(std::memory_order_relaxed);

		count = counter.exchange(0, std::memory_order_relaxed);

		tps = static_cast<double>(count) / elapsed.count();
		result = (tps > static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
			? std::numeric_limits<std::uint32_t>::max()
			: static_cast<std::uint32_t>(tps);

		last_tps.store(result, std::memory_order_relaxed);
		return result;
	}

	Server::Session *Server::acquire_session(SessionID id) noexcept
	{
		std::pair<std::uint32_t, std::uint32_t> decoded;
		Session *session;
		int ref_count;

		decoded = this->unpack_session_id(id);

		if (!this->sessions || decoded.first >= this->max_sessions)
			return nullptr;

		session = &this->sessions[decoded.first];

		ref_count = session->ref_count.load(std::memory_order_relaxed);
		while (ref_count)
			if (session->ref_count.compare_exchange_weak(ref_count, ref_count + 1, std::memory_order_relaxed))
				break;

		if (!ref_count)
			return nullptr;

		if (session->generation.load(std::memory_order_relaxed) != decoded.second)
		{
			this->release_session(session);
			return nullptr;
		}

		return session;
	}

	void Server::release_session(Session *session) noexcept
	{
		if (session->ref_count.fetch_sub(1, std::memory_order_release) == 1)
		{
			SessionID id;

			try
			{
				std::lock_guard<std::mutex> barrier(session->lock);
			}
			catch (const std::system_error &error)
			{
				this->logger->log(
					utility::Logger::LogLevel::Fatal,
					"std::lock_guard(session->lock): [Code: %d] %s",
					error.code().value(),
					error.what());
				std::terminate();
			}

			id = this->pack_session_id(session->index, session->generation.load(std::memory_order_relaxed));

			::closesocket(session->socket);
			session->socket = INVALID_SOCKET;

			session->generation.fetch_add(1, std::memory_order_relaxed);
			session->recv_buffer.clear();
			while (session->pending_sends.dequeue().has_value())
				;
			session->in_flight_packets.clear();
			session->in_flight_offset = 0;
			session->pending_send_count.store(0, std::memory_order_relaxed);
			session->send_flag.store(false, std::memory_order_relaxed);
			session->disconnect_flag.store(false, std::memory_order_relaxed);

			if (!this->free_indices.push(session->index))
				this->logger->log(utility::Logger::LogLevel::Error, "free_indices.push() failed. Slot lost. index: %u", session->index);

			this->active_session_count.fetch_sub(1, std::memory_order_relaxed);

			this->on_client_leave(id);
		}
	}

	bool Server::recv_post(Session *session) noexcept
	{
		WSABUF wsabuf[2];
		DWORD flags;
		DWORD recv_bytes;
		std::size_t total_free;
		std::size_t first;
		std::size_t second;
		SessionID id;

		id = this->pack_session_id(session->index, session->generation.load(std::memory_order_relaxed));
		total_free = session->recv_buffer.get_free_size();
		if (!total_free)
		{
			this->logger->log(utility::Logger::LogLevel::Warning, "recv_post(): recv_buffer is full. Id: %llu", id);
			this->disconnect(id);
			return false;
		}

		first = session->recv_buffer.get_direct_write_size();
		second = total_free - first;

		wsabuf[0].buf = static_cast<char *>(session->recv_buffer.get_direct_write_ptr());
		wsabuf[0].len = static_cast<ULONG>(first);
		wsabuf[1].buf = wsabuf[0].buf - (session->recv_buffer.get_capacity() - first);
		wsabuf[1].len = static_cast<ULONG>(second);

		flags = 0;
		recv_bytes = 0;
		std::memset(&session->recv_overlapped, 0, sizeof(OVERLAPPED));

		if (session->disconnect_flag.load(std::memory_order_relaxed))
			return false;

		if (!this->acquire_session(id))
			return false;

		if (::WSARecv(session->socket, wsabuf, second ? 2 : 1, &recv_bytes, &flags, &session->recv_overlapped, nullptr) == SOCKET_ERROR)
		{
			int error_code;

			error_code = ::WSAGetLastError();
			if (error_code != WSA_IO_PENDING)
			{
				utility::Logger::LogLevel level;

				switch (error_code)
				{
				case WSAECONNRESET:
					level = utility::Logger::LogLevel::Debug;
					break;
				default:
					level = utility::Logger::LogLevel::Error;
					break;
				}
				this->log_winapi_error("WSARecv()", error_code, level);
				this->release_session(session);
				return false;
			}
		}
		return true;
	}

	void Server::send_post(Session *session) noexcept
	{
		WSABUF wsabuf[SEND_BATCH_SIZE];
		std::size_t wsabuf_count;
		DWORD flags;
		DWORD send_bytes;
		SessionID id;

		if (session->send_flag.exchange(true, std::memory_order_acq_rel))
			return;

		id = this->pack_session_id(session->index, session->generation.load(std::memory_order_relaxed));

		while (true)
		{
			if (session->in_flight_packets.empty())
			{
				std::optional<std::shared_ptr<const Packet>> dequeued;
				std::size_t drained;

				drained = 0;
				while (drained < SEND_BATCH_SIZE && (dequeued = session->pending_sends.dequeue()).has_value())
				{
					try
					{
						session->in_flight_packets.push_back(std::move(dequeued.value()));
					}
					catch (const std::bad_alloc &error)
					{
						this->logger->log(
							utility::Logger::LogLevel::Fatal,
							"std::deque::push_back(in_flight_packets): %s",
							error.what());
						std::terminate();
					}
					++drained;
				}
				session->in_flight_offset = 0;
			}

			if (session->in_flight_packets.empty())
			{
				session->send_flag.store(false, std::memory_order_release);

				if (session->pending_sends.is_empty())
					return;
				if (session->send_flag.exchange(true, std::memory_order_acq_rel))
					return;

				continue;
			}

			wsabuf_count = 0;
			for (std::size_t i = 0; i < session->in_flight_packets.size(); ++i, ++wsabuf_count)
			{
				const Packet *current;
				const std::byte *buffer_ptr;
				std::size_t packet_size;

				current = session->in_flight_packets[i].get();
				buffer_ptr = static_cast<const std::byte *>(current->get_buffer_ptr());
				packet_size = current->get_packet_size();

				if (!i)
				{
					buffer_ptr += session->in_flight_offset;
					packet_size -= session->in_flight_offset;
				}

				wsabuf[wsabuf_count].buf = const_cast<char *>(reinterpret_cast<const char *>(buffer_ptr));
				wsabuf[wsabuf_count].len = static_cast<ULONG>(packet_size);
			}

			flags = 0;
			send_bytes = 0;
			std::memset(&session->send_overlapped, 0, sizeof(OVERLAPPED));

			if (session->disconnect_flag.load(std::memory_order_relaxed))
			{
				session->send_flag.store(false, std::memory_order_release);
				return;
			}

			if (!this->acquire_session(id))
			{
				session->send_flag.store(false, std::memory_order_release);
				return;
			}

			if (::WSASend(session->socket, wsabuf, static_cast<DWORD>(wsabuf_count), &send_bytes, flags, &session->send_overlapped, nullptr) == SOCKET_ERROR)
			{
				int error_code;

				error_code = ::WSAGetLastError();
				if (error_code != WSA_IO_PENDING)
				{
					utility::Logger::LogLevel level;

					switch (error_code)
					{
					case WSAECONNRESET:
						level = utility::Logger::LogLevel::Debug;
						break;
					default:
						level = utility::Logger::LogLevel::Error;
						break;
					}
					this->log_winapi_error("WSASend()", error_code, level);
					this->release_session(session);
					session->send_flag.store(false, std::memory_order_release);
					return;
				}
			}

			break;
		}
	}

	void Server::log_winapi_error(const char *func_name, DWORD error_code, utility::Logger::LogLevel level) noexcept
	{
		LPSTR buffer;

		buffer = nullptr;
		::FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			error_code,
			MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
			reinterpret_cast<LPSTR>(&buffer),
			0,
			NULL);

		if (buffer)
		{
			buffer[std::strcspn(buffer, "\r\n")] = '\0';
			this->logger->log(level, "%s: %s", func_name, buffer);
		}
		else
			this->logger->log(level, "%s: Unknown WinAPI error (Code: %lu)", func_name, error_code);

		::LocalFree(buffer);
	}
}
