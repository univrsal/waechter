/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <mutex>

#include "Buffer.hpp"
#include "Client.hpp"
#include "MemoryStats.hpp"

class WMemoryUsageWindow
{
	std::mutex   Mutex;
	WMemoryStats Stats;
	bool         bVisible{};
	WBytes       TotalUsage = 0;

public:
	void Draw();
	void Show() { bVisible = !bVisible; }

	void HandleUpdate(WBuffer& Update)
	{
		std::lock_guard Lock(Mutex);
		if (WClient::ReadMessage(Update, Stats) == false)
		{
			spdlog::error("Failed to read connection history update");
			return;
		}
		TotalUsage = 0;

		// Calculate total usage
		for (auto const& Stat : Stats.Stats)
		{
			for (auto const& Entry : Stat.ChildEntries)
			{
				TotalUsage += Entry.Usage;
			}
		}
	}
};
