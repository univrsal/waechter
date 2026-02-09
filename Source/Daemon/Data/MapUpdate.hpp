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

	mutable std::mutex Mutex;

	WTrafficTreeUpdates Updates;

	void Clear()
	{
		AddedSockets.clear();
		AddedTuples.clear();
		RemovedItems.clear();
		MarkedForRemovalItems.clear();
		SocketStateChanges.clear();
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
		std::scoped_lock Lock(Mutex);
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
		std::scoped_lock Lock(Mutex);
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
