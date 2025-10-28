//
// Created by usr on 26/10/2025.
//

#pragma once
#include "AboutDialog.hpp"
#include "Client.hpp"

class WMainWindow
{
	WAboutDialog AboutDialog{};
	WClient      Client{};

	void DrawConnectionIndicator();

public:
	void Draw();
};
