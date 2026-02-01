/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "MemoryUsage.hpp"

#include "Daemon.hpp"
#include "Data/AppIconAtlasBuilder.hpp"
#include "Data/ConnectionHistory.hpp"
#include "Data/SystemMap.hpp"
#include "Net/IPLink.hpp"
#include "Net/Resolver.hpp"
#include "Rules/RuleManager.hpp"

WMemoryStats WMemoryUsage::GetMemoryStats()
{
	auto const RuleManagerStats = WRuleManager::GetInstance().GetMemoryUsage();
	auto const ResolverStats = WResolver::GetInstance().GetMemoryUsage();
	auto const IPLinkStats = WIPLink::GetInstance().GetMemoryUsage();

	auto&      SysMap = WSystemMap::GetInstance();
	auto const SystemMapUsage = SysMap.GetMemoryUsage();

	SysMap.DataMutex.lock();
	auto const MapUpdateUsage = SysMap.GetMapUpdate().GetMemoryUsage();
	SysMap.DataMutex.unlock();

	auto const ConnectionHistoryStats = WConnectionHistory::GetInstance().GetMemoryUsage();
	auto const IconResolverStats = WAppIconAtlasBuilder::GetInstance().GetResolver().GetMemoryUsage();
	auto const DaemonStats = WDaemon::GetInstance().GetMemoryUsage();

	WMemoryStats Stats{};
	Stats.Stats.push_back(RuleManagerStats);
	Stats.Stats.push_back(ResolverStats);
	Stats.Stats.push_back(IPLinkStats);
	Stats.Stats.push_back(SystemMapUsage);
	Stats.Stats.push_back(MapUpdateUsage);
	Stats.Stats.push_back(ConnectionHistoryStats);
	Stats.Stats.push_back(IconResolverStats);
	Stats.Stats.push_back(DaemonStats);
	return Stats;
}