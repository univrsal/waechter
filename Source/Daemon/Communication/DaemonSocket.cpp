#include "DaemonSocket.hpp"

#include <filesystem>
#include <spdlog/spdlog.h>

#include "Json.hpp"
#include "Messages.hpp"
#include "Data/SystemMap.hpp"

void WDaemonSocket::ListenThreadFunction()
{
	while (Running)
	{
		bool bTimedOut = false;
		if (auto ClientSocket = Socket.Accept(500, &bTimedOut))
		{
			spdlog::info("Client connected");
			auto NewClient = std::make_shared<WDaemonClient>(ClientSocket, this);
			NewClient->StartListenThread();

			// TODO: Re-do with cereal serialization
			// auto TrafficJson = WSystemMap::GetInstance().ToJson();
			// NewClient->Send<WMessageTrafficTree>(TrafficJson);

			ClientsMutex.lock();
			Clients.push_back(NewClient);
			ClientsMutex.unlock();
		}
		else if (Running && !bTimedOut)
		{
			// if the socket is not valid and the server is still running, print an error message
			spdlog::error("Error accepting client connection: {} ({})", strerror(errno), errno);
		}
		RemoveInactiveClients();
	}
}

bool WDaemonSocket::StartListenThread()
{
	if (std::filesystem::exists(SocketPath))
	{
		unlink(SocketPath.c_str());
	}

	if (!Socket.BindAndListen())
	{
		return false;
	}
	Running = true;
	ListenThread = std::thread(&WDaemonSocket::ListenThreadFunction, this);
	return true;
}

void WDaemonSocket::BroadcastTrafficUpdate()
{
	auto&           SystemMap = WSystemMap::GetInstance();
	std::lock_guard Lock(ClientsMutex);

	if (!SystemMap.HasNewData() || Clients.empty())
	{
		return;
	}

	// TODO: Re-do with cereal serialization
	// auto Json = SystemMap.UpdateJson();
	// for (auto& Client : Clients)
	// {
	// 	Client->Send<WMessageTrafficUpdate>(Json);
	// }
}
