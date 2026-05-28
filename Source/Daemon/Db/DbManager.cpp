/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DbManager.hpp"

#include <stdexcept>

void WDbManager::Initialize(EDbBackend backend, std::string const& path)
{
	Conn = IDbConnection::Create(backend, path);
	Conn->Connect();
}

IDbConnection& WDbManager::Connection()
{
	if (!Conn)
		throw std::runtime_error("WDbManager: not initialized");
	return *Conn;
}

IDbConnection const& WDbManager::Connection() const
{
	if (!Conn)
		throw std::runtime_error("WDbManager: not initialized");
	return *Conn;
}