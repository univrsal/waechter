/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>

#include "Types.hpp"
#include "MemoryStats.hpp"
#include "Singleton.hpp"
#include "IPAddress.hpp"
#include "Promise.hpp"
#include "Data/TrafficItem.hpp"
#include "Data/Stats.hpp"

class WStatsManager final : public TSingleton<WStatsManager>, public IMemoryTrackable
{
	struct WQueuedRequest
	{
		WStatsRequest                   Request;
		TPromise<WStatsResponse const&> Promise;
	};

	std::mutex DataMutex;
	std::thread                RequestThread;
	std::atomic<bool>          bRunning{ false };
	std::mutex                 QueueMutex;
	std::condition_variable    QueueCondition;
	std::queue<WQueuedRequest> PendingRequests;

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

	static void ProcessStatsRequest(WStatsRequest const& Request, WStatsResponse& Response);

	void RequestThreadFunction();

public:
	WStatsManager() = default;
	~WStatsManager() override = default;

	std::mutex& GetDataMutex() { return DataMutex; }

	WMemoryStat GetMemoryUsage() override;

	void UpdateAppStats(WTrafficItemId AppId, WIPAddress const& RemoteHost, WBytes In, WBytes Out);

	void MakeSnapshot();

	void StartRequestProcessThread();
	void StopRequestProcessThread();

	TPromise<WStatsResponse const&> RequestStats(WStatsRequest const& Request);
};