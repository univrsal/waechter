/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>

#include "Socket.hpp"
#include "Communication/IServerSocket.hpp"

class WDaemonUnixSocket final : public IServerSocket
{
	std::unique_ptr<WServerSocket> Socket;

	std::atomic<bool> Running{ false };
	std::thread       ListenThread{};
	std::string       SocketPath{};

	void                                                   ListenThreadFunction() const;
	sigslot::signal<std::shared_ptr<WDaemonClient> const&> OnNewConnection;

public:
	WDaemonUnixSocket();

	void Stop() override;

	bool StartListenThread() override;

	sigslot::signal<std::shared_ptr<WDaemonClient> const&>& GetNewConnectionSignal() override
	{
		return OnNewConnection;
	}
};
