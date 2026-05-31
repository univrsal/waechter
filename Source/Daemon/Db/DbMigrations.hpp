/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Format.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "spdlog/spdlog.h"
#include "sqlpp11/sqlpp11.h"

#include "IDbConnection.hpp"
#include "Schema.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Migration files are loaded at runtime from the first migrations directory
// that contains applicable files.  Search order:
//   1. ./migrations/
//   2. /etc/waechterd/migrations/
//
// File naming convention:  NNNN_description[.backend].sql
//   *.sqlite.sql   → applied only when the active backend is SQLite
//   *.sql          → applied for all backends
//
// The C++ layer owns the transaction for each migration; SQL files must NOT
// contain BEGIN TRANSACTION / COMMIT statements.
// ─────────────────────────────────────────────────────────────────────────────

class WDbMigrations
{
	struct WEntry
	{
		int64_t               Version{};
		std::filesystem::path Path{};
	};

	/// Split @p Sql on ';' and execute each non-empty statement individually.
	/// This is necessary because some backends (e.g. SQLite via sqlite3_exec)
	/// do not support multi-statement strings in a single call.
	static void ExecuteStatements(IDbConnection& Db, std::string const& Sql)
	{
		std::istringstream Stream(Sql);
		std::string        Statement;
		while (std::getline(Stream, Statement, ';'))
		{
			// Trim leading/trailing whitespace
			auto const First = Statement.find_first_not_of(" \t\r\n");
			if (First == std::string::npos)
				continue; // blank segment between semicolons

			Db.ExecuteRaw(Statement.substr(First));
		}
	}

	static std::vector<WEntry> FindMigrations(IDbConnection const& Db)
	{
		static constexpr char const* kSearchPaths[] = {
			"./migrations",
			"/etc/waechter/migrations",
		};

		EDbBackend const    DbBackend = Db.Backend();
		bool                AnyDirFound = false;
		std::vector<WEntry> Migrations;

		for (auto const* searchPath : kSearchPaths)
		{
			std::filesystem::path const Dir(searchPath);
			if (!std::filesystem::is_directory(Dir))
			{
				continue;
			}

			AnyDirFound = true;

			for (auto const& Entry : std::filesystem::directory_iterator(Dir))
			{
				if (!Entry.is_regular_file())
				{
					continue;
				}

				std::string const FName = Entry.path().filename().string();

				if (!WStringFormat::EndsWith(FName, ".sql"))
				{
					continue;
				}

				// Backend-specific suffix filtering:
				if (WStringFormat::EndsWith(FName, ".sqlite.sql") && DbBackend != EDbBackend::SQLite)
				{
					continue;
				}

				size_t i = 0;
				while (i < FName.size() && std::isdigit(static_cast<unsigned char>(FName[i])))
				{
					++i;
				}

				if (i == 0)
				{
					continue; // no leading digits — not a migration file
				}

				int64_t const Version = std::stoll(FName.substr(0, i));
				Migrations.push_back({ Version, Entry.path() });
			}

			if (!Migrations.empty())
			{
				break; // use the first directory that has applicable files
			}
		}

		if (Migrations.empty())
		{
			if (!AnyDirFound)
			{
				spdlog::critical("DbMigrations: no migrations directory found "
								 "(searched ./migrations and /etc/waechter/migrations)");
			}
			spdlog::critical("DbMigrations: migrations directory found but contains no applicable "
							 "migration files for the active backend");
		}
		return Migrations;
	}

public:
	template <typename Fn>
	static void Apply(IDbConnection& DbConn, Fn&& RunFn)
	{
		// This must exist before we can check which migrations are applied.
		DbConn.ExecuteRaw(R"sql(
			CREATE TABLE IF NOT EXISTS "Migration" (
				"Version"   INTEGER,
				"AppliedAt" INTEGER NOT NULL,
				PRIMARY KEY("Version")
			)
		)sql");

		// Determine the highest applied version
		int64_t CurrentVersion = -1;
		RunFn([&](auto& Conn) {
			constexpr Db::Schema::Migration MigrationTable;
			for (auto const& Row :
				Conn(sqlpp::select(sqlpp::max(MigrationTable.Version)).from(MigrationTable).unconditionally()))
			{
				if (!Row.max.is_null())
				{
					CurrentVersion = static_cast<int64_t>(Row.max);
				}
			}
		});

		std::vector<WEntry> Migrations = FindMigrations(DbConn);

		std::sort(Migrations.begin(), Migrations.end(),
			[](WEntry const& A, WEntry const& B) { return A.Version < B.Version; });

		spdlog::info("DbMigrations: current version is {}, {} migration(s) found", CurrentVersion, Migrations.size());

		// Apply each pending migration inside its own transaction
		for (auto const& [Version, Path] : Migrations)
		{
			if (Version <= CurrentVersion)
			{
				continue;
			}

			std::ifstream FileStream(Path);
			if (!FileStream.is_open())
			{
				spdlog::critical("DbMigrations: failed to open migration file: " + Path.string());
			}

			std::ostringstream Buf;
			Buf << FileStream.rdbuf();
			if (FileStream.fail() && !FileStream.eof())
			{
				spdlog::critical("DbMigrations: failed to read migration file: " + Path.string());
			}

			std::string const MigrationSql = Buf.str();

			DbConn.BeginTransaction();
			try
			{
				spdlog::info("Applying migration {}: {}", Version, Path.filename().string());
				ExecuteStatements(DbConn, MigrationSql);
				// Record that this migration was applied
				auto const Now = std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::system_clock::now().time_since_epoch())
									 .count();

				RunFn([&](auto& SqlConn) {
					constexpr Db::Schema::Migration MigrationTable;
					SqlConn(sqlpp::insert_into(MigrationTable)
							.set(MigrationTable.Version = Version, MigrationTable.AppliedAt = Now));
				});

				DbConn.CommitTransaction();
			}
			catch (std::exception const& E)
			{
				DbConn.RollbackTransaction();
				spdlog::critical("DbMigrations: failed to apply migration version {} from file {}: {}", Version,
					Path.string(), E.what());
			}
		}
	}
};
