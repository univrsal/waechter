//
// Created by usr on 26/10/2025.
//

#pragma once
#include "AboutDialog.hpp"
#include "Client.hpp"
#include "LogWindow.hpp"
#include "NetworkGraphWindow.hpp"
#include "DetailsWindow.hpp"

class WMainWindow
{
	WAboutDialog        AboutDialog{};
	WClient             Client{};
	WLogWindow          LogWindow{};
	WNetworkGraphWindow NetworkGraphWindow{};
	WDetailsWindow      DetailsWindow{};

	bool bInit{ false };
	void DrawConnectionIndicator();
	void Init(ImGuiID Main);

public:
	void Draw();

	WNetworkGraphWindow& GetNetworkGraphWindow()
	{
		return NetworkGraphWindow;
	}

	~WMainWindow() = default;
};
