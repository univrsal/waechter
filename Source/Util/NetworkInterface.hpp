/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <vector>

class WNetworkInterface
{
public:
	static std::vector<std::string> List();

	static unsigned int GetIfIndex(std::string const& Ifname);
};
