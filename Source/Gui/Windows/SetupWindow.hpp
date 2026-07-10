/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Data/Protocol.hpp"

class WSetupWindow
{
	bool bVisible{};

	WDaemonConfigMessage DaemonConfig{};
	char                 DaemonSocketPath[1024]{};
	char                 DaemonUser[256]{};
	char                 DaemonGroup[256]{};
	char                 DaemonMode[8]{};

public:
	void Show() { bVisible = true; }
	[[nodiscard]] bool IsVisible() const { return bVisible; }
	void               DrawDaemonSettings();
	void Draw();

	void HandleDaemonConfig(WDaemonConfigMessage const& Cfg);
};
