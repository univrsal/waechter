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

enum WSocketRuleLevel : uint8_t
{
	SRL_None = 0,
	SRL_Application = 1,
	SRL_Process = 2,
	SRL_Socket = 3,
};

struct WSocketRulesEntry
{
	WNetworkItemRules Rules{};
	WSocketRuleLevel  Level{ SRL_None };
};

class WRuleManager : public TSingleton<WRuleManager>
{

	std::mutex Mutex;

	// All rules of all levels get flattened into this map and synced to the eBPF map
	// since ultimately all rules are applied at the socket level in eBPF
	std::unordered_map<WSocketCookie, WSocketRulesEntry> SocketRulesMap;

	// we keep track of application and process level rules so that newly created
	// sockets can inherit them
	std::unordered_map<WTrafficItemId, WNetworkItemRules> ApplicationRules;
	std::unordered_map<WTrafficItemId, WNetworkItemRules> ProcessRules;

	void UpdateLocalRuleCache(WRuleUpdate const& Update);
	void SyncWithEbpfMap();
	void HandleSocketRuleUpdate(
		std::shared_ptr<ITrafficItem> Item, WNetworkItemRules const& Rules, WSocketRuleLevel Level = SRL_Socket);
	void HandleProcessRuleUpdate(std::shared_ptr<ITrafficItem> const& Item, WNetworkItemRules const& Rules,
		WSocketRuleLevel Level = SRL_Process);
	void HandleApplicationRuleUpdate(std::shared_ptr<ITrafficItem> const& Item, WNetworkItemRules const& Rules);

	void OnSocketCreated(std::shared_ptr<WSocketCounter> const& Socket);

public:
	void RegisterSignalHandlers();
	void HandleRuleChange(WBuffer const& Buf);
};
