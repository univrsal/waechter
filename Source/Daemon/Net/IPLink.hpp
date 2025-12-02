//
// Created by usr on 02/12/2025.
//

#pragma once
#include "Singleton.hpp"

class WIPLink : public TSingleton<WIPLink>
{
	unsigned int WaechterIngressIfIndex{ 0 };

public:
	bool Init();
	bool Deinit();
};
