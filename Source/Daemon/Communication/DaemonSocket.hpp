/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <string_view>

#include "DaemonClient.hpp"
#include "Communication/IServerSocket.hpp"
#include "spdlog/spdlog.h"

struct WConnectionHistoryUpdate;

class WDaemonSocket
{
	std::unique_ptr<IServerSocket> Socket;
	std::string       Hostname{};

	std::mutex                                  ClientsMutex{};
	std::atomic<bool>                           bHasClients{};
	std::vector<std::shared_ptr<WDaemonClient>> Clients;

	void        AttachLogSink();
	static void DetachLogSink();
	void OnNewConnection(std::shared_ptr<WDaemonClient> const& NewClient);

public:
	explicit WDaemonSocket(std::string const& Path);
	~WDaemonSocket();

	bool StartListenThread()
	{
		Socket->GetNewConnectionSignal().connect(
			[&](std::shared_ptr<WDaemonClient> const& Client) { OnNewConnection(Client); });
		return Socket->StartListenThread();
	}

	void Stop() const { Socket->Stop(); }

	std::vector<std::shared_ptr<WDaemonClient>>& GetClients() { return Clients; }

	[[nodiscard]] IServerSocket* GetSocketImpl() const { return Socket.get(); }

	[[nodiscard]] bool HasClients() const { return bHasClients; }

	void RemoveInactiveClients()
	{
		ClientsMutex.lock();
		std::erase_if(Clients, [](auto& Client) { return !Client->IsRunning(); });
		bHasClients = !Clients.empty();
		ClientsMutex.unlock();
	}

	void BroadcastMemoryUsageUpdate();
	void BroadcastTrafficUpdate();
	void BroadcastConnectionHistoryUpdate(WConnectionHistoryUpdate const& Update);
	void BroadcastAtlasUpdate();

	template <typename T>
	void BroadcastMessage(EMessageType Type, T const& Message, WDaemonClient const* Except = nullptr)
	{
		std::string const& Msg = WDaemonClient::MakeMessage(Type, Message);
		ClientsMutex.lock();
		auto const ClientsCopy = Clients;
		ClientsMutex.unlock();

		for (auto const& Client : ClientsCopy)
		{
			if (Client.get() == Except)
			{
				continue;
			}
			if (Client->SendFramedData(Msg) < 0)
			{
				spdlog::error("Failed to send message to client: {}", WErrnoUtil::StrError());
				Client->GetSocket()->Close();
			}
		}
	}
};
