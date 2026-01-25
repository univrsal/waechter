/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <imgui.h>
#include <thread>

class WTrayIcon
{
	std::thread PollThread;

public:
	void Init();
	void        DeInit();
	static void UseDarkColor();
	static void UseLightColor();
	static void SetColor(ImVec4 const& color);
	~WTrayIcon();
};
