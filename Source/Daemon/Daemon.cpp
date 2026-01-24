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
#include "Data/AppIconAtlasBuilder.hpp"
#include "Data/ConnectionHistory.hpp"
#include "Data/SystemMap.hpp"
#include "Rules/RuleManager.hpp"

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
	while (!SignalHandler.bStop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		{
			ZoneScopedN("RefreshAllTrafficCounters");
			WSystemMap::GetInstance().RefreshAllTrafficCounters();
		}
		{
			ZoneScopedN("BroadcastTrafficUpdate");
			DaemonSocket->BroadcastTrafficUpdate();
		}
		{
			ZoneScopedN("WConnectionHistory::Update");
			auto Updates = WConnectionHistory::GetInstance().Update();
			if (!Updates.Changes.empty())
			{
				DaemonSocket->BroadcastConnectionHistoryUpdate(Updates);
			}
		}

		if (DaemonSocket->HasClients() && WAppIconAtlasBuilder::GetInstance().IsDirty())
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