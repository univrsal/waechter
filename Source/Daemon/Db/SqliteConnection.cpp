/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SqliteConnection.hpp"

#include <stdexcept>

std::unique_ptr<IDbConnection> IDbConnection::Create(EDbBackend Backend, std::string const& DatabaseSource)
{
	switch (Backend)
	{
		case EDbBackend::SQLite:
			return std::make_unique<SqliteConnection>(DatabaseSource);
	}
	throw std::invalid_argument("Unsupported database backend");
}

SqliteConnection::SqliteConnection(std::string const& ConnectionString) : DbPath(ConnectionString) {}

void SqliteConnection::Connect()
{
	auto Config = std::make_shared<sqlpp::sqlite3::connection_config>();
	Config->path_to_database = DbPath;
	Config->flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	Conn.connectUsing(Config); // throws on failure
}

void SqliteConnection::Disconnect()
{
	Conn = sqlpp::sqlite3::connection{};
}

bool SqliteConnection::IsConnected() const
{
	return Conn.is_connected();
}

void SqliteConnection::BeginTransaction()
{
	Conn.execute("BEGIN");
}

void SqliteConnection::CommitTransaction()
{
	Conn.execute("COMMIT");
}

void SqliteConnection::RollbackTransaction()
{
	Conn.execute("ROLLBACK");
}

void SqliteConnection::ExecuteRaw(std::string const& sql)
{
	Conn.execute(sql);
}
