//
// Created by usr on 02/12/2025.
//

#include "IPLink.hpp"

#include <cstdlib>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "NetworkInterface.hpp"
#include "DaemonConfig.hpp"
#include "ErrnoUtil.hpp"

#define SYSFMT(_fmt, ...)                                                                                             \
	if (auto RC = system(fmt::format(_fmt, __VA_ARGS__).c_str()); RC != 0)                                            \
	{                                                                                                                 \
		auto CmdFormatted = fmt::format(_fmt, __VA_ARGS__);                                                           \
		spdlog::error("Failed to execute system command '{}': {} (RC={})", CmdFormatted, WErrnoUtil::StrError(), RC); \
		return false;                                                                                                 \
	}

#define SYSFMT2(_fmt, ...)                                                                                            \
	if (auto RC = system(fmt::format(_fmt, __VA_ARGS__).c_str()); RC != 0)                                            \
	{                                                                                                                 \
		auto CmdFormatted = fmt::format(_fmt, __VA_ARGS__);                                                           \
		spdlog::error("Failed to execute system command '{}': {} (RC={})", CmdFormatted, WErrnoUtil::StrError(), RC); \
	}

constexpr int kIngressPortFilterPriority = 1;

WBandwidthLimit::WBandwidthLimit(
	uint32_t Mark_, uint16_t MinorId_, WBytesPerSecond RateLimit_, ELimitDirection Direction_)
	: Direction(Direction_), Mark(Mark_), MinorId(MinorId_), RateLimit(RateLimit_)
{
}

WBandwidthLimit::~WBandwidthLimit()
{
	// clean up tc classes and filters
	std::string IfName =
		(Direction == ELimitDirection::Upload) ? WDaemonConfig::GetInstance().NetworkInterfaceName : WIPLink::IfbDev;

	if (Direction == ELimitDirection::Upload)
	{
		SYSFMT2("tc filter delete dev {} parent 1: protocol ip pref 1 handle 0x{:x} fw classid 1:{}", IfName, Mark,
			MinorId);
	}
	SYSFMT2("tc class delete dev {} classid 1:{}", IfName, MinorId);

	for (auto Port : RoutedPorts)
	{
		SYSFMT2("tc filter delete dev {} parent 1: protocol ip prio {} u32 match ip dport {} 0xffff flowid 1:{}",
			IfName, kIngressPortFilterPriority, Port, MinorId);
	}
}

bool WIPLink::SetupHTBLimitClass(
	std::shared_ptr<WBandwidthLimit> const& Limit, std::string const& IfName, bool bAttachMarkFilter)
{
	spdlog::info("Setting up HTB class on interface {}: classid=1:{}, mark=0x{:x}, rate={} B/s", IfName, Limit->MinorId,
		Limit->Mark, Limit->RateLimit);
	SYSFMT("tc class replace dev {} parent 1:1 classid 1:{} htb rate {}bit ceil {}bit", IfName, Limit->MinorId,
		static_cast<uint64_t>(Limit->RateLimit * 8), static_cast<uint64_t>(Limit->RateLimit * 8));

	if (bAttachMarkFilter)
	{
		SYSFMT("tc filter replace dev {} parent 1: protocol ip pref 1 handle 0x{:x} fw classid 1:{}", IfName,
			Limit->Mark, Limit->MinorId);
	}
	return true;
}

bool WIPLink::Init()
{
	// For bandwidth limiting we need
	//  - A new interface ifb0 for ingress redirection
	//	- HTB on the main interface (egress)
	//	- HTB on the ifb0 interface (ingress)
	//	- An ingress qdisc on the main interface that redirects all traffic to ifb
	//  - A root qdisc on ifb0 to shape the ingress traffic
	//  - A root qdisc on the main interface to shape the egress traffic
	//  - A filter on the ingress qdisc to redirect all traffic to ifb0

	// Create the ifb0 interface if it does not exist
	SYSFMT("ip link show {0} >/dev/null 2>&1 || ip link add {0} type ifb", IfbDev);
	SYSFMT("ip link set {0} up", IfbDev);
	WaechterIngressIfIndex = WNetworkInterface::GetIfIndex(IfbDev);

	// Ingress HTB setup
	SYSFMT("tc qdisc replace dev {} root handle 1: htb default 0x10", IfbDev);
	SYSFMT("tc class replace dev {} parent 1: classid 1:1 htb rate 1Gbit ceil 1Gbit", IfbDev);

	// leaf class for ifb
	SYSFMT("tc class replace dev {} parent 1:1 classid 1:10 "
		   "htb rate 1Gbit ceil 1Gbit",
		IfbDev);
	// Default filter
	SYSFMT("tc filter replace dev {} parent 1: prio 100 protocol all u32 match u32 0 0 flowid 1:10", IfbDev);

	// // Egress shaping on actual interface
	// SYSFMT("tc qdisc add dev {} root handle 1: htb default 0x10", WDaemonConfig::GetInstance().NetworkInterfaceName);
	//
	// SYSFMT("tc class replace dev {} parent 1: classid 1:1 htb rate 1Gbit ceil 1Gbit",
	// 	WDaemonConfig::GetInstance().NetworkInterfaceName);
	// // leaf class for actual interface
	// SYSFMT("tc class replace dev {} parent 1:1 classid 1:10 "
	// 	   "htb rate 1Gbit ceil 1Gbit",
	// 	WDaemonConfig::GetInstance().NetworkInterfaceName);
	//

	// Redirect ingress traffic to ifb0
	SYSFMT("tc qdisc replace dev {} handle ffff: ingress", WDaemonConfig::GetInstance().IngressNetworkInterfaceName);
	SYSFMT(
		"tc filter replace dev {} parent ffff: protocol all prio 100 u32 match u32 0 0 action mirred egress redirect dev {}",
		WDaemonConfig::GetInstance().IngressNetworkInterfaceName, IfbDev);

	return true;
}

bool WIPLink::Deinit()
{
	std::lock_guard Lock(Mutex);
	ActiveUploadLimits.clear();
	ActiveDownloadLimits.clear();
	SYSFMT2("tc filter delete dev {} parent ffff:", WDaemonConfig::GetInstance().IngressNetworkInterfaceName);
	// SYSFMT2("tc qdisc delete dev {} handle ffff: ingress", WDaemonConfig::GetInstance().NetworkInterfaceName);
	// SYSFMT2("tc qdisc delete dev {} root handle 1:", WDaemonConfig::GetInstance().NetworkInterfaceName);
	SYSFMT2("ip link delete {} type ifb", IfbDev);
	return true;
}

bool WIPLink::SetupEgressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit)
{
	return SetupHTBLimitClass(Limit, WDaemonConfig::GetInstance().NetworkInterfaceName, true);
}

bool WIPLink::SetupIngressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit)
{
	return SetupHTBLimitClass(Limit, IfbDev, false);
}

bool WIPLink::SetupIngressPortRouting(WTrafficItemId Item, uint32_t QDiscId, uint16_t Dport)
{
	spdlog::info("Setting up ingress port routing for traffic item ID {}: dport={}, qdisc id={}", Item, Dport, QDiscId);
	std::lock_guard Lock(Mutex);
	if (ActiveDownloadLimits.contains(Item))
	{
		auto Limit = ActiveDownloadLimits[Item];

		if (Limit->RoutedPorts.contains(Dport))
		{
			// Already routed
			spdlog::info("Port already routed for traffic item ID {}: dport={}, qdisc id={}", Item, Dport, QDiscId);
			return true;
		}
		SYSFMT("tc filter add dev {} protocol ip parent 1: prio {} u32 match ip dport {} 0xffff flowid 1:{}", IfbDev,
			kIngressPortFilterPriority, Dport, QDiscId);

		Limit->RoutedPorts.insert(Dport);

		return true;
	}
	spdlog::error("No active download limit for traffic item ID {}", Item);
	return false;
}

void WIPLink::RemoveUploadLimit(WTrafficItemId const& ItemId)
{
	std::lock_guard Lock(Mutex);
	if (ActiveUploadLimits.contains(ItemId))
	{
		ActiveUploadLimits.erase(ItemId);
	}
}

void WIPLink::RemoveDownloadLimit(WTrafficItemId const& ItemId)
{
	std::lock_guard Lock(Mutex);
	if (ActiveDownloadLimits.contains(ItemId))
	{
		ActiveDownloadLimits.erase(ItemId);
	}
}

std::shared_ptr<WBandwidthLimit> WIPLink::GetUploadLimit(WTrafficItemId const& ItemId, WBytesPerSecond const& Limit)
{
	std::lock_guard Lock(Mutex);
	if (ActiveUploadLimits.contains(ItemId))
	{
		auto ExistingLimit = ActiveUploadLimits[ItemId];
		if (ExistingLimit->RateLimit != Limit)
		{
			// Update existing limit
			ExistingLimit->RateLimit = Limit;
			SetupEgressHTBClass(ExistingLimit);
		}
		return ExistingLimit;
	}
	auto NewLimit = std::make_shared<WBandwidthLimit>(
		NextMark.fetch_add(1), NextMinorId.fetch_add(1), Limit, ELimitDirection::Upload);

	ActiveUploadLimits[ItemId] = NewLimit;

	SetupEgressHTBClass(NewLimit);

	return NewLimit;
}

std::shared_ptr<WBandwidthLimit> WIPLink::GetDownloadLimit(WTrafficItemId const& ItemId, WBytesPerSecond const& Limit)
{
	std::lock_guard Lock(Mutex);
	if (ActiveDownloadLimits.contains(ItemId))
	{
		return ActiveDownloadLimits[ItemId];
	}
	auto NewLimit = std::make_shared<WBandwidthLimit>(
		NextMark.fetch_add(1), NextMinorId.fetch_add(1), Limit, ELimitDirection::Download);
	ActiveDownloadLimits[ItemId] = NewLimit;

	SetupIngressHTBClass(NewLimit);

	return NewLimit;
}
