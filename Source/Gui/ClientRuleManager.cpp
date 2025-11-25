//
// Created by usr on 20/11/2025.
//

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