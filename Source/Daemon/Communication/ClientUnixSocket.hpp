/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Communication/IClientSocket.hpp"

class WClientUnixSocket final : public IClientSocket
{
	std::shared_ptr<WClientSocket> Socket;

public:
	explicit WClientUnixSocket(std::shared_ptr<WClientSocket> socket) : Socket(std::move(socket)) {}

	sigslot::signal<>&         GetClosedSignal() override { return Socket->OnClosed; }
	sigslot::signal<WBuffer&>& GetDataSignal() override { return Socket->OnData; }

	void StartListenThread() override { Socket->StartListenThread(); }

	[[nodiscard]] bool IsConnected() const override { return Socket->IsConnected(); }

	void Close() override { Socket->Close(); }

	ssize_t SendFramed(std::string const& Data) override
	{
		return Socket->SendFramed(Data) ? static_cast<ssize_t>(Data.size()) : -1;
	}
};
