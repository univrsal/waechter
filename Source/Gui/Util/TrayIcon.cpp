/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "TrayIcon.hpp"

#include "Settings.hpp"
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
	XTrayInitConfig Config{};
	Config.IconSize = 32;
	Config.IconData = GIconData;
	Config.IconDataSize = GIconSize;
	Config.IconName = "Wächter";
	Config.Callback = [](int X, int Y, enum XTrayButton Button, void*) {
		spdlog::info("Tray icon clicked at ({}, {}) with button {}", X, Y, static_cast<int>(Button));
	};
	if (XTrayInit(Config) == 0)
	{
		spdlog::error("Failed to initialize tray icon");
	}
	else
	{
		PollThread = std::thread(ProcessEvents);
		static constexpr unsigned char BackgroundBright[] = { 239, 240, 241 };
		static constexpr unsigned char BackgroundDark[] = { 32, 35, 38 };

		if (WSettings::GetInstance().bUseDarkTheme)
		{
			XTraySetBackgroundColor(BackgroundDark[0], BackgroundDark[1], BackgroundDark[2]);
		}
		else
		{
			XTraySetBackgroundColor(BackgroundBright[0], BackgroundBright[1], BackgroundBright[2]);
		}
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
