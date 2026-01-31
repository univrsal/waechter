/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
#include <emscripten/val.h>

#include "sigslot/signal.hpp"

#include "Communication/IDaemonSocket.hpp"

class WEmscriptenSocketSource : public IDaemonSocket
{
	std::string               Url;
	std::string               AuthToken;
	emscripten::val           WebSocket{ emscripten::val::null() };
	sigslot::signal<WBuffer&> OnData;
	sigslot::signal<>         OnClosed;

	std::atomic<bool> bConnected{ false };
	std::atomic<bool> bConnectionFailed{ false };

	std::mutex                    SendMutex;
	std::queue<std::vector<char>> SendQueue;

	std::vector<char> ReceiveBuffer;

	void ProcessPendingMessages();

public:
	explicit WEmscriptenSocketSource(std::string WebSocketUrl, std::string AuthToken);
	~WEmscriptenSocketSource() override;

	void Start() override;
	void Stop() override;

	sigslot::signal<WBuffer&>& GetDataSignal() override { return OnData; }
	sigslot::signal<>&         GetClosedSignal() override { return OnClosed; }

	bool SendFramed(std::string const& Data) override;
	bool IsConnected() const override;

	// Called from JavaScript callbacks
	void OnConnected();
	void OnConnectionError();
	void OnConnectionClosed();
	void HandleReceive(char const* Data, size_t Len);

	std::string const& GetAuthToken() const { return AuthToken; }
};