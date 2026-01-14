//
// Created by usr on 11/01/2026.
//

#include "ConnectionHistory.hpp"

#include <cassert>
#include <spdlog/spdlog.h>

#include "Counters.hpp"
#include "NetworkEvents.hpp"

bool WConnectionHistoryEntry::Update()
{
	if (!Socket)
	{
		return false;
	}
	bool bChanged = false;

	if (Tuple)
	{
		if (DataIn != Tuple->TotalDownloadBytes || DataOut != Tuple->TotalUploadBytes)
		{
			bChanged = true;
		}
		DataIn = Tuple->TotalDownloadBytes;
		DataOut = Tuple->TotalUploadBytes;
	}
	else
	{
		if (DataIn != Socket->TotalDownloadBytes || DataOut != Socket->TotalUploadBytes)
		{
			bChanged = true;
		}
		DataIn = Socket->TotalDownloadBytes;
		DataOut = Socket->TotalUploadBytes;
	}

	if (Socket->ConnectionState == ESocketConnectionState::Closed)
	{
		Socket = nullptr;
		Tuple = nullptr;
		// technically we should set this when the socket actually closes but this is close enough
		EndTime = WTime::GetUnixNow();
		bChanged = true;
	}
	return bChanged;
}

WConnectionHistoryEntry::WConnectionHistoryEntry(std::shared_ptr<WAppCounter> App_,
	std::shared_ptr<WSocketItem> Socket_, std::shared_ptr<WTupleItem> Tuple_, WEndpoint const& RemoteEndpoint_)
	: App(std::move(App_)), Socket(std::move(Socket_)), Tuple(std::move(Tuple_)), RemoteEndpoint(RemoteEndpoint_)
{
	ConectionItemId = Socket ? Socket->ItemId : (Tuple ? Tuple->ItemId : 0);
	assert(ConectionItemId != 0);
}

void WConnectionHistory::OnSocketConnected(WSocketCounter const* SocketCounter)
{
	auto App = SocketCounter->ParentProcess->ParentApp;
	Push(App, SocketCounter->TrafficItem, nullptr, SocketCounter->TrafficItem->SocketTuple.RemoteEndpoint);
}

void WConnectionHistory::OnUDPTupleCreated(
	std::shared_ptr<WTupleCounter> const& TupleCounter, WEndpoint const& Endpoint)
{
	auto App = TupleCounter->ParentSocket->ParentProcess->ParentApp;
	Push(App, TupleCounter->ParentSocket->TrafficItem, TupleCounter->TrafficItem, Endpoint);
}

void WConnectionHistory::RegisterSignalHandlers()
{
	WNetworkEvents::GetInstance().OnUDPTupleCreated.connect(
		[this](std::shared_ptr<WTupleCounter> const& TupleCounter, WEndpoint const& Endpoint) {
			OnUDPTupleCreated(TupleCounter, Endpoint);
		});

	WNetworkEvents::GetInstance().OnSocketConnected.connect(
		[this](WSocketCounter const* SocketCounter) { OnSocketConnected(SocketCounter); });
}

WConnectionHistoryUpdate WConnectionHistory::Update()
{
	WConnectionHistoryUpdate Update{};
	std::scoped_lock         Lock(Mutex);
	for (auto& Entry : History)
	{
		if (Entry.Update())
		{
			Update.Changes[Entry.ConectionItemId] = WConnectionHistoryChange{
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
			NewEntry.ConnectionId = Entry.ConectionItemId;
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
		WNewConnectionHistoryEntry NewEntry{};
		NewEntry.AppId = Entry.App->TrafficItem->ItemId;
		NewEntry.RemoteEndpoint = Entry.RemoteEndpoint;
		NewEntry.StartTime = Entry.StartTime;
		NewEntry.EndTime = Entry.EndTime;
		NewEntry.DataIn = Entry.DataIn;
		NewEntry.DataOut = Entry.DataOut;
		NewEntry.ConnectionId = Entry.ConectionItemId;
		Update.NewEntries.push_back(NewEntry);
	}

	return Update;
}