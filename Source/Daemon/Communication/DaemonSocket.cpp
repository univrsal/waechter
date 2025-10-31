#include "DaemonSocket.hpp"

#include <filesystem>
#include <spdlog/spdlog.h>
#include <cereal/types/array.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

#include "Messages.hpp"
#include "Data/SystemMap.hpp"

void WDaemonSocket::ListenThreadFunction()
{
	WBuffer Buffer;
	while (Running)
	{
		bool bTimedOut = false;
		if (auto ClientSocket = Socket.Accept(500, &bTimedOut))
		{
			spdlog::info("Client connected");
			auto NewClient = std::make_shared<WDaemonClient>(ClientSocket, this);
			NewClient->StartListenThread();

			// create a binary stream for cereal to write to
			std::ostringstream Os(std::ios::binary);
			{
				Os << MT_TrafficTree;
				cereal::BinaryOutputArchive Archive(Os);
				auto&                       SystemMap = WSystemMap::GetInstance();
				Archive(SystemMap.GetSystemItem());
			}

			Buffer.Resize(Os.str().size());
			std::memcpy(Buffer.GetData(), Os.str().data(), Os.str().size());
			NewClient->SendData(Buffer);

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

	auto const& Cfg = WDaemonConfig::GetInstance();
	if (!WFilesystem::SetSocketOwnerAndPermsByName(SocketPath, Cfg.DaemonUser, Cfg.DaemonGroup, Cfg.DaemonSocketMode))
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
