/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <memory>
#include <string>

enum class EDbBackend
{
	SQLite
};

class IDbConnection
{
public:
	virtual ~IDbConnection() = default;

	virtual void Connect() = 0;

	virtual void Disconnect() = 0;

	[[nodiscard]] virtual bool IsConnected() const = 0;

	virtual void BeginTransaction() = 0;

	virtual void CommitTransaction() = 0;

	virtual void RollbackTransaction() = 0;

	virtual void ExecuteRaw(std::string const& sql) = 0;

	[[nodiscard]] virtual EDbBackend Backend() const = 0;

	static std::unique_ptr<IDbConnection> Create(EDbBackend Backend, std::string const& DatabaseSource = ":memory:");
};
