//
// Created by usr on 09/10/2025.
//

#pragma once

#include <atomic>

#include "Singleton.hpp"

class WSignalHandler : public TSingleton<WSignalHandler>
{
public:
	WSignalHandler();

	std::atomic<bool> bStop{ false };
};
