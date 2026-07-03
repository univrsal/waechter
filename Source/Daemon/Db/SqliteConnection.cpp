/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SqliteConnection.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "spdlog/spdlog.h"

#include "Types.hpp"

namespace
{
	constexpr uintmax_t kMaxDatabaseSizeBytes = 400ULL WMiB;

	bool IsSpecialSqlitePath(std::string const& path)
	{
		return path.empty() || path == ":memory:" || path.rfind("file:", 0) == 0;
	}

	void EnsureSqlitePathReady(std::string const& DbPath)
	{
		if (IsSpecialSqlitePath(DbPath))
		{
			return;
		}

		std::filesystem::path const Path(DbPath);
		std::filesystem::path const Parent = Path.parent_path();

		if (!Parent.empty())
		{
			std::filesystem::create_directories(Parent);
		}

		if (std::filesystem::exists(Path) && std::filesystem::is_directory(Path))
		{
			throw std::runtime_error("SQLite database path points to a directory: '" + DbPath + "'");
		}
	}

	void RotateDatabaseIfNeeded(std::string const& DbPath)
	{
		if (IsSpecialSqlitePath(DbPath))
		{
			return;
		}

		std::filesystem::path const Path(DbPath);
		std::error_code             Ec;

		if (!std::filesystem::is_regular_file(Path, Ec))
		{
			return;
		}

		uintmax_t const FileSize = std::filesystem::file_size(Path, Ec);
		if (Ec || FileSize < kMaxDatabaseSizeBytes)
		{
			return;
		}

		auto const Now = std::chrono::system_clock::now();
		auto const Time = std::chrono::system_clock::to_time_t(Now);
		std::tm    Tm = {};
		localtime_r(&Time, &Tm);

		std::ostringstream BackupName;
		BackupName << Path.stem().string() << '.' << std::put_time(&Tm, "%Y-%m-%d") << Path.extension().string();

		std::filesystem::path const BackupPath = Path.parent_path() / BackupName.str();

		spdlog::info("Rotating database: {} ({} bytes) → {}", DbPath, FileSize, BackupPath.string());

		std::filesystem::rename(Path, BackupPath, Ec);
		if (Ec)
		{
			spdlog::error("Failed to rotate database {}: {}", DbPath, Ec.message());
		}
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
	RotateDatabaseIfNeeded(DbPath);

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
