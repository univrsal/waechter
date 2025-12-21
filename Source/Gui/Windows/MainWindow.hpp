/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "AboutDialog.hpp"
#include "LogWindow.hpp"
#include "NetworkGraphWindow.hpp"
#include "DetailsWindow.hpp"
#include "RegisterDialog.hpp"
#include "Util/FlagAtlas.hpp"
#include "Util/LibCurl.hpp"

class WMainWindow
{
	WAboutDialog        AboutDialog{};
	WLogWindow          LogWindow{};
	WNetworkGraphWindow NetworkGraphWindow{};
	WDetailsWindow      DetailsWindow{};
	WFlagAtlas          FlagAtlas{};
	WLibCurl            LibCurl{};
	WRegisterDialog     RegisterDialog{};

	bool bInit{ false };
	void DrawConnectionIndicator();
	void Init(ImGuiID Main);

public:
	void Draw();

	WLibCurl& GetLibCurl() { return LibCurl; }

	WNetworkGraphWindow& GetNetworkGraphWindow() { return NetworkGraphWindow; }

	bool IsRegistered() const { return RegisterDialog.IsRegistered(); }

	WFlagAtlas& GetFlagAtlas() { return FlagAtlas; }

	~WMainWindow() = default;
};
