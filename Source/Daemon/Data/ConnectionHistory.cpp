//
// Created by usr on 11/01/2026.
//

#include "ConnectionHistory.hpp"

#include "Counters.hpp"
#include "NetworkEvents.hpp"

void WConnectionHistoryEntry::Update()
{
	if (!Socket)
	{
		return;
	}

	if (Tuple)
	{
		DataIn = Tuple->TotalDownloadBytes;
		DataOut = Tuple->TotalUploadBytes;
	}
	else
	{
		DataIn = Socket->TotalDownloadBytes;
		DataOut = Socket->TotalUploadBytes;
	}

	if (Socket->ConnectionState == ESocketConnectionState::Closed)
	{
		Socket = nullptr;
		Tuple = nullptr;
		// technically we should set this when the socket actually closes but this is close enough
		EndTime = WTime::GetUnixNow();
	}
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

void WConnectionHistory::Update()
{
	std::scoped_lock Lock(Mutex);
	for (auto& Entry : History)
	{
		Entry.Update();
	}
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
		Update.NewEntries.push_back(NewEntry);
	}

	return Update;
}