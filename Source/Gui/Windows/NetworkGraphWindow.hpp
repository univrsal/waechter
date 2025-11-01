//
// Created by usr on 01/11/2025.
//

#pragma once
#include "Types.hpp"

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
	double           Time{};
	double           History{ 60.0 };
	WScrollingBuffer UploadBuffer{};
	WScrollingBuffer DownloadBuffer{};
	std::mutex       Mutex{};

public:
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
	}
	void Draw();
};
