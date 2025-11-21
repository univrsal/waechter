//
// Created by usr on 26/10/2025.
//

#pragma once
#include "AboutDialog.hpp"
#include "Client.hpp"
#include "LogWindow.hpp"
#include "NetworkGraphWindow.hpp"
#include "DetailsWindow.hpp"
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

	bool bInit{ false };
	void DrawConnectionIndicator();
	void Init(ImGuiID Main);

public:
	void Draw();

	WLibCurl& GetLibCurl() { return LibCurl; }

	WNetworkGraphWindow& GetNetworkGraphWindow() { return NetworkGraphWindow; }

	WFlagAtlas& GetFlagAtlas() { return FlagAtlas; }

	~WMainWindow() = default;
};
