/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <vector>

class WNetworkInterface
{
public:
	// Returns a list of all network interface names present on the system.
	// The list is de-duplicated and not guaranteed to be sorted.
	static std::vector<std::string> list();

	static unsigned int GetIfIndex(std::string const& Ifname);
};
