/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

class WSettingsWindow
{
	bool bVisible{};

public:
	void Draw();
	void Show() { bVisible = true; }
};
