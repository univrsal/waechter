/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <thread>

class WTrayIcon
{
	std::thread PollThread;

public:
	void Init();
	~WTrayIcon();
};
