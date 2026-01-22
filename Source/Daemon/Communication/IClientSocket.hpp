/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "sigslot/signal.hpp"

#include "Buffer.hpp"

class IClientSocket
{
public:
	virtual ~IClientSocket() = default;

	virtual sigslot::signal<WBuffer&>& GetDataSignal() = 0;
	virtual sigslot::signal<>&         GetClosedSignal() = 0;

	virtual void StartListenThread() = 0;
	virtual bool IsConnected() const { return false; }

	virtual void Close() = 0;

	virtual ssize_t SendFramed(std::string const&) { return -1; }
};