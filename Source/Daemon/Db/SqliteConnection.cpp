/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SqliteConnection.hpp"

#include <filesystem>
#include <stdexcept>

namespace
{
	bool IsSpecialSqlitePath(std::string const& path)
	{
		return path.empty() || path == ":memory:" || path.rfind("file:", 0) == 0;
	}

	void EnsureSqlitePathReady(std::string const& dbPath)
	{
		if (IsSpecialSqlitePath(dbPath))
			return;

		std::filesystem::path const Path(dbPath);
		std::filesystem::path const Parent = Path.parent_path();

		if (!Parent.empty())
			std::filesystem::create_directories(Parent);

		if (std::filesystem::exists(Path) && std::filesystem::is_directory(Path))
			throw std::runtime_error("SQLite database path points to a directory: '" + dbPath + "'");
	}
}

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
	EnsureSqlitePathReady(DbPath);

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
