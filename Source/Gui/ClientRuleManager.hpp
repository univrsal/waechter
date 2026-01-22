/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <unordered_map>
#include <mutex>

#include "Client.hpp"
#include "Singleton.hpp"
#include "Data/TrafficItem.hpp"
#include "Data/Rule.hpp"

class WClientRuleManager : public TSingleton<WClientRuleManager>
{
	std::mutex Mutex;

	std::unordered_map<WTrafficItemId, WTrafficItemRules> Rules;

public:
	WTrafficItemRules& GetOrCreateRules(WTrafficItemId TrafficItemId)
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
		WTrafficItemId TrafficItemId, WTrafficItemId ParentApp, WTrafficItemRules const& ChangedRule);

	void RemoveRules(WTrafficItemId TrafficItemId)
	{
		std::lock_guard lock(Mutex);
		Rules.erase(TrafficItemId);
	}
};
