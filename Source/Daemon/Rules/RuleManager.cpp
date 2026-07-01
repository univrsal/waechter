/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "RuleManager.hpp"

#include "spdlog/spdlog.h"
#include "sqlpp11/sqlpp11.h"

#include "Daemon.hpp"
#include "Messages.hpp"
#include "EBPF/EbpfData.hpp"
#include "Data/RuleUpdate.hpp"
#include "Data/SystemMap.hpp"
#include "Data/NetworkEvents.hpp"
#include "Data/ApplicationItem.hpp"
#include "Data/ProcessItem.hpp"
#include "Db/DbManager.hpp"
#include "Db/Schema.hpp"
#include "Net/IPLink.hpp"

inline ESwitchState GetEffectiveSwitchState(
	ESwitchState const ParentState, ESwitchState const ItemState, ERuleType const ItemRuleType)
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

inline uint32_t GetEffectiveMark(uint32_t const ParentMark, uint32_t const ItemMark, ERuleType const ItemRuleType)
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

inline WBytesPerSecond GetEffectiveLimit(
	WBytesPerSecond const ParentLimit, WBytesPerSecond const ItemLimit, ERuleType const ItemRuleType)
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
	EffectiveRules.DownloadMark =
		GetEffectiveMark(ParentRules.DownloadMark, ItemRules.DownloadMark, ItemRules.RuleType);
	EffectiveRules.DownloadLimit =
		GetEffectiveLimit(ParentRules.DownloadLimit, ItemRules.DownloadLimit, ItemRules.RuleType);
	EffectiveRules.UploadLimit = GetEffectiveLimit(ParentRules.UploadLimit, ItemRules.UploadLimit, ItemRules.RuleType);
	return EffectiveRules;
}

inline bool operator==(WTrafficItemRulesBase const& A, WTrafficItemRules const& B)
{
	return A.UploadSwitch == B.UploadSwitch && A.DownloadSwitch == B.DownloadSwitch && A.UploadMark == B.UploadMark
		&& A.DownloadMark == B.DownloadMark;
}

void WRuleManager::OnSocketConnected(WSocketCounter const* Socket)
{
	std::lock_guard Lock(Mutex);
	auto const      ProcessItemId = Socket->ParentProcess->TrafficItem->ItemId;
	auto const      AppItem = Socket->ParentProcess->ParentApp->TrafficItem->ItemId;
	auto const      ProcessPid = Socket->ParentProcess->TrafficItem->ProcessId;

	auto const App = Socket->ParentProcess->ParentApp;

	auto const AppRules = ApplicationRules.contains(AppItem) ? ApplicationRules[AppItem] : WTrafficItemRules{};
	auto const ProcRules = ProcessRules.contains(ProcessItemId) ? ProcessRules[ProcessItemId] : WTrafficItemRules{};
	WTrafficItemRules const EffectiveProcRules = GetEffectiveRules(AppRules, ProcRules);

	if (!EffectiveProcRules.IsDefault())
	{
		// todo: As of now sockets will always prefer the limits higher up in the hierarchy
		//       i.e. a socket will never use its own limit if the process or application has one set
		//       this ensures that the higher-ranked limit is always enforced but it could technically mean
		//       that a lower ranked limit is exceeded. Ideally we'd set up a hierarchy of the different HTB limits
		//       so that both limits are enforced properly.
		UpdateRuleCache(App->TrafficItem);
		SyncRules();

		// Ensure the PID mark is set for this process if there's a download limit.
		// This enables immediate rate limiting for new sockets from this process
		if (EffectiveProcRules.DownloadMark != 0)
		{
			WIPLink::SetPidDownloadMark(static_cast<uint32_t>(ProcessPid), EffectiveProcRules.DownloadMark);
		}
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

	// Clean up PID download mark
	WIPLink::RemovePidDownloadMark(static_cast<uint32_t>(ProcessItem->TrafficItem->ProcessId));
}

void WRuleManager::OnAppFirstTimeConnected(std::shared_ptr<WAppCounter> const& App)
{
	WDbManager::GetInstance().Run([&](auto& DbConn) {
		constexpr Db::Schema::Rule Rule;
		auto                       RuleResult =
			DbConn(sqlpp::select(Rule.DownloadLimit, Rule.UploadLimit, Rule.DownloadSwitchState, Rule.UploadSwitchState)
					.from(Rule)
					.where(Rule.Target == App->TrafficItem->ApplicationPath));

		if (!RuleResult.empty())
		{
			spdlog::debug("Loading rule for {}", App->TrafficItem->ApplicationPath);
			WTrafficItemRules Rules;
			Rules.DownloadSwitch = static_cast<ESwitchState>(static_cast<int>(RuleResult.front().DownloadSwitchState));
			Rules.UploadSwitch = static_cast<ESwitchState>(static_cast<int>(RuleResult.front().UploadSwitchState));
			Rules.RuleType = ERuleType::Explicit;
			Rules.DownloadLimit = RuleResult.front().DownloadLimit;
			Rules.UploadLimit = RuleResult.front().UploadLimit;

			if (Rules.DownloadLimit > 0)
			{
				auto const Limit =
					WIPLink::GetInstance().GetDownloadLimit(App->TrafficItem->ItemId, Rules.DownloadLimit);
				Rules.DownloadMark = Limit->Mark;
			}
			if (Rules.UploadLimit > 0)
			{
				auto const Limit = WIPLink::GetInstance().GetUploadLimit(App->TrafficItem->ItemId, Rules.UploadLimit);
				Rules.UploadMark = Limit->Mark;
			}

			if (!Rules.IsDefault())
			{
				std::lock_guard Lock(Mutex);
				ApplicationRules[App->TrafficItem->ItemId] = Rules;
				UpdateRuleCache(App->TrafficItem);
				SyncRules();
			}
		}
	});
}

void WRuleManager::UpdateRuleCache(std::shared_ptr<ITrafficItem> const& AppItem)
{
	auto const App = std::dynamic_pointer_cast<WApplicationItem>(AppItem);
	if (!App)
	{
		return;
	}

	WTrafficItemRules const AppRules =
		ApplicationRules.contains(App->ItemId) ? ApplicationRules[App->ItemId] : WTrafficItemRules{};

	for (auto const& Proc : App->Processes | std::views::values)
	{
		auto const ProcRules = ProcessRules.contains(Proc->ItemId) ? ProcessRules[Proc->ItemId] : WTrafficItemRules{};
		WTrafficItemRules const EffectiveProcRules = GetEffectiveRules(AppRules, ProcRules);

		for (auto const& [Cookie, Sock] : Proc->Sockets)
		{
			auto const SockRules = SocketRules.contains(Sock->ItemId) ? SocketRules[Sock->ItemId] : WTrafficItemRules{};
			WTrafficItemRules const EffectiveSockRules = GetEffectiveRules(EffectiveProcRules, SockRules);
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
	auto const EbpfData = WDaemon::GetInstance().GetEbpfObj().GetData();
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
		if (auto TrafficItem = WSystemMap::GetInstance().GetTrafficItemById(SockRules.SocketId))
		{
			if (auto const SocketItem = std::dynamic_pointer_cast<WSocketItem>(TrafficItem))
			{
				if (SockRules.Rules.DownloadMark == 0)
				{
					WIPLink::RemoveIngressPortRouting(SocketItem->SocketTuple.LocalEndpoint.Port);
				}
				else if (SocketItem->SocketTuple.LocalEndpoint.Port != 0)
				{
					WIPLink::SetupIngressPortRouting(
						SockRules.SocketId, SockRules.Rules.DownloadMark, SocketItem->SocketTuple.LocalEndpoint.Port);
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
		if (It->second.Rules.DownloadMark == 0 && It->second.Rules.UploadMark == 0
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

void WRuleManager::WriteAppRuleToDb(WRuleUpdate const& Update, std::shared_ptr<WApplicationItem> const& App)
{
	WDbManager::GetInstance().Run([&](auto& DbConn) {
		constexpr Db::Schema::Rule Rule;
		auto RuleResult = DbConn(sqlpp::select(Rule.ID).from(Rule).where(Rule.Target == App->ApplicationPath));

		if (RuleResult.empty() && !Update.Rules.IsDefault()) // insert the rule if it's not empty
		{
			DbConn(sqlpp::insert_into(Rule).set(Rule.Target = App->ApplicationPath,
				Rule.DownloadLimit = Update.Rules.DownloadLimit, Rule.UploadLimit = Update.Rules.UploadLimit,
				Rule.DownloadSwitchState = static_cast<int>(Update.Rules.DownloadSwitch),
				Rule.UploadSwitchState = static_cast<int>(Update.Rules.UploadSwitch)));
		}
		else
		{
			int64_t const RuleID = RuleResult.front().ID.value();

			if (Update.Rules.IsDefault())
			{
				DbConn(sqlpp::remove_from(Rule).where(Rule.ID == RuleID));
			}
			else
			{
				DbConn(sqlpp::update(Rule)
						.set(Rule.DownloadLimit = Update.Rules.DownloadLimit,
							Rule.UploadLimit = Update.Rules.UploadLimit,
							Rule.DownloadSwitchState = static_cast<int>(Update.Rules.DownloadSwitch),
							Rule.UploadSwitchState = static_cast<int>(Update.Rules.UploadSwitch))
						.where(Rule.ID == RuleID));
			}
		}
	});
}

void WRuleManager::SendCurrentRulesToClient(std::shared_ptr<WDaemonClient> const& Client)
{
	std::scoped_lock Lock(Mutex);

	auto SendMap = [Client](std::unordered_map<WTrafficItemId, WTrafficItemRules> const& Map) {
		for (auto const& [TrafficItemId, Rules] : Map)
		{
			WRuleUpdate Update{};
			Update.TrafficItemId = TrafficItemId;
			Update.ParentAppId = 0; // unused
			Update.Rules = Rules;
			Client->SendMessage(MT_RuleUpdate, Update);
		}
	};
	SendMap(ApplicationRules);
	SendMap(ProcessRules);
	SendMap(SocketRules);
}

void WRuleManager::RegisterSignalHandlers()
{
	WNetworkEvents::GetInstance().OnSocketConnected.connect(
		[this](WSocketCounter const* Socket) { OnSocketConnected(Socket); });
	WNetworkEvents::GetInstance().OnSocketRemoved.connect(
		[this](std::shared_ptr<WSocketCounter> const& Socket) { OnSocketRemoved(Socket); });
	WNetworkEvents::GetInstance().OnProcessRemoved.connect(
		[this](std::shared_ptr<WProcessCounter> const& Process) { OnProcessRemoved(Process); });
	WNetworkEvents::GetInstance().OnAppFirstTimeConnected.connect(
		[this](std::shared_ptr<WAppCounter> const& App) { OnAppFirstTimeConnected(App); });
}

void WRuleManager::HandleRuleChange(WBuffer const& Buf, WDaemonClient const* Sender)
{
	WRuleUpdate Update{};
	if (!DeserializeMessage(Buf, Update))
	{
		spdlog::error("Failed to deserialize rule update");
		return;
	}

	spdlog::info("Rule Change for {}: {}", Update.TrafficItemId, Update.Rules.ToString());

	std::lock_guard Lock(Mutex);

	auto const Item = WSystemMap::GetInstance().GetTrafficItemById(Update.TrafficItemId);
	auto const AppItem = WSystemMap::GetInstance().GetTrafficItemById(Update.ParentAppId);

	if (!Item || !AppItem)
	{
		spdlog::warn("Received rule update for unknown traffic item ID {}", Update.TrafficItemId);
		return;
	}

	if (Update.Rules.DownloadLimit == 0)
	{
		if (auto const SocketItem = std::dynamic_pointer_cast<WSocketItem>(Item))
		{
			WIPLink::RemoveIngressPortRouting(SocketItem->SocketTuple.LocalEndpoint.Port);
		}
		WIPLink::GetInstance().RemoveDownloadLimit(Update.TrafficItemId);
	}
	else
	{
		auto const Limit = WIPLink::GetInstance().GetDownloadLimit(Update.TrafficItemId, Update.Rules.DownloadLimit);
		spdlog::info("Set download limit for traffic item ID {} to {} B/s (class id: {}, mark: {})",
			Update.TrafficItemId, Limit->RateLimit, Limit->MinorId, Limit->Mark);
		Update.Rules.DownloadMark = Limit->Mark;
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
		{
			ApplicationRules[Update.TrafficItemId] = Update.Rules;
			// Set PID download marks for all processes under this application
			auto const AppItemCast = std::dynamic_pointer_cast<WApplicationItem>(Item);
			if (AppItemCast && Update.Rules.DownloadMark != 0)
			{
				for (auto const& Pid : AppItemCast->Processes | std::views::keys)
				{
					WIPLink::SetPidDownloadMark(static_cast<uint32_t>(Pid), Update.Rules.DownloadMark);
				}
			}
			else if (AppItemCast && Update.Rules.DownloadLimit == 0)
			{
				// Remove PID marks when limit is removed
				for (auto const& Pid : AppItemCast->Processes | std::views::keys)
				{
					WIPLink::RemovePidDownloadMark(static_cast<uint32_t>(Pid));
				}
			}
			WriteAppRuleToDb(Update, AppItemCast);
			break;
		}
		case TI_Process:
		{
			ProcessRules[Update.TrafficItemId] = Update.Rules;
			// Set PID download mark for this specific process
			auto const ProcessItem = std::dynamic_pointer_cast<WProcessItem>(Item);
			if (ProcessItem && Update.Rules.DownloadMark != 0)
			{
				WIPLink::SetPidDownloadMark(static_cast<uint32_t>(ProcessItem->ProcessId), Update.Rules.DownloadMark);
			}
			else if (ProcessItem && Update.Rules.DownloadLimit == 0)
			{
				WIPLink::RemovePidDownloadMark(static_cast<uint32_t>(ProcessItem->ProcessId));
			}
			break;
		}
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

	WDaemon::GetInstance().GetDaemonSocket()->BroadcastMessage(MT_RuleUpdate, Update, Sender);
}

WMemoryStat WRuleManager::GetMemoryUsage()
{
	std::scoped_lock Lock(Mutex);

	WMemoryStat Stats;
	Stats.Name = "WRuleManager";

	Stats.ChildEntries.emplace_back(WMemoryStatEntry{
		.Name = "ApplicationRules", .Usage = CALC_MAP_USAGE(ApplicationRules, WTrafficItemId, WTrafficItemRules) });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{
		.Name = "ProcessRules", .Usage = CALC_MAP_USAGE(ProcessRules, WTrafficItemId, WTrafficItemRules) });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{
		.Name = "SocketRules", .Usage = CALC_MAP_USAGE(SocketRules, WTrafficItemId, WTrafficItemRules) });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{
		.Name = "SocketCookieRules", .Usage = CALC_MAP_USAGE(SocketCookieRules, WSocketCookie, WSocketRules) });

	return Stats;
}