/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <libwebsockets.h>
#include <sigslot/signal.hpp>

#include "Communication/IDaemonSocket.hpp"

class WWebSocketSource : public IDaemonSocket
{
	std::string               Url;
	lws_context*              Context{ nullptr };
	lws*                      Wsi{ nullptr };
	int                       TimerId{ -1 };
	sigslot::signal<WBuffer&> OnData;
	sigslot::signal<>         OnClosed;

	std::thread       ListenerThread;
	std::atomic<bool> bListenThreadRunning{ false };
	std::atomic<bool> bConnected{ false };
	std::atomic<bool> bConnectionFailed{ false };

	// Outgoing message queue
	std::mutex                    SendMutex;
	std::queue<std::vector<char>> SendQueue;

	// Buffer for receiving fragmented messages
	std::vector<char> ReceiveBuffer;

	void ListenThreadFunction();

public:
	explicit WWebSocketSource(std::string WebSocketUrl);
	~WWebSocketSource() override;

	void Start() override;
	void Stop() override;

	sigslot::signal<WBuffer&>& GetDataSignal() override { return OnData; }
	sigslot::signal<>&         GetClosedSignal() override { return OnClosed; }

	bool SendFramed(std::string const& Data) override;
	bool IsConnected() const override;

	// Called by callback
	void OnConnected();
	void OnConnectionError();
	void OnConnectionClosed();
	void HandleReceive(char const* Data, size_t Len);
	void HandleWritable();
	void RequestWrite() const;
};
