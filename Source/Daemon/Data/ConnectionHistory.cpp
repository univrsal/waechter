//
// Created by usr on 11/01/2026.
//

#include "ConnectionHistory.hpp"

#include <cassert>
#include <spdlog/spdlog.h>

#include "Counters.hpp"
#include "NetworkEvents.hpp"
#include "SystemMap.hpp"

#include <ranges>

bool WConnectionHistoryEntry::Update()
{
	if (!Set || !App)
	{
		return false;
	}

	if (Set->Connections.empty())
	{
		Set = nullptr;
		App = nullptr;
		// technically we should set this when the socket actually closes but this is close enough
		EndTime = WTime::GetUnixNow();
		return true;
	}

	bool bChanged = false;

	auto const OldDataIn = DataIn;
	auto const OldDataOut = DataOut;
	// Sum up data from all connections in the set
	// including past connections that have disconnected and
	// are now accounted for in BaseDataIn/Out
	DataIn = Set->BaseDataIn;
	DataOut = Set->BaseDataOut;

	for (auto const& Connection : Set->Connections)
	{
		DataIn += Connection->TotalDownloadBytes;
		DataOut += Connection->TotalUploadBytes;
	}

	if (DataIn != OldDataIn || DataOut != OldDataOut)
	{
		bChanged = true;
	}

	return bChanged;
}

WConnectionHistoryEntry::WConnectionHistoryEntry(
	std::shared_ptr<WAppCounter> App_, std::shared_ptr<WConnectionSet> Set_, WEndpoint const& RemoteEndpoint_)
	: App(std::move(App_)), Set(std::move(Set_)), RemoteEndpoint(RemoteEndpoint_)
{
	// Generate a unique ID for this connection history entry
	// no relation to any traffic items
	ConnectionId = WSystemMap::GetInstance().GetNextItemId();
}

void WConnectionHistory::OnSocketConnected(WSocketCounter const* Socket)
{
	if (Socket->TrafficItem->SocketTuple.Protocol != EProtocol::TCP)
	{
		// only track TCP connections here, UDP is handled via tuples
		return;
	}

	auto App = Socket->ParentProcess->ParentApp;
	auto AppName = App->TrafficItem->ApplicationName;
	auto Endpoint = Socket->TrafficItem->SocketTuple.RemoteEndpoint;

	std::scoped_lock Lock(Mutex);

	auto const Key = std::make_pair(AppName, Endpoint);

	auto const bHaveConnection = ActiveConnections.contains(Key);

	if (bHaveConnection && ActiveConnections[Key]->Connections.contains(Socket->TrafficItem))
	{
		// already tracked
		return;
	}

	if (!bHaveConnection)
	{
		// insert new connection set
		auto NewSet = std::make_shared<WConnectionSet>();
		NewSet->Connections.insert(Socket->TrafficItem);
		ActiveConnections[Key] = NewSet;
		Push(App, NewSet, Endpoint);
		return;
	}
	// add new tuple to existing connection set
	ActiveConnections[Key]->Connections.insert(Socket->TrafficItem);
}

void WConnectionHistory::OnSocketRemoved(std::shared_ptr<WSocketCounter> const& SocketCounter)
{
	std::scoped_lock Lock(Mutex);
	auto             App = SocketCounter->ParentProcess->ParentApp;
	auto             AppName = App->TrafficItem->ApplicationName;
	auto             Endpoint = SocketCounter->TrafficItem->SocketTuple.RemoteEndpoint;
	auto const       Key = std::make_pair(AppName, Endpoint);
	if (!App)
	{
		spdlog::error("No app for socket {}", SocketCounter->TrafficItem->ItemId);
		return;
	}

	if (ActiveConnections.contains(Key))
	{
		auto ConnectionSet = ActiveConnections[Key];
		// not tracked
		ConnectionSet->BaseDataIn += SocketCounter->TrafficItem->TotalDownloadBytes;
		ConnectionSet->BaseDataOut += SocketCounter->TrafficItem->TotalUploadBytes;
		ConnectionSet->Connections.erase(SocketCounter->TrafficItem);

		// The history entry will still maintain a reference until it's popped from the deque
		if (ConnectionSet->Connections.empty())
		{
			ActiveConnections.erase(Key);
		}
	}

	for (auto const& [TupleEndpoint, UDPCounter] : SocketCounter->UDPPerConnectionCounters)
	{
		// the tuples can have different remote endpoints
		// so we have to look them up individually
		auto const TupleKey = std::make_pair(AppName, TupleEndpoint);
		if (!ActiveConnections.contains(TupleKey))
		{
			continue;
		}
		auto ConnectionSet = ActiveConnections[TupleKey];
		ConnectionSet->BaseDataIn += UDPCounter->TrafficItem->TotalDownloadBytes;
		ConnectionSet->BaseDataOut += UDPCounter->TrafficItem->TotalUploadBytes;
		ConnectionSet->Connections.erase(UDPCounter->TrafficItem);

		if (ConnectionSet->Connections.empty())
		{
			ActiveConnections.erase(TupleKey);
		}
	}
}

void WConnectionHistory::OnUDPTupleCreated(
	std::shared_ptr<WTupleCounter> const& TupleCounter, WEndpoint const& Endpoint)
{
	auto App = TupleCounter->ParentSocket->ParentProcess->ParentApp;
	auto AppName = App->TrafficItem->ApplicationName;
	if (!App)
	{
		spdlog::info("No app for tuple {}", TupleCounter->TrafficItem->ItemId);
		return;
	}
	std::scoped_lock Lock(Mutex);

	auto const Key = std::make_pair(AppName, Endpoint);
	auto const bHaveConnection = ActiveConnections.contains(Key);

	if (bHaveConnection && ActiveConnections[Key]->Connections.contains(TupleCounter->TrafficItem))
	{
		// already tracked
		return;
	}

	if (!bHaveConnection)
	{
		// insert new connection set
		auto NewSet = std::make_shared<WConnectionSet>();
		NewSet->Connections.insert(TupleCounter->TrafficItem);
		ActiveConnections[Key] = NewSet;
		Push(App, NewSet, Endpoint);
		return;
	}
	// add new tuple to existing connection set
	ActiveConnections[Key]->Connections.insert(TupleCounter->TrafficItem);
}

void WConnectionHistory::RegisterSignalHandlers()
{
	WNetworkEvents::GetInstance().OnUDPTupleCreated.connect(
		[this](std::shared_ptr<WTupleCounter> const& TupleCounter, WEndpoint const& Endpoint) {
			OnUDPTupleCreated(TupleCounter, Endpoint);
		});

	WNetworkEvents::GetInstance().OnSocketConnected.connect(
		[this](WSocketCounter const* SocketCounter) { OnSocketConnected(SocketCounter); });

	WNetworkEvents::GetInstance().OnSocketRemoved.connect(
		[this](std::shared_ptr<WSocketCounter> const& SocketCounter) { OnSocketRemoved(SocketCounter); });
}

WConnectionHistoryUpdate WConnectionHistory::Update()
{
	WConnectionHistoryUpdate Update{};
	std::scoped_lock         Lock(Mutex);
	for (auto& Entry : History)
	{
		if (Entry.Update())
		{
			Update.Changes[Entry.ConnectionId] = WConnectionHistoryChange{
				.NewDataIn = Entry.DataIn,
				.NewDataOut = Entry.DataOut,
				.NewEndTime = Entry.EndTime,
			};
		}
	}

	// get the new items since last update
	if (NewItemCounter > 0)
	{
		auto It = History.end();
		for (uint32_t i = 0; i < NewItemCounter; ++i)
		{
			--It;
		}
		for (; It != History.end(); ++It)
		{
			auto const&                Entry = *It;
			WNewConnectionHistoryEntry NewEntry{};
			NewEntry.AppId = Entry.App->TrafficItem->ItemId;
			NewEntry.RemoteEndpoint = Entry.RemoteEndpoint;
			NewEntry.StartTime = Entry.StartTime;
			NewEntry.EndTime = Entry.EndTime;
			NewEntry.DataIn = Entry.DataIn;
			NewEntry.DataOut = Entry.DataOut;
			NewEntry.ConnectionId = Entry.ConnectionId;
			Update.NewEntries.push_back(NewEntry);
		}
		NewItemCounter = 0;
	}
	return Update;
}

WConnectionHistoryUpdate WConnectionHistory::Serialize()
{
	WConnectionHistoryUpdate Update{};
	std::scoped_lock         Lock(Mutex);
	for (auto const& Entry : History)
	{
		if (!Entry.App)
		{
			// connection exited before we could send it
			continue;
		}
		WNewConnectionHistoryEntry NewEntry{};
		NewEntry.AppId = Entry.App->TrafficItem->ItemId;
		NewEntry.RemoteEndpoint = Entry.RemoteEndpoint;
		NewEntry.StartTime = Entry.StartTime;
		NewEntry.EndTime = Entry.EndTime;
		NewEntry.DataIn = Entry.DataIn;
		NewEntry.DataOut = Entry.DataOut;
		NewEntry.ConnectionId = Entry.ConnectionId;
		Update.NewEntries.push_back(NewEntry);
	}

	return Update;
}