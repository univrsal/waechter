/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DbManager.hpp"
#include "DbMigrations.hpp"

#include <stdexcept>

void WDbManager::Initialize(EDbBackend backend, std::string const& path)
{
	spdlog::info("Initializing database with backend {} at path '{}'", IDbConnection::BackendToString(backend), path);
	Conn = IDbConnection::Create(backend, path);
	Conn->Connect();
	WDbMigrations::Apply(*Conn, [this](auto&& Fn) { return Run(std::forward<decltype(Fn)>(Fn)); });
}

IDbConnection& WDbManager::Connection()
{
	if (!Conn)
		throw std::runtime_error("DbManager: not initialized");
	return *Conn;
}

IDbConnection const& WDbManager::Connection() const
{
	if (!Conn)
		throw std::runtime_error("DbManager: not initialized");
	return *Conn;
}