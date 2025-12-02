//
// Created by usr on 02/12/2025.
//

#pragma once
#include <string>

#include "Singleton.hpp"

class WIPLink : public TSingleton<WIPLink>
{
	static constexpr std::string WIfName{ "ifb0" };
	unsigned int                 WaechterIngressIfIndex{ 0 };

public:
	bool Init();
	bool Deinit();
};
