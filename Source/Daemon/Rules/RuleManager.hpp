//
// Created by usr on 20/11/2025.
//

#pragma once
#include "Buffer.hpp"
#include "EBPFCommon.h"

#include <unordered_map>
#include <mutex>
#include <memory>

#include "Singleton.hpp"
#include "Data/RuleUpdate.hpp"
#include "Data/TrafficItem.hpp"

struct WSocketCounter;
struct WProcessCounter;

struct WSocketRules
{
	WNetworkItemRules Rules;

	bool bDirty{ true };
};

class WRuleManager : public TSingleton<WRuleManager>
{
	std::mutex Mutex;

	std::unordered_map<WTrafficItemId, WNetworkItemRules> ApplicationRules;
	std::unordered_map<WTrafficItemId, WNetworkItemRules> ProcessRules;
	std::unordered_map<WTrafficItemId, WNetworkItemRules> SocketRules;

	std::unordered_map<WSocketCookie, WSocketRules> SocketCookieRules;

	void OnSocketCreated(std::shared_ptr<WSocketCounter> const& Socket);
	void OnSocketRemoved(std::shared_ptr<WSocketCounter> const& Socket);
	void OnProcessRemoved(std::shared_ptr<WProcessCounter> const& ProcessItem);

	void UpdateRuleCache(std::shared_ptr<ITrafficItem> const& AppItem);

	void SyncToEBPF();

public:
	void RegisterSignalHandlers();
	void HandleRuleChange(WBuffer const& Buf);
};
