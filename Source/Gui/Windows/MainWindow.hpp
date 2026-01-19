/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Windows/AboutDialog.hpp"
#include "Windows/LogWindow.hpp"
#include "Windows/NetworkGraphWindow.hpp"
#include "Windows/DetailsWindow.hpp"
#include "Windows/RegisterDialog.hpp"
#include "Windows/SettingsWindow.hpp"
#include "Windows/ConnectionHistoryWindow.hpp"
#include "Util/FlagAtlas.hpp"

class WMainWindow
{
	WAboutDialog             AboutDialog{};
	WLogWindow               LogWindow{};
	WNetworkGraphWindow      NetworkGraphWindow{};
	WDetailsWindow           DetailsWindow{};
	WFlagAtlas               FlagAtlas{};
	WRegisterDialog          RegisterDialog{};
	WSettingsWindow          SettingsWindow{};
	WConnectionHistoryWindow ConnectionHistoryWindow{};

	bool bInit{ false };
	static void DrawConnectionIndicator();
	void Init(ImGuiID Main);

public:
	static WMainWindow& Get();
	void Draw();

	WNetworkGraphWindow& GetNetworkGraphWindow() { return NetworkGraphWindow; }

	WConnectionHistoryWindow& GetConnectionHistoryWindow() { return ConnectionHistoryWindow; }

	bool IsRegistered() const { return RegisterDialog.IsRegistered(); }

	WFlagAtlas& GetFlagAtlas() { return FlagAtlas; }

	~WMainWindow() = default;
};
