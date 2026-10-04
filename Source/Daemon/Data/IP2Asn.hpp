/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <queue>

#include "Singleton.hpp"
#include "IPAddress.hpp"
#include "MemoryStats.hpp"
#include "Promise.hpp"
#include "IP2Asn/IP2AsnDB.hpp"

class WIP2Asn : public TSingleton<WIP2Asn>, public IMemoryTrackable
{
	bool bHaveDatabaseDownloaded{ false };
	std::atomic<bool> bUpdateInProgress{ false };
	std::thread       DownloadThread{};
	std::mutex        DownloadMutex{};
	std::atomic<float> DownloadProgress{ 0.0f };

	std::unique_ptr<WIP2AsnDB> Database{};

	static bool ExtractDatabase(std::filesystem::path const& GzPath, std::filesystem::path const& OutPath);
	// Every unique remote address ends up in here (including failed lookups),
	// so on a public facing machine this has to be capped
	static constexpr size_t MaxCacheEntries = 16384;
	std::mutex              CacheMutex;
	std::unordered_map<WIPAddress, std::optional<WIP2AsnLookupResult>> Cache{};

	// Caller must hold CacheMutex
	void AddToCache(WIPAddress const& Address, std::optional<WIP2AsnLookupResult> const& Result);

	struct WQueuedRequest
	{
		WIPAddress                                          AddressToResolve;
		TPromise<std::optional<WIP2AsnLookupResult> const&> Promise;
	};
	std::thread                LookupThread;
	std::atomic<bool>          bRunning{ false };
	std::mutex                 QueueMutex;
	std::condition_variable    QueueCondition;
	std::queue<WQueuedRequest> PendingAddresses;
	void                       LookupAddress(WQueuedRequest const& Request);

	void LookupThreadFunc();

public:
	WIP2Asn() = default;
	~WIP2Asn() override
	{
		if (DownloadThread.joinable())
		{
			DownloadThread.join();
		}
	}
	void Init();
	void Stop();

	void UpdateDatabase();

	bool IsUpdateInProgress() const noexcept { return bUpdateInProgress.load(); }
	bool HasDatabase() const noexcept { return Database != nullptr; }

	TPromise<std::optional<WIP2AsnLookupResult> const&> Lookup(WIPAddress const& IpAddress);

	std::optional<WIP2AsnLookupResult> LookupSync(WIPAddress const& IpAddress);

	WMemoryStat GetMemoryUsage() override;
};
