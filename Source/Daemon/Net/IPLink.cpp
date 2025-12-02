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
	SYSFMT2("tc filter delete dev {} parent ffff: protocol all prio 100",
		WDaemonConfig::GetInstance().NetworkInterfaceName);
	SYSFMT2("tc qdisc delete dev {} handle ffff: ingress", WDaemonConfig::GetInstance().NetworkInterfaceName);
	SYSFMT2("tc qdisc delete dev {} root handle 1:", WDaemonConfig::GetInstance().NetworkInterfaceName);
	SYSFMT2("ip link delete {} type ifb", WIfName);
	return true;
}
