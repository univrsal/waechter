/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "IDbConnection.hpp"
#include "Singleton.hpp"
#include "SqliteConnection.hpp"

#include <memory>
#include <stdexcept>

class WDbManager final : public TSingleton<WDbManager>
{
	std::mutex RunMutex;

public:
	void Initialize(
		EDbBackend backend = EDbBackend::SQLite, std::string const& path = "/var/lib/waechter/waechter.sqlite");

	/// Access the backend-agnostic interface (lifecycle, transactions, raw SQL).
	IDbConnection&                     Connection();
	[[nodiscard]] IDbConnection const& Connection() const;

	template <typename Fn>
	auto Run(Fn&& fn)
	{
		if (!Conn)
			throw std::runtime_error("WDbManager: not initialized");

		switch (Conn->Backend())
		{
			case EDbBackend::SQLite:
			{
				std::scoped_lock Lock(RunMutex);
				auto* conn = static_cast<SqliteConnection*>(Conn.get());
				return std::forward<Fn>(fn)(conn->Get());
			}
		}
		throw std::runtime_error("WDbManager::Run: unsupported backend");
	}

private:
	std::unique_ptr<IDbConnection> Conn;
};
