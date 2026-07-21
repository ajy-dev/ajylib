/**
 * File: chat_server.cpp
 * Path: ajylib/examples/chat_server/multi_thread/chat_server.cpp
 * Description:
 *	Definition of ChatServer (declared in chat_server.hpp): the server
 *	lifecycle (content thread and send-worker pool spawn/join). Worker
 *	callbacks, the content thread loop, job/packet handlers, sector utilities,
 *	response builders and the send-worker pool follow in later sections.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: 2026-07-21
 * Version: 0.1.0
 */

#include <chat_server/multi_thread/chat_server.hpp>
#include <chat_server/protocol.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <exception>
#include <functional>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <random>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

ChatServer::ChatServer(std::string_view logger_name, unsigned int send_worker_count) noexcept
	: NetServer(logger_name, ChatServerConfig::PROTOCOL_CODE, ChatServerConfig::FIXED_KEY, ChatServerConfig::MAX_PACKET_PAYLOAD)
	, content_wake(false)
	, content_running(false)
	, content_job_count(0)
	, last_content_query(ServerClock::now())
	, last_content_tps(0)
	, player_count(0)
	, send_worker_count(!send_worker_count ? 1 : send_worker_count)
{
}

ChatServer::~ChatServer(void) noexcept
{
	this->stop();
}

bool ChatServer::start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept
{
	if (this->content_thread.joinable())
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "ChatServer::start(): already running.");
		return false;
	}

	this->content_running.store(true, std::memory_order_relaxed);

	this->start_send_pool(this->send_worker_count);

	try
	{
		this->content_thread = std::thread(content_thread_proc, this);
	}
	catch (const std::system_error &error)
	{
		this->logger->log(
			ajy::utility::Logger::LogLevel::Error,
			"std::thread(content_thread_proc): [Code: %d] %s",
			error.code().value(),
			error.what());
		this->content_running.store(false, std::memory_order_relaxed);
		this->stop_send_pool();
		return false;
	}

	if (!NetServer::start(bind_ip, port, worker_thread_count, nagle, max_sessions))
	{
		{
			std::lock_guard<std::mutex> lock(this->content_mutex);
			this->content_running.store(false, std::memory_order_relaxed);
			this->content_wake = true;
		}
		this->content_cv.notify_one();

		if (this->content_thread.joinable())
		{
			try
			{
				this->content_thread.join();
			}
			catch (const std::system_error &error)
			{
				this->logger->log(
					ajy::utility::Logger::LogLevel::Fatal,
					"std::thread::join(content_thread_proc): [Code: %d] %s",
					error.code().value(),
					error.what());
				std::terminate();
			}
		}

		this->stop_send_pool();
		return false;
	}

	return true;
}

void ChatServer::stop(void) noexcept
{
	NetServer::stop();

	if (this->content_thread.joinable())
	{
		{
			std::lock_guard<std::mutex> lock(this->content_mutex);
			this->content_running.store(false, std::memory_order_relaxed);
			this->content_wake = true;
		}
		this->content_cv.notify_one();

		try
		{
			this->content_thread.join();
		}
		catch (const std::system_error &error)
		{
			this->logger->log(
				ajy::utility::Logger::LogLevel::Fatal,
				"std::thread::join(content_thread_proc): [Code: %d] %s",
				error.code().value(),
				error.what());
			std::terminate();
		}
	}

	this->stop_send_pool();

	while (true)
	{
		std::optional<Job *> job_optional;

		job_optional = this->job_queue.dequeue();
		if (!job_optional.has_value())
			break;
		this->job_pool.destroy(*job_optional);
	}

	for (std::pair<const SessionID, Player *> &entry : this->player_map)
		this->player_pool.destroy(entry.second);
	this->player_map.clear();

	for (Sector &sector : this->sectors)
		sector.players.clear();

	this->content_wake = false;
}

std::uint32_t ChatServer::get_content_job_tps(void) noexcept
{
	ServerClock::time_point now;
	ServerClock::time_point previous;
	std::uint32_t count;
	std::chrono::duration<double> elapsed;
	double tps;

	now = ServerClock::now();
	previous = this->last_content_query.exchange(now, std::memory_order_relaxed);

	elapsed = now - previous;
	if (elapsed.count() <= 0.0)
		return this->last_content_tps.load(std::memory_order_relaxed);

	count = this->content_job_count.exchange(0, std::memory_order_relaxed);

	tps = static_cast<double>(count) / elapsed.count();
	if (tps > static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
		this->last_content_tps.store(std::numeric_limits<std::uint32_t>::max(), std::memory_order_relaxed);
	else
		this->last_content_tps.store(static_cast<std::uint32_t>(tps), std::memory_order_relaxed);

	return this->last_content_tps.load(std::memory_order_relaxed);
}

std::uint32_t ChatServer::get_player_count(void) const noexcept
{
	return this->player_count.load(std::memory_order_relaxed);
}

std::size_t ChatServer::get_job_pool_in_use(void) const noexcept
{
	return this->job_pool.get_in_use_count();
}

bool ChatServer::on_connection_request(const char *, std::uint16_t)
{
	return true;
}

void ChatServer::on_client_join(SessionID id) noexcept
{
	Job *job;

	job = this->job_pool.create(JobType::ClientJoin, id);
	if (!job)
	{
		std::fprintf(stderr, "ChatServer::on_client_join(): job_pool.create() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	if (!this->job_queue.enqueue(job))
	{
		std::fprintf(stderr, "ChatServer::on_client_join(): job_queue.enqueue() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	this->wake_content_thread();
}

void ChatServer::on_client_leave(SessionID id) noexcept
{
	Job *job;

	job = this->job_pool.create(JobType::ClientLeave, id);
	if (!job)
	{
		std::fprintf(stderr, "ChatServer::on_client_leave(): job_pool.create() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	if (!this->job_queue.enqueue(job))
	{
		std::fprintf(stderr, "ChatServer::on_client_leave(): job_queue.enqueue() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	this->wake_content_thread();
}

void ChatServer::on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
{
	Job *job;

	job = this->job_pool.create(JobType::Recv, id, std::shared_ptr<Packet>(std::move(packet)));
	if (!job)
	{
		std::fprintf(stderr, "ChatServer::on_recv(): job_pool.create() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	if (!this->job_queue.enqueue(job))
	{
		std::fprintf(stderr, "ChatServer::on_recv(): job_queue.enqueue() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	this->wake_content_thread();
}

void ChatServer::on_send(SessionID, std::size_t) noexcept
{
}

void ChatServer::on_worker_thread_begin(void) noexcept
{
}

void ChatServer::on_worker_thread_end(void) noexcept
{
}

ChatServer::Job::Job(JobType type, SessionID session_id, std::shared_ptr<Packet> packet) noexcept
	: type(type)
	, session_id(session_id)
	, packet(std::move(packet))
{
}

void ChatServer::content_thread_proc(ChatServer *server) noexcept
{
	while (server->content_running.load(std::memory_order_relaxed))
	{
		{
			std::unique_lock<std::mutex> lock(server->content_mutex);
			server->content_cv.wait_for(
				lock,
				std::chrono::milliseconds(ChatServerConfig::HEARTBEAT_CHECK_INTERVAL_MS),
				[server]
				{
					return server->content_wake || !server->content_running.load(std::memory_order_relaxed);
				});
			server->content_wake = false;
		}

		server->drain_jobs();
		server->check_heartbeat_timeout();
	}

	server->drain_jobs();
}

void ChatServer::drain_jobs(void) noexcept
{
	while (true)
	{
		std::optional<Job *> dequeued;
		Job *job;

		dequeued = this->job_queue.dequeue();
		if (!dequeued.has_value())
			break;
		job = *dequeued;

		switch (job->type)
		{
		case JobType::ClientJoin:
			this->handle_client_join(job->session_id);
			break;
		case JobType::ClientLeave:
			this->handle_client_leave(job->session_id);
			break;
		case JobType::Recv:
			this->handle_recv(job->session_id, job->packet.get());
			break;
		}

		this->content_job_count.fetch_add(1, std::memory_order_relaxed);

		this->job_pool.destroy(job);
	}
}

void ChatServer::wake_content_thread(void) noexcept
{
	{
		std::lock_guard<std::mutex> lock(this->content_mutex);
		this->content_wake = true;
	}
	this->content_cv.notify_one();
}

void ChatServer::handle_client_join(SessionID id) noexcept
{
	Player *player;

	player = this->player_pool.create();
	if (!player)
	{
		std::fprintf(stderr, "ChatServer::handle_client_join(): player_pool.create() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	player->session_id = id;
	player->account_no = 0;
	player->id[0] = L'\0';
	player->nickname[0] = L'\0';
	player->sector = ChatServerConfig::INVALID_SECTOR;
	player->logged_in = false;
	player->last_recv_time = ServerClock::now();

	try
	{
		this->player_map.emplace(id, player);
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "ChatServer::handle_client_join(): player_map.emplace() failed: %s\n", error.what());
		std::terminate();
	}
}

void ChatServer::handle_client_leave(SessionID id) noexcept
{
	std::unordered_map<SessionID, Player *>::iterator it;
	Player *player;

	it = this->player_map.find(id);
	if (it == this->player_map.end())
		return;

	player = it->second;

	if (player->sector != ChatServerConfig::INVALID_SECTOR)
		this->sector_remove_player(player);

	this->player_map.erase(it);
	if (player->logged_in)
		this->player_count.fetch_sub(1, std::memory_order_relaxed);

	this->player_pool.destroy(player);
}

void ChatServer::handle_recv(SessionID id, Packet *packet) noexcept
{
	std::unordered_map<SessionID, Player *>::iterator it;
	Player *player;
	std::uint16_t type;

	it = this->player_map.find(id);
	if (it == this->player_map.end())
		return;

	player = it->second;

	if (!packet->deserialize(&type, sizeof(type)))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_recv(): packet too small for Type. id: %llu", id);
		this->disconnect(id);
		return;
	}

	player->last_recv_time = ServerClock::now();

	switch (static_cast<PacketType>(type))
	{
	case PacketType::REQ_LOGIN:
		this->handle_req_login(player, packet);
		break;
	case PacketType::REQ_SECTOR_MOVE:
		this->handle_req_sector_move(player, packet);
		break;
	case PacketType::REQ_MESSAGE:
		this->handle_req_message(player, packet);
		break;
	case PacketType::REQ_HEARTBEAT:
		this->handle_req_heartbeat(player);
		break;
	default:
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_recv(): unknown packet type %u. id: %llu", type, id);
		this->disconnect(id);
		break;
	}
}

void ChatServer::check_heartbeat_timeout(void) noexcept
{
	ServerClock::time_point now;
	std::vector<SessionID> timed_out;

	now = ServerClock::now();

	try
	{
		for (std::pair<const SessionID, Player *> &entry : this->player_map)
			if (now - entry.second->last_recv_time >= std::chrono::milliseconds(ChatServerConfig::HEARTBEAT_TIMEOUT_MS))
				timed_out.push_back(entry.first);
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "ChatServer::check_heartbeat_timeout(): timed_out.push_back() failed: %s\n", error.what());
		std::terminate();
	}

	for (SessionID id : timed_out)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "heartbeat timeout, disconnecting. id: %llu", id);
		this->disconnect(id);
	}
}

void ChatServer::handle_req_login(Player *player, Packet *packet) noexcept
{
	constexpr std::size_t SESSION_KEY_SIZE = 64;

	if (player->logged_in)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_req_login(): duplicate login. id: %llu", player->session_id);
		this->disconnect(player->session_id);
		return;
	}

	if (packet->get_data_size() < sizeof(player->account_no) + sizeof(player->id) + sizeof(player->nickname) + SESSION_KEY_SIZE)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_req_login(): payload too small. id: %llu", player->session_id);
		this->disconnect(player->session_id);
		return;
	}

	*packet >> player->account_no >> player->id >> player->nickname; // SessionKey is present (validated above) but not read or stored.

	player->logged_in = true;
	this->player_count.fetch_add(1, std::memory_order_relaxed);

	this->send_res_login(player->session_id, 1, player->account_no);
}

void ChatServer::handle_req_sector_move(Player *player, Packet *packet) noexcept
{
	std::int64_t account_no;
	std::uint16_t sector_x;
	std::uint16_t sector_y;

	if (!player->logged_in)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_req_sector_move(): not logged in. id: %llu", player->session_id);
		this->disconnect(player->session_id);
		return;
	}

	if (packet->get_data_size() < sizeof(account_no) + sizeof(sector_x) + sizeof(sector_y))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_req_sector_move(): payload too small. id: %llu", player->session_id);
		this->disconnect(player->session_id);
		return;
	}

	// Client-sent account_no is read to advance the cursor but ignored;
	// the response uses the server-stored account_no (server authority).
	*packet >> account_no >> sector_x >> sector_y;

	if (sector_x >= ChatServerConfig::SECTOR_MAX_X || sector_y >= ChatServerConfig::SECTOR_MAX_Y)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_req_sector_move(): out of range (%u, %u). id: %llu", sector_x, sector_y, player->session_id);
		this->disconnect(player->session_id);
		return;
	}

	if (player->sector != ChatServerConfig::INVALID_SECTOR)
		this->sector_remove_player(player);

	player->sector = this->to_sector_index(sector_x, sector_y);
	this->sector_add_player(player);

	this->send_res_sector_move(player->session_id, player->account_no, sector_x, sector_y);
}

void ChatServer::handle_req_message(Player *player, Packet *packet) noexcept
{
	std::int64_t account_no;
	std::uint16_t message_len;
	wchar_t message[ChatServerConfig::MAX_MESSAGE_CHARS];

	if (!player->logged_in)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_req_message(): not logged in. id: %llu", player->session_id);
		this->disconnect(player->session_id);
		return;
	}

	if (player->sector == ChatServerConfig::INVALID_SECTOR)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_req_message(): chat before sector set. id: %llu", player->session_id);
		this->disconnect(player->session_id);
		return;
	}

	if (packet->get_data_size() < sizeof(account_no) + sizeof(message_len))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_req_message(): header too small. id: %llu", player->session_id);
		this->disconnect(player->session_id);
		return;
	}

	// Client-sent account_no ignored (server authority); broadcast uses sender.
	*packet >> account_no >> message_len;

	if (message_len == 0 || message_len > sizeof(message) || message_len % sizeof(wchar_t) != 0)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_req_message(): invalid message_len %u. id: %llu", message_len, player->session_id);
		this->disconnect(player->session_id);
		return;
	}

	if (!packet->deserialize(message, message_len))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_req_message(): message payload too short. id: %llu", player->session_id);
		this->disconnect(player->session_id);
		return;
	}

	this->broadcast_res_message(player, message, message_len);
}

void ChatServer::handle_req_heartbeat(Player *) noexcept
{
}

std::uint16_t ChatServer::to_sector_index(std::uint16_t x, std::uint16_t y) noexcept
{
	return static_cast<std::uint16_t>(y * ChatServerConfig::SECTOR_MAX_X + x);
}

void ChatServer::get_around_sectors(std::uint16_t sector, std::uint16_t *out) noexcept
{
	int center_x;
	int center_y;
	std::size_t index;

	center_x = sector % ChatServerConfig::SECTOR_MAX_X;
	center_y = sector / ChatServerConfig::SECTOR_MAX_X;
	index = 0;

	for (int dy = -ChatServerConfig::SECTOR_AOI_RANGE; dy <= ChatServerConfig::SECTOR_AOI_RANGE; ++dy)
	{
		for (int dx = -ChatServerConfig::SECTOR_AOI_RANGE; dx <= ChatServerConfig::SECTOR_AOI_RANGE; ++dx)
		{
			int nx = center_x + dx;
			int ny = center_y + dy;

			if (nx < 0 || nx >= ChatServerConfig::SECTOR_MAX_X || ny < 0 || ny >= ChatServerConfig::SECTOR_MAX_Y)
				out[index] = ChatServerConfig::INVALID_SECTOR;
			else
				out[index] = static_cast<std::uint16_t>(ny * ChatServerConfig::SECTOR_MAX_X + nx);

			++index;
		}
	}
}

void ChatServer::sector_add_player(Player *player) noexcept
{
	try
	{
		this->sectors[player->sector].players.push_back(player);
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "ChatServer::sector_add_player(): push_back() failed: %s\n", error.what());
		std::terminate();
	}
}

void ChatServer::sector_remove_player(Player *player) noexcept
{
	std::vector<Player *> &players = this->sectors[player->sector].players;

	for (std::size_t i = 0; i < players.size(); ++i)
	{
		if (players[i] == player)
		{
			players[i] = players.back();
			players.pop_back();
			return;
		}
	}
}

std::shared_ptr<ChatServer::Packet> ChatServer::make_res_login(std::uint8_t status, std::int64_t account_no) noexcept
{
	constexpr std::size_t PAYLOAD_SIZE = sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::int64_t);

	std::shared_ptr<Packet> packet;
	std::uint16_t type;

	packet = this->alloc_packet(PAYLOAD_SIZE);
	if (!packet)
	{
		std::fprintf(stderr, "ChatServer::make_res_login(): alloc_packet() failed (out of memory).\n");
		std::terminate();
	}

	type = static_cast<std::uint16_t>(PacketType::RES_LOGIN);
	*packet << type << status << account_no;

	return packet;
}

std::shared_ptr<ChatServer::Packet> ChatServer::make_res_sector_move(std::int64_t account_no, std::uint16_t x, std::uint16_t y) noexcept
{
	constexpr std::size_t PAYLOAD_SIZE = sizeof(std::uint16_t) + sizeof(std::int64_t) + sizeof(std::uint16_t) + sizeof(std::uint16_t);

	std::shared_ptr<Packet> packet;
	std::uint16_t type;

	packet = this->alloc_packet(PAYLOAD_SIZE);
	if (!packet)
	{
		std::fprintf(stderr, "ChatServer::make_res_sector_move(): alloc_packet() failed (out of memory).\n");
		std::terminate();
	}

	type = static_cast<std::uint16_t>(PacketType::RES_SECTOR_MOVE);
	*packet << type << account_no << x << y;

	return packet;
}

std::shared_ptr<ChatServer::Packet> ChatServer::make_res_message(const Player *sender, const wchar_t *msg, std::uint16_t msg_len) noexcept
{
	std::size_t payload_size;
	std::shared_ptr<Packet> packet;
	std::uint16_t type;

	payload_size = sizeof(std::uint16_t) + sizeof(sender->account_no) + sizeof(sender->id) + sizeof(sender->nickname) + sizeof(std::uint16_t) + msg_len;

	packet = this->alloc_packet(payload_size);
	if (!packet)
	{
		std::fprintf(stderr, "ChatServer::make_res_message(): alloc_packet() failed (out of memory).\n");
		std::terminate();
	}

	type = static_cast<std::uint16_t>(PacketType::RES_MESSAGE);
	*packet << type << sender->account_no << sender->id << sender->nickname << msg_len;
	packet->serialize(msg, msg_len);

	return packet;
}

void ChatServer::send_res_login(SessionID id, std::uint8_t status, std::int64_t account_no) noexcept
{
	std::shared_ptr<Packet> packet;

	packet = this->make_res_login(status, account_no);
	this->finalize_packet(packet.get());
	this->enqueue_send(id, std::move(packet));
}

void ChatServer::send_res_sector_move(SessionID id, std::int64_t account_no, std::uint16_t x, std::uint16_t y) noexcept
{
	std::shared_ptr<Packet> packet;

	packet = this->make_res_sector_move(account_no, x, y);
	this->finalize_packet(packet.get());
	this->enqueue_send(id, std::move(packet));
}

void ChatServer::broadcast_res_message(const Player *sender, const wchar_t *msg, std::uint16_t msg_len) noexcept
{
	constexpr std::size_t AROUND_COUNT = static_cast<std::size_t>(2 * ChatServerConfig::SECTOR_AOI_RANGE + 1) * (2 * ChatServerConfig::SECTOR_AOI_RANGE + 1);

	std::shared_ptr<Packet> packet;
	std::uint16_t around[AROUND_COUNT];

	packet = this->make_res_message(sender, msg, msg_len);
	this->finalize_packet(packet.get());

	this->get_around_sectors(sender->sector, around);

	for (std::size_t i = 0; i < AROUND_COUNT; ++i)
	{
		if (around[i] == ChatServerConfig::INVALID_SECTOR)
			continue;

		for (Player *target : this->sectors[around[i]].players)
			this->enqueue_send(target->session_id, packet);
	}
}

void ChatServer::finalize_packet(Packet *packet) noexcept
{
	thread_local std::random_device random_device;
	thread_local std::mt19937 engine(random_device());
	thread_local std::uniform_int_distribution<unsigned int> distribution(0, 255);

	std::uint8_t random_key;

	random_key = static_cast<std::uint8_t>(distribution(engine));
	packet->build_header(ChatServerConfig::PROTOCOL_CODE, random_key);
	packet->encode(ChatServerConfig::FIXED_KEY);
}

void ChatServer::enqueue_send(SessionID id, std::shared_ptr<Packet> packet) noexcept
{
	std::hash<SessionID> hasher;
	unsigned int worker_index;
	SendWorker *worker;

	worker_index = static_cast<unsigned int>(hasher(id) % this->send_workers.size());
	worker = this->send_workers[worker_index].get();

	if (!worker->queue.enqueue(SendTask{id, std::move(packet)}))
	{
		std::fprintf(stderr, "ChatServer::enqueue_send(): queue.enqueue() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	worker->wake.store(true, std::memory_order_relaxed);
	worker->wake.notify_one();
}

void ChatServer::start_send_pool(unsigned int count) noexcept
{
	try
	{
		this->send_workers.reserve(count);
		for (unsigned int i = 0; i < count; ++i)
		{
			std::unique_ptr<SendWorker> worker;

			worker = std::make_unique<SendWorker>();
			worker->wake.store(false, std::memory_order_relaxed);
			this->send_workers.push_back(std::move(worker));
		}

		for (unsigned int i = 0; i < count; ++i)
			this->send_workers[i]->thread = std::thread(send_thread_proc, this, i);
	}
	catch (const std::exception &error)
	{
		std::fprintf(stderr, "ChatServer::start_send_pool(): failed to start send pool: %s\n", error.what());
		std::terminate();
	}
}

void ChatServer::stop_send_pool(void) noexcept
{
	for (std::unique_ptr<SendWorker> &worker : this->send_workers)
	{
		worker->queue.enqueue(SendTask{0, nullptr}); // STOP_SENTINEL: null packet ends the worker
		worker->wake.store(true, std::memory_order_relaxed);
		worker->wake.notify_one();
	}

	for (std::unique_ptr<SendWorker> &worker : this->send_workers)
	{
		if (worker->thread.joinable())
		{
			try
			{
				worker->thread.join();
			}
			catch (const std::system_error &error)
			{
				std::fprintf(stderr, "ChatServer::stop_send_pool(): thread.join() failed: [Code: %d] %s\n", error.code().value(), error.what());
				std::terminate();
			}
		}
	}

	this->send_workers.clear();
}

void ChatServer::send_thread_proc(ChatServer *server, unsigned int worker_index) noexcept
{
	SendWorker *worker;
	bool stop_requested;

	worker = server->send_workers[worker_index].get();
	stop_requested = false;

	while (!stop_requested)
	{
		worker->wake.wait(false, std::memory_order_relaxed);
		worker->wake.store(false, std::memory_order_relaxed);

		while (true)
		{
			std::optional<SendTask> task;

			task = worker->queue.dequeue();
			if (!task.has_value())
				break;

			if (!task->packet) // STOP_SENTINEL
			{
				stop_requested = true;
				break;
			}

			server->send_packet(task->session_id, task->packet);
		}
	}
}
