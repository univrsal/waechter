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
	Config.LogCallback = [](XTrayLogLevel Level, char const* Message, void*) {
		switch (Level)
		{
			default:
			case XTrayLogLevelDebug:
				spdlog::debug("{}", Message);
				break;
			case XTrayLogLevelInfo:
				spdlog::info("{}", Message);
				break;
			case XTrayLogLevelWarning:
				spdlog::warn("{}", Message);
				break;
			case XTrayLogLevelError:
				spdlog::error("{}", Message);
				break;
		}
	};

	if (XTrayInit(Config) == 0)
	{
		spdlog::error("Failed to initialize tray icon");
	}
	else
	{
		PollThread = std::thread(ProcessEvents);
		if (WSettings::GetInstance().bUseCustomTrayIconColor)
		{
			SetColor(WSettings::GetInstance().TrayIconBackgroundColor.Color);
		}
		else
		{
			if (WSettings::GetInstance().bUseDarkTheme)
			{
				UseDarkColor();
			}
			else
			{
				UseLightColor();
			}
		}
	}
}

void WTrayIcon::DeInit()
{
	XTrayCleanup();
	if (PollThread.joinable())
	{
		PollThread.join();
	}
}

void WTrayIcon::UseDarkColor()
{
	static constexpr unsigned char BackgroundDark[] = { 32, 35, 38 };
	XTraySetBackgroundColor(BackgroundDark[0], BackgroundDark[1], BackgroundDark[2]);
}

void WTrayIcon::UseLightColor()
{
	static constexpr unsigned char BackgroundBright[] = { 239, 240, 241 };
	XTraySetBackgroundColor(BackgroundBright[0], BackgroundBright[1], BackgroundBright[2]);
}

void WTrayIcon::SetColor(ImVec4 const& Color)
{
	XTraySetBackgroundColor(
		static_cast<uint8_t>(Color.x * 255), static_cast<uint8_t>(Color.y * 255), static_cast<uint8_t>(Color.z * 255));
}

WTrayIcon::~WTrayIcon()
{
	DeInit();
}
