/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ConnectionHistory.hpp"

#include <ranges>

#include "spdlog/spdlog.h"
#include "sqlpp11/sqlpp11.h"

#include "Counters.hpp"
#include "DaemonConfig.hpp"
#include "IP2Asn.hpp"
#include "NetworkEvents.hpp"
#include "SystemMap.hpp"

#include "Db/DbManager.hpp"
#include "Db/Schema.hpp"

bool WConnectionHistoryEntry::Update()
{
	auto const SetPin = Set.lock();
	if (!SetPin || !App)
	{
		if (State == EState::Pending_Close)
		{
			State = EState::Closed;
			return true;
		}
		return false;
	}

	if (SetPin->Connections.empty())
	{
		if (State == EState::Pending_Close)
		{
			State = EState::Closed;
			return true;
		}
		return false;
	}

	bool bChanged = false;

	auto const OldDataIn = DataIn;
	auto const OldDataOut = DataOut;
	// Sum up data from all connections in the set,
	// including past connections that have disconnected and
	// are now accounted for in BaseDataIn/Out
	DataIn = SetPin->BaseDataIn;
	DataOut = SetPin->BaseDataOut;

	for (auto const& Connection : SetPin->Connections)
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

WConnectionHistoryEntry::WConnectionHistoryEntry(std::shared_ptr<WAppCounter> const& App_,
	std::shared_ptr<WConnectionSet> const& Set_, WEndpoint const& RemoteEndpoint_)
	: App(App_), Set(Set_), RemoteEndpoint(RemoteEndpoint_)
{
	// Generate a unique ID for this connection history entry,
	// no relation to any traffic items
	ConnectionId = WSystemMap::GetInstance().GetNextItemId();
}

void WConnectionHistory::AddToConnectionSet(WConnectionKey const& Key, std::shared_ptr<ITrafficItem> const& Item,
	std::shared_ptr<WAppCounter> const& App, WEndpoint const& RemoteEndpoint)
{
	if (auto const It = RegisteredItems.find(Item->ItemId); It != RegisteredItems.end())
	{
		if (It->second == Key)
		{
			// already tracked
			return;
		}
		// The item was registered under a different key before (e.g. bind event before
		// the connection was established), move it over to the new set
		spdlog::debug("ConnectionHistory: Item {} moved from {} to {}", Item->ItemId, It->second.second.ToString(),
			Key.second.ToString());
		RemoveFromConnectionSet(Item);
	}

	auto& Set = ActiveConnections[Key];
	if (!Set)
	{
		// insert the new connection set
		Set = std::make_shared<WConnectionSet>();
		Set->ParentEntry = Push(App, Set, RemoteEndpoint);
	}
	Set->Connections.insert(Item);
	RegisteredItems[Item->ItemId] = Key;
}

void WConnectionHistory::RemoveFromConnectionSet(std::shared_ptr<ITrafficItem> const& Item)
{
	auto const It = RegisteredItems.find(Item->ItemId);
	if (It == RegisteredItems.end())
	{
		// not tracked
		return;
	}
	auto const Key = std::move(It->second);
	RegisteredItems.erase(It);

	auto const SetIt = ActiveConnections.find(Key);
	if (SetIt == ActiveConnections.end())
	{
		return;
	}
	auto const ConnectionSet = SetIt->second;
	ConnectionSet->BaseDataIn += Item->TotalDownloadBytes;
	ConnectionSet->BaseDataOut += Item->TotalUploadBytes;
	ConnectionSet->Connections.erase(Item);

	// The history entry will still maintain a reference until it's popped from the deque
	if (ConnectionSet->Connections.empty())
	{
		ActiveConnections.erase(SetIt);
		HandleEmptySet(ConnectionSet);
	}
}

void WConnectionHistory::OnSocketConnected(WSocketCounter const* SocketCounter)
{
	if (SocketCounter->TrafficItem->SocketTuple.Protocol != EProtocol::TCP)
	{
		// only track TCP connections here, UDP is handled via tuples
		return;
	}

	auto const App = SocketCounter->ParentProcess->ParentApp;
	auto const Endpoint = SocketCounter->TrafficItem->SocketTuple.RemoteEndpoint;

	std::scoped_lock Lock(Mutex);
	AddToConnectionSet(std::make_pair(App->TrafficItem->ApplicationPath, Endpoint), SocketCounter->TrafficItem, App,
		Endpoint);
}

void WConnectionHistory::OnSocketRemoved(std::shared_ptr<WSocketCounter> const& SocketCounter)
{
	std::scoped_lock Lock(Mutex);
	RemoveFromConnectionSet(SocketCounter->TrafficItem);

	for (auto const& UDPCounter : SocketCounter->UDPPerConnectionCounters | std::views::values)
	{
		RemoveFromConnectionSet(UDPCounter->TrafficItem);
	}
}

void WConnectionHistory::OnUDPTupleCreated(std::shared_ptr<WTupleCounter> const& TupleCounter)
{
	auto const App = TupleCounter->ParentSocket->ParentProcess->ParentApp;
	if (!App)
	{
		spdlog::info("No app for tuple {}", TupleCounter->TrafficItem->ItemId);
		return;
	}
	auto const& Endpoint = TupleCounter->TrafficItem->Endpoint;

	std::scoped_lock Lock(Mutex);
	AddToConnectionSet(
		std::make_pair(App->TrafficItem->ApplicationPath, Endpoint), TupleCounter->TrafficItem, App, Endpoint);
}

void WConnectionHistory::OnUDPTupleRemoved(std::shared_ptr<WTupleCounter> const& TupleCounter)
{
	std::scoped_lock Lock(Mutex);
	RemoveFromConnectionSet(TupleCounter->TrafficItem);
}

std::shared_ptr<WConnectionHistoryEntry> WConnectionHistory::Push(std::shared_ptr<WAppCounter> const& App,
	std::shared_ptr<WConnectionSet> const& Set, WEndpoint const& RemoteEndpoint)
{
	if (WDaemonConfig::GetInstance().IsIgnoredConnectionHistoryApp(App->TrafficItem->ApplicationName)
		|| WDaemonConfig::GetInstance().IsIgnoredConnectionHistoryPort(RemoteEndpoint.Port))
	{
		return {};
	}

	// no lock here, it's already acquired by the caller
	auto Entry = std::make_shared<WConnectionHistoryEntry>(App, Set, RemoteEndpoint);
	// WriteToDatabase(Entry);
	History.push_back(Entry);
	spdlog::debug("New connection for app {}", App->TrafficItem->ApplicationName);
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
	return Entry;
}

void WConnectionHistory::HandleEmptySet(std::shared_ptr<WConnectionSet> const& EmptySet)
{
	if (EmptySet->ParentEntry)
	{
		if (EmptySet->ParentEntry->State >= WConnectionHistoryEntry::EState::Pending_Close)
		{
			spdlog::warn("Handle empty set called on an already closed set");
			return;
		}
		EmptySet->ParentEntry->State = WConnectionHistoryEntry::EState::Pending_Close;
		// technically we should set this when the socket actually closes, but this is close enough
		EmptySet->ParentEntry->EndTime = WTime::GetEpochSeconds();
		WriteToDatabase(EmptySet->ParentEntry);
		spdlog::debug(
			"ConnectionHistory: Connection to {} ended at {} ({} in, {} out), Set: {} refs left, entry: {} refs left",
			EmptySet->ParentEntry->RemoteEndpoint.ToString(), EmptySet->ParentEntry->EndTime, EmptySet->BaseDataIn,
			EmptySet->BaseDataOut, EmptySet.use_count(), EmptySet->ParentEntry.use_count());
		EmptySet->ParentEntry = nullptr;
	}
	else
	{
		spdlog::error("ConnectionHistory: Empty connection set has no valid parent");
	}
}

void WConnectionHistory::WriteToDatabase(std::shared_ptr<WConnectionHistoryEntry> const& Entry)
{
	WDbManager::GetInstance().Run([&](auto& DbConn) {
		constexpr Db::Schema::TrafficItem            TrafficItem;
		constexpr Db::Schema::Host                   Host;
		constexpr Db::Schema::ConnectionHistoryEntry CHE;
		constexpr Db::Schema::Asn                    Asn;

		auto const Set = Entry->Set.lock();
		if (!Set)
		{
			assert(false && "ConnectionHistory: Attempted to write to database with a null connection set");
			spdlog::error("ConnectionHistory: Attempted to write to database with a null connection set");
			return;
		}

		auto AppResult = DbConn(sqlpp::select(TrafficItem.ID)
				.from(TrafficItem)
				.where(TrafficItem.Name == Entry->App->TrafficItem->ApplicationPath));

		int64_t const AppID = AppResult.empty()
			? static_cast<int64_t>(DbConn(
				  sqlpp::insert_into(TrafficItem).set(TrafficItem.Name = Entry->App->TrafficItem->ApplicationPath)))
			: AppResult.front().ID.value();

		auto const HostResult = DbConn(
			sqlpp::select(Host.ID).from(Host).where(Host.IPAddress == Entry->RemoteEndpoint.Address.GetBytesVector()));
		int64_t HostID = 0;

		if (HostResult.empty())
		{
			auto const AsnResult = WIP2Asn::GetInstance().LookupSync(Entry->RemoteEndpoint.Address);
			int64_t    AsnID = 0;
			if (AsnResult)
			{
				auto AsnIdResult = DbConn(sqlpp::select(Asn.ID).from(Asn).where(Asn.Number == AsnResult->ASN
					&& Asn.Country == AsnResult->Country && Asn.Organization == AsnResult->Organization));
				if (AsnIdResult.empty())
				{
					AsnID = static_cast<int64_t>(DbConn(sqlpp::insert_into(Asn).set(Asn.Number = AsnResult->ASN,
						Asn.Country = AsnResult->Country, Asn.Organization = AsnResult->Organization)));
					spdlog::debug("Inserted new ASN {} into database with ID {}", AsnResult->ASN, AsnID);
				}
				else
				{
					AsnID = AsnIdResult.front().ID.value();
				}
			}

			int32_t Family = Entry->RemoteEndpoint.Address.Family;
			if (Family == 0)
			{
				if (!Entry->RemoteEndpoint.Address.IsZero())
				{
					spdlog::warn("Remote endpoint {} has unknown address family but is not a zero address",
						Entry->RemoteEndpoint.ToString());
				}
				Family = 4;
			}

			if (AsnID > 0)
			{
				HostID = static_cast<int64_t>(
					DbConn(sqlpp::insert_into(Host).set(Host.IPAddress = Entry->RemoteEndpoint.Address.GetBytesVector(),
						Host.Family = Family, Host.AsnID = AsnID)));
			}
			else
			{
				HostID = static_cast<int64_t>(
					DbConn(sqlpp::insert_into(Host).set(Host.IPAddress = Entry->RemoteEndpoint.Address.GetBytesVector(), Host.Family = Family)));
			}
		}
		else
		{
			HostID = HostResult.front().ID.value();
		}
		DbConn(sqlpp::insert_into(CHE).set(CHE.ItemID = AppID, CHE.RemoteHostID = HostID,
			CHE.Port = Entry->RemoteEndpoint.Port, CHE.StartTime = Entry->StartTime, CHE.EndTime = Entry->EndTime,
			CHE.DataIn = Set->BaseDataIn, CHE.DataOut = Set->BaseDataOut));
	});
}

void WConnectionHistory::RegisterSignalHandlers()
{
	WNetworkEvents::GetInstance().OnUDPTupleCreated.connect(
		[this](std::shared_ptr<WTupleCounter> const& TupleCounter) { OnUDPTupleCreated(TupleCounter); });

	WNetworkEvents::GetInstance().OnUDPTupleRemoved.connect(
		[this](std::shared_ptr<WTupleCounter> const& TupleCounter) { OnUDPTupleRemoved(TupleCounter); });

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
		if (Entry->Update())
		{
			Update.Changes[Entry->ConnectionId] = WConnectionHistoryChange{
				.NewDataIn = Entry->DataIn,
				.NewDataOut = Entry->DataOut,
				.NewEndTime = Entry->EndTime,
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
				Entry->App->TrafficItem->ApplicationName, Entry->RemoteEndpoint.ToString(), Entry->StartTime,
				Entry->ConnectionId);
			assert(Entry->App);
			if (!Entry->App)
			{
				continue;
			}
			WNewConnectionHistoryEntry NewEntry{};
			NewEntry.AppId = Entry->App->TrafficItem->ItemId;
			NewEntry.RemoteEndpoint = Entry->RemoteEndpoint;
			NewEntry.StartTime = Entry->StartTime;
			NewEntry.EndTime = Entry->EndTime;
			NewEntry.DataIn = Entry->DataIn;
			NewEntry.DataOut = Entry->DataOut;
			NewEntry.ConnectionId = Entry->ConnectionId;
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
		assert(Entry->App);
		if (!Entry->App)
		{
			continue;
		}
		WNewConnectionHistoryEntry NewEntry{};
		NewEntry.AppId = Entry->App->TrafficItem->ItemId;
		NewEntry.RemoteEndpoint = Entry->RemoteEndpoint;
		NewEntry.StartTime = Entry->StartTime;
		NewEntry.EndTime = Entry->EndTime;
		NewEntry.DataIn = Entry->DataIn;
		NewEntry.DataOut = Entry->DataOut;
		NewEntry.ConnectionId = Entry->ConnectionId;
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
		assert(E->App);
		if (!E->App)
		{
			continue;
		}
		HistoryEntry.Usage += sizeof(WConnectionHistoryEntry);
		HistoryEntry.Usage += sizeof(WConnectionSet);
		if (auto const Set = E->Set.lock())
		{
			HistoryEntry.Usage += sizeof(std::shared_ptr<ITrafficItem>) * Set->Connections.size();
		}
	}

	WMemoryStatEntry ActiveConnectionsEntry{};
	ActiveConnectionsEntry.Name = "Active connections";
	ActiveConnectionsEntry.Usage = sizeof(decltype(ActiveConnections));
	for (auto const& [Key, Set] : ActiveConnections)
	{
		ActiveConnectionsEntry.Usage += sizeof(WConnectionKey) + Key.first.capacity()
			+ sizeof(std::shared_ptr<WConnectionSet>) + sizeof(WConnectionSet)
			+ Set->Connections.size() * (sizeof(std::shared_ptr<ITrafficItem>) + sizeof(void*));
	}

	WMemoryStatEntry RegisteredItemsEntry{};
	RegisteredItemsEntry.Name = "Registered items";
	RegisteredItemsEntry.Usage = sizeof(decltype(RegisteredItems));
	for (auto const& Key : RegisteredItems | std::views::values)
	{
		RegisteredItemsEntry.Usage += sizeof(WTrafficItemId) + sizeof(WConnectionKey) + Key.first.capacity();
	}

	Stats.ChildEntries.emplace_back(HistoryEntry);
	Stats.ChildEntries.emplace_back(ActiveConnectionsEntry);
	Stats.ChildEntries.emplace_back(RegisteredItemsEntry);

	return Stats;
}