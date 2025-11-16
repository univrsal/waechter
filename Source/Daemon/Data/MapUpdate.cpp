//
// Created by usr on 15/11/2025.
//

#include "MapUpdate.hpp"

#include "SystemMap.hpp"

void WMapUpdate::AddStateChange(WTrafficItemId const Id, ESocketConnectionState const NewState,
	uint8_t const SocketType, std::optional<WSocketTuple> const& SocketTuple)
{
	WTrafficTreeSocketStateChange StateChange;
	StateChange.ItemId = Id;
	StateChange.NewState = NewState;
	StateChange.SocketType = SocketType;
	StateChange.SocketTuple = SocketTuple;

	SocketStateChanges.emplace_back(StateChange);
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
		Updates.UpdatedItems.emplace_back(WTrafficTreeTrafficUpdate{ *SM.SystemItem });
	}

	for (auto const& App : SM.Applications | std::views::values)
	{
		if (App->IsActive())
		{
			Updates.UpdatedItems.emplace_back(WTrafficTreeTrafficUpdate{ *App->TrafficItem });
		}
	}

	for (auto const& Process : SM.Processes | std::views::values)
	{
		if (Process->IsActive())
		{
			Updates.UpdatedItems.emplace_back(WTrafficTreeTrafficUpdate{ *Process->TrafficItem });
		}
	}

	for (auto const& Socket : SM.Sockets | std::views::values)
	{
		if (!Socket->IsActive())
		{
			continue;
		}

		Updates.UpdatedItems.emplace_back(WTrafficTreeTrafficUpdate{ *Socket->TrafficItem });

		if (Socket->TrafficItem->SocketTuple.Protocol == EProtocol::UDP)
		{
			for (auto const& TupleCounter : Socket->UDPPerConnectionCounters | std::views::values)
			{
				if (TupleCounter->IsActive())
				{
					Updates.UpdatedItems.emplace_back(WTrafficTreeTrafficUpdate{ *TupleCounter->TrafficItem });
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
		Updates.AddedTuples.emplace_back(Addition);
	}

	Clear();
	return Updates;
}