//
// Created by usr on 26/10/2025.
//

#pragma once
#include "AboutDialog.hpp"
#include "WClient.hpp"

class WMainWindow
{
	WAboutDialog AboutDialog{};
	WClient      Client{};

public:
	void Draw();
};
