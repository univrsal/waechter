/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "MapUpdate.hpp"

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
		Addition.ResolvedAddress =
			WResolver::GetInstance().Resolve(Socket->TrafficItem->SocketTuple.RemoteEndpoint.Address);
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
		Addition.ResolvedAddress = WResolver::GetInstance().Resolve(Addition.Endpoint.Address);
	}

	Clear();
	return Updates;
}