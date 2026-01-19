/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include <curl/curl.h>
#include <sigslot/signal.hpp>

#include "Communication/IDaemonSocket.hpp"

class WWebSocketSource : public IDaemonSocket
{
	std::string Url;
	CURL*       Curl{ nullptr };

	sigslot::signal<WBuffer&> OnData;
	sigslot::signal<>         OnClosed;

	std::thread        ListenerThread;
	std::atomic<bool>  bListenThreadRunning{ false };
	std::atomic<bool>  bConnected{ false };
	mutable std::mutex CurlMutex;

	void ListenThreadFunction();
	bool Connect();

public:
	explicit WWebSocketSource(std::string WebSocketUrl);
	~WWebSocketSource() override;

	void Start() override;
	void Stop() override;

	sigslot::signal<WBuffer&>& GetDataSignal() override { return OnData; }
	sigslot::signal<>&         GetClosedSignal() override { return OnClosed; }

	bool SendFramed(std::string const& Data) override;
	bool IsConnected() const override;
};
