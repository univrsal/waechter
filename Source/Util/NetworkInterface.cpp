//
// Created by usr on 08/10/2025.
//

#include "NetworkInterface.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <sys/types.h>
#include <unordered_set>
#include <vector>
#include <string>

std::vector<std::string> NetworkInterface::list()
{
	std::vector<std::string> result;
	std::unordered_set<std::string> seen;

	ifaddrs* ifaddr = nullptr;
	if (getifaddrs(&ifaddr) != 0 || !ifaddr)
	{
		return result; // return empty on failure
	}

	for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
	{
		if (!ifa->ifa_name)
			continue;
		std::string name{ifa->ifa_name};
		if (seen.insert(name).second)
		{
			result.emplace_back(std::move(name));
		}
	}

	freeifaddrs(ifaddr);
	return result;
}
