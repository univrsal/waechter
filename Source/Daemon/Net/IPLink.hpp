//
// Created by usr on 02/12/2025.
//

#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>

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

struct WIngressPortRouting
{
	uint16_t Port{ 0 };
	uint32_t QDiscId{ 0 };
};

struct WSocketCounter;

class WIPLink : public TSingleton<WIPLink>
{

	std::mutex            Mutex;
	std::atomic<uint32_t> NextMark{ 1 };
	std::atomic<uint16_t> NextMinorId{ 11 }; // 10 is root

	std::unordered_map<WTrafficItemId, std::shared_ptr<WBandwidthLimit>> ActiveUploadLimits;
	std::unordered_map<WTrafficItemId, std::shared_ptr<WBandwidthLimit>> ActiveDownloadLimits;

	std::unordered_map<WTrafficItemId, WIngressPortRouting> IngressPortRoutings;

	std::unique_ptr<WClientSocket> IpProcSocket;

	void SetupHTBLimitClass(
		std::shared_ptr<WBandwidthLimit> const& Limit, std::string const& IfName, bool bAttachMarkFilter) const;
	void OnSocketRemoved(std::shared_ptr<WSocketCounter> const& Socket);

public:
	unsigned int                 WaechterIngressIfIndex{ 0 };
	static constexpr std::string IfbDev{ "ifb0" };

	bool Init();
	bool Deinit();

	void SetupEgressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit);
	void SetupIngressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit);

	void SetupIngressPortRouting(WTrafficItemId Item, uint32_t QDiscId, uint16_t Dport);
	void RemoveIngressPortRouting(WTrafficItemId Item);

	void RemoveUploadLimit(WTrafficItemId const& ItemId);
	void RemoveDownloadLimit(WTrafficItemId const& ItemId);

	std::shared_ptr<WBandwidthLimit> GetUploadLimit(WTrafficItemId const& ItemId, WBytesPerSecond const& Limit);
	std::shared_ptr<WBandwidthLimit> GetDownloadLimit(WTrafficItemId const& ItemId, WBytesPerSecond const& Limit);
};
