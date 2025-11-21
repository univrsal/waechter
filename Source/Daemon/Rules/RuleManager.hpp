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

enum WSocketRuleLevel : uint8_t
{
	SRL_None = 0,
	SRL_Application = 1,
	SRL_Process = 2,
	SRL_Socket = 3,
};

struct WSocketRulesEntry
{
	WSocketRules     Rules;
	WSocketRuleLevel Level{ SRL_None };
};

class WRuleManager : public TSingleton<WRuleManager>
{

	std::mutex Mutex;

	std::unordered_map<WSocketCookie, WSocketRulesEntry> Rules;

	void UpdateLocalRuleCache(WRuleUpdate const& Update);
	void SyncWithEbpfMap();
	void HandleSocketRuleUpdate(
		std::shared_ptr<ITrafficItem> Item, WRuleUpdate const& Update, WSocketRuleLevel Level = SRL_Socket);
	void HandleProcessRuleUpdate(
		std::shared_ptr<ITrafficItem> Item, WRuleUpdate const& Update, WSocketRuleLevel Level = SRL_Process);
	void HandleApplicationRuleUpdate(std::shared_ptr<ITrafficItem> Item, WRuleUpdate const& Update);

public:
	void HandleRuleChange(WBuffer const& Buf);
};
