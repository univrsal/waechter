/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "IDbConnection.hpp"

#include <sqlpp11/sqlite3/sqlite3.h>

class SqliteConnection final : public IDbConnection
{
public:
	explicit SqliteConnection(std::string const& path = ":memory:");

	void                     Connect() override;
	void                     Disconnect() override;
	[[nodiscard]] bool       IsConnected() const override;
	void                     BeginTransaction() override;
	void                     CommitTransaction() override;
	void                     RollbackTransaction() override;
	void                     ExecuteRaw(std::string const& sql) override;
	[[nodiscard]] EDbBackend Backend() const override { return EDbBackend::SQLite; }

	sqlpp::sqlite3::connection&                     Get() { return Conn; }
	[[nodiscard]] sqlpp::sqlite3::connection const& Get() const { return Conn; }

private:
	std::string                m_Path;
	sqlpp::sqlite3::connection Conn;
};
