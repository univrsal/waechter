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
	std::atomic<bool>                           bHasClients{};
	std::vector<std::shared_ptr<WDaemonClient>> Clients;

	void OnNewConnection(std::shared_ptr<WDaemonClient> const& NewClient);

public:
	explicit WDaemonSocket(std::string const& Path);

	bool StartListenThread()
	{
		Socket->GetNewConnectionSignal().connect(
			[&](std::shared_ptr<WDaemonClient> const& Client) { OnNewConnection(Client); });
		return Socket->StartListenThread();
	}

	void Stop() const { Socket->Stop(); }

	~WDaemonSocket() { Stop(); }

	std::vector<std::shared_ptr<WDaemonClient>>& GetClients() { return Clients; }

	IServerSocket* GetSocketImpl() const { return Socket.get(); }

	[[nodiscard]] bool HasClients() const { return bHasClients; }

	void RemoveInactiveClients()
	{
		ClientsMutex.lock();
		Clients.erase(std::remove_if(Clients.begin(), Clients.end(), [](auto& Client) { return !Client->IsRunning(); }),
			Clients.end());
		bHasClients = !Clients.empty();
		ClientsMutex.unlock();
	}

	void BroadcastMemoryUsageUpdate();
	void BroadcastTrafficUpdate();
	void BroadcastConnectionHistoryUpdate(WConnectionHistoryUpdate const& Update);
	void BroadcastAtlasUpdate();
};
