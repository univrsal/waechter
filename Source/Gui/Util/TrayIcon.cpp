/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "TrayIcon.hpp"

#include "xtray.h"

extern unsigned char const GIconData[];
extern unsigned int const  GIconSize;

void ProcessEvents()
{
	xtray_poll();
}

void WTrayIcon::Init()
{
	xtray_init(GIconData, GIconSize);
	PollThread = std::thread(ProcessEvents);
}

WTrayIcon::~WTrayIcon()
{
	xtray_cleanup();
	if (PollThread.joinable())
		PollThread.join();
}
