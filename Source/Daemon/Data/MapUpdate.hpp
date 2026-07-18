/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_set>

#include "MemoryStats.hpp"
#include "SignalHandler.hpp"
#include "Data/TrafficTreeUpdate.hpp"
#include "Data/Counters.hpp"

class WMapUpdate : public IMemoryTrackable
{
	std::vector<WTrafficItemId>                                       MarkedForRemovalItems{};
	std::vector<WTrafficItemId>                                       RemovedItems{};
	std::vector<WTrafficTreeSocketStateChange>                        SocketStateChanges{};
	std::vector<std::shared_ptr<WSocketCounter>>                      AddedSockets{};
	std::vector<std::pair<WEndpoint, std::shared_ptr<WTupleCounter>>> AddedTuples{};

	// Sometimes we create items that are part of the system map but were never sent
	// to the clients (i.e., an uninitialized socket).
	// So to prevent the daemon from sending updates for items that clients don't know
	// about, we keep a set of all items the clients have here.
	std::unordered_set<WTrafficItemId> ClientItems{};

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

	void ClearMap()
	{
		std::scoped_lock Lock(Mutex);
		Clear();
	}

	void MarkItemForRemoval(WTrafficItemId ItemId)
	{
		std::scoped_lock Lock(Mutex);
		if (!ClientItems.contains(ItemId))
		{
			spdlog::warn("Attempted to mark item '{}' for removal, but it is not tracked by the client", ItemId);
			WSignalHandler::Breakpoint();
			return;
		}
		if (!TrackUpdates())
		{
			return;
		}
		MarkedForRemovalItems.emplace_back(ItemId);
	}

	bool HasUpdates() const
	{
		return !RemovedItems.empty() || !MarkedForRemovalItems.empty() || !SocketStateChanges.empty()
			|| !AddedSockets.empty() || !AddedTuples.empty();
	}

	void AddStateChange(WTrafficItemId Id, ESocketConnectionState NewState, uint8_t SocketType,
		std::shared_ptr<WSocketTuple> const& SocketTuple = nullptr);

	void AddTupleAddition(WEndpoint const& Endpoint, std::shared_ptr<WTupleCounter> const& Tuple)
	{
		std::scoped_lock Lock(Mutex);
		ClientItems.insert(Tuple->TrafficItem->ItemId);
		if (!TrackUpdates())
		{
			return;
		}
		AddedTuples.emplace_back(Endpoint, Tuple);
	}

	void AddSocketAddition(std::shared_ptr<WSocketCounter> const& Socket)
	{
		std::scoped_lock Lock(Mutex);
		ClientItems.insert(Socket->ParentProcess->TrafficItem->ItemId);
		ClientItems.insert(Socket->ParentProcess->ParentApp->TrafficItem->ItemId);
		ClientItems.insert(Socket->TrafficItem->ItemId);
		if (!TrackUpdates())
		{
			return;
		}
		AddedSockets.emplace_back(Socket);
	}

	void AddItemRemoval(WTrafficItemId ItemId)
	{
		std::scoped_lock Lock(Mutex);
		if (!ClientItems.contains(ItemId))
		{
			spdlog::warn("Attempted to remove item '{}' for removal, but it is not tracked by the client", ItemId);
			WSignalHandler::Breakpoint();
			return;
		}
		ClientItems.erase(ItemId);
		if (!TrackUpdates())
		{
			return;
		}
		RemovedItems.emplace_back(ItemId);
	}

	WTrafficTreeUpdates const& GetUpdates();

	WMemoryStat GetMemoryUsage() override;
};
