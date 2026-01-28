/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "NetworkInterface.hpp"

#include <unordered_set>
#include <vector>
#include <string>

#include <net/if.h>
#include <ifaddrs.h>

#include "spdlog/spdlog.h"

#include "ErrnoUtil.hpp"

std::vector<std::string> WNetworkInterface::List()
{
	std::vector<std::string>        Result;
	std::unordered_set<std::string> Seen;

	ifaddrs* Ifaddr = nullptr;
	if (getifaddrs(&Ifaddr) != 0 || !Ifaddr)
	{
		return Result;
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
