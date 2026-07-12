/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ClientRuleManager.hpp"

#include "Data/RuleUpdate.hpp"

void WClientRuleManager::HandleRuleUpdate(WBuffer const& Buf)
{
	WRuleUpdate Update{};
	if (WClient::ReadMessage(Buf, Update) == false)
	{
		spdlog::error("Failed to read rule update");
		return;
	}
	std::scoped_lock Lock(Mutex);
	Rules[Update.TrafficItemId] = Update.Rules;
}

void WClientRuleManager::SendRuleStateUpdate(
	WTrafficItemId const TrafficItemId, WTrafficItemId const ParentApp, WTrafficItemRules const& ChangedRule)
{
	if (!WClient::GetInstance().IsConnected())
	{
		return;
	}
	WRuleUpdate Update{};
	Update.TrafficItemId = TrafficItemId;
	Update.ParentAppId = ParentApp;
	Update.Rules = ChangedRule;
	WClient::GetInstance().SendMessage(MT_RuleUpdate, Update);
}