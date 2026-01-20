/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>
#include <sigslot/signal.hpp>

class WDaemonClient;
class IServerSocket
{
public:
	IServerSocket() = default;
	virtual ~IServerSocket() = default;
	virtual bool StartListenThread() { return false; }

	virtual void                                                    Stop() {}
	virtual sigslot::signal<std::shared_ptr<WDaemonClient> const&>& GetNewConnectionSignal() = 0;
};