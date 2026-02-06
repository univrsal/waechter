/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>

#include "spdlog/spdlog.h"

#include "MemoryStats.hpp"
#include "Singleton.hpp"
#include "Types.hpp"
#include "Data/TrafficItem.hpp"
#include "Socket.hpp"

enum class ELimitDirection
{
	Upload,
	Download
};

struct WBandwidthLimit
{
	ELimitDirection Direction;
	uint32_t        Mark{ 0 };      // Traffic mark used in tc filters to redirect the packet to the correct class
	uint16_t        MinorId{ 0 };   // Minor class ID in the HTB qdisc
	WBytesPerSecond RateLimit{ 0 }; // Bandwidth limit in bytes per second

	WBandwidthLimit(uint32_t Mark, uint16_t MinorId, WBytesPerSecond RateLimit, ELimitDirection Direction);
	~WBandwidthLimit();
};

struct WSocketCounter;

class WIPLink : public TSingleton<WIPLink>, public IMemoryTrackable
{
	friend struct WBandwidthLimit;
	std::mutex            Mutex;
	std::atomic<uint32_t> NextFilterhandle{ 1 };
	std::atomic<uint32_t> NextMark{ 1 };
	std::atomic<uint16_t> NextMinorId{ 11 }; // 10 is root

	std::unordered_map<WTrafficItemId, std::shared_ptr<WBandwidthLimit>> ActiveUploadLimits;
	std::unordered_map<WTrafficItemId, std::shared_ptr<WBandwidthLimit>> ActiveDownloadLimits;

	std::unique_ptr<WClientSocket> IpProcSocket;

	void SetupHTBLimitClass(std::shared_ptr<WBandwidthLimit> const& Limit, std::string const& IfName) const;
	void OnSocketRemoved(std::shared_ptr<WSocketCounter> const& Socket);

public:
	unsigned int             WaechterIngressIfIndex{ 0 };
	static std::string const IfbDev;

	bool Init();
	bool Deinit();

	void SetupEgressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit) const;
	void SetupIngressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit) const;

	static void SetupIngressPortRouting(WTrafficItemId Item, uint32_t QDiscId, uint16_t Dport);
	static void RemoveIngressPortRouting(uint16_t Dport);

	void RemoveUploadLimit(WTrafficItemId const& ItemId);
	void RemoveDownloadLimit(WTrafficItemId const& ItemId);

	std::shared_ptr<WBandwidthLimit> GetUploadLimit(
		WTrafficItemId const& ItemId, WBytesPerSecond const& Limit, bool* bExists = nullptr);
	std::shared_ptr<WBandwidthLimit> GetDownloadLimit(
		WTrafficItemId const& ItemId, WBytesPerSecond const& Limit, bool* bExists = nullptr);

	// PID-based download marks for immediate rule application to new sockets
	static void SetPidDownloadMark(uint32_t Pid, uint32_t Mark);
	static void RemovePidDownloadMark(uint32_t Pid);

	void PrintStats() const
	{
		spdlog::info("{} active upload limits, {} active download limits", ActiveUploadLimits.size(),
			ActiveDownloadLimits.size());
	}

	WMemoryStat GetMemoryUsage() override;
};
