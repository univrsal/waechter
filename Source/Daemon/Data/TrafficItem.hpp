//
// Created by usr on 28/10/2025.
//

#pragma once
#include "TrafficCounter.hpp"
#include "spdlog/spdlog.h"

class ITrafficItem
{
	WTrafficCounter TrafficCounter{};

protected:
	uint64_t ItemId{};
	bool     bHasNewData{};

public:
	ITrafficItem()
	{
		static uint64_t NextId = 1; // 0 is system/root node
		ItemId = NextId++;
	}

	WTrafficCounter const& GetTrafficCounter() const { return TrafficCounter; }

	void RefreshTrafficCounter()
	{
		auto OldDownloadSpeed = TrafficCounter.GetDownloadSpeed();
		auto OldUploadSpeed = TrafficCounter.GetUploadSpeed();
		TrafficCounter.Refresh();
		if (std::abs(OldDownloadSpeed - TrafficCounter.GetDownloadSpeed()) > 0.01 || std::abs(OldUploadSpeed - TrafficCounter.GetUploadSpeed()) > 0.01)
		{
			bHasNewData = true;
		}
	}

	void MarkForRemoval()
	{
		TrafficCounter.MarkForRemoval();
	}

	void CountIncomingTraffic(WBytes Bytes)
	{
		TrafficCounter.PushIncomingTraffic(Bytes);
		bHasNewData = true;
	}

	void CountOutgoingTraffic(WBytes Bytes)
	{
		TrafficCounter.PushOutgoingTraffic(Bytes);
		bHasNewData = true;
	}

	void ResetNewDataFlag()
	{
		bHasNewData = false;
	}

	bool HasNewData() const
	{
		return bHasNewData;
	}

	uint64_t GetItemId() const { return ItemId; }
};