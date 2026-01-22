/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <libwebsockets.h>

#include "Communication/IServerSocket.hpp"

class WClientWebSocket;

class WDaemonWebSocket final : public IServerSocket
{
	sigslot::signal<std::shared_ptr<WDaemonClient> const&> OnNewConnection;

	lws_context*      Context{ nullptr };
	std::thread       ListenThread;
	std::atomic<bool> Running{ false };
	std::mutex        ClientsMutex;

	// Map from lws* to client wrapper
	std::unordered_map<lws*, std::shared_ptr<WClientWebSocket>> Clients;

	void ListenThreadFunction() const;

public:
	WDaemonWebSocket();
	~WDaemonWebSocket() override;

	sigslot::signal<std::shared_ptr<WDaemonClient> const&>& GetNewConnectionSignal() override
	{
		return OnNewConnection;
	}

	bool StartListenThread() override;
	void Stop() override;

	// Called from the callback to register/unregister clients
	void                              RegisterClient(lws* Wsi, std::shared_ptr<WClientWebSocket> Client);
	void                              UnregisterClient(lws* Wsi);
	std::shared_ptr<WClientWebSocket> GetClient(lws* Wsi);
};
