/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DaemonSocket.hpp"

#include <filesystem>
#include <sys/sysinfo.h>

#include "spdlog/spdlog.h"
#include "tracy/Tracy.hpp"
// ReSharper disable CppUnusedIncludeDirective
#include "cereal/types/array.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/optional.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/string.hpp"
#include "cereal/archives/binary.hpp"
// ReSharper restore CppUnusedIncludeDirective

#if WAECHTER_WITH_WEBSOCKETSERVER
	#include "DaemonWebSocket.hpp"
#endif
#include "DaemonConfig.hpp"
#include "DaemonUnixSocket.hpp"
#include "ErrnoUtil.hpp"
#include "Filesystem.hpp"
#include "MemoryUsage.hpp"
#include "Messages.hpp"
#include "NetworkInterface.hpp"
#include "Time.hpp"
#include "Data/AppIconAtlasBuilder.hpp"
#include "Data/ConnectionHistory.hpp"
#include "Data/Protocol.hpp"
#include "Data/SystemMap.hpp"
#include "Rules/RuleManager.hpp"

static WSec GetSystemBootTime()
{
	struct sysinfo Info{};
	if (sysinfo(&Info) != 0)
		return 0;
	return WTime::GetEpochSeconds() - Info.uptime;
}

// ReSharper disable once CppDFAUnreachableFunctionCall
static void SendInitialDataToClient(std::shared_ptr<WDaemonClient> const& Client)
{
	auto& SystemMap = WSystemMap::GetInstance();
	auto const&        Cfg = WDaemonConfig::GetInstance();
	WDaemonConfigMessage InitialConfig{};
	InitialConfig.bFirstTimeSetupRan = Cfg.bFirstTimeSetupRun;
	InitialConfig.SocketMode = static_cast<int>(Cfg.DaemonSocketMode);
	InitialConfig.DaemonGroup = Cfg.DaemonGroup;
	InitialConfig.DaemonUser = Cfg.DaemonUser;
	InitialConfig.SocketPath = Cfg.DaemonSocketPath;
	InitialConfig.MainInterface = Cfg.NetworkInterfaceName;
	InitialConfig.VpnInterface = Cfg.IngressNetworkInterfaceName;
	InitialConfig.NetworkInterfaces = WNetworkInterface::List();

	Client->SendMessage(
		MT_Handshake, WProtocolHandshake{ WAECHTER_PROTOCOL_VERSION, GetSystemBootTime(), GIT_COMMIT_HASH });
	{
		std::lock_guard Lock(SystemMap.DataMutex);
		Client->SendMessage(MT_TrafficTree, *SystemMap.GetSystemItem());
		Client->SendMessage(MT_ConnectionHistory, WConnectionHistory::GetInstance().Serialize());
		Client->SendMessage(MT_DaemonConfig, InitialConfig);
	}

	Client->SendMessage(MT_MemoryStats, WMemoryUsage::GetMemoryStats());

	WRuleManager::GetInstance().SendCurrentRulesToClient(Client);

	// Send app icon atlas
	auto const        ActiveApps = SystemMap.GetActiveApplicationPaths();
	WAppIconAtlasData Data{};
	if (WAppIconAtlasBuilder::GetInstance().GetAtlasData(Data, ActiveApps))
	{
		spdlog::info("Atlas has {} icons", Data.UvData.size());
		Client->SendMessage(MT_AppIconAtlasData, Data);
	}
}

void WDaemonSocket::OnNewConnection(std::shared_ptr<WDaemonClient> const& NewClient)
{
	SendInitialDataToClient(NewClient);
	ClientsMutex.lock();
	Clients.push_back(NewClient);
	bHasClients = true;
	ClientsMutex.unlock();
	RemoveInactiveClients();
}

WDaemonSocket::WDaemonSocket(std::string const& Path)
{
	char hostname[HOST_NAME_MAX];
	if (gethostname(hostname, HOST_NAME_MAX) == 0)
	{
		Hostname = hostname;
	}
	else
	{
		Hostname = "System";
	}

	if (Path.starts_with("/"))
	{
		Socket = std::make_unique<WDaemonUnixSocket>();
	}
#if WAECHTER_WITH_WEBSOCKETSERVER
	else if (Path.starts_with("ws"))
	{
		Socket = std::make_unique<WDaemonWebSocket>();
	}
#endif
}

void WDaemonSocket::BroadcastMemoryUsageUpdate()
{
	std::stringstream Os{};
	{
		auto Stats = WMemoryUsage::GetMemoryStats();
		Os << MT_MemoryStats;
		cereal::BinaryOutputArchive Archive(Os);
		Archive(Stats);
	}

	std::lock_guard    Lock(ClientsMutex);
	std::string const& Str = Os.str();

	for (auto const& Client : Clients)
	{
		if (Client->SendFramedData(Str) < 0)
		{
			spdlog::error("Failed to send memory usage update to client: {}", WErrnoUtil::StrError());
			Client->GetSocket()->Close();
		}
	}
}

void WDaemonSocket::BroadcastTrafficUpdate()
{
	if (!HasClients())
	{
		return;
	}
	auto&           SystemMap = WSystemMap::GetInstance();
	std::lock_guard Lock(ClientsMutex);

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
		if (Client->SendFramedData(Str) < 0)
		{
			spdlog::error("Failed to send traffic update to client: {}", WErrnoUtil::StrError());
			Client->GetSocket()->Close();
		}
	}
}

void WDaemonSocket::BroadcastConnectionHistoryUpdate(WConnectionHistoryUpdate const& Update)
{
	std::lock_guard Lock(ClientsMutex);
	for (auto const& Client : Clients)
	{
		if (Client->SendMessage(MT_ConnectionHistoryUpdate, Update) < 0)
		{
			spdlog::error("Failed to send app icon atlas update to client: {}", WErrnoUtil::StrError());
			Client->GetSocket()->Close();
		}
	}
}

void WDaemonSocket::BroadcastAtlasUpdate()
{
	auto&             SystemMap = WSystemMap::GetInstance();
	std::lock_guard   Lock(ClientsMutex);
	auto              ActiveApps = SystemMap.GetActiveApplicationPaths();
	WAppIconAtlasData Data{};
	if (WAppIconAtlasBuilder::GetInstance().GetAtlasData(Data, ActiveApps))
	{
		auto Msg = WDaemonClient::MakeMessage(MT_AppIconAtlasData, Data);
		spdlog::debug("App icon atlas is dirty, broadcasting atlas update with {} KiB to clients", Msg.length() / 1024);
		ZoneScopedN("BroadcastAtlasUpdate.SendMessage");
		for (auto const& Client : Clients)
		{
			if (Client->SendFramedData(Msg) < 0)
			{
				spdlog::error("Failed to send app icon atlas update to client: {}", WErrnoUtil::StrError());
				Client->GetSocket()->Close();
			}
		}
	}
	WAppIconAtlasBuilder::GetInstance().ClearDirty();
}
