/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Daemon.hpp"

#include <pthread.h>

#include "spdlog/spdlog.h"
#include "tracy/Tracy.hpp"

#include "DaemonConfig.hpp"
#include "ErrnoUtil.hpp"
#include "SignalHandler.hpp"
#include "Communication/ClientWebSocket.hpp"
#include "Data/AppIconAtlasBuilder.hpp"
#include "Data/ConnectionHistory.hpp"
#include "Data/SystemMap.hpp"
#include "EBPF/EbpfData.hpp"
#include "Rules/RuleManager.hpp"

#if WAECHTER_WITH_WEBSOCKETSERVER
	#include "Communication/DaemonWebSocket.hpp"
#endif

void WDaemon::EbpfPollThreadFunction()
{
	tracy::SetThreadName("ebpf-poll");
	pthread_setname_np(pthread_self(), "ebpf-poll");
	auto const& SignalHandler = WSignalHandler::GetInstance();

	while (!SignalHandler.bStop)
	{
		EbpfObj.UpdateData();
	}
}

void WDaemon::PeriodicUpdatesThreadFunction() const
{
	tracy::SetThreadName("periodic-updates");
	pthread_setname_np(pthread_self(), "per-updates");

	auto const& SignalHandler = WSignalHandler::GetInstance();
	int         Counter = 0;
	while (!SignalHandler.bStop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		Counter++;

		{
			ZoneScopedN("RefreshAllTrafficCounters");
			WSystemMap::GetInstance().RefreshAllTrafficCounters();
		}
		if (!DaemonSocket->HasClients())
		{
			continue;
		}

		if (WSystemMap::GetInstance().HasNewData())
		{
			ZoneScopedN("BroadcastTrafficUpdate");
			DaemonSocket->BroadcastTrafficUpdate();
		}

		if (Counter >= 5)
		{
			ZoneScopedN("BroadcastMemoryUsageUpdate");
			Counter = 0;
			DaemonSocket->BroadcastMemoryUsageUpdate();
		}

		{
			ZoneScopedN("WConnectionHistory::Update");
			auto Updates = WConnectionHistory::GetInstance().Update();
			if (!Updates.Changes.empty())
			{
				DaemonSocket->BroadcastConnectionHistoryUpdate(Updates);
			}
		}

		if (WAppIconAtlasBuilder::GetInstance().IsDirty())
		{
			ZoneScopedN("BroadcastAtlasUpdate");
			DaemonSocket->BroadcastAtlasUpdate();
		}
	}
}

WDaemon::WDaemon()
{
	WDaemonConfig::BTFTest();
}

bool WDaemon::InitEbpfObj()
{
	if (auto Result = EbpfObj.Init(); Result != EEbpfInitResult::Success)
	{
		spdlog::error("EbpfObj.Init() failed: {}", static_cast<int>(Result));
		return false;
	}
	return true;
}

bool WDaemon::InitSocket()
{
	DaemonSocket = std::make_shared<WDaemonSocket>(WDaemonConfig::GetInstance().DaemonSocketPath);

	if (!DaemonSocket->StartListenThread())
	{
		spdlog::error("Failed to bind and listen on daemon socket: {} ({})", WErrnoUtil::StrError(), errno);
		return false;
	}

	return true;
}

void WDaemon::RegisterSignalHandlers()
{
	WRuleManager::GetInstance().RegisterSignalHandlers();
}

void WDaemon::RunLoop()
{
	tracy::SetThreadName("main");
	pthread_setname_np(pthread_self(), "main");

	EbpfPollThread = std::thread(&WDaemon::EbpfPollThreadFunction, this);
	PeriodicUpdatesThread = std::thread(&WDaemon::PeriodicUpdatesThreadFunction, this);

	WSignalHandler::GetInstance().Wait();
	EbpfPollThread.join();
	PeriodicUpdatesThread.join();
	DaemonSocket->Stop();
}

WMemoryStat WDaemon::GetMemoryUsage()
{
	WMemoryStat Stats{};
	Stats.Name = "WDaemon";

	WMemoryStatEntry EbpfDataEntry{};
	EbpfDataEntry.Name = "Ebpf data";
	auto const& Data = EbpfObj.GetData();
	EbpfDataEntry.Usage += sizeof(Data->PidDownloadMarks) * 3; // all three maps should be the same, none cache any data

	EbpfDataEntry.Usage += sizeof(Data->SocketEvents);
	Data->SocketEvents->GetDataMutex().lock();
	EbpfDataEntry.Usage += Data->SocketEvents->GetData().size() * sizeof(WSocketEvent);
	Data->SocketEvents->GetDataMutex().unlock();

	WMemoryStatEntry SocketEntry{};
	SocketEntry.Name = "Daemon socket";
#if WAECHTER_WITH_WEBSOCKETSERVER
	auto const& Socket = dynamic_cast<WDaemonWebSocket*>(DaemonSocket->GetSocketImpl());
	if (Socket)
	{
		std::scoped_lock Lock(Socket->GetClientsMutex());
		SocketEntry.Usage +=
			Socket->GetClients().size() * (sizeof(WClientWebSocket) + sizeof(std::shared_ptr<WClientWebSocket>));
		// we don't care about more detailed usage here for now since the increasing memory usage
		// happens even if no clients are connected
	}
#endif

	Stats.ChildEntries.emplace_back(EbpfDataEntry);
	Stats.ChildEntries.emplace_back(SocketEntry);
	return Stats;
}