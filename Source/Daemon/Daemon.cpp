//
// Created by usr on 19/11/2025.
//

#include "Daemon.hpp"

#include <spdlog/spdlog.h>

#include "DaemonConfig.hpp"
#include "ErrnoUtil.hpp"
#include "SignalHandler.hpp"
#include "Data/AppIconAtlasBuilder.hpp"
#include "Data/SystemMap.hpp"
#include "Rules/RuleManager.hpp"

WDaemon::WDaemon()
{
	WDaemonConfig::GetInstance().LogConfig();
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
	WSignalHandler& SignalHandler = WSignalHandler::GetInstance();

	// Poll ring buffers and periodically print aggregate stats
	auto LastPrint = std::chrono::steady_clock::now();
	auto LastTrafficUpdate = std::chrono::steady_clock::now();

	while (!SignalHandler.bStop)
	{
		EbpfObj.UpdateData();
		auto now = std::chrono::steady_clock::now();
		if (now - LastPrint >= std::chrono::milliseconds(30000))
		{
			EbpfObj.PrintStats();
			LastPrint = now;
		}

		if (now - LastTrafficUpdate >= std::chrono::milliseconds(1000))
		{
			WSystemMap::GetInstance().RefreshAllTrafficCounters();
			DaemonSocket->BroadcastTrafficUpdate();

			if (DaemonSocket->HasClients() && WAppIconAtlasBuilder::GetInstance().IsDirty())
			{
				DaemonSocket->BroadcastAtlasUpdate();
			}
			LastTrafficUpdate = now;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	DaemonSocket->Stop();
}