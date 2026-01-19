/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>
#include <mutex>

#include "Socket.hpp"
#include "Communication/IDaemonSocket.hpp"

class WUnixSocketSource : public IDaemonSocket
{
	std::shared_ptr<WClientSocket> Socket;
	mutable std::mutex             SocketMutex;

public:
	WUnixSocketSource();

	void Start() override;
	void Stop() override { Socket->Close(); }

	bool SendFramed(std::string const& Data) override
	{
		std::lock_guard lk(SocketMutex);
		if (!Socket->IsConnected())
		{
			return false;
		}
		return Socket->SendFramed(Data);
	}

	bool IsConnected() const override
	{
		std::lock_guard lk(SocketMutex);
		return Socket->IsConnected();
	}

	sigslot::signal<WBuffer&>& GetDataSignal() override { return Socket->OnData; }
	sigslot::signal<>&         GetClosedSignal() override { return Socket->OnClosed; }
};
