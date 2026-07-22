/**
 * File: login_server.cpp
 * Path: ajylib/examples/login_gated_chat/login_server/login_server.cpp
 * Description:
 *	Definition of LoginServer (declared in login_server.hpp): the server
 *	lifecycle (shard-worker pool spawn/join). The recv-path type dispatch,
 *	the worker loop, and the DB / Redis / response stages follow in later
 *	sections.
 * Author: ajy-dev
 * Created: 2026-07-22
 * Updated: 2026-08-09
 * Version: 0.1.0
 */

#include <login_gated_chat/login_server/login_server.hpp>
#include <login_gated_chat/login_server/login_server_config.hpp>
#include <login_gated_chat/protocol.hpp>

#include <ajy/utility/logger.hpp>
#include <ajy/windows.hpp>

#include <hiredis.h>
#include <mysql.h>

#include <cassert>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace
{
	/*
	 * "<section>:<instance>:<account_no>" — sections and the instance name come
	 * from LoginServerConfig, the account number is at most 20 characters.
	 */
	constexpr std::size_t REDIS_KEY_SIZE = 64;

	/*
	 * Issues an auth ticket unless the account already holds a session on any
	 * registered instance, all in one atomic step.
	 * KEYS[1] is the ticket key, KEYS[2..] the session keys to check.
	 * Returns 1 issued / 0 already in session.
	 */
	constexpr const char *ISSUE_TICKET_SCRIPT =
		"for i = 2, #KEYS do "
		"if redis.call('EXISTS', KEYS[i]) == 1 then return 0 end "
		"end "
		"redis.call('SET', KEYS[1], ARGV[1], 'EX', ARGV[2]) "
		"return 1";
}

LoginServer::LoginServer(std::string_view logger_name, unsigned int worker_count) noexcept
	: NetServer(logger_name, LoginServerConfig::PROTOCOL_CODE, LoginServerConfig::FIXED_KEY, LoginServerConfig::MAX_PACKET_PAYLOAD)
	, worker_count(!worker_count ? 1 : worker_count)
	, last_auth_query(ServerClock::now())
	, timeout_running(false)
	, content_link(logger_name)
{
	if (::mysql_library_init(0, nullptr, nullptr) != 0)
	{
		std::fprintf(stderr, "LoginServer::LoginServer(): mysql_library_init() failed.\n");
		std::terminate();
	}
}

LoginServer::~LoginServer(void) noexcept
{
	this->stop();
	::mysql_library_end();
}

bool LoginServer::start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept
{
	if (!this->workers.empty())
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "LoginServer::start(): already running.");
		return false;
	}

	this->start_worker_pool();
	this->start_timeout_thread();

	if (!this->content_link.start(nullptr, LoginServerConfig::SS_PORT, 1, false, LoginServerConfig::SS_MAX_SESSIONS))
	{
		this->stop_timeout_thread();
		this->stop_worker_pool();
		return false;
	}

	if (!NetServer::start(bind_ip, port, worker_thread_count, nagle, max_sessions))
	{
		this->content_link.stop();
		this->stop_timeout_thread();
		this->stop_worker_pool();
		return false;
	}

	return true;
}

void LoginServer::stop(void) noexcept
{
	NetServer::stop();
	this->content_link.stop();
	this->stop_timeout_thread();
	this->stop_worker_pool();
}

std::uint32_t LoginServer::get_auth_tps(void) noexcept
{
	ServerClock::time_point now;
	ServerClock::time_point previous;
	std::uint32_t count;
	std::chrono::duration<double> elapsed;
	double tps;

	now = ServerClock::now();
	previous = this->last_auth_query.exchange(now, std::memory_order_relaxed);

	elapsed = now - previous;
	if (elapsed.count() <= 0.0)
		return this->last_auth_tps.load(std::memory_order_relaxed);

	count = 0;
	for (const std::unique_ptr<Worker> &worker : this->workers)
		count += worker->auth_count.exchange(0, std::memory_order_relaxed);

	tps = static_cast<double>(count) / elapsed.count();
	if (tps > static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
		this->last_auth_tps.store(std::numeric_limits<std::uint32_t>::max(), std::memory_order_relaxed);
	else
		this->last_auth_tps.store(static_cast<std::uint32_t>(tps), std::memory_order_relaxed);

	return this->last_auth_tps.load(std::memory_order_relaxed);
}

std::size_t LoginServer::get_job_pool_in_use(void) const noexcept
{
	return this->job_pool.get_in_use_count();
}

bool LoginServer::on_connection_request(const char *, std::uint16_t)
{
	return true;
}

void LoginServer::on_client_join(SessionID id) noexcept
{
	{
		std::lock_guard<std::mutex> lock(this->client_sessions_mutex);

		try
		{
			this->client_sessions.emplace(id, ServerClock::now());
		}
		catch (const std::bad_alloc &error)
		{
			std::fprintf(stderr, "LoginServer::on_client_join(): client_sessions.emplace() failed: %s\n", error.what());
			std::terminate();
		}
	}
}

void LoginServer::on_client_leave(SessionID id) noexcept
{
	std::lock_guard<std::mutex> lock(this->client_sessions_mutex);
	this->client_sessions.erase(id);
}

void LoginServer::on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
{
	std::uint16_t type;

	if (!packet->deserialize(&type, sizeof(type)))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "on_recv(): packet too small for Type. id: %llu", id);
		this->disconnect(id);
		return;
	}

	switch (static_cast<PacketType>(type))
	{
	case PacketType::LOGIN_REQ_LOGIN:
		this->dispatch_login(id, packet.get());
		break;
	default:
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "on_recv(): unknown packet type %u. id: %llu", type, id);
		this->disconnect(id);
		break;
	}
}

void LoginServer::on_send(SessionID, std::size_t) noexcept
{
}

void LoginServer::on_worker_thread_begin(void) noexcept
{
}

void LoginServer::on_worker_thread_end(void) noexcept
{
}

void LoginServer::worker_thread_proc(LoginServer *server, unsigned int index) noexcept
{
	Worker *worker;
	bool stop_requested;

	worker = server->workers[index].get();
	worker->db = server->connect_db();
	worker->redis = server->connect_redis();
	stop_requested = false;

	while (!stop_requested)
	{
		worker->wake.wait(false, std::memory_order_relaxed);
		worker->wake.store(false, std::memory_order_relaxed);

		while (true)
		{
			std::optional<Job *> job;

			job = worker->queue.dequeue();
			if (!job.has_value())
				break;

			if (job.value() == STOP_SENTINEL)
			{
				stop_requested = true;
				break;
			}

			server->process_login(worker, job.value());

			server->job_pool.destroy(job.value());
		}
	}

	::redisFree(worker->redis);
	worker->redis = nullptr;
	::mysql_close(worker->db);
	worker->db = nullptr;
}

void LoginServer::timeout_thread_proc(LoginServer *server) noexcept
{
	ServerClock::time_point next_sweep;

	next_sweep = ServerClock::now();

	while (server->timeout_running.load(std::memory_order_relaxed))
	{
		next_sweep += std::chrono::milliseconds(LoginServerConfig::TIMEOUT_CHECK_INTERVAL_MS);

		server->sweep_timeouts();

		std::this_thread::sleep_until(next_sweep);
	}
}

void LoginServer::start_worker_pool(void) noexcept
{
	try
	{
		this->workers.reserve(this->worker_count);
		for (unsigned int i = 0; i < this->worker_count; ++i)
		{
			std::unique_ptr<Worker> worker;

			worker = std::make_unique<Worker>();
			worker->db = nullptr;
			worker->redis = nullptr;
			this->workers.push_back(std::move(worker));
		}

		for (unsigned int i = 0; i < this->worker_count; ++i)
			this->workers[i]->thread = std::thread(worker_thread_proc, this, i);
	}
	catch (const std::exception &error)
	{
		std::fprintf(stderr, "LoginServer::start_worker_pool(): failed to start worker pool: %s\n", error.what());
		std::terminate();
	}
}

void LoginServer::stop_worker_pool(void) noexcept
{
	if (this->workers.empty())
		return;

	for (std::unique_ptr<Worker> &worker : this->workers)
	{
		worker->queue.enqueue(STOP_SENTINEL);
		worker->wake.store(true, std::memory_order_relaxed);
		worker->wake.notify_one();
	}

	for (std::unique_ptr<Worker> &worker : this->workers)
	{
		if (worker->thread.joinable())
		{
			try
			{
				worker->thread.join();
			}
			catch (const std::system_error &error)
			{
				std::fprintf(stderr, "LoginServer::stop_worker_pool(): thread.join() failed: [Code: %d] %s\n", error.code().value(), error.what());
				std::terminate();
			}
		}
	}

	this->workers.clear();
}

void LoginServer::start_timeout_thread(void) noexcept
{
	this->timeout_running.store(true, std::memory_order_relaxed);

	try
	{
		this->timeout_thread = std::thread(timeout_thread_proc, this);
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "LoginServer::start_timeout_thread(): std::thread failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
}

void LoginServer::stop_timeout_thread(void) noexcept
{
	if (!this->timeout_thread.joinable())
		return;

	this->timeout_running.store(false, std::memory_order_relaxed);

	try
	{
		this->timeout_thread.join();
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "LoginServer::stop_timeout_thread(): thread.join() failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
}

void LoginServer::dispatch_login(SessionID id, Packet *packet) noexcept
{
	std::int64_t account_no;
	std::uint8_t session_key[64];
	Job *job;
	std::hash<std::int64_t> hasher;
	unsigned int worker_index;
	Worker *worker;

	if (packet->get_data_size() < sizeof(account_no) + sizeof(session_key))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "dispatch_login(): payload too small. id: %llu", id);
		this->disconnect(id);
		return;
	}

	*packet >> account_no >> session_key;

	job = this->job_pool.create();
	if (!job)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Error, "dispatch_login(): job_pool.create() failed (out of memory), dropping. id: %llu", id);
		return;
	}

	job->session_id = id;
	job->account_no = account_no;
	std::memcpy(job->session_key, session_key, sizeof(job->session_key));

	worker_index = static_cast<unsigned int>(hasher(account_no) % this->worker_count);
	worker = this->workers[worker_index].get();

	if (!worker->queue.enqueue(job))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Error, "dispatch_login(): queue.enqueue() failed (out of memory), dropping. id: %llu", id);
		this->job_pool.destroy(job);
		return;
	}

	worker->wake.store(true, std::memory_order_relaxed);
	worker->wake.notify_one();
}

void LoginServer::sweep_timeouts(void) noexcept
{
	ServerClock::time_point now;
	std::vector<SessionID> timed_out;

	now = ServerClock::now();

	{
		std::lock_guard<std::mutex> lock(this->client_sessions_mutex);

		try
		{
			for (const std::pair<const SessionID, ServerClock::time_point> &entry : this->client_sessions)
				if (now - entry.second >= std::chrono::milliseconds(LoginServerConfig::CLIENT_TIMEOUT_MS))
					timed_out.push_back(entry.first);
		}
		catch (const std::bad_alloc &error)
		{
			std::fprintf(stderr, "LoginServer::sweep_timeouts(): timed_out.push_back() failed: %s\n", error.what());
			std::terminate();
		}
	}

	for (SessionID id : timed_out)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "Client connection timeout, disconnecting. id: %llu", id);
		this->disconnect(id);
	}
}

void LoginServer::process_login(Worker *worker, Job *job) noexcept
{
	AccountRecord record;
	LoginStatus status;
	std::vector<ContentLink::ServerInfo> servers;

	worker->auth_count.fetch_add(1, std::memory_order_relaxed);

	servers = this->content_link.get_servers();
	record = this->query_account(worker, job->account_no);

	if (record.result == QueryResult::FOUND)
	{
		if (ContentLink::find(servers, LoginServerConfig::CHAT_INSTANCE_NAME))
		{
			this->logger->log(
				ajy::utility::Logger::LogLevel::Info,
				"process_login(): FOUND. account: %lld, session: %llu",
				static_cast<long long>(job->account_no),
				job->session_id);
			status = this->issue_ticket(worker, job, servers);
		}
		else
		{
			this->logger->log(
				ajy::utility::Logger::LogLevel::Error,
				"process_login(): NOSERVER, no chat instance registered. account: %lld",
				static_cast<long long>(job->account_no));
			status = LoginStatus::NOSERVER;
		}
	}
	else if (record.result == QueryResult::MISS)
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Info,
			"process_login(): ACCOUNT_MISS. account: %lld",
			static_cast<long long>(job->account_no));
		status = LoginStatus::ACCOUNT_MISS;
	}
	else
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Error,
			"process_login(): DB_FAIL, rejecting login. account: %lld",
			static_cast<long long>(job->account_no));
		status = LoginStatus::FAIL;
	}

	this->send_res_login(job->session_id, job->account_no, status, record, servers);
}

LoginServer::AccountRecord LoginServer::query_account(Worker *worker, std::int64_t account_no) noexcept
{
	AccountRecord record{};
	char query[128];
	MYSQL_RES *result;
	MYSQL_ROW row;

	std::snprintf(
		query,
		sizeof(query),
		"SELECT userid, usernick FROM v_account WHERE accountno = %lld",
		static_cast<long long>(account_no));

	if (::mysql_query(worker->db, query) != 0)
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Error,
			"query_account(): query failed: %s. account: %lld",
			::mysql_error(worker->db),
			static_cast<long long>(account_no));
		return record;
	}

	result = ::mysql_store_result(worker->db);
	if (!result)
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Error,
			"query_account(): store_result failed: %s. account: %lld",
			::mysql_error(worker->db),
			static_cast<long long>(account_no));
		return record;
	}

	row = ::mysql_fetch_row(result);
	if (row)
	{
		record.result = QueryResult::FOUND;
		this->to_wide_field(record.userid, 20, row[0]);
		this->to_wide_field(record.usernick, 20, row[1]);
	}
	else
		record.result = QueryResult::MISS;

	::mysql_free_result(result);

	return record;
}

LoginStatus LoginServer::issue_ticket(Worker *worker, const Job *job, const std::vector<ContentLink::ServerInfo> &servers) noexcept
{
	constexpr std::size_t MAX_EVAL_ARGS = 6 + LoginServerConfig::SS_MAX_SESSIONS;

	redisReply *reply;
	LoginStatus status;
	std::to_chars_result convert_result;
	char ticket_key[REDIS_KEY_SIZE];
	char session_keys[LoginServerConfig::SS_MAX_SESSIONS][REDIS_KEY_SIZE];
	char key_count_arg[std::numeric_limits<std::size_t>::digits10 + 2];
	char ttl_arg[std::numeric_limits<std::uint32_t>::digits10 + 2];
	const char *argv[MAX_EVAL_ARGS];
	std::size_t argv_length[MAX_EVAL_ARGS];
	std::size_t session_key_index;
	std::size_t argc;

	assert(servers.size() <= LoginServerConfig::SS_MAX_SESSIONS && "issue_ticket: registration count exceeds the SS listener limit");

	std::snprintf(
		ticket_key,
		sizeof(ticket_key),
		"%.*s:%.*s:%lld",
		static_cast<int>(LoginServerConfig::TICKET_SECTION.size()),
		LoginServerConfig::TICKET_SECTION.data(),
		static_cast<int>(LoginServerConfig::CHAT_INSTANCE_NAME.size()),
		LoginServerConfig::CHAT_INSTANCE_NAME.data(),
		static_cast<long long>(job->account_no));

	for (std::size_t i = 0; i < servers.size(); ++i)
	{
		std::snprintf(
			session_keys[i],
			sizeof(session_keys[i]),
			"%.*s:%s:%lld",
			static_cast<int>(LoginServerConfig::SESSION_SECTION.size()),
			LoginServerConfig::SESSION_SECTION.data(),
			servers[i].name,
			static_cast<long long>(job->account_no));
	}

	convert_result = std::to_chars(key_count_arg, key_count_arg + sizeof(key_count_arg), servers.size() + 1);
	*convert_result.ptr = '\0';

	convert_result = std::to_chars(ttl_arg, ttl_arg + sizeof(ttl_arg), LoginServerConfig::TICKET_TTL_SEC);
	*convert_result.ptr = '\0';

	argc = 0;

	argv[argc++] = "EVAL";
	argv[argc++] = ISSUE_TICKET_SCRIPT;
	argv[argc++] = key_count_arg;
	argv[argc++] = ticket_key;
	for (std::size_t i = 0; i < servers.size(); ++i)
		argv[argc++] = session_keys[i];
	session_key_index = argc;
	argv[argc++] = reinterpret_cast<const char *>(job->session_key);
	argv[argc++] = ttl_arg;

	// The session key is binary and may contain nulls
	for (std::size_t i = 0; i < argc; ++i)
		argv_length[i] = i == session_key_index ? sizeof(job->session_key) : std::strlen(argv[i]);

	reply = static_cast<redisReply *>(::redisCommandArgv(worker->redis, static_cast<int>(argc), argv, argv_length));
	if (!reply)
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Error,
			"issue_ticket(): EVAL failed: %s. account: %lld",
			worker->redis->errstr,
			static_cast<long long>(job->account_no));
		return LoginStatus::FAIL;
	}

	if (reply->type != REDIS_REPLY_INTEGER)
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Error,
			"issue_ticket(): unexpected EVAL reply type %d. account: %lld",
			reply->type,
			static_cast<long long>(job->account_no));
		::freeReplyObject(reply);
		return LoginStatus::FAIL;
	}

	if (reply->integer == 1)
		status = LoginStatus::OK;
	else if (!reply->integer)
	{
		this->content_link.broadcast_disconnect(job->account_no);
		this->logger->log(
			ajy::utility::Logger::LogLevel::Warning,
			"issue_ticket(): duplicate login, notified content servers. account: %lld",
			static_cast<long long>(job->account_no));
		status = LoginStatus::GAME;
	}
	else
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Error,
			"issue_ticket(): unexpected EVAL return %lld. account: %lld",
			reply->integer,
			static_cast<long long>(job->account_no));
		status = LoginStatus::FAIL;
	}

	::freeReplyObject(reply);

	return status;
}

void LoginServer::to_wide_field(std::uint16_t *dst, int dst_count, const char *src) noexcept
{
	int converted;

	converted = ::MultiByteToWideChar(
		CP_UTF8,
		0,
		src,
		-1,
		reinterpret_cast<wchar_t *>(dst),
		dst_count);

	if (!converted)
	{
		std::memset(dst, 0, static_cast<std::size_t>(dst_count) * sizeof(std::uint16_t));
		return;
	}

	dst[dst_count - 1] = 0;
}

std::shared_ptr<LoginServer::Packet> LoginServer::make_res_login(
	std::int64_t account_no,
	LoginStatus status,
	const AccountRecord &record,
	const std::vector<ContentLink::ServerInfo> &servers) noexcept
{
	constexpr std::size_t PAYLOAD_SIZE =
		sizeof(std::uint16_t) // type
		+ sizeof(std::int64_t) // account_no
		+ sizeof(std::int8_t) // status
		+ sizeof(AccountRecord::userid) // id[20]
		+ sizeof(AccountRecord::usernick) // nickname[20]
		+ sizeof(std::uint16_t) * 16 // game_server_ip[16]
		+ sizeof(std::uint16_t) // game_server_port
		+ sizeof(std::uint16_t) * 16 // chat_server_ip[16]
		+ sizeof(std::uint16_t); // chat_server_port

	std::shared_ptr<Packet> packet;
	std::uint16_t type;
	std::int8_t status_byte;
	const ContentLink::ServerInfo *game;
	const ContentLink::ServerInfo *chat;
	std::uint16_t game_ip[16];
	std::uint16_t chat_ip[16];
	std::uint16_t game_port;
	std::uint16_t chat_port;

	packet = this->alloc_packet(PAYLOAD_SIZE);
	if (!packet)
	{
		std::fprintf(stderr, "LoginServer::make_res_login(): alloc_packet() failed (out of memory).\n");
		std::terminate();
	}

	game = ContentLink::find(servers, LoginServerConfig::GAME_INSTANCE_NAME);
	chat = ContentLink::find(servers, LoginServerConfig::CHAT_INSTANCE_NAME);

	this->to_wide_field(game_ip, 16, game ? game->ip : "");
	this->to_wide_field(chat_ip, 16, chat ? chat->ip : "");
	game_port = game ? game->port : 0;
	chat_port = chat ? chat->port : 0;

	type = static_cast<std::uint16_t>(PacketType::LOGIN_RES_LOGIN);
	status_byte = static_cast<std::int8_t>(status);

	*packet << type
		<< account_no
		<< status_byte
		<< record.userid
		<< record.usernick
		<< game_ip
		<< game_port
		<< chat_ip
		<< chat_port;

	return packet;
}

void LoginServer::send_res_login(
	SessionID id,
	std::int64_t account_no,
	LoginStatus status,
	const AccountRecord &record,
	const std::vector<ContentLink::ServerInfo> &servers) noexcept
{
	std::shared_ptr<Packet> packet;

	packet = this->make_res_login(account_no, status, record, servers);
	this->send_packet(id, packet);
}

MYSQL *LoginServer::connect_db(void) noexcept
{
	MYSQL *handle;
	MYSQL *connect_ret;

	handle = ::mysql_init(nullptr);
	if (!handle)
	{
		std::fprintf(stderr, "LoginServer::connect_db(): mysql_init() failed.\n");
		std::terminate();
	}

	connect_ret = ::mysql_real_connect(
		handle,
		LoginServerConfig::DB_HOST.data(),
		LoginServerConfig::DB_USER.data(),
		LoginServerConfig::DB_PASSWORD.data(),
		LoginServerConfig::DB_NAME.data(),
		LoginServerConfig::DB_PORT,
		nullptr,
		0);

	if (!connect_ret)
	{
		std::fprintf(stderr, "LoginServer::connect_db(): mysql_real_connect() failed: %s\n", ::mysql_error(handle));
		::mysql_close(handle);
		std::terminate();
	}

	if (::mysql_set_character_set(handle, "utf8mb4") != 0)
	{
		std::fprintf(stderr, "LoginServer::connect_db(): mysql_set_character_set() failed: %s\n", ::mysql_error(handle));
		::mysql_close(handle);
		std::terminate();
	}

	return handle;
}

redisContext *LoginServer::connect_redis(void) noexcept
{
	redisContext *context;

	context = ::redisConnect(LoginServerConfig::REDIS_HOST.data(), LoginServerConfig::REDIS_PORT);
	if (!context)
	{
		std::fprintf(stderr, "LoginServer::connect_redis(): redisConnect() failed (allocation).\n");
		std::terminate();
	}

	if (context->err)
	{
		std::fprintf(stderr, "LoginServer::connect_redis(): redisConnect() failed: %s\n", context->errstr);
		::redisFree(context);
		std::terminate();
	}

	return context;
}
