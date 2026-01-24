/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
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

#include "IPAddress.hpp"
#include "Singleton.hpp"
#include "Types.hpp"

class WResolver : public TSingleton<WResolver>
{
	static constexpr WBytes                     MaxCacheRamUsage = 5 WMiB;
	std::unordered_map<WIPAddress, std::string> ResolvedAddresses{};

	std::thread             ResolverThread;
	std::atomic<bool>       bRunning{ false };
	std::mutex              QueueMutex;
	std::condition_variable QueueCondition;
	WBytes                  CurrentCacheRamUsage{ 0 };
	std::queue<WIPAddress>  PendingAddresses;

	void ResolveAddress(WIPAddress const& ip);

	void ResolverThreadFunc();

public:
	void Start();
	void Stop();

	std::string const& Resolve(WIPAddress const& Address);

	std::mutex                                  ResolvedAddressesMutex;
	std::unordered_map<WIPAddress, std::string> GetResolvedAddresses() const { return ResolvedAddresses; }
};