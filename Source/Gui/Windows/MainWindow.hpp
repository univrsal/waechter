//
// Created by usr on 26/10/2025.
//

#pragma once
#include "AboutDialog.hpp"
#include "Client.hpp"
#include "LogWindow.hpp"

class WMainWindow
{
	WAboutDialog AboutDialog{};
	WClient      Client{};
	WLogWindow   LogWindow{};

	bool bInit{ false };
	void DrawConnectionIndicator();

public:
	void Init();
	void Draw();
};
