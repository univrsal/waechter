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

	std::unordered_map<WTrafficItemId, WSocketRules> Rules;

public:
	WSocketRules& GetOrCreateRules(WTrafficItemId TrafficItemId)
	{
		std::lock_guard lock(Mutex);
		return Rules[TrafficItemId];
	}
	static void SendRuleStateUpdate(WTrafficItemId TrafficItemId, WSocketRules const& ChangedRule);
};
