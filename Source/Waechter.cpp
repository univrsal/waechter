//
// Created by usr on 08/10/2025.
//

#include "Util/NetworkInterface.h"

#include <iostream>

int main(int, char**)
{
	auto interfaces = NetworkInterface::list();
	for (const auto& iface : interfaces)
		std::cout << iface << std::endl;
	return 0;
}