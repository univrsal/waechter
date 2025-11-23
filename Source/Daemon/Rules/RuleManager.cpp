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

inline ESwitchState GetEffectiveSwitchState(ESwitchState ParentState, ESwitchState ItemState)
{
	if (ItemState == SS_None)
	{
		return ParentState;
	}
	return ItemState;
}

inline WNetworkItemRules GetEffectiveRules(WNetworkItemRules const& ParentRules, WNetworkItemRules const& ItemRules)
{
	WNetworkItemRules EffectiveRules{};
	EffectiveRules.UploadSwitch = GetEffectiveSwitchState(ParentRules.UploadSwitch, ItemRules.UploadSwitch);
	EffectiveRules.DownloadSwitch = GetEffectiveSwitchState(ParentRules.DownloadSwitch, ItemRules.DownloadSwitch);
	return EffectiveRules;
}

inline bool IsRuleDefault(WNetworkItemRules const& Rules)
{
	return Rules.UploadSwitch == SS_None && Rules.DownloadSwitch == SS_None;
}

inline bool operator==(WNetworkItemRules const& A, WNetworkItemRules const& B)
{
	return A.UploadSwitch == B.UploadSwitch && A.DownloadSwitch == B.DownloadSwitch;
}

void WRuleManager::OnSocketCreated(std::shared_ptr<WSocketCounter> const& Socket)
{
	std::lock_guard Lock(Mutex);
	auto            ProcessItem = Socket->ParentProcess->TrafficItem->ItemId;
	auto            AppItem = Socket->ParentProcess->ParentApp->TrafficItem->ItemId;

	auto              AppRules = ApplicationRules.contains(AppItem) ? ApplicationRules[AppItem] : WNetworkItemRules{};
	auto              ProcRules = ProcessRules.contains(ProcessItem) ? ProcessRules[ProcessItem] : WNetworkItemRules{};
	WNetworkItemRules EffectiveProcRules = GetEffectiveRules(AppRules, ProcRules);

	if (!IsRuleDefault(EffectiveProcRules))
	{
		SocketRules[Socket->TrafficItem->ItemId] = ProcRules;
		SocketCookieRules[Socket->TrafficItem->Cookie] = WSocketRules{ .Rules = EffectiveProcRules, .bDirty = true };
	}
	// UpdateRuleCache(Socket->ParentProcess->ParentApp->TrafficItem);
	SyncToEBPF();
}

void WRuleManager::UpdateRuleCache(std::shared_ptr<ITrafficItem> const& AppItem)
{
	auto App = std::dynamic_pointer_cast<WApplicationItem>(AppItem);
	if (!App)
	{
		return;
	}

	WNetworkItemRules AppRules =
		ApplicationRules.contains(App->ItemId) ? ApplicationRules[App->ItemId] : WNetworkItemRules{};

	for (auto const& Proc : App->Processes | std::views::values)
	{
		auto ProcRules = ProcessRules.contains(Proc->ItemId) ? ProcessRules[Proc->ItemId] : WNetworkItemRules{};
		WNetworkItemRules EffectiveProcRules = GetEffectiveRules(AppRules, ProcRules);

		for (auto const& [Cookie, Sock] : Proc->Sockets)
		{
			auto SockRules = SocketRules.contains(Sock->ItemId) ? SocketRules[Sock->ItemId] : WNetworkItemRules{};
			WNetworkItemRules EffectiveSockRules = GetEffectiveRules(EffectiveProcRules, SockRules);

			if (auto SocketCookieRule = SocketCookieRules.find(Cookie); SocketCookieRule != SocketCookieRules.end())
			{
				if (SocketCookieRule->second.Rules == EffectiveSockRules)
				{
					continue;
				}
				SocketCookieRule->second.Rules = EffectiveSockRules;
				SocketCookieRule->second.bDirty = true;
			}
			else
			{
				WSocketRules NewRule{};
				NewRule.Rules = EffectiveSockRules;
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