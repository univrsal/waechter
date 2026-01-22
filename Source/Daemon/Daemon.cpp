/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Daemon.hpp"

#include "spdlog/spdlog.h"
#include "tracy/Tracy.hpp"

#include "DaemonConfig.hpp"
#include "ErrnoUtil.hpp"
#include "SignalHandler.hpp"
#include "Data/AppIconAtlasBuilder.hpp"
#include "Data/ConnectionHistory.hpp"
#include "Data/SystemMap.hpp"
#include "Rules/RuleManager.hpp"

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
	WSignalHandler& SignalHandler = WSignalHandler::GetInstance();

	// Poll ring buffers and periodically print aggregate stats
	auto LastPrint = std::chrono::steady_clock::now();
	auto LastTrafficUpdate = std::chrono::steady_clock::now();

	while (!SignalHandler.bStop)
	{
		{
			ZoneScopedN("EbpfObj.UpdateData");
			EbpfObj.UpdateData();
		}
		auto now = std::chrono::steady_clock::now();
		if (now - LastPrint >= std::chrono::milliseconds(30000))
		{
			EbpfObj.PrintStats();
			LastPrint = now;
		}

		if (now - LastTrafficUpdate >= std::chrono::milliseconds(1000))
		{
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
			LastTrafficUpdate = now;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	DaemonSocket->Stop();
}