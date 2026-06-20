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
#include <string>

#include "Types.hpp"
#include "MemoryStats.hpp"
#include "Singleton.hpp"
#include "IPAddress.hpp"
#include "Messages.hpp"
#include "Buffer.hpp"
#include "Promise.hpp"
#include "Data/TrafficItem.hpp"
#include "Data/Stats.hpp"

class WStatsManager final : public TSingleton<WStatsManager>, public IMemoryTrackable
{
	struct WRequestData
	{
		EMessageType          Type;
		WBuffer               Request;
		TPromise<std::string> Response;
	};
	std::mutex DataMutex;
	std::thread              RequestThread;
	std::atomic<bool>        bRunning{ false };
	std::mutex               QueueMutex;
	std::condition_variable  QueueCondition;
	std::queue<WRequestData> PendingRequests;

	struct WTrafficStats
	{
		WBytes BytesIn;
		WBytes BytesOut;
	};
	struct WItemStats
	{
		std::string                                  ApplicationPath;
		std::unordered_map<WIPAddress, WTrafficStats> Traffic;
	};
	struct WSnapshot
	{
		std::unordered_map<WTrafficItemId, WItemStats> Apps{};
		std::unordered_map<std::string, WTrafficStats> Filters{};
	};

	WSnapshot CurrentSnapshot{};

	static void ProcessHistoryRequest(WConnectionHistoryRequest const& Request, TPromise<std::string> const& Promise);
	static void ProcessStatsRequest(WStatsRequest const& Request, TPromise<std::string> const& Promise);

	void RequestThreadFunction();

public:
	WStatsManager() = default;
	~WStatsManager() override = default;

	std::mutex& GetDataMutex() { return DataMutex; }

	WMemoryStat GetMemoryUsage() override;

	void UpdateFilterStats(std::string const& FilterName, WBytes In, WBytes Out);

	void UpdateAppStats(
		WTrafficItemId AppId, std::string const& ApplicationPath, WIPAddress const& RemoteHost, WBytes In, WBytes Out);

	void MakeSnapshot();

	void StartRequestProcessThread();
	void StopRequestProcessThread();

	TPromise<std::string> RequestStats(WBuffer const& Request, EMessageType Type);

#if WDEBUG
	void PushDebugSnapshots();
#endif
};