/**
 * File: monitor_server.cpp
 * Path: ajylib/examples/monitor_server/monitor_server.cpp
 * Description:
 *	Definition of MonitorServer (declared in monitor_server.hpp): the server
 *	lifecycle (content and sampler thread spawn/join). Worker callbacks, the
 *	content/sampler loops, job/packet handlers, aggregation and response
 *	builders follow in later sections.
 * Author: ajy-dev
 * Created: 2026-07-07
 * Updated: Never
 * Version: 0.1.0
 */

#include <monitor_server/monitor_server.hpp>
#include <monitor_server/monitor_server_config.hpp>
#include <monitor_server/protocol.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>

MonitorServer::MonitorServer(std::string_view logger_name) noexcept
	: NetServer(logger_name, MonitorServerConfig::PROTOCOL_CODE, MonitorServerConfig::FIXED_KEY)
	, content_wake(false)
	, content_running(false)
{
}

MonitorServer::~MonitorServer(void) noexcept
{
	this->stop();
}

bool MonitorServer::start(const char *bind_ip, std::uint16_t port, int worker_thread_count, bool nagle, std::uint32_t max_sessions) noexcept
{
	if (this->content_thread.joinable())
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "MonitorServer::start(): already running.");
		return false;
	}

	this->content_running.store(true, std::memory_order_relaxed);

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
		return false;
	}

	if (!NetServer::start(bind_ip, port, worker_thread_count, nagle, max_sessions))
	{
		this->shutdown_threads();
		return false;
	}

	this->db_writer.start();

	return true;
}

void MonitorServer::stop(void) noexcept
{
	NetServer::stop();

	this->shutdown_threads();

	// Content thread is now joined, so no further batches will be enqueued;
	// stopping the writer here lets it drain everything already queued.
	this->db_writer.stop();

	while (true)
	{
		std::optional<Job *> job_optional;

		job_optional = this->job_queue.dequeue();
		if (!job_optional.has_value())
			break;
		this->job_pool.destroy(*job_optional);
	}

	for (std::pair<const SessionID, MonitorSession *> &entry : this->session_map)
		this->session_pool.destroy(entry.second);
	this->session_map.clear();

	this->relay_targets.clear();
	this->aggregator.clear();

	this->content_wake = false;
}

void MonitorServer::shutdown_threads(void) noexcept
{
	if (!this->content_thread.joinable())
		return;

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

bool MonitorServer::on_connection_request(const char *, std::uint16_t)
{
	return true;
}

void MonitorServer::on_client_join(SessionID id) noexcept
{
	Job *job;

	job = this->job_pool.create(Job::Type::ClientJoin, id);
	if (!job)
	{
		std::fprintf(stderr, "MonitorServer::on_client_join(): job_pool.create() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	if (!this->job_queue.enqueue(job))
	{
		std::fprintf(stderr, "MonitorServer::on_client_join(): job_queue.enqueue() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	this->wake_content_thread();
}

void MonitorServer::on_client_leave(SessionID id) noexcept
{
	Job *job;

	job = this->job_pool.create(Job::Type::ClientLeave, id);
	if (!job)
	{
		std::fprintf(stderr, "MonitorServer::on_client_leave(): job_pool.create() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	if (!this->job_queue.enqueue(job))
	{
		std::fprintf(stderr, "MonitorServer::on_client_leave(): job_queue.enqueue() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	this->wake_content_thread();
}

void MonitorServer::on_recv(SessionID id, std::unique_ptr<Packet> packet) noexcept
{
	Job *job;

	job = this->job_pool.create(Job::Type::Recv, id, std::shared_ptr<Packet>(std::move(packet)));
	if (!job)
	{
		std::fprintf(stderr, "MonitorServer::on_recv(): job_pool.create() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	if (!this->job_queue.enqueue(job))
	{
		std::fprintf(stderr, "MonitorServer::on_recv(): job_queue.enqueue() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	this->wake_content_thread();
}

void MonitorServer::on_send(SessionID, std::size_t) noexcept
{
}

void MonitorServer::on_worker_thread_begin(void) noexcept
{
}

void MonitorServer::on_worker_thread_end(void) noexcept
{
}

MonitorServer::Job::Job(Type type, SessionID session_id, std::shared_ptr<Packet> packet) noexcept
	: type(type)
	, session_id(session_id)
	, packet(std::move(packet))
	, sample{}
{
}

MonitorServer::Job::Job(SampleData sample) noexcept
	: type(Type::LocalSample)
	, session_id(0)
	, packet(nullptr)
	, sample(sample)
{
}

void MonitorServer::submit_local_sample(std::uint8_t data_type, std::int32_t data_value, std::int32_t time_stamp) noexcept
{
	Job *job;

	job = this->job_pool.create(SampleData{data_type, data_value, time_stamp});
	if (!job)
	{
		std::fprintf(stderr, "MonitorServer::submit_local_sample(): job_pool.create() failed (out of memory).\n");
		std::terminate();
	}

	if (!this->job_queue.enqueue(job))
	{
		std::fprintf(stderr, "MonitorServer::submit_local_sample(): job_queue.enqueue() failed (out of memory).\n");
		std::terminate();
	}

	this->wake_content_thread();
}

void MonitorServer::content_thread_proc(MonitorServer *server) noexcept
{
	ServerClock::time_point last_flush;

	last_flush = ServerClock::now();

	while (server->content_running.load(std::memory_order_relaxed))
	{
		ServerClock::time_point now;

		{
			std::unique_lock<std::mutex> lock(server->content_mutex);
			server->content_cv.wait_for(
				lock,
				std::chrono::milliseconds(MonitorServerConfig::CONTENT_CHECK_INTERVAL_MS),
				[server]
				{
					return server->content_wake || !server->content_running.load(std::memory_order_relaxed);
				});
			server->content_wake = false;
		}

		server->drain_jobs();
		server->check_timeout();

		now = ServerClock::now();
		if (now - last_flush >= std::chrono::milliseconds(MonitorServerConfig::DB_FLUSH_INTERVAL_MS))
		{
			server->flush_aggregator();
			last_flush = now;
		}
	}

	server->drain_jobs();
}

void MonitorServer::drain_jobs(void) noexcept
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
		case Job::Type::ClientJoin:
			this->handle_client_join(job->session_id);
			break;
		case Job::Type::ClientLeave:
			this->handle_client_leave(job->session_id);
			break;
		case Job::Type::Recv:
			this->handle_recv(job->session_id, job->packet.get());
			break;
		case Job::Type::LocalSample:
			this->handle_local_sample(job->sample);
			break;
		}

		this->job_pool.destroy(job);
	}
}

void MonitorServer::wake_content_thread(void) noexcept
{
	{
		std::lock_guard<std::mutex> lock(this->content_mutex);
		this->content_wake = true;
	}
	this->content_cv.notify_one();
}

void MonitorServer::handle_client_join(SessionID id) noexcept
{
	MonitorSession *session;

	session = this->session_pool.create();
	if (!session)
	{
		std::fprintf(stderr, "MonitorServer::handle_client_join(): session_pool.create() failed (out of memory). id: %llu\n", id);
		std::terminate();
	}

	session->session_id = id;
	session->role = MonitorSession::Role::UNASSIGNED;
	session->server_no = 0; // unused until REPORTER login
	session->last_recv_time = ServerClock::now();

	try
	{
		this->session_map.emplace(id, session);
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "MonitorServer::handle_client_join(): session_map.emplace() failed: %s\n", error.what());
		std::terminate();
	}
}

void MonitorServer::handle_client_leave(SessionID id) noexcept
{
	std::unordered_map<SessionID, MonitorSession *>::iterator it;
	MonitorSession *session;

	it = this->session_map.find(id);
	if (it == this->session_map.end())
		return;

	session = it->second;

	if (session->role == MonitorSession::Role::MONITORING_TOOL)
	{
		std::vector<SessionID>::iterator target;

		target = std::find(this->relay_targets.begin(), this->relay_targets.end(), id);
		if (target != this->relay_targets.end())
			this->relay_targets.erase(target);
	}

	this->session_map.erase(it);
	this->session_pool.destroy(session);
}

void MonitorServer::handle_recv(SessionID id, Packet *packet) noexcept
{
	std::unordered_map<SessionID, MonitorSession *>::iterator it;
	MonitorSession *session;
	std::uint16_t type;

	it = this->session_map.find(id);
	if (it == this->session_map.end())
		return;

	session = it->second;

	if (!packet->deserialize(&type, sizeof(type)))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_recv(): packet too small for Type. id: %llu", id);
		this->disconnect(id);
		return;
	}

	session->last_recv_time = ServerClock::now();

	switch (session->role)
	{
	case MonitorSession::Role::UNASSIGNED:
		switch (static_cast<MonitorPacketType>(type))
		{
		case MonitorPacketType::SS_MONITOR_LOGIN:
			this->handle_ss_login(session, packet);
			break;
		case MonitorPacketType::CS_MONITOR_TOOL_REQ_LOGIN:
			this->handle_cs_tool_login(session, packet);
			break;
		default:
			this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_recv(): unexpected packet type %u for UNASSIGNED. id: %llu", type, id);
			this->disconnect(id);
			break;
		}
		break;
	case MonitorSession::Role::REPORTER:
		if (static_cast<MonitorPacketType>(type) == MonitorPacketType::SS_MONITOR_DATA_UPDATE)
		{
			this->handle_ss_data_update(session, packet);
		}
		else
		{
			this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_recv(): unexpected packet type %u for REPORTER. id: %llu", type, id);
			this->disconnect(id);
		}
		break;
	case MonitorSession::Role::MONITORING_TOOL:
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_recv(): unexpected packet type %u for MONITORING_TOOL. id: %llu", type, id);
		this->disconnect(id);
		break;
	}
}

void MonitorServer::handle_local_sample(const SampleData &sample) noexcept
{
	this->ingest(MonitorServerConfig::SELF_SERVER_NO, sample.data_type, sample.data_value, sample.time_stamp);
}

void MonitorServer::handle_ss_login(MonitorSession *session, Packet *packet) noexcept
{
	std::int32_t server_no;

	if (!packet->deserialize(&server_no, sizeof(server_no)))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_ss_login(): payload too small. id: %llu", session->session_id);
		this->disconnect(session->session_id);
		return;
	}

	session->role = MonitorSession::Role::REPORTER;
	session->server_no = server_no;

	// SS is fire-and-forget: no login response.
}

void MonitorServer::handle_cs_tool_login(MonitorSession *session, Packet *packet) noexcept
{
	char key[MonitorServerConfig::TOOL_LOGIN_KEY_FIELD_SIZE];

	if (!packet->deserialize(key, sizeof(key)))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_cs_tool_login(): payload too small. id: %llu", session->session_id);
		this->disconnect(session->session_id);
		return;
	}

	// The tool config supplies a null-terminated C string within the field;
	// compare up to the field size. ERR_NO_SERVER is unreachable here (no
	// server-name field in this protocol), so only OK / ERR_SESSION_KEY occur.
	if (std::strncmp(key, MonitorServerConfig::TOOL_LOGIN_KEY.data(), sizeof(key)) != 0)
	{
		this->send_res_tool_login(session->session_id, ToolLoginStatus::ERR_SESSION_KEY);
		this->disconnect(session->session_id);
		return;
	}

	session->role = MonitorSession::Role::MONITORING_TOOL;

	try
	{
		this->relay_targets.push_back(session->session_id);
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "MonitorServer::handle_cs_tool_login(): relay_targets.push_back() failed: %s\n", error.what());
		std::terminate();
	}

	this->send_res_tool_login(session->session_id, ToolLoginStatus::OK);
}

void MonitorServer::handle_ss_data_update(MonitorSession *session, Packet *packet) noexcept
{
	std::uint8_t data_type;
	std::int32_t data_value;
	std::int32_t time_stamp;

	if (!packet->deserialize(&data_type, sizeof(data_type))
		|| !packet->deserialize(&data_value, sizeof(data_value))
		|| !packet->deserialize(&time_stamp, sizeof(time_stamp)))
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "handle_ss_data_update(): payload too small. id: %llu", session->session_id);
		this->disconnect(session->session_id);
		return;
	}

	// server_no is not on the wire; it comes from the session (registered at login).
	this->ingest(session->server_no, data_type, data_value, time_stamp);
}

void MonitorServer::ingest(std::int32_t server_no, std::uint8_t data_type, std::int32_t data_value, std::int32_t time_stamp) noexcept
{
	Accumulator *accumulator;

	try
	{
		accumulator = &this->aggregator[server_no][data_type];
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "MonitorServer::ingest(): aggregator insert failed: %s\n", error.what());
		std::terminate();
	}

	accumulator->count += 1;
	accumulator->sum += data_value;
	accumulator->min = std::min(accumulator->min, data_value);
	accumulator->max = std::max(accumulator->max, data_value);

	this->relay_to_tools(server_no, data_type, data_value, time_stamp);
}

void MonitorServer::relay_to_tools(std::int32_t server_no, std::uint8_t data_type, std::int32_t data_value, std::int32_t time_stamp) noexcept
{
	std::shared_ptr<Packet> packet;

	if (this->relay_targets.empty())
		return;

	packet = this->make_data_update(server_no, data_type, data_value, time_stamp);

	for (SessionID id : this->relay_targets)
		this->send_packet(id, packet);
}

void MonitorServer::flush_aggregator(void) noexcept
{
	std::vector<AggregateRow> batch;

	for (std::pair<const std::int32_t, std::unordered_map<std::uint8_t, Accumulator>> &server_entry : this->aggregator)
	{
		std::int32_t server_no;

		server_no = server_entry.first;

		for (std::pair<const std::uint8_t, Accumulator> &type_entry : server_entry.second)
		{
			std::uint8_t data_type;
			const Accumulator &accumulator = type_entry.second;
			AggregateRow row;

			if (accumulator.count == 0)
				continue; // no samples this window (avoids div-by-zero)

			data_type = type_entry.first;

			row.server_no = server_no;
			row.data_type = data_type;
			row.avg = static_cast<std::int32_t>(accumulator.sum / accumulator.count);
			row.min = accumulator.min;
			row.max = accumulator.max;

			try
			{
				batch.push_back(row);
			}
			catch (const std::bad_alloc &error)
			{
				std::fprintf(stderr, "MonitorServer::flush_aggregator(): batch.push_back() failed: %s\n", error.what());
				std::terminate();
			}
		}
	}

	this->aggregator.clear();

	if (!batch.empty())
		this->db_writer.enqueue(std::move(batch));
}

void MonitorServer::check_timeout(void) noexcept
{
	ServerClock::time_point now;
	std::vector<SessionID> timed_out;

	now = ServerClock::now();

	try
	{
		for (std::pair<const SessionID, MonitorSession *> &entry : this->session_map)
		{
			MonitorSession *session;
			std::int64_t threshold_ms;

			session = entry.second;

			switch (session->role)
			{
			case MonitorSession::Role::UNASSIGNED:
				threshold_ms = MonitorServerConfig::UNASSIGNED_TIMEOUT_MS;
				break;
			case MonitorSession::Role::REPORTER:
				threshold_ms = MonitorServerConfig::REPORTER_TIMEOUT_MS;
				break;
			case MonitorSession::Role::MONITORING_TOOL:
				continue; // exempt: tools are pure receivers, silence is normal
			}

			if (now - session->last_recv_time >= std::chrono::milliseconds(threshold_ms))
				timed_out.push_back(entry.first);
		}
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "MonitorServer::check_timeout(): timed_out.push_back() failed: %s\n", error.what());
		std::terminate();
	}

	for (SessionID id : timed_out)
	{
		this->logger->log(ajy::utility::Logger::LogLevel::Warning, "session timeout, disconnecting. id: %llu", id);
		this->disconnect(id);
	}
}

std::shared_ptr<MonitorServer::Packet> MonitorServer::make_res_tool_login(ToolLoginStatus status) noexcept
{
	constexpr std::size_t PAYLOAD_SIZE = sizeof(std::uint16_t) + sizeof(std::uint8_t);

	std::shared_ptr<Packet> packet;
	std::uint16_t type;

	packet = this->alloc_packet(PAYLOAD_SIZE);
	if (!packet)
	{
		std::fprintf(stderr, "MonitorServer::make_res_tool_login(): alloc_packet() failed (out of memory).\n");
		std::terminate();
	}

	type = static_cast<std::uint16_t>(MonitorPacketType::CS_MONITOR_TOOL_RES_LOGIN);
	*packet << type << static_cast<std::uint8_t>(status);

	return packet;
}

std::shared_ptr<MonitorServer::Packet> MonitorServer::make_data_update(std::int32_t server_no, std::uint8_t data_type, std::int32_t data_value, std::int32_t time_stamp) noexcept
{
	constexpr std::size_t PAYLOAD_SIZE = sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::int32_t) + sizeof(std::int32_t);

	std::shared_ptr<Packet> packet;
	std::uint16_t type;

	packet = this->alloc_packet(PAYLOAD_SIZE);
	if (!packet)
	{
		std::fprintf(stderr, "MonitorServer::make_data_update(): alloc_packet() failed (out of memory).\n");
		std::terminate();
	}

	type = static_cast<std::uint16_t>(MonitorPacketType::CS_MONITOR_TOOL_DATA_UPDATE);
	// ServerNo narrows to BYTE on the tool-facing update (see protocol.hpp).
	*packet << type << static_cast<std::uint8_t>(server_no) << data_type << data_value << time_stamp;

	return packet;
}

void MonitorServer::send_res_tool_login(SessionID id, ToolLoginStatus status) noexcept
{
	this->send_packet(id, this->make_res_tool_login(status));
}
