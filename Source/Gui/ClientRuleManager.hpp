//
// Created by usr on 20/11/2025.
//

#pragma once

#include "Client.hpp"

#include <unordered_map>
#include <mutex>

#include "EBPFCommon.h"
#include "Singleton.hpp"
#include "Data/TrafficItem.hpp"

class WClientRuleManager : public TSingleton<WClientRuleManager>
{
	std::mutex Mutex;

	std::unordered_map<WTrafficItemId, WNetworkItemRules> Rules;

public:
	WNetworkItemRules& GetOrCreateRules(WTrafficItemId TrafficItemId)
	{
		std::lock_guard lock(Mutex);
		return Rules[TrafficItemId];
	}

	bool HasRules(WTrafficItemId TrafficItemId)
	{
		std::lock_guard lock(Mutex);
		return Rules.find(TrafficItemId) != Rules.end();
	}

	static void SendRuleStateUpdate(
		WTrafficItemId TrafficItemId, WTrafficItemId ParentApp, WNetworkItemRules const& ChangedRule);

	void RemoveRules(WTrafficItemId TrafficItemId)
	{
		std::lock_guard lock(Mutex);
		Rules.erase(TrafficItemId);
	}
};
