//
// Created by usr on 15/11/2025.
//

#pragma once
#include <vector>
#include <memory>

#include "Data/TrafficTreeUpdate.hpp"
#include "Data/Counters.hpp"

class WMapUpdate
{
	std::vector<WTrafficItemId>                  MarkedForRemovalItems{};
	std::vector<WTrafficItemId>                  RemovedItems{};
	std::vector<WTrafficTreeSocketStateChange>   SocketStateChanges{};
	std::vector<std::shared_ptr<WSocketCounter>> AddedSockets{};
	std::vector<std::shared_ptr<WTupleCounter>>  AddedTuples{};

	WTrafficTreeUpdates Updates;

	void Clear()
	{
		AddedSockets.clear();
		AddedTuples.clear();
		RemovedItems.clear();
		MarkedForRemovalItems.clear();
		SocketStateChanges.clear();
	}

public:
	WMapUpdate() = default;

	void MarkItemForRemoval(WTrafficItemId ItemId) { MarkedForRemovalItems.emplace_back(ItemId); }

	bool HasUpdates() const
	{
		return !RemovedItems.empty() || !MarkedForRemovalItems.empty() || !SocketStateChanges.empty()
			|| !AddedSockets.empty() || !AddedTuples.empty();
	}

	void AddStateChange(WTrafficItemId Id, ESocketConnectionState NewState, uint8_t SocketType,
		std::optional<WSocketTuple> const& SocketTuple = std::nullopt);

	void AddTupleAddition(std::shared_ptr<WTupleCounter> const& Tuple) { AddedTuples.emplace_back(Tuple); }

	void AddSocketAddition(std::shared_ptr<WSocketCounter> const& Socket) { AddedSockets.emplace_back(Socket); }

	void AddItemRemoval(WTrafficItemId ItemId) { RemovedItems.emplace_back(ItemId); }

	WTrafficTreeUpdates const& GetUpdates();
};
