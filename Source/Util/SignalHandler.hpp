/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <atomic>

#include "Singleton.hpp"

class WSignalHandler : public TSingleton<WSignalHandler>
{
public:
	WSignalHandler();

	std::atomic<bool> bStop{ false };
};
