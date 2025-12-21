/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <unordered_map>
#include <mutex>
#include <memory>

#include "Buffer.hpp"
#include "Data/Rule.hpp"
#include "Singleton.hpp"
#include "Data/RuleUpdate.hpp"
#include "Data/TrafficItem.hpp"

struct WSocketCounter;
struct WProcessCounter;

struct WSocketRules
{
	WTrafficItemRulesBase Rules;
	WTrafficItemId        SocketId{};

	bool bDirty{ true };
};

class WRuleManager : public TSingleton<WRuleManager>
{
	std::mutex Mutex;

	std::unordered_map<WTrafficItemId, WTrafficItemRules> ApplicationRules;
	std::unordered_map<WTrafficItemId, WTrafficItemRules> ProcessRules;
	std::unordered_map<WTrafficItemId, WTrafficItemRules> SocketRules;

	std::unordered_map<WSocketCookie, WSocketRules> SocketCookieRules;

	void OnSocketCreated(std::shared_ptr<WSocketCounter> const& Socket);
	void OnSocketRemoved(std::shared_ptr<WSocketCounter> const& Socket);
	void OnProcessRemoved(std::shared_ptr<WProcessCounter> const& ProcessItem);

	void UpdateRuleCache(std::shared_ptr<ITrafficItem> const& AppItem);

	void SyncRules();

public:
	void RegisterSignalHandlers();
	void HandleRuleChange(WBuffer const& Buf);
};
