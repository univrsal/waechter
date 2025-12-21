/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ClientRuleManager.hpp"

#include "Data/RuleUpdate.hpp"

void WClientRuleManager::SendRuleStateUpdate(
	WTrafficItemId TrafficItemId, WTrafficItemId ParentApp, WTrafficItemRules const& ChangedRule)
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