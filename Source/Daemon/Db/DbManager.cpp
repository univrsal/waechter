/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DbManager.hpp"

#include <memory>

#include "sqlpp11/sqlite3/sqlite3.h"

void WDbManager::Initialize()
{
	auto config = std::make_shared<sqlpp::sqlite3::connection_config>();
	config->path_to_database = ":memory:";
	config->flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

	// Create a connection
	sqlpp::sqlite3::connection db;
	db.connectUsing(config); // This can throw an exception.
}