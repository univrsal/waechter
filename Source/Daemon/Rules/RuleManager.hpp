/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <unordered_map>
#include <mutex>
#include <memory>

#include "MemoryStats.hpp"
#include "Buffer.hpp"
#include "Data/Rule.hpp"
#include "Singleton.hpp"
#include "Data/TrafficItem.hpp"

struct WSocketCounter;
struct WProcessCounter;
struct WRuleUpdate;
struct WAppCounter;
class WApplicationItem;
class WDaemonClient;

struct WSocketRules
{
	WTrafficItemRulesBase Rules;
	WTrafficItemId        SocketId{};

	bool bDirty{ true };
};

class WRuleManager : public TSingleton<WRuleManager>, public IMemoryTrackable
{
	std::mutex Mutex;

	std::unordered_map<WTrafficItemId, WTrafficItemRules> ApplicationRules;
	std::unordered_map<WTrafficItemId, WTrafficItemRules> ProcessRules;
	std::unordered_map<WTrafficItemId, WTrafficItemRules> SocketRules;

	std::unordered_map<WSocketCookie, WSocketRules> SocketCookieRules;

	void OnSocketConnected(WSocketCounter const* Socket);
	void OnSocketRemoved(std::shared_ptr<WSocketCounter> const& Socket);
	void OnProcessRemoved(std::shared_ptr<WProcessCounter> const& ProcessItem);
	void OnAppFirstTimeConnected(std::shared_ptr<WAppCounter> const& App);

	void UpdateRuleCache(std::shared_ptr<ITrafficItem> const& AppItem);
	void SyncRules();
	void RemoveEmptyRules();
	static void WriteAppRuleToDb(WRuleUpdate const& Update, std::shared_ptr<WApplicationItem> const& App);

public:
	void SendCurrentRulesToClient(std::shared_ptr<WDaemonClient> const& Client);

	void RegisterSignalHandlers();
	void HandleRuleChange(WBuffer const& Buf, WDaemonClient const* Sender);

	WMemoryStat GetMemoryUsage() override;
};
