/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <atomic>
#include "Singleton.hpp"

struct traycon;

class WTrayIcon : public TSingleton<WTrayIcon>
{
	traycon*         Traycon{};
	std::atomic<int> PendingVisibility{ -1 }; // -1 = no-op, 0 = hide, 1 = show

	static void OnClick(traycon* Tray, void* Userdata);
	static void OnMenuClick(traycon* Tray, int MenuId, void* Userdata);

	enum EMenuItem
	{
		MI_ToggleWindow,
		MI_Exit
	};

public:
	void Init();
	void Destroy();

	// Poll tray events and apply any pending visibility changes; call once per frame on the main thread
	void Step();
};
