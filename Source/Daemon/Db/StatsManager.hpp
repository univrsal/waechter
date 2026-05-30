/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <unordered_map>
#include <mutex>

#include "Types.hpp"
#include "MemoryStats.hpp"
#include "Singleton.hpp"
#include "IPAddress.hpp"
#include "Data/TrafficItem.hpp"

class WStatsManager final : public TSingleton<WStatsManager>, public IMemoryTrackable
{
	std::mutex DataMutex;
	struct WTrafficStats
	{
		WBytes BytesIn;
		WBytes BytesOut;
	};

	struct WAppStats
	{
		std::unordered_map<WIPAddress, WTrafficStats> Traffic;
	};

	struct WTrafficSnapshot
	{
		std::unordered_map<WTrafficItemId, WAppStats> Apps;
	};

	WTrafficSnapshot CurrentSnapshot{};

public:
	WStatsManager() = default;
	~WStatsManager() override = default;

	std::mutex& GetDataMutex() { return DataMutex; }

	WMemoryStat GetMemoryUsage() override;

	void UpdateAppStats(WTrafficItemId AppId, WIPAddress const& RemoteHost, WBytes In, WBytes Out);

	void MakeSnapshot();
};
