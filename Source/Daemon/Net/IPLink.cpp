//
// Created by usr on 02/12/2025.
//

#include "IPLink.hpp"

#include "NetworkInterface.hpp"

#include <cstdlib>
#include <spdlog/spdlog.h>
#include <net/if.h>

bool WIPLink::Init()
{
	// Create ifb0 (ignore error if it already exists)
	auto RC = system("ip link show ifb0 >/dev/null 2>&1 || ip link add ifb0 type ifb");
	if (RC != 0)
	{
		spdlog::error("Failed to initialize IP link");
		return false;
	}

	// Bring it up
	RC = system("ip link set ifb0 up");
	if (RC != 0)
	{
		spdlog::error("Failed to bring up ifb0");
		return false;
	}

	WaechterIngressIfIndex = WNetworkInterface::GetIfIndex("ifb0");
	return true;
}

bool WIPLink::Deinit()
{
	// Delete ifb0
	auto RC = system("ip link delete ifb0 type ifb");
	if (RC != 0)
	{
		spdlog::error("Failed to delete ifb0");
		return false;
	}
	return true;
}