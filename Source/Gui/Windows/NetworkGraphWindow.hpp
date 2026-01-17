/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <cmath>
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

class WNetworkGraphWindow
{
	double Time{};
	double History{ 60.0 };
	int    HistoryIndex{ 0 }; // 0=1min, 1=5min, 2=15min

	float            LineWidth{ 1.0f };
	WBytesPerSecond  CurrentMaxUploadRate{};
	WBytesPerSecond  CurrentMaxDownloadRate{};
	WScrollingBuffer UploadBuffer{ 900 }; // 15 minutes at 1 sample/sec
	WScrollingBuffer DownloadBuffer{ 900 };
	std::mutex       Mutex{};

public:
	struct WUnitFmt
	{
		char const* Label{ "B/s" }; // e.g., "KiB/s"
		double      Factor{ 1.0 };  // 1, 1024, 1024^2, ...
	};
	WUnitFmt UploadFmt{};
	WUnitFmt DownloadFmt{};

	WNetworkGraphWindow()
	{
		// Init
		UploadBuffer.AddPoint(0, 0);
		DownloadBuffer.AddPoint(0, 0);
	}

	void AddData(WBytesPerSecond Upload, WBytesPerSecond Download)
	{
		std::lock_guard Lock(Mutex);
		UploadBuffer.AddPoint(static_cast<float>(Time), static_cast<float>(Upload));
		DownloadBuffer.AddPoint(static_cast<float>(Time), static_cast<float>(Download));

		CurrentMaxUploadRate = 0.0;
		CurrentMaxDownloadRate = 0.0;
		double const T0 = Time - History;

		auto CalcMaxRate = [&](auto const& Buffer, WBytesPerSecond& MaxRate) {
			for (auto const& Point : Buffer.Data)
			{
				if (std::isfinite(static_cast<double>(Point.y)) && static_cast<double>(Point.x) >= T0)
				{
					if (static_cast<double>(Point.y) > MaxRate)
					{
						MaxRate = static_cast<double>(Point.y);
					}
				}
			}
		};

		CalcMaxRate(UploadBuffer, CurrentMaxUploadRate);
		CalcMaxRate(DownloadBuffer, CurrentMaxDownloadRate);

		// Choose unit and factor (binary prefixes) independently for each
		auto ChooseUnit = [](WBytesPerSecond Rate) -> WUnitFmt {
			if (Rate >= 1 WGiB)
			{
				return { "GiB/s", 1 WGiB };
			}
			if (Rate >= 1 WMiB)
			{
				return { "MiB/s", 1 WMiB };
			}
			if (Rate >= 1 WKiB)
			{
				return { "KiB/s", 1 WKiB };
			}
			return { "B/s", 1.0 };
		};

		UploadFmt = ChooseUnit(CurrentMaxUploadRate);
		DownloadFmt = ChooseUnit(CurrentMaxDownloadRate);
	}
	void Draw();
};
