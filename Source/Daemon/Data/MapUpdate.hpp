/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <vector>
#include <memory>
#include <mutex>

#include "MemoryStats.hpp"
#include "Data/TrafficTreeUpdate.hpp"
#include "Data/Counters.hpp"

class WMapUpdate : public IMemoryTrackable
{
	std::vector<WTrafficItemId>                                       MarkedForRemovalItems{};
	std::vector<WTrafficItemId>                                       RemovedItems{};
	std::vector<WTrafficTreeSocketStateChange>                        SocketStateChanges{};
	std::vector<std::shared_ptr<WSocketCounter>>                      AddedSockets{};
	std::vector<std::pair<WEndpoint, std::shared_ptr<WTupleCounter>>> AddedTuples{};

	std::mutex Mutex;

	WTrafficTreeUpdates Updates;

	void Clear()
	{
		MarkedForRemovalItems.clear();
		RemovedItems.clear();
		SocketStateChanges.clear();
		AddedSockets.clear();
		AddedTuples.clear();

		MarkedForRemovalItems.shrink_to_fit();
		RemovedItems.shrink_to_fit();
		SocketStateChanges.shrink_to_fit();
		AddedSockets.shrink_to_fit();
		AddedTuples.shrink_to_fit();
	}

	static bool TrackUpdates();

public:
	WMapUpdate() = default;

	void MarkItemForRemoval(WTrafficItemId ItemId)
	{
		if (!TrackUpdates())
		{
			return;
		}
		std::scoped_lock Lock(Mutex);
		MarkedForRemovalItems.emplace_back(ItemId);
	}

	bool HasUpdates() const
	{
		return !RemovedItems.empty() || !MarkedForRemovalItems.empty() || !SocketStateChanges.empty()
			|| !AddedSockets.empty() || !AddedTuples.empty();
	}

	void AddStateChange(WTrafficItemId Id, ESocketConnectionState NewState, uint8_t SocketType,
		std::optional<WSocketTuple> const& SocketTuple = std::nullopt);

	void AddTupleAddition(WEndpoint const& Endpoint, std::shared_ptr<WTupleCounter> const& Tuple)
	{
		if (!TrackUpdates())
		{
			return;
		}
		std::scoped_lock Lock(Mutex);
		AddedTuples.emplace_back(Endpoint, Tuple);
	}

	void AddSocketAddition(std::shared_ptr<WSocketCounter> const& Socket)
	{
		if (!TrackUpdates())
		{
			return;
		}
		AddedSockets.emplace_back(Socket);
	}

	void AddItemRemoval(WTrafficItemId ItemId)
	{
		if (!TrackUpdates())
		{
			return;
		}
		std::scoped_lock Lock(Mutex);
		RemovedItems.emplace_back(ItemId);
	}

	WTrafficTreeUpdates const& GetUpdates();

	WMemoryStat GetMemoryUsage() override;
};
