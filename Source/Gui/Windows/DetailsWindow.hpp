/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <memory>

class WTrafficTree;

class WDetailsWindow
{
	std::shared_ptr<WTrafficTree> Tree{};

	void DrawSystemDetails() const;
	void DrawApplicationDetails() const;
	void DrawProcessDetails() const;
	void DrawSocketDetails() const;
	void DrawTupleDetails() const;

	std::string FormattedUptime{};

public:
	explicit WDetailsWindow();
	void Draw() const;
};
