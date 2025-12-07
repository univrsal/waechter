//
// Created by usr on 08/10/2025.
//

#include "NetworkInterface.hpp"

#include "ErrnoUtil.hpp"
#include "spdlog/spdlog.h"

#include <ifaddrs.h>
#include <unordered_set>
#include <vector>
#include <string>
#include <net/if.h>

std::vector<std::string> WNetworkInterface::list()
{
	std::vector<std::string>        Result;
	std::unordered_set<std::string> Seen;

	ifaddrs* Ifaddr = nullptr;
	if (getifaddrs(&Ifaddr) != 0 || !Ifaddr)
	{
		return Result; // return empty on failure
	}

	for (ifaddrs* Ifa = Ifaddr; Ifa != nullptr; Ifa = Ifa->ifa_next)
	{
		if (!Ifa->ifa_name)
			continue;
		std::string Name{ Ifa->ifa_name };
		if (Seen.insert(Name).second)
		{
			Result.emplace_back(std::move(Name));
		}
	}

	freeifaddrs(Ifaddr);
	return Result;
}

unsigned int WNetworkInterface::GetIfIndex(std::string const& Ifname)
{
	if (Ifname.empty())
	{
		spdlog::warn("WNetworkInterface::GetIfIndex: ifname is empty");
		return 0;
	}

	unsigned int IfIndex = if_nametoindex(Ifname.c_str());
	if (IfIndex == 0)
	{
		spdlog::critical("Failed to get index of network interface '{}': {}", Ifname, WErrnoUtil::StrError());
	}
	return IfIndex;
}
