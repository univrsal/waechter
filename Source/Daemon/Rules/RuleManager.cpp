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
#include "Data/NetworkEvents.hpp"

void WRuleManager::UpdateLocalRuleCache(WRuleUpdate const& Update)
{
	auto&            Map = WSystemMap::GetInstance();
	std::scoped_lock Lock(Mutex, Map.DataMutex);

	auto Item = Map.GetTrafficItemById(Update.TrafficItemId);

	switch (Item->GetType())
	{
		case TI_Socket:
			HandleSocketRuleUpdate(Item, Update.Rules);
			break;
		case TI_Process:
			HandleProcessRuleUpdate(Item, Update.Rules);
			break;
		case TI_Application:
			HandleApplicationRuleUpdate(Item, Update.Rules);
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

	for (auto& [Cookie, Entry] : SocketRulesMap)
	{
		if (!Entry.bDirty)
		{
			continue;
		}

		spdlog::info("Setting rule for cookie {}: {:08b}", Cookie, Entry.Rules.SwitchFlags);
		if (EbpfData->SocketRules->Update(Cookie, Entry.Rules))
		{
			Entry.bDirty = false;
		}
		else
		{
			spdlog::error("Failed to update socket rules in eBPF map for cookie {}", Cookie);
		}
	}
}

void WRuleManager::HandleSocketRuleUpdate(
	std::shared_ptr<ITrafficItem> const& Item, WNetworkItemRules const& Rules, WSocketRuleLevel Level)
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

	if (SocketRulesMap.contains(Socket->Cookie))
	{
		auto& ExistingEntry = SocketRulesMap[Socket->Cookie];
		if (ExistingEntry.Level > Level)
		{
			spdlog::info(
				"Skipping rule update for socket item {} since existing rule has higher priority", Item->ItemId);
			return;
		}
	}

	WSocketRulesEntry NewEntry{};
	NewEntry.Rules = Rules;
	NewEntry.Level = Level;
	NewEntry.bDirty = true;
	SocketRulesMap[Socket->Cookie] = NewEntry;
}

void WRuleManager::HandleProcessRuleUpdate(
	std::shared_ptr<ITrafficItem> const& Item, WNetworkItemRules const& Rules, WSocketRuleLevel Level)
{
	auto Process = std::dynamic_pointer_cast<WProcessItem>(Item);
	if (!Process)
	{
		return;
	}

	ProcessRules[Process->ItemId] = Rules;

	for (auto const& SocketItem : Process->Sockets | std::views::values)
	{
		if (SocketItem)
		{
			HandleSocketRuleUpdate(SocketItem, Rules, Level);
		}
	}
}

void WRuleManager::HandleApplicationRuleUpdate(
	std::shared_ptr<ITrafficItem> const& Item, WNetworkItemRules const& Rules)
{
	auto App = std::dynamic_pointer_cast<WApplicationItem>(Item);
	if (!App)
	{
		return;
	}

	ApplicationRules[App->ItemId] = Rules;
	spdlog::info("Storing application level rules for app {}: {:08b}", App->ApplicationName, Rules.SwitchFlags);

	for (auto const& ProcessItem : App->Processes | std::views::values)
	{
		if (ProcessItem)
		{
			HandleProcessRuleUpdate(ProcessItem, Rules, SRL_Application);
		}
	}
}

void WRuleManager::OnSocketCreated(std::shared_ptr<WSocketCounter> const& Socket)
{
	// Apply any existing rules for this socket
	auto Process = Socket->ParentProcess;
	auto App = Process->ParentApp;
	if (!Process || !App)
	{
		return;
	}

	std::lock_guard Lock(Mutex);
	// Prioritize process rules over application rules
	if (ProcessRules.contains(App->TrafficItem->ItemId))
	{
		auto const& ProcRules = ProcessRules[App->TrafficItem->ItemId];
		// No point in applying empty rules
		if (ProcRules.SwitchFlags != 0)
		{
			spdlog::info("Applying process rule to newly created socket {} for {}", Socket->TrafficItem->ItemId,
				App->TrafficItem->ApplicationName);
			HandleSocketRuleUpdate(Socket->TrafficItem, ProcRules, SRL_Process);
			SyncWithEbpfMap();
		}
	}
	else if (ApplicationRules.contains(App->TrafficItem->ItemId))
	{
		auto const& AppRules = ApplicationRules[App->TrafficItem->ItemId];
		if (AppRules.SwitchFlags != 0)
		{
			spdlog::info("Applying process rule to newly created socket {} for {}", Socket->TrafficItem->ItemId,
				App->TrafficItem->ApplicationName);
			HandleSocketRuleUpdate(Socket->TrafficItem, AppRules, SRL_Application);
			SyncWithEbpfMap();
		}
	}
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
	spdlog::info("Update for {}, {:08b}", Update.TrafficItemId, Update.Rules.SwitchFlags);
	UpdateLocalRuleCache(Update);
	std::lock_guard Lock(Mutex);
	SyncWithEbpfMap();
}