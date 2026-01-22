/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <imgui.h>
#include <mutex>

#include "Types.hpp"

struct WScrollingBuffer
{
	int              MaxSize;
	int              Offset;
	ImVector<ImVec2> Data;
	explicit WScrollingBuffer(int max_size = 100)
	{
		MaxSize = max_size;
		Offset = 0;
		Data.reserve(MaxSize);
	}
	void AddPoint(float x, float y)
	{
		if (Data.size() < MaxSize)
			Data.push_back(ImVec2(x, y));
		else
		{
			Data[Offset] = ImVec2(x, y);
			Offset = (Offset + 1) % MaxSize;
		}
	}
	void Erase()
	{
		if (!Data.empty())
		{
			Data.shrink(0);
			Offset = 0;
		}
	}
};

enum ENetworkGraphHistory
{
	NGH_1Min,
	NGH_5Min,
	NGH_15Min,
	NGH_Count
};

class WNetworkGraphWindow
{
	double Time{};
	double           History{ 60.0 };
	WBytesPerSecond  CurrentMaxUploadRate{};
	WBytesPerSecond  CurrentMaxDownloadRate{};
	WScrollingBuffer UploadBuffer{ 900 }; // 15 minutes at 1 sample/sec
	WScrollingBuffer DownloadBuffer{ 900 };
	std::mutex       Mutex{};
	static double constexpr HistoryDuration[] = { 60.0, 300.0, 900.0 };

public:
	struct WUnitFmt
	{
		char const* Label{ "B/s" }; // e.g., "KiB/s"
		double      Factor{ 1.0 };  // 1, 1024, 1024^2, ...
	};
	WUnitFmt UploadFmt{};
	WUnitFmt DownloadFmt{};

	WNetworkGraphWindow();

	void AddData(WBytesPerSecond Upload, WBytesPerSecond Download);
	void Draw();
};
