/*
 * Copyright (c) 2026, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "RuleManager.hpp"

#include <spdlog/spdlog.h>
#include <cereal/archives/binary.hpp>

#include "Daemon.hpp"
#include "EBPF/EbpfData.hpp"
#include "Data/RuleUpdate.hpp"
#include "Data/SystemMap.hpp"
#include "Data/NetworkEvents.hpp"
#include "Net/IPLink.hpp"

inline ESwitchState GetEffectiveSwitchState(ESwitchState ParentState, ESwitchState ItemState, ERuleType ItemRuleType)
{
	if (ItemRuleType == ERuleType::Implicit)
	{
		return ParentState;
	}

	if (ItemState == SS_None)
	{
		return ParentState;
	}
	return ItemState;
}

inline uint32_t GetEffectiveMark(uint32_t ParentMark, uint32_t ItemMark, ERuleType ItemRuleType)
{
	if (ItemRuleType == ERuleType::Implicit)
	{
		return ParentMark;
	}
	if (ItemMark == 0)
	{
		return ParentMark;
	}
	return ItemMark;
}

inline WBytesPerSecond GetEffectiveLimit(WBytesPerSecond ParentLimit, WBytesPerSecond ItemLimit, ERuleType ItemRuleType)
{
	if (ItemRuleType == ERuleType::Implicit)
	{
		return ParentLimit;
	}
	if (ParentLimit != 0)
	{
		return ParentLimit;
	}
	return ItemLimit;
}

inline WTrafficItemRules GetEffectiveRules(WTrafficItemRules const& ParentRules, WTrafficItemRules const& ItemRules)
{
	WTrafficItemRules EffectiveRules = ItemRules;
	EffectiveRules.UploadSwitch =
		GetEffectiveSwitchState(ParentRules.UploadSwitch, ItemRules.UploadSwitch, ItemRules.RuleType);
	EffectiveRules.DownloadSwitch =
		GetEffectiveSwitchState(ParentRules.DownloadSwitch, ItemRules.DownloadSwitch, ItemRules.RuleType);
	EffectiveRules.UploadMark = GetEffectiveMark(ParentRules.UploadMark, ItemRules.UploadMark, ItemRules.RuleType);
	EffectiveRules.DownloadQdiscId =
		GetEffectiveMark(ParentRules.DownloadQdiscId, ItemRules.DownloadQdiscId, ItemRules.RuleType);
	EffectiveRules.DownloadLimit =
		GetEffectiveLimit(ParentRules.DownloadLimit, ItemRules.DownloadLimit, ItemRules.RuleType);
	EffectiveRules.UploadLimit = GetEffectiveLimit(ParentRules.UploadLimit, ItemRules.UploadLimit, ItemRules.RuleType);
	return EffectiveRules;
}

inline bool operator==(WTrafficItemRulesBase const& A, WTrafficItemRules const& B)
{
	return A.UploadSwitch == B.UploadSwitch && A.DownloadSwitch == B.DownloadSwitch && A.UploadMark == B.UploadMark
		&& A.DownloadQdiscId == B.DownloadQdiscId;
}

void WRuleManager::OnSocketConnected(WSocketCounter const* Socket)
{
	std::lock_guard Lock(Mutex);
	auto            ProcessItem = Socket->ParentProcess->TrafficItem->ItemId;
	auto            AppItem = Socket->ParentProcess->ParentApp->TrafficItem->ItemId;

	auto App = Socket->ParentProcess->ParentApp;

	auto              AppRules = ApplicationRules.contains(AppItem) ? ApplicationRules[AppItem] : WTrafficItemRules{};
	auto              ProcRules = ProcessRules.contains(ProcessItem) ? ProcessRules[ProcessItem] : WTrafficItemRules{};
	WTrafficItemRules EffectiveProcRules = GetEffectiveRules(AppRules, ProcRules);

	if (!EffectiveProcRules.IsDefault())
	{
		// todo: As of now sockets will always prefer the limits higher up in the hierarchy
		//       i.e. a socket will never use its own limit if the process or application has one set
		//       this ensures that the higher ranked limit is always enforced but it could technically mean
		//       that a lower ranked limit is exceeded. Ideally we'd set up a hierarchy of the different HTB limits
		//       so that both limits are enforced properly.
		UpdateRuleCache(App->TrafficItem);
		SyncRules();
	}
}

void WRuleManager::OnSocketRemoved(std::shared_ptr<WSocketCounter> const& Socket)
{
	std::lock_guard Lock(Mutex);
#if WDEBUG
	if (SocketRules.contains(Socket->TrafficItem->ItemId) || SocketCookieRules.contains(Socket->TrafficItem->Cookie))
	{
		spdlog::debug(
			"Removing rules for socket ID {} cookie {}", Socket->TrafficItem->ItemId, Socket->TrafficItem->Cookie);
	}
#endif
	SocketRules.erase(Socket->TrafficItem->ItemId);
	SocketCookieRules.erase(Socket->TrafficItem->Cookie);
}

void WRuleManager::OnProcessRemoved(std::shared_ptr<WProcessCounter> const& ProcessItem)
{
	std::lock_guard Lock(Mutex);
#if WDEBUG
	if (ProcessRules.contains(ProcessItem->TrafficItem->ItemId))
	{
		spdlog::debug("Removing rules for process ID {}", ProcessItem->TrafficItem->ItemId);
	}
#endif
	ProcessRules.erase(ProcessItem->TrafficItem->ItemId);
}

void WRuleManager::UpdateRuleCache(std::shared_ptr<ITrafficItem> const& AppItem)
{
	auto App = std::dynamic_pointer_cast<WApplicationItem>(AppItem);
	if (!App)
	{
		return;
	}

	WTrafficItemRules AppRules =
		ApplicationRules.contains(App->ItemId) ? ApplicationRules[App->ItemId] : WTrafficItemRules{};

	for (auto const& Proc : App->Processes | std::views::values)
	{
		auto ProcRules = ProcessRules.contains(Proc->ItemId) ? ProcessRules[Proc->ItemId] : WTrafficItemRules{};
		WTrafficItemRules EffectiveProcRules = GetEffectiveRules(AppRules, ProcRules);

		for (auto const& [Cookie, Sock] : Proc->Sockets)
		{
			auto SockRules = SocketRules.contains(Sock->ItemId) ? SocketRules[Sock->ItemId] : WTrafficItemRules{};
			WTrafficItemRules EffectiveSockRules = GetEffectiveRules(EffectiveProcRules, SockRules);
			SocketRules[Sock->ItemId] = SockRules;

			if (auto SocketCookieRule = SocketCookieRules.find(Cookie); SocketCookieRule != SocketCookieRules.end())
			{
				if (SocketCookieRule->second.Rules == EffectiveSockRules)
				{
					continue;
				}
				SocketCookieRule->second.Rules = EffectiveSockRules.AsBase();
				SocketCookieRule->second.bDirty = true;
			}
			else
			{
				WSocketRules NewRule{};
				NewRule.Rules = EffectiveSockRules.AsBase();
				NewRule.bDirty = true;
				NewRule.SocketId = Sock->ItemId;
				SocketCookieRules[Cookie] = NewRule;
			}
		}
	}
}

void WRuleManager::SyncRules()
{
	auto EbpfData = WDaemon::GetInstance().GetEbpfObj().GetData();
	if (!EbpfData)
	{
		return;
	}

	for (auto& [Cookie, SockRules] : SocketCookieRules)
	{
		if (!SockRules.bDirty)
		{
			continue;
		}

		if (!EbpfData->SocketRules->Update(Cookie, SockRules.Rules))
		{
			spdlog::error("Failed to update eBPF rules for socket cookie {}", Cookie);
		}
		else
		{
			spdlog::debug("Updated eBPF rules for socket cookie {}: UploadSwitch={}, DownloadSwitch={}", Cookie,
				static_cast<int>(SockRules.Rules.UploadSwitch), static_cast<int>(SockRules.Rules.DownloadSwitch));
			SockRules.bDirty = false;
		}
		auto TrafficItem = WSystemMap::GetInstance().GetTrafficItemById(SockRules.SocketId);
		if (TrafficItem)
		{
			auto SocketItem = std::dynamic_pointer_cast<WSocketItem>(TrafficItem);
			if (SocketItem)
			{
				if (SockRules.Rules.DownloadQdiscId == 0)
				{
					WIPLink::GetInstance().RemoveIngressPortRouting(SocketItem->SocketTuple.LocalEndpoint.Port);
				}
				else if (SocketItem->SocketTuple.LocalEndpoint.Port != 0)
				{
					WIPLink::GetInstance().SetupIngressPortRouting(SockRules.SocketId, SockRules.Rules.DownloadQdiscId,
						SocketItem->SocketTuple.LocalEndpoint.Port);
				}
			}
		}
	}
}

void WRuleManager::RemoveEmptyRules()
{
	auto CleanUp = [](std::unordered_map<WTrafficItemId, WTrafficItemRules>& Map) {
		for (auto It = Map.begin(); It != Map.end();)
		{
			if (It->second.IsDefault())
			{
				It = Map.erase(It);
			}
			else
			{
				++It;
			}
		}
	};
	CleanUp(ApplicationRules);
	CleanUp(ProcessRules);
	CleanUp(SocketRules);

	for (auto It = SocketCookieRules.begin(); It != SocketCookieRules.end();)
	{
		if (It->second.Rules.DownloadQdiscId == 0 && It->second.Rules.UploadMark == 0
			&& It->second.Rules.UploadSwitch == SS_None && It->second.Rules.DownloadSwitch == SS_None)
		{
			It = SocketCookieRules.erase(It);
		}
		else
		{
			++It;
		}
	}
}

void WRuleManager::RegisterSignalHandlers()
{
	WNetworkEvents::GetInstance().OnSocketConnected.connect(
		std::bind(&WRuleManager::OnSocketConnected, this, std::placeholders::_1));
	WNetworkEvents::GetInstance().OnSocketRemoved.connect(
		std::bind(&WRuleManager::OnSocketRemoved, this, std::placeholders::_1));
	WNetworkEvents::GetInstance().OnProcessRemoved.connect(
		std::bind(&WRuleManager::OnProcessRemoved, this, std::placeholders::_1));
}

void WRuleManager::HandleRuleChange(WBuffer const& Buf)
{
	WRuleUpdate       Update{};
	std::stringstream ss;
	ss.write(Buf.GetData(), static_cast<long int>(Buf.GetWritePos()));
	try
	{
		{
			ss.seekg(1); // Skip message type
			cereal::BinaryInputArchive iar(ss);
			iar(Update);
		}
	}
	catch (std::exception const& e)
	{
		spdlog::error("Failed to deserialize rule update: {}", e.what());
		return;
	}

	spdlog::info("Rule Change: {}", Update.Rules.ToString());

	std::lock_guard Lock(Mutex);

	auto Item = WSystemMap::GetInstance().GetTrafficItemById(Update.TrafficItemId);
	auto AppItem = WSystemMap::GetInstance().GetTrafficItemById(Update.ParentAppId);

	if (!Item || !AppItem)
	{
		spdlog::warn("Received rule update for unknown traffic item ID {}", Update.TrafficItemId);
		return;
	}

	if (Update.Rules.DownloadLimit == 0)
	{
		auto SocketItem = std::dynamic_pointer_cast<WSocketItem>(Item);
		if (SocketItem)
		{
			WIPLink::GetInstance().RemoveIngressPortRouting(SocketItem->SocketTuple.LocalEndpoint.Port);
		}
		WIPLink::GetInstance().RemoveDownloadLimit(Update.TrafficItemId);
	}
	else
	{
		auto const Limit = WIPLink::GetInstance().GetDownloadLimit(Update.TrafficItemId, Update.Rules.DownloadLimit);
		spdlog::info("Set download limit for traffic item ID {} to {} B/s (class id: {}, mark: {})",
			Update.TrafficItemId, Limit->RateLimit, Limit->MinorId, Limit->Mark);
		Update.Rules.DownloadQdiscId = Limit->MinorId;
	}

	if (Update.Rules.UploadLimit == 0)
	{
		WIPLink::GetInstance().RemoveUploadLimit(Update.TrafficItemId);
	}
	else
	{
		auto const Limit = WIPLink::GetInstance().GetUploadLimit(Update.TrafficItemId, Update.Rules.UploadLimit);
		spdlog::info("Set upload limit for traffic item ID {} to {} B/s (class id: {}, mark: {})", Update.TrafficItemId,
			Limit->RateLimit, Limit->MinorId, Limit->Mark);
		Update.Rules.UploadMark = Limit->Mark;
	}

	switch (Item->GetType())
	{
		case TI_Application:
			ApplicationRules[Update.TrafficItemId] = Update.Rules;
			break;
		case TI_Process:
			ProcessRules[Update.TrafficItemId] = Update.Rules;
			break;
		case TI_Socket:
			SocketRules[Update.TrafficItemId] = Update.Rules;
			break;
		default:;
	}

	UpdateRuleCache(AppItem);
	SyncRules();
	RemoveEmptyRules();
	spdlog::debug("{} app rules, {} proc rules, {} sock rules, {} sock cookie rules in cache", ApplicationRules.size(),
		ProcessRules.size(), SocketRules.size(), SocketCookieRules.size());
	WIPLink::GetInstance().PrintStats();
}