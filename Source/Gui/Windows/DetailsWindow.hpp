/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <memory>

class WTrafficTree;

class WDetailsWindow
{
	std::shared_ptr<WTrafficTree> Tree{};

	void DrawSystemDetails();
	void DrawApplicationDetails();
	void DrawProcessDetails();
	void DrawSocketDetails();
	void DrawTupleDetails();

	std::string FormattedUptime{};

public:
	explicit WDetailsWindow();
	void Draw();
};
