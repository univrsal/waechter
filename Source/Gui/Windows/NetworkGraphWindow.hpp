//
// Created by usr on 01/11/2025.
//

#pragma once
#include "Types.hpp"

#include <cmath>
#include <imgui.h>
#include <mutex>

struct WScrollingBuffer
{
	int              MaxSize;
	int              Offset;
	ImVector<ImVec2> Data;
	explicit WScrollingBuffer(int max_size = 2000)
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

	WBytesPerSecond  CurrentMaxRateInGraph{};
	WScrollingBuffer UploadBuffer{};
	WScrollingBuffer DownloadBuffer{};
	std::mutex       Mutex{};

public:
	struct WUnitFmt
	{
		const char* Label{ "B/s" }; // e.g., "KiB/s"
		double      Factor{ 1.0 };  // 1, 1024, 1024^2, ...
	} TrafficFmt{};

	WNetworkGraphWindow()
	{
		// Init
		UploadBuffer.AddPoint(0, 0);
		DownloadBuffer.AddPoint(0, 0);
	}

	void AddData(WBytesPerSecond Upload, WBytesPerSecond Download)
	{
		std::lock_guard lock(Mutex);
		UploadBuffer.AddPoint(static_cast<float>(Time), static_cast<float>(Upload));
		DownloadBuffer.AddPoint(static_cast<float>(Time), static_cast<float>(Download));

		CurrentMaxRateInGraph = 0.0;
		const double T0 = Time - History;

		auto CalcMaxRate = [&](const auto& Buffer) {
			for (const auto& Point : Buffer.Data)
			{
				if (std::isfinite(static_cast<double>(Point.y)) && static_cast<double>(Point.x) >= T0)
				{
					if (static_cast<double>(Point.y) > CurrentMaxRateInGraph)
					{
						CurrentMaxRateInGraph = static_cast<double>(Point.y);
					}
				}
			}
		};

		CalcMaxRate(UploadBuffer);
		CalcMaxRate(DownloadBuffer);

		// 2) Choose unit and factor (binary prefixes)
		if (CurrentMaxRateInGraph >= 1 WGiB)
			TrafficFmt = { "GiB/s", 1 WGiB };
		else if (CurrentMaxRateInGraph >= 1 WMiB)
			TrafficFmt = { "MiB/s", 1 WMiB };
		else if (CurrentMaxRateInGraph >= 1 WKiB)
			TrafficFmt = { "KiB/s", 1 WKiB };
	}
	void Draw();
};
