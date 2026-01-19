/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <atomic>
#include <mutex>
#include <queue>
#include <vector>

#include <libwebsockets.h>
#include <sigslot/signal.hpp>

#include "Communication/IClientSocket.hpp"

class WClientWebSocket final : public IClientSocket
{
	lws*              Wsi{ nullptr };
	std::atomic<bool> bConnected{ true };

	sigslot::signal<WBuffer&> OnData;
	sigslot::signal<>         OnClosed;

	// Outgoing message queue (each message is already framed with length prefix)
	std::mutex                    SendMutex;
	std::queue<std::vector<char>> SendQueue;

	// Buffer for receiving fragmented messages
	std::vector<char> ReceiveBuffer;

public:
	explicit WClientWebSocket(lws* Wsi_);

	sigslot::signal<WBuffer&>& GetDataSignal() override { return OnData; }
	sigslot::signal<>&         GetClosedSignal() override { return OnClosed; }

	void StartListenThread() override;

	[[nodiscard]] bool IsConnected() const override;

	void Close() override;

	ssize_t SendFramed(std::string const& Data) override;

	// Called by DaemonWebSocket callback
	void HandleReceive(char const* Data, size_t Len);
	void HandleWritable();
	void HandleClose();

	// Request to write when possible
	void RequestWrite();
};
