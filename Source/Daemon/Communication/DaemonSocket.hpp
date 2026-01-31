/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>

#include "DaemonClient.hpp"
#include "Communication/IServerSocket.hpp"

struct WConnectionHistoryUpdate;

class WDaemonSocket
{
	std::unique_ptr<IServerSocket> Socket;
	std::string       Hostname{};

	std::mutex                                  ClientsMutex{};
	std::vector<std::shared_ptr<WDaemonClient>> Clients;

	void OnNewConnection(std::shared_ptr<WDaemonClient> const& NewClient);

public:
	explicit WDaemonSocket(std::string const& Path);

	bool StartListenThread()
	{
		Socket->GetNewConnectionSignal().connect(
			std::bind(&WDaemonSocket::OnNewConnection, this, std::placeholders::_1));
		return Socket->StartListenThread();
	}

	void Stop() const { Socket->Stop(); }

	~WDaemonSocket() { Stop(); }

	std::vector<std::shared_ptr<WDaemonClient>>& GetClients() { return Clients; }

	IServerSocket* GetSocketImpl() const { return Socket.get(); }

	bool HasClients()
	{
		std::lock_guard Lock(ClientsMutex);
		return !Clients.empty();
	}

	void RemoveInactiveClients()
	{
		ClientsMutex.lock();
		Clients.erase(std::remove_if(Clients.begin(), Clients.end(), [](auto& Client) { return !Client->IsRunning(); }),
			Clients.end());
		ClientsMutex.unlock();
	}

	void BroadcastMemoryUsageUpdate();
	void BroadcastTrafficUpdate();
	void BroadcastConnectionHistoryUpdate(WConnectionHistoryUpdate const& Update);
	void BroadcastAtlasUpdate();
};
