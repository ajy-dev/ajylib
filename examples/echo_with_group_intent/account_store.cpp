/**
 * File: account_store.cpp
 * Path: ajylib/examples/echo_with_group_intent/account_store.cpp
 * Description:
 *	A session-to-account map shared across groups.
 * Author: ajy-dev
 * Created: 2026-08-14
 * Updated: Never
 * Version: 0.1.0
 */

#include "account_store.hpp"

#include <cstdio>
#include <exception>
#include <new>
#include <system_error>

AccountStore::AccountStore(void) noexcept
{
}

AccountStore::~AccountStore(void) noexcept
{
}

void AccountStore::put(SessionID id, std::int64_t account_no) noexcept
{
	try
	{
		std::lock_guard<std::mutex> guard(this->lock);

		this->accounts[id] = account_no;
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "AccountStore::put(): std::lock_guard(lock) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
	catch (const std::bad_alloc &error)
	{
		std::fprintf(stderr, "AccountStore::put(): std::unordered_map::operator[] failed: %s\n", error.what());
		std::terminate();
	}
}

bool AccountStore::take(SessionID id, std::int64_t &account_no) noexcept
{
	std::unordered_map<SessionID, std::int64_t>::iterator found;

	try
	{
		std::lock_guard<std::mutex> guard(this->lock);

		found = this->accounts.find(id);
		if (found == this->accounts.end())
			return false;

		account_no = found->second;
		this->accounts.erase(found);
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "AccountStore::take(): std::lock_guard(lock) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}

	return true;
}

void AccountStore::remove(SessionID id) noexcept
{
	try
	{
		std::lock_guard<std::mutex> guard(this->lock);

		this->accounts.erase(id);
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "AccountStore::remove(): std::lock_guard(lock) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}
}

std::size_t AccountStore::get_size(void) noexcept
{
	std::size_t size;

	size = 0;

	try
	{
		std::lock_guard<std::mutex> guard(this->lock);

		size = this->accounts.size();
	}
	catch (const std::system_error &error)
	{
		std::fprintf(stderr, "AccountStore::get_size(): std::lock_guard(lock) failed: [Code: %d] %s\n", error.code().value(), error.what());
		std::terminate();
	}

	return size;
}
