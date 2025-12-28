/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DaemonSocket.hpp"

#include <filesystem>
#include <spdlog/spdlog.h>
#include <cstdint>

// ReSharper disable CppUnusedIncludeDirective
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>
// ReSharper restore CppUnusedIncludeDirective

#include "DaemonConfig.hpp"
#include "ErrnoUtil.hpp"
#include "Filesystem.hpp"
#include "Messages.hpp"
#include "Data/AppIconAtlasBuilder.hpp"
#include "Data/Protocol.hpp"
#include "Data/SystemMap.hpp"
#include "Net/Resolver.hpp"
#include "tracy/Tracy.hpp"

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
			auto& SystemMap = WSystemMap::GetInstance();
			NewClient->SendMessage(MT_Handshake, WProtocolHandshake{ WAECHTER_PROTOCOL_VERSION, GIT_COMMIT_HASH });
			{
				std::lock_guard Lock(SystemMap.DataMutex);
				NewClient->SendMessage(MT_TrafficTree, *SystemMap.GetSystemItem());
			}
			{
				std::lock_guard Lock(WResolver::GetInstance().ResolvedAddressesMutex);
				NewClient->SendMessage(MT_ResolvedAddresses, WResolver::GetInstance().GetResolvedAddresses());
			}

			// Send app icon atlas
			auto              ActiveApps = SystemMap.GetActiveApplicationPaths();
			WAppIconAtlasData Data{};
			if (WAppIconAtlasBuilder::GetInstance().GetAtlasData(Data, ActiveApps))
			{
				spdlog::info("Atlas has {} icons", Data.UvData.size());
				NewClient->SendMessage(MT_AppIconAtlasData, Data);
			}
			else
			{
				spdlog::warn("No app icon atlas data to send to client");
			}

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

	std::stringstream Os{};
	{
		std::lock_guard            DataLock(SystemMap.DataMutex);
		WTrafficTreeUpdates const& Updates = SystemMap.GetUpdates();
		Os << MT_TrafficTreeUpdate;
		cereal::BinaryOutputArchive Archive(Os);
		ZoneScopedN("Archive");
		Archive(Updates);
	}
	std::string const& Str = Os.str();

	for (auto const& Client : Clients)
	{
		ZoneScopedN("SendTrafficUpdate");
		auto Sent = Client->SendFramedData(Str);
		if (Sent < 0)
		{
			spdlog::error("Failed to send traffic update to client: {}", WErrnoUtil::StrError());
		}
	}
}

void WDaemonSocket::BroadcastAtlasUpdate()
{
	auto&           SystemMap = WSystemMap::GetInstance();
	std::lock_guard Lock(ClientsMutex);
	spdlog::info("App icon atlas is dirty, broadcasting atlas update to clients");
	auto              ActiveApps = SystemMap.GetActiveApplicationPaths();
	WAppIconAtlasData Data{};
	if (WAppIconAtlasBuilder::GetInstance().GetAtlasData(Data, ActiveApps))
	{
		for (auto const& Client : Clients)
		{
			Client->SendMessage(MT_AppIconAtlasData, Data);
		}
	}
	WAppIconAtlasBuilder::GetInstance().ClearDirty();
}
