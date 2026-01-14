/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <vector>
#include <unordered_map>

#include "IPAddress.hpp"
#include "TrafficItem.hpp"
#include "Types.hpp"

static constexpr std::size_t kMaxHistorySize = 512;

struct WConnectionHistoryChange
{
	WBytes NewDataIn{ 0 };
	WBytes NewDataOut{ 0 };
	WSec   NewEndTime{ 0 };

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(NewDataIn, NewDataOut, NewEndTime);
	}
};

struct WNewConnectionHistoryEntry
{
	WTrafficItemId AppId{};
	WTrafficItemId ConnectionId{};
	WEndpoint      RemoteEndpoint{};
	WSec           StartTime{};
	WSec           EndTime{ 0 };
	WBytes         DataIn{ 0 };
	WBytes         DataOut{ 0 };

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(AppId, ConnectionId, RemoteEndpoint, StartTime, EndTime, DataIn, DataOut);
	}
};

struct WConnectionHistoryUpdate
{
	std::unordered_map<WTrafficItemId, WConnectionHistoryChange> Changes;

	std::vector<WNewConnectionHistoryEntry> NewEntries;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(Changes, NewEntries);
	}
};