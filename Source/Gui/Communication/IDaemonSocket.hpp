/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "sigslot/signal.hpp"

#include "Buffer.hpp"

class IDaemonSocket
{
public:
	IDaemonSocket() = default;
	virtual ~IDaemonSocket() = default;

	virtual void                       Start() = 0;
	virtual void                       Stop() = 0;
	virtual sigslot::signal<WBuffer&>& GetDataSignal() = 0;
	virtual sigslot::signal<>&         GetClosedSignal() = 0;

	virtual bool SendFramed(std::string const&) { return false; }

	virtual bool IsConnected() const { return false; }
};