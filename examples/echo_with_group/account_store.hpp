/**
 * File: account_store.hpp
 * Path: ajylib/examples/echo_with_group/account_store.hpp
 * Description:
 *	A session-to-account map shared across groups.
 * Note:
 *	The auth group writes an entry before moving a session, and the echo
 *	group takes it on entry. The two run on different group threads, so the
 *	map is mutex-guarded; contention is negligible since each session
 *	touches it twice over its lifetime.
 * Author: ajy-dev
 * Created: 2026-08-10
 * Updated: Never
 * Version: 0.1.0
 */

#ifndef ACCOUNT_STORE_HPP
#define ACCOUNT_STORE_HPP

#include <ajy/network/server.hpp>

#include <cstdint>
#include <mutex>
#include <unordered_map>

class AccountStore
{
public:
	using SessionID = ajy::network::Server::SessionID;

	AccountStore(void) noexcept;
	~AccountStore(void) noexcept;

	AccountStore(const AccountStore &other) = delete;
	AccountStore &operator=(const AccountStore &other) = delete;
	AccountStore(AccountStore &&other) = delete;
	AccountStore &operator=(AccountStore &&other) = delete;

	void put(SessionID id, std::int64_t account_no) noexcept;
	bool take(SessionID id, std::int64_t &account_no) noexcept;
	void remove(SessionID id) noexcept;

private:
	std::unordered_map<SessionID, std::int64_t> accounts;
	std::mutex lock;
};

#endif
