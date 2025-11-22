//
// Created by usr on 20/11/2025.
//

#include "RuleManager.hpp"

#include <spdlog/spdlog.h>
#include <cereal/archives/binary.hpp>

#include "Daemon.hpp"
#include "Data/RuleUpdate.hpp"
#include "Data/SystemMap.hpp"
#include "EbpfData.hpp"

void WRuleManager::UpdateLocalRuleCache(WRuleUpdate const& Update)
{
	auto&           Map = WSystemMap::GetInstance();
	std::lock_guard MapLock(Map.DataMutex);
	std::lock_guard Lock(Mutex);
	auto            Item = Map.GetTrafficItemById(Update.TrafficItemId);

	switch (Item->GetType())
	{
		case TI_Socket:
			HandleSocketRuleUpdate(Item, Update);
			break;
		case TI_Process:
			HandleProcessRuleUpdate(Item, Update);
			break;
		case TI_Application:
			HandleApplicationRuleUpdate(Item, Update);
			break;
		case TI_System:
			// todo
			break;
		default:;
	}
}

void WRuleManager::SyncWithEbpfMap()
{
	auto EbpfData = WDaemon::GetInstance().GetEbpfObj().GetData();

	for (auto const& [Cookie, Entry] : Rules)
	{
		spdlog::info("Setting rule for cookie {}: bDownloadBlocked={}, bUploadBlocked={}", Cookie,
			Entry.Rules.bDownloadBlocked, Entry.Rules.bUploadBlocked);
		if (!EbpfData->SocketRules->Update(Cookie, Entry.Rules))
		{
			spdlog::error("Failed to update socket rules in eBPF map for cookie {}", Cookie);
		}
	}
}

void WRuleManager::HandleSocketRuleUpdate(
	std::shared_ptr<ITrafficItem> Item, WRuleUpdate const& Update, WSocketRuleLevel Level)
{
	auto Socket = std::dynamic_pointer_cast<WSocketItem>(Item);
	if (!Socket)
	{
		return;
	}
	if (Socket->Cookie == 0)
	{
		spdlog::warn("Received rule update for socket item {} with invalid cookie 0", Item->ItemId);
		return;
	}

	if (Rules.contains(Socket->Cookie))
	{
		auto& ExistingEntry = Rules[Socket->Cookie];
		if (ExistingEntry.Level > Level)
		{
			spdlog::info(
				"Skipping rule update for socket item {} since existing rule has higher priority", Item->ItemId);
			return;
		}
	}

	WSocketRulesEntry NewEntry{};
	NewEntry.Rules = Update.Rules;
	NewEntry.Level = Level;
	Rules[Socket->Cookie] = NewEntry;
}

void WRuleManager::HandleProcessRuleUpdate(
	std::shared_ptr<ITrafficItem> Item, WRuleUpdate const& Update, WSocketRuleLevel Level)
{
	auto Process = std::dynamic_pointer_cast<WProcessItem>(Item);
	if (!Process)
	{
		return;
	}
	for (auto const& SocketItem : Process->Sockets | std::views::values)
	{
		if (SocketItem)
		{
			HandleSocketRuleUpdate(SocketItem, Update, Level);
		}
	}
}

void WRuleManager::HandleApplicationRuleUpdate(std::shared_ptr<ITrafficItem> Item, WRuleUpdate const& Update)
{
	auto App = std::dynamic_pointer_cast<WApplicationItem>(Item);
	if (!App)
	{
		return;
	}

	for (auto const& ProcessItem : App->Processes | std::views::values)
	{
		if (ProcessItem)
		{
			HandleProcessRuleUpdate(ProcessItem, Update, SRL_Application);
		}
	}
}

void WRuleManager::OnSocketCreated(std::shared_ptr<ITrafficItem>)
{
	spdlog::info("Socket created");
}

void WRuleManager::RegisterSignalHandlers()
{
	WNetworkEvents::GetInstance().OnSocketCreated.connect(
		std::bind(&WRuleManager::OnSocketCreated, this, std::placeholders::_1));
}

void WRuleManager::HandleRuleChange(WBuffer const& Buf)
{
	WRuleUpdate       Update{};
	std::stringstream ss;
	ss.write(Buf.GetData(), static_cast<long int>(Buf.GetWritePos()));
	{
		ss.seekg(1); // Skip message type
		cereal::BinaryInputArchive iar(ss);
		iar(Update);
	}
	spdlog::info("Update for {}, bDownloadBlocked={}, bUploadBlocked={}", Update.TrafficItemId,
		Update.Rules.bDownloadBlocked, Update.Rules.bUploadBlocked);
	UpdateLocalRuleCache(Update);
	SyncWithEbpfMap();
}