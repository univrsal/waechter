//
// Created by usr on 20/11/2025.
//

#pragma once
#include "Buffer.hpp"
#include "EBPFCommon.h"

#include <unordered_map>
#include <mutex>

#include "Singleton.hpp"
#include "Data/TrafficItem.hpp"

class WRuleManager : public TSingleton<WRuleManager>
{
	std::mutex Mutex;

	std::unordered_map<WTrafficItemId, WSocketRules> Rules;

public:
	WSocketRules& GetOrCreateRules(WTrafficItemId TrafficItemId)
	{
		std::lock_guard lock(Mutex);
		return Rules[TrafficItemId];
	}

	void HandleRuleChange(WBuffer const& Buf);
};
