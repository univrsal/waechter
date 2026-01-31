/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "MapUpdate.hpp"

#include "tracy/Tracy.hpp"

#include "Daemon.hpp"
#include "SystemMap.hpp"
#include "Net/Resolver.hpp"

bool WMapUpdate::TrackUpdates()
{
	// When there are no clients there is no point in tracking updates
	// as the first client that connects gets the entire tree sent anyway
	if (auto const Sock = WDaemon::GetInstance().GetDaemonSocket(); Sock && Sock->HasClients())
	{
		return true;
	}
	return false;
}

void WMapUpdate::AddStateChange(WTrafficItemId const Id, ESocketConnectionState const NewState,
	uint8_t const SocketType, std::optional<WSocketTuple> const& SocketTuple)
{
	if (!TrackUpdates())
	{
		return;
	}
	WTrafficTreeSocketStateChange StateChange;
	StateChange.ItemId = Id;
	StateChange.NewState = NewState;
	StateChange.SocketType = SocketType;
	StateChange.SocketTuple = SocketTuple;

	SocketStateChanges.emplace_back(StateChange);
}

template <typename K, typename V, typename U>
void FetchActiveCounters(std::unordered_map<K, std::shared_ptr<V>> const& Counters, std::vector<U>& OutUpdatedItems)
{
	ZoneScopedN("FetchActiveCounters");
	for (auto const& CounterPair : Counters | std::views::values)
	{
		if (CounterPair->IsActive())
		{
			OutUpdatedItems.emplace_back(*CounterPair->TrafficItem);
		}
	}
}

WTrafficTreeUpdates const& WMapUpdate::GetUpdates()
{
	auto& SM = WSystemMap::GetInstance();
	Updates.Reset();
	Updates.RemovedItems = RemovedItems;
	Updates.MarkedForRemovalItems = MarkedForRemovalItems;
	Updates.SocketStateChange = SocketStateChanges;

	if (SM.TrafficCounter.IsActive())
	{
		Updates.UpdatedItems.emplace_back(*SM.SystemItem);
	}

	FetchActiveCounters(SM.Applications, Updates.UpdatedItems);
	FetchActiveCounters(SM.Processes, Updates.UpdatedItems);

	for (auto const& Socket : SM.Sockets | std::views::values)
	{
		if (!Socket->IsActive())
		{
			continue;
		}

		Updates.UpdatedItems.emplace_back(*Socket->TrafficItem);

		if (Socket->TrafficItem->SocketTuple.Protocol == EProtocol::UDP)
		{
			for (auto const& TupleCounter : Socket->UDPPerConnectionCounters | std::views::values)
			{
				if (TupleCounter->IsActive())
				{
					Updates.UpdatedItems.emplace_back(*TupleCounter->TrafficItem);
				}
			}
		}
	}

	for (auto const& Socket : AddedSockets)
	{
		if (Socket->GetState() == CS_PendingRemoval)
		{
			// No point in sending additions for sockets that are being removed
			continue;
		}

		WTrafficTreeSocketAddition Addition{};

		auto const PPTI = Socket->ParentProcess->TrafficItem;
		auto const PATI = Socket->ParentProcess->ParentApp->TrafficItem;

		Addition.ItemId = Socket->TrafficItem->ItemId;
		Addition.ProcessItemId = PPTI->ItemId;
		Addition.ApplicationItemId = PATI->ItemId;
		Addition.ProcessId = PPTI->ProcessId;
		Addition.ApplicationPath = PATI->ApplicationPath;
		Addition.ApplicationName = PATI->ApplicationName;
		Addition.ApplicationCommandLine = PATI->ApplicationCommandLine;
		{
			ZoneScopedN("ResolveAddress");
			Addition.ResolvedAddress =
				WResolver::GetInstance().Resolve(Socket->TrafficItem->SocketTuple.RemoteEndpoint.Address);
		}
		Addition.SocketTuple = Socket->TrafficItem->SocketTuple;
		Addition.ConnectionState = Socket->TrafficItem->ConnectionState;
		Updates.AddedSockets.emplace_back(Addition);
	}

	for (auto const& TupleCounter : AddedTuples)
	{
		if (TupleCounter.second->ParentSocket->GetState() == CS_PendingRemoval)
		{
			// No point in sending additions for tuples whose parent socket is being removed
			continue;
		}

		WTrafficTreeTupleAddition Addition{};

		Addition.ItemId = TupleCounter.second->TrafficItem->ItemId;
		Addition.SocketItemId = TupleCounter.second->ParentSocket->TrafficItem->ItemId;
		Addition.Endpoint = TupleCounter.first;
		{
			ZoneScopedN("ResolveAddress");
			Addition.ResolvedAddress = WResolver::GetInstance().Resolve(Addition.Endpoint.Address);
		}
	}

	Clear();
	return Updates;
}

WMemoryStat WMapUpdate::GetMemoryUsage()
{
	WMemoryStat Stats{};
	Stats.Name = "WMapUpdate";

	Stats.ChildEntries.emplace_back(WMemoryStatEntry{
		.Name = "MarkedForRemovalItems", .Usage = sizeof(WTrafficItemId) * MarkedForRemovalItems.capacity() });
	Stats.ChildEntries.emplace_back(
		WMemoryStatEntry{ .Name = "RemovedItems", .Usage = sizeof(WTrafficItemId) * RemovedItems.capacity() });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{
		.Name = "SocketStateChanges", .Usage = sizeof(WTrafficTreeSocketStateChange) * SocketStateChanges.capacity() });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{
		.Name = "AddedSockets", .Usage = sizeof(std::shared_ptr<WSocketCounter>) * AddedSockets.capacity() });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{ .Name = "AddedTuples",
		.Usage = sizeof(std::pair<WEndpoint, std::shared_ptr<WTupleCounter>>) * AddedTuples.capacity() });

	Stats.ChildEntries.emplace_back(WMemoryStatEntry{
		.Name = "Updates.UpdatedItems", .Usage = sizeof(WTrafficTreeTrafficUpdate) * Updates.UpdatedItems.capacity() });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{ .Name = "Updates.MarkedForRemovalItems",
		.Usage = sizeof(WTrafficItemId) * Updates.MarkedForRemovalItems.capacity() });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{
		.Name = "Updates.RemovedItems", .Usage = sizeof(WTrafficItemId) * Updates.RemovedItems.capacity() });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{ .Name = "Updates.SocketStateChange",
		.Usage = sizeof(WTrafficTreeSocketStateChange) * Updates.SocketStateChange.capacity() });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{ .Name = "Updates.AddedSockets",
		.Usage = sizeof(WTrafficTreeSocketAddition) * Updates.AddedSockets.capacity() });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{
		.Name = "Updates.AddedTuples", .Usage = sizeof(WTrafficTreeTupleAddition) * Updates.AddedTuples.capacity() });
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{
		.Name = "Updates.UpdatedItems", .Usage = sizeof(WTrafficTreeTrafficUpdate) * Updates.UpdatedItems.capacity() });

	return Stats;
}