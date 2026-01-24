/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "TrayIcon.hpp"

#include "spdlog/spdlog.h"
#include "xtray.h"

extern unsigned char const GIconData[];
extern unsigned int const  GIconSize;

void ProcessEvents()
{
	XTrayEventLoop();
}

void WTrayIcon::Init()
{
	if (XTrayInit(GIconData, GIconSize) == 0)
	{
		spdlog::error("Failed to initialize tray icon");
	}
	else
	{
		PollThread = std::thread(ProcessEvents);
	}
}

WTrayIcon::~WTrayIcon()
{
	XTrayCleanup();
	if (PollThread.joinable())
	{
		PollThread.join();
	}
}
