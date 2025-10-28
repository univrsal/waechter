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

public:
	ITrafficItem()
	{
		static uint64_t NextId = 1; // 0 is system/root node
		ItemId = NextId++;
	}

	WTrafficCounter const& GetTrafficCounter() const { return TrafficCounter; }

	void RefreshTrafficCounter()
	{
		TrafficCounter.Refresh();
	}

	void MarkForRemoval()
	{
		TrafficCounter.MarkForRemoval();
	}

	void CountIncomingTraffic(WBytes Bytes)
	{
		TrafficCounter.PushIncomingTraffic(Bytes);
	}

	void CountOutgoingTraffic(WBytes Bytes)
	{
		TrafficCounter.PushOutgoingTraffic(Bytes);
	}

	bool HasNewData() const
	{
		return TrafficCounter.GetState() == CS_Active;
	}

	uint64_t GetItemId() const { return ItemId; }
};