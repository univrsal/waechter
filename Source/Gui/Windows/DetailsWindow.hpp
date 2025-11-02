//
// Created by usr on 01/11/2025.
//

#pragma once

#include <string>

class WTrafficTree;

class WDetailsWindow
{
	WTrafficTree* Tree;

	void DrawSystemDetails();
	void DrawApplicationDetails();
	void DrawProcessDetails();
	void DrawSocketDetails();

	std::string FormattedUptime{};

public:
	WDetailsWindow(WTrafficTree* Tree_);
	void Draw();
};
