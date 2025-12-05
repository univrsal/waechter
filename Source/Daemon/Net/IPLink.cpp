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

#define SYSFMT(_fmt, ...)                                                                                     \
	if (auto RC = system(fmt::format(_fmt, __VA_ARGS__).c_str()); RC != 0)                                    \
	{                                                                                                         \
		spdlog::error("Failed to execute system command '{}': {} (RC={})", _fmt, WErrnoUtil::StrError(), RC); \
		return false;                                                                                         \
	}

#define SYSFMT2(_fmt, ...)                                                                                    \
	if (auto RC = system(fmt::format(_fmt, __VA_ARGS__).c_str()); RC != 0)                                    \
	{                                                                                                         \
		spdlog::error("Failed to execute system command '{}': {} (RC={})", _fmt, WErrnoUtil::StrError(), RC); \
	}

WBandwidthLimit::WBandwidthLimit(
	uint32_t Mark_, uint16_t MinorId_, WBytesPerSecond RateLimit_, ELimitDirection Direction_)
	: Direction(Direction_), Mark(Mark_), MinorId(MinorId_), RateLimit(RateLimit_)
{
}

WBandwidthLimit::~WBandwidthLimit()
{
	// clean up tc classes and filters
	std::string IfName =
		(Direction == ELimitDirection::Upload) ? WDaemonConfig::GetInstance().NetworkInterfaceName : WIPLink::WIfName;

	SYSFMT2(
		"tc filter delete dev {} parent 1: protocol ip pref 1 handle 0x{:x} fw classid 1:{}", IfName, Mark, MinorId);
	// SYSFMT2(
	// 	"tc filter delete dev {} parent 1: protocol ipv6 pref 1 handle 0x{:x} fw classid 1:{}", IfName, Mark, MinorId);
	SYSFMT2("tc class delete dev {} classid 1:{}", IfName, MinorId);
}

bool WIPLink::SetupHTBLimitClass(std::shared_ptr<WBandwidthLimit> const& Limit, std::string const& IfName)
{
	spdlog::info("Setting up HTB class on interface {}: classid=1:{}, mark=0x{:x}, rate={} B/s", IfName, Limit->MinorId,
		Limit->Mark, Limit->RateLimit);
	SYSFMT("tc class replace dev {} parent 1:1 classid 1:{} htb rate {}bit ceil {}bit", IfName, Limit->MinorId,
		static_cast<uint64_t>(Limit->RateLimit * 8), static_cast<uint64_t>(Limit->RateLimit * 8));

	SYSFMT("tc filter replace dev {} parent 1: protocol ip pref 1 "
		   "handle 0x{:x} fw classid 1:{}",
		IfName, Limit->Mark, Limit->MinorId);
	// SYSFMT("tc filter replace dev {} parent 1: protocol ipv6 pref 1 "
	// 	   "handle 0x{:x} fw classid 1:{}",
	// 	IfName, Limit->Mark, Limit->MinorId);
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
	SYSFMT("ip link show {0} >/dev/null 2>&1 || ip link add {0} type ifb", WIfName);
	SYSFMT("ip link set {0} up", WIfName);
	WaechterIngressIfIndex = WNetworkInterface::GetIfIndex(WIfName);

	// Ingress HTB setup
	SYSFMT("tc qdisc add dev {} root handle 1: htb default 0x10", WIfName);
	SYSFMT("tc class replace dev {} parent 1: classid 1:1 htb rate 1Gbit ceil 1Gbit", WIfName);

	// leaf class for ifb
	SYSFMT("tc class replace dev {} parent 1:1 classid 1:10 "
		   "htb rate 1Gbit ceil 1Gbit",
		WIfName);

	// Egress shaping on actual interface
	SYSFMT("tc qdisc add dev {} root handle 1: htb default 0x10", WDaemonConfig::GetInstance().NetworkInterfaceName);

	SYSFMT("tc class replace dev {} parent 1: classid 1:1 htb rate 1Gbit ceil 1Gbit",
		WDaemonConfig::GetInstance().NetworkInterfaceName);
	// leaf class for actual interface
	SYSFMT("tc class replace dev {} parent 1:1 classid 1:10 "
		   "htb rate 1Gbit ceil 1Gbit",
		WDaemonConfig::GetInstance().NetworkInterfaceName);

	// Redirect ingress traffic to ifb0
	SYSFMT("tc qdisc replace dev {} handle ffff: ingress", WDaemonConfig::GetInstance().NetworkInterfaceName);
	SYSFMT(
		"tc filter replace dev {} parent ffff: protocol all prio 100 u32 match u32 0 0 action mirred egress redirect dev {}",
		WDaemonConfig::GetInstance().NetworkInterfaceName, WIfName);

	return true;
}

bool WIPLink::Deinit()
{
	std::lock_guard Lock(Mutex);
	ActiveUploadLimits.clear();
	ActiveDownloadLimits.clear();
	SYSFMT2("tc filter delete dev {} parent ffff: protocol all prio 100",
		WDaemonConfig::GetInstance().NetworkInterfaceName);
	SYSFMT2("tc qdisc delete dev {} handle ffff: ingress", WDaemonConfig::GetInstance().NetworkInterfaceName);
	SYSFMT2("tc qdisc delete dev {} root handle 1:", WDaemonConfig::GetInstance().NetworkInterfaceName);
	SYSFMT2("ip link delete {} type ifb", WIfName);
	return true;
}

bool WIPLink::SetupEgressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit)
{
	return SetupHTBLimitClass(Limit, WDaemonConfig::GetInstance().NetworkInterfaceName);
}

bool WIPLink::SetupIngressHTBClass(std::shared_ptr<WBandwidthLimit> const& Limit)
{
	return SetupHTBLimitClass(Limit, WIfName);
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
