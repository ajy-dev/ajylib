/**
 * File: server.cpp
 * Path: ajylib/source/network/windows/iocp/server.cpp
 * Description:
 *	A Windows IOCP TCP server definition.
 * Author: ajy-dev
 * Created: 2026-06-30
 * Updated: Never
 * Version: 0.1.0
 */

#include <ajy/network/windows/iocp/server.hpp>
#include <ajy/container/serialization_buffer.hpp>
#include <ajy/windows.hpp>

#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ajy::network::windows::iocp
{
	Server::Server(std::string_view logger_name) noexcept
		: logger(utility::Logger::get(logger_name))
		, iocp_handle(nullptr)
		, listen_socket(INVALID_SOCKET)
		, running(false)
		, max_sessions(0)
		, next_sid(0)
		, session_pool()
		, accept_tps(0)
		, recv_count(0)
		, send_count(0)
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

		try
		{
			std::unique_lock<std::shared_mutex> lock(this->session_map_lock);

			for (std::pair<const SessionID, Session *> &pair : this->session_map)
			{
				Session *session;

				session = pair.second;
				::closesocket(session->socket);
				this->session_pool.destroy(session);
			}
			this->session_map.clear();
		}
		catch (const std::system_error &error)
		{
			this->logger->log(
				utility::Logger::LogLevel::Fatal,
				"std::unique_lock(session_map_lock): [Code: %d] %s",
				error.code().value(),
				error.what());
			std::terminate();
		}

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

		session = nullptr;

		try
		{
			std::shared_lock<std::shared_mutex> lock(this->session_map_lock);
			std::unordered_map<SessionID, Session *>::iterator it;

			it = this->session_map.find(id);
			if (it != this->session_map.end())
			{
				int ref_count;

				session = it->second;
				ref_count = session->ref_count.load(std::memory_order_relaxed);
				while (ref_count)
					if (session->ref_count.compare_exchange_weak(ref_count, ref_count + 1, std::memory_order_relaxed))
						break;

				if (!ref_count)
					session = nullptr;
			}
		}
		catch (const std::system_error &error)
		{
			this->logger->log(
				utility::Logger::LogLevel::Fatal,
				"std::shared_lock(session_map_lock): [Code: %d] %s",
				error.code().value(),
				error.what());
			std::terminate();
		}

		if (!session)
			return false;

		session->disconnect_flag.store(true, std::memory_order_relaxed);
		::CancelIoEx(reinterpret_cast<HANDLE>(session->socket), nullptr);

		this->decrement_ref_count(session);
		return true;
	}

	bool Server::send_packet(SessionID id, container::SerializationBuffer *packet) noexcept
	{
		Session *session;
		const void *payload;
		std::uint16_t payload_length;
		bool success;

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
		if (packet->get_data_size() > std::numeric_limits<std::uint16_t>::max())
		{
			this->logger->log(utility::Logger::LogLevel::Error, "send_packet(): packet too large (%zu bytes). id: %llu", packet->get_data_size(), id);
			return false;
		}

		session = nullptr;

		try
		{
			std::shared_lock<std::shared_mutex> lock(this->session_map_lock);
			std::unordered_map<SessionID, Session *>::iterator it;

			it = this->session_map.find(id);
			if (it != this->session_map.end())
			{
				int ref_count;

				session = it->second;
				ref_count = session->ref_count.load(std::memory_order_relaxed);
				while (ref_count)
					if (session->ref_count.compare_exchange_weak(ref_count, ref_count + 1, std::memory_order_relaxed))
						break;

				if (!ref_count)
					session = nullptr;
			}
		}
		catch (const std::system_error &error)
		{
			this->logger->log(
				utility::Logger::LogLevel::Fatal,
				"std::shared_lock(session_map_lock): [Code: %d] %s",
				error.code().value(),
				error.what());
			std::terminate();
		}
		if (!session)
			return false;

		payload = packet->get_buffer_ptr();
		payload_length = static_cast<std::uint16_t>(packet->get_data_size());
		success = false;

		try
		{
			std::lock_guard<std::mutex> lock(session->lock);

			if (session->send_buffer.get_free_size() >= sizeof(payload_length) + payload_length)
			{
				session->send_buffer.write(&payload_length, sizeof(payload_length));
				session->send_buffer.write(payload, payload_length);

				if (!session->send_flag.exchange(true, std::memory_order_relaxed))
					if (!this->send_post(session))
						session->send_flag.store(false, std::memory_order_relaxed);

				success = true;
			}
			else
				this->logger->log(utility::Logger::LogLevel::Error, "send_packet(): send_buffer full. id: %llu", id);
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

		this->decrement_ref_count(session);
		return success;
	}

	std::uint32_t Server::get_session_count(void) const noexcept
	{
		std::uint32_t count;

		try
		{
			std::shared_lock<std::shared_mutex> lock(this->session_map_lock);

			count = static_cast<std::uint32_t>(this->session_map.size());
		}
		catch (const std::system_error &error)
		{
			this->logger->log(
				utility::Logger::LogLevel::Fatal,
				"std::shared_lock(session_map_lock): [Code: %d] %s",
				error.code().value(),
				error.what());
			std::terminate();
		}

		return count;
	}

	std::uint32_t Server::get_accept_tps(void) noexcept
	{
		return this->accept_tps.exchange(0, std::memory_order_relaxed);
	}

	std::uint32_t Server::get_recv_message_tps(void) noexcept
	{
		return this->recv_count.exchange(0, std::memory_order_relaxed);
	}

	std::uint32_t Server::get_send_message_tps(void) noexcept
	{
		return this->send_count.exchange(0, std::memory_order_relaxed);
	}

	void Server::accept_thread_proc(Server *server) noexcept
	{
		SOCKADDR_IN addr;
		int addr_len;
		SOCKET client_socket;
		char ip[16];
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

			session = server->session_pool.create();
			if (!session)
			{
				server->logger->log(utility::Logger::LogLevel::Error, "Session allocation failed. No memory.");
				::closesocket(client_socket);
				continue;
			}

			session->id = server->next_sid.fetch_add(1, std::memory_order_relaxed);
			session->socket = client_socket;

			if (!::CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), server->iocp_handle, reinterpret_cast<ULONG_PTR>(session), 0))
			{
				server->log_winapi_error("CreateIoCompletionPort()", ::GetLastError());
				server->session_pool.destroy(session);
				::closesocket(client_socket);
				continue;
			}

			try
			{
				std::unique_lock<std::shared_mutex> lock(server->session_map_lock);

				if (server->session_map.size() >= server->max_sessions)
				{
					lock.unlock();
					server->logger->log(utility::Logger::LogLevel::Warning, "Session limit reached. Connection refused.");
					server->session_pool.destroy(session);
					::closesocket(client_socket);
					continue;
				}
				server->session_map.emplace(session->id, session);
			}
			catch (const std::system_error &error)
			{
				server->logger->log(
					utility::Logger::LogLevel::Fatal,
					"std::unique_lock(session_map_lock): [Code: %d] %s",
					error.code().value(),
					error.what());
				std::terminate();
			}
			catch (const std::bad_alloc &error)
			{
				server->logger->log(
					utility::Logger::LogLevel::Fatal,
					"std::unordered_map::emplace(session_map): %s",
					error.what());
				std::terminate();
			}

			server->accept_tps.fetch_add(1, std::memory_order_relaxed);
			session->ref_count.fetch_add(1, std::memory_order_relaxed);
			if (server->recv_post(session))
				server->on_client_join(session->id);
			server->decrement_ref_count(session);
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
				std::sprintf(func_name, "GetQueuedCompletionStatus(id: %llu)", session->id);
				server->log_winapi_error(func_name, error_code, level);
				server->decrement_ref_count(session);
				server->on_worker_thread_end();
				continue;
			}

			if (!bytes_transferred)
			{
				server->logger->log(utility::Logger::LogLevel::Info, "Session gracefully disconnected. id: %llu", session->id);
				server->decrement_ref_count(session);
				server->on_worker_thread_end();
				continue;
			}

			if (overlapped == &session->recv_overlapped)
			{
				constexpr std::size_t HEADER_SIZE = sizeof(std::uint16_t);
				std::vector<container::SerializationBuffer> packets;

				try
				{
					std::lock_guard<std::mutex> lock(session->lock);

					session->recv_buffer.commit_direct_write(bytes_transferred);
					server->recv_post(session);

					while (true)
					{
						std::uint16_t payload_length;
						std::size_t total_size;

						if (session->recv_buffer.get_used_size() < HEADER_SIZE)
							break;
						session->recv_buffer.peek(&payload_length, HEADER_SIZE);
						total_size = HEADER_SIZE + payload_length;

						if (session->recv_buffer.get_used_size() < total_size)
							break;

						session->recv_buffer.commit_direct_read(HEADER_SIZE);

						packets.emplace_back(payload_length);
						session->recv_buffer.read(packets.back().get_buffer_ptr(), payload_length);
						packets.back().commit_direct_serialize(payload_length);
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
						"std::vector::emplace_back(packets): %s",
						error.what());
					std::terminate();
				}

				for (container::SerializationBuffer &packet : packets)
				{
					server->recv_count.fetch_add(1, std::memory_order_relaxed);
					server->on_recv(session->id, &packet);
				}
			}
			else if (overlapped == &session->send_overlapped)
			{
				try
				{
					std::lock_guard<std::mutex> lock(session->lock);

					session->send_buffer.commit_direct_read(bytes_transferred);

					if (session->send_buffer.get_used_size())
					{
						if (!server->send_post(session))
							session->send_flag.store(false, std::memory_order_relaxed);
					}
					else
						session->send_flag.store(false, std::memory_order_relaxed);
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

				server->send_count.fetch_add(1, std::memory_order_relaxed);
				server->on_send(session->id, static_cast<std::size_t>(bytes_transferred));
			}

			server->decrement_ref_count(session);
			server->on_worker_thread_end();
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
		int ref_count;

		total_free = session->recv_buffer.get_free_size();
		if (!total_free)
		{
			this->logger->log(utility::Logger::LogLevel::Error, "recv_post(): recv_buffer is full. id: %llu", session->id);
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

		ref_count = session->ref_count.load(std::memory_order_relaxed);
		while (ref_count)
			if (session->ref_count.compare_exchange_weak(ref_count, ref_count + 1, std::memory_order_relaxed))
				break;

		if (!ref_count)
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
				this->decrement_ref_count(session);
				return false;
			}
		}
		return true;
	}

	bool Server::send_post(Session *session) noexcept
	{
		WSABUF wsabuf[2];
		DWORD flags;
		DWORD send_bytes;
		std::size_t total_used;
		std::size_t first;
		std::size_t second;
		int ref_count;

		total_used = session->send_buffer.get_used_size();
		if (!total_used)
			return false;

		first = session->send_buffer.get_direct_read_size();
		second = total_used - first;

		wsabuf[0].buf = static_cast<char *>(session->send_buffer.get_direct_read_ptr());
		wsabuf[0].len = static_cast<ULONG>(first);
		wsabuf[1].buf = wsabuf[0].buf - (session->send_buffer.get_capacity() - first);
		wsabuf[1].len = static_cast<ULONG>(second);

		flags = 0;
		send_bytes = 0;
		std::memset(&session->send_overlapped, 0, sizeof(OVERLAPPED));

		if (session->disconnect_flag.load(std::memory_order_relaxed))
			return false;

		ref_count = session->ref_count.load(std::memory_order_relaxed);
		while (ref_count)
			if (session->ref_count.compare_exchange_weak(ref_count, ref_count + 1, std::memory_order_relaxed))
				break;

		if (!ref_count)
			return false;

		if (::WSASend(session->socket, wsabuf, second ? 2 : 1, &send_bytes, flags, &session->send_overlapped, nullptr) == SOCKET_ERROR)
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
				this->decrement_ref_count(session);
				return false;
			}
		}

		return true;
	}

	void Server::decrement_ref_count(Session *session) noexcept
	{
		if (session->ref_count.fetch_sub(1, std::memory_order_release) == 1)
		{
			SessionID id;

			id = session->id;

			try
			{
				std::unique_lock<std::shared_mutex> lock(this->session_map_lock);

				this->session_map.erase(id);

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
			}
			catch (const std::system_error &error)
			{
				this->logger->log(
					utility::Logger::LogLevel::Fatal,
					"std::unique_lock(session_map_lock): [Code: %d] %s",
					error.code().value(),
					error.what());
				std::terminate();
			}

			::closesocket(session->socket);
			this->session_pool.destroy(session);

			this->on_client_leave(id);
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
