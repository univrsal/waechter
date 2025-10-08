//
// Created by usr on 08/10/2025.
//

#pragma once

#include <string>
#include <vector>

class NetworkInterface
{
public:
	// Returns a list of all network interface names present on the system.
	// The list is de-duplicated and not guaranteed to be sorted.
	static std::vector<std::string> list();
};
