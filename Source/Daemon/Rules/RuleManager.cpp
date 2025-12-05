//
// Created by usr on 20/11/2025.
//

#include "RuleManager.hpp"

#include <spdlog/spdlog.h>
#include <cereal/archives/binary.hpp>

#include "Daemon.hpp"
#include "EbpfData.hpp"
#include "Data/RuleUpdate.hpp"
#include "Data/SystemMap.hpp"
#include "Data/NetworkEvents.hpp"
#include "Net/IPLink.hpp"

inline ESwitchState GetEffectiveSwitchState(ESwitchState ParentState, ESwitchState ItemState)
{
	if (ItemState == SS_None)
	{
		return ParentState;
	}
	return ItemState;
}

inline uint32_t GetEffectiveMark(uint32_t ParentMark, uint32_t ItemMark)
{
	if (ItemMark == 0)
	{
		return ParentMark;
	}
	return ItemMark;
}

inline WTrafficItemRules GetEffectiveRules(WTrafficItemRules const& ParentRules, WTrafficItemRules const& ItemRules)
{
	WTrafficItemRules EffectiveRules = ItemRules;
	EffectiveRules.UploadSwitch = GetEffectiveSwitchState(ParentRules.UploadSwitch, ItemRules.UploadSwitch);
	EffectiveRules.DownloadSwitch = GetEffectiveSwitchState(ParentRules.DownloadSwitch, ItemRules.DownloadSwitch);
	EffectiveRules.UploadMark = GetEffectiveMark(ParentRules.UploadMark, ItemRules.UploadMark);
	EffectiveRules.DownloadMark = GetEffectiveMark(ParentRules.DownloadMark, ItemRules.DownloadMark);
	return EffectiveRules;
}

inline bool IsRuleDefault(WTrafficItemRules const& Rules)
{
	return Rules.UploadSwitch == SS_None && Rules.DownloadSwitch == SS_None;
}

inline bool operator==(WTrafficItemRulesBase const& A, WTrafficItemRules const& B)
{
	return A.UploadSwitch == B.UploadSwitch && A.DownloadSwitch == B.DownloadSwitch;
}

void WRuleManager::OnSocketCreated(std::shared_ptr<WSocketCounter> const& Socket)
{
	std::lock_guard Lock(Mutex);
	auto            ProcessItem = Socket->ParentProcess->TrafficItem->ItemId;
	auto            AppItem = Socket->ParentProcess->ParentApp->TrafficItem->ItemId;

	auto              AppRules = ApplicationRules.contains(AppItem) ? ApplicationRules[AppItem] : WTrafficItemRules{};
	auto              ProcRules = ProcessRules.contains(ProcessItem) ? ProcessRules[ProcessItem] : WTrafficItemRules{};
	WTrafficItemRules EffectiveProcRules = GetEffectiveRules(AppRules, ProcRules);

	if (!IsRuleDefault(EffectiveProcRules))
	{
		SocketRules[Socket->TrafficItem->ItemId] = ProcRules;
		SocketCookieRules[Socket->TrafficItem->Cookie] =
			WSocketRules{ .Rules = EffectiveProcRules.AsBase(), .bDirty = true };
		SyncToEBPF();
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
				SocketCookieRules[Cookie] = NewRule;
			}
		}
	}
}

void WRuleManager::SyncToEBPF()
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
			spdlog::info("Updated eBPF rules for socket cookie {}: UploadSwitch={}, DownloadSwitch={}", Cookie,
				static_cast<int>(SockRules.Rules.UploadSwitch), static_cast<int>(SockRules.Rules.DownloadSwitch));
			SockRules.bDirty = false;
		}
	}
}

void WRuleManager::RegisterSignalHandlers()
{
	WNetworkEvents::GetInstance().OnSocketCreated.connect(
		std::bind(&WRuleManager::OnSocketCreated, this, std::placeholders::_1));
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
	{
		ss.seekg(1); // Skip message type
		cereal::BinaryInputArchive iar(ss);
		iar(Update);
	}

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
			spdlog::info("Got application rule update for ID {}", Update.TrafficItemId);
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
	SyncToEBPF();
}