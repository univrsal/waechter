/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "MemoryUsageWindow.hpp"
#include "Windows/AboutDialog.hpp"
#include "Windows/LogWindow.hpp"
#include "Windows/NetworkGraphWindow.hpp"
#include "Windows/DetailsWindow.hpp"
#include "Windows/RegisterDialog.hpp"
#include "Windows/SettingsWindow.hpp"
#include "Windows/ConnectionHistoryWindow.hpp"
#include "Windows/StatWindow.hpp"
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
	WMemoryUsageWindow       MemoryUsageWindow{};
	WConnectionHistoryWindow ConnectionHistoryWindow{};

	std::vector<std::unique_ptr<WStatWindow>> StatWindows{};

	bool        bInit{ false };
	static void DrawConnectionIndicator();
	void        Init(ImGuiID Main);

public:
	static WMainWindow& Get();
	static std::shared_ptr<WTrafficTree> const& GetTrafficTree();
	void                Draw();

	WNetworkGraphWindow&      GetNetworkGraphWindow() { return NetworkGraphWindow; }
	WDetailsWindow&           GetDetailsWindow() { return DetailsWindow; }
	WConnectionHistoryWindow& GetConnectionHistoryWindow() { return ConnectionHistoryWindow; }
	WMemoryUsageWindow&       GetMemoryUsageWindow() { return MemoryUsageWindow; }

	bool IsRegistered() const { return RegisterDialog.IsRegistered(); }

	WFlagAtlas& GetFlagAtlas() { return FlagAtlas; }

	~WMainWindow() = default;

	void OpenStatsWindow(WStatsRequest Request, WConnectionHistoryRequest HistoryRequest);
	void HandleStatsResponse(WBuffer const& Buf) const;
	void HandleHistoryResponse(WBuffer const& Buf) const;
};
