/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>

#include "MemoryStats.hpp"
#include "IPAddress.hpp"
#include "Singleton.hpp"
#include "Types.hpp"
#include "Promise.hpp"

class WResolver : public TSingleton<WResolver>, public IMemoryTrackable
{
	struct WQueuedRequest
	{
		WIPAddress                   AddressToResolve;
		TPromise<std::string const&> Promise;
	};

	static constexpr WBytes                     MaxCacheRamUsage = 1 WMiB;
	std::unordered_map<WIPAddress, std::string> ResolvedAddresses{};

	std::thread             ResolverThread;
	std::atomic<bool>       bRunning{ false };
	std::mutex              QueueMutex;
	std::condition_variable QueueCondition;
	WBytes                  CurrentCacheRamUsage{ 0 };
	std::queue<WQueuedRequest> PendingAddresses;

	void ResolveAddress(WQueuedRequest const& Request);

	void ResolverThreadFunc();

public:
	void Start();
	void Stop();

	TPromise<std::string const&> Resolve(WIPAddress const& Address);

	std::mutex                                  ResolvedAddressesMutex;
	std::unordered_map<WIPAddress, std::string> GetResolvedAddresses() const { return ResolvedAddresses; }

	WMemoryStat GetMemoryUsage() override;
};