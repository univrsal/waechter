/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ConnectionHistory.hpp"

#include "spdlog/spdlog.h"

#include "Counters.hpp"
#include "DaemonConfig.hpp"
#include "NetworkEvents.hpp"
#include "SystemMap.hpp"

bool WConnectionHistoryEntry::Update()
{
	if (!Set || !App)
	{
		return false;
	}

	if (Set->Connections.empty())
	{
		// technically we should set this when the socket actually closes, but this is close enough
		EndTime = WTime::GetEpochSeconds();
		return true;
	}

	bool bChanged = false;

	auto const OldDataIn = DataIn;
	auto const OldDataOut = DataOut;
	// Sum up data from all connections in the set,
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
	// Generate a unique ID for this connection history entry,
	// no relation to any traffic items
	ConnectionId = WSystemMap::GetInstance().GetNextItemId();
}

void WConnectionHistory::OnSocketConnected(WSocketCounter const* SocketCounter)
{
	if (SocketCounter->TrafficItem->SocketTuple.Protocol != EProtocol::TCP)
	{
		// only track TCP connections here, UDP is handled via tuples
		return;
	}

	auto const App = SocketCounter->ParentProcess->ParentApp;
	auto AppName = App->TrafficItem->ApplicationName;
	auto       Endpoint = SocketCounter->TrafficItem->SocketTuple.RemoteEndpoint;

	std::scoped_lock Lock(Mutex);

	auto const Key = std::make_pair(AppName, Endpoint);

	auto const bHaveConnection = ActiveConnections.contains(Key);

	if (bHaveConnection && ActiveConnections[Key]->Connections.contains(SocketCounter->TrafficItem))
	{
		// already tracked
		return;
	}

	if (!bHaveConnection)
	{
		// insert the new connection set
		auto const NewSet = std::make_shared<WConnectionSet>();
		NewSet->Connections.insert(SocketCounter->TrafficItem);
		ActiveConnections[Key] = NewSet;
		Push(App, NewSet, Endpoint);
		return;
	}
	// add the new tuple to the existing connection set
	ActiveConnections[Key]->Connections.insert(SocketCounter->TrafficItem);
}

void WConnectionHistory::OnSocketRemoved(std::shared_ptr<WSocketCounter> const& SocketCounter)
{
	std::scoped_lock Lock(Mutex);
	auto const       App = SocketCounter->ParentProcess->ParentApp;
	if (!App)
	{
		spdlog::error("No app for socket {}", SocketCounter->TrafficItem->ItemId);
		return;
	}
	auto             AppName = App->TrafficItem->ApplicationName;
	auto             Endpoint = SocketCounter->TrafficItem->SocketTuple.RemoteEndpoint;
	auto const       Key = std::make_pair(AppName, Endpoint);

	if (ActiveConnections.contains(Key))
	{
		auto const ConnectionSet = ActiveConnections[Key];
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
		// the tuples can have different remote endpoints,
		// so we have to look them up individually
		auto const TupleKey = std::make_pair(AppName, TupleEndpoint);
		if (!ActiveConnections.contains(TupleKey))
		{
			continue;
		}
		auto const ConnectionSet = ActiveConnections[TupleKey];
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
	auto const App = TupleCounter->ParentSocket->ParentProcess->ParentApp;
	if (!App)
	{
		spdlog::info("No app for tuple {}", TupleCounter->TrafficItem->ItemId);
		return;
	}
	auto AppName = App->TrafficItem->ApplicationName;
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
		// insert the new connection set
		auto const NewSet = std::make_shared<WConnectionSet>();
		NewSet->Connections.insert(TupleCounter->TrafficItem);
		ActiveConnections[Key] = NewSet;
		Push(App, NewSet, Endpoint);
		return;
	}
	// add a new tuple to the existing connection set
	ActiveConnections[Key]->Connections.insert(TupleCounter->TrafficItem);
}

void WConnectionHistory::Push(std::shared_ptr<WAppCounter> const& App, std::shared_ptr<WConnectionSet> const& Set,
	WEndpoint const& RemoteEndpoint)
{
	if (WDaemonConfig::GetInstance().IsIgnoredConnectionHistoryApp(App->TrafficItem->ApplicationName)
		|| WDaemonConfig::GetInstance().IsIgnoredConnectionHistoryPort(RemoteEndpoint.Port))
	{
		return;
	}

	// no lock here, it's already acquired by the caller
	WConnectionHistoryEntry Entry{ App, Set, RemoteEndpoint };
	// todo: (maybe) the rest should be pushed into the database
	History.push_back(Entry);
	spdlog::info("New connection for app {}", App->TrafficItem->ApplicationName);
	++NewItemCounter;
	if (History.size() > kMaxHistorySize)
	{
		History.pop_front();
		// When we pop from the front, we need to adjust NewItemCounter
		// so that Update() doesn't try to iterate beyond the actual history size
		if (NewItemCounter > 0)
		{
			--NewItemCounter;
		}
	}
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
		// Ensure we don't try to iterate beyond the history size
		uint32_t ItemsToProcess = std::min(NewItemCounter, static_cast<uint32_t>(History.size()));

		auto It = History.end();
		for (uint32_t i = 0; i < ItemsToProcess; ++i)
		{
			--It;
		}
		for (; It != History.end(); ++It)
		{
			auto const&                Entry = *It;
			spdlog::debug("New connection: app='{}', remote='{}', start={}, id={}",
				Entry.App->TrafficItem->ApplicationName, Entry.RemoteEndpoint.ToString(), Entry.StartTime,
				Entry.ConnectionId);
			assert(Entry.App && Entry.Set);
			if (!Entry.App || !Entry.Set)
			{
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
		assert(Entry.App && Entry.Set);
		if (!Entry.App || !Entry.Set)
		{
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

WMemoryStat WConnectionHistory::GetMemoryUsage()
{
	std::scoped_lock Lock(Mutex);
	WMemoryStat      Stats{};
	Stats.Name = "WConnectionHistory";

	WMemoryStatEntry HistoryEntry{};
	HistoryEntry.Name = "History";
	for (auto const& E : History)
	{
		assert(E.App && E.Set);
		if (!E.App || !E.Set)
		{
			continue;
		}
		HistoryEntry.Usage += sizeof(WConnectionHistoryEntry);
		HistoryEntry.Usage += sizeof(WConnectionSet);
		HistoryEntry.Usage += sizeof(std::shared_ptr<ITrafficItem>) * E.Set->Connections.size();
	}

	WMemoryStatEntry ActiveConnectionsEntry{};
	ActiveConnectionsEntry.Name = "Active connections";
	ActiveConnectionsEntry.Usage = sizeof(decltype(ActiveConnections))
		+ ActiveConnections.size()
			* (sizeof(std::pair<std::string, WEndpoint>) + sizeof(std::shared_ptr<WConnectionSet>));

	return Stats;
}