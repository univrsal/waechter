//
// Created by usr on 20/11/2025.
//

#include "ClientRuleManager.hpp"

#include "Data/RuleUpdate.hpp"

void WClientRuleManager::SendRuleStateUpdate(WTrafficItemId TrafficItemId, WNetworkItemRules const& ChangedRule)
{
	if (!WClient::GetInstance().IsConnected())
	{
		return;
	}

	WClient::GetInstance().SendMessage(MT_RuleUpdate, WRuleUpdate{ ChangedRule, TrafficItemId });
}