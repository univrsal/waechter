//
// Created by usr on 31/10/2025.
//

#pragma once
#include <optional>

#include "IPAddress.hpp"
#include "SocketItem.hpp"
#include "TrafficItem.hpp"

struct WTrafficTreeTrafficUpdate
{
	WTrafficItemId  ItemId{};
	WBytesPerSecond NewDownloadSpeed{};
	WBytesPerSecond NewUploadSpeed{};
	WBytes          TotalDownloadBytes{};
	WBytes          TotalUploadBytes{};

	WTrafficTreeTrafficUpdate() = default;

	explicit WTrafficTreeTrafficUpdate(ITrafficItem const& Item)
		: ItemId(Item.ItemId)
		, NewDownloadSpeed(Item.DownloadSpeed)
		, NewUploadSpeed(Item.UploadSpeed)
		, TotalDownloadBytes(Item.TotalDownloadBytes)
		, TotalUploadBytes(Item.TotalUploadBytes)
	{
	}

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, NewDownloadSpeed, NewUploadSpeed, TotalDownloadBytes, TotalUploadBytes);
	}
};

struct WTrafficTreeSocketAddition
{
	WTrafficItemId         ItemId{};
	WTrafficItemId         ProcessItemId{};
	WTrafficItemId         ApplicationItemId{};
	WProcessId             ProcessId{};
	std::string            ApplicationPath{};
	std::string            ApplicationName{};
	std::string            ApplicationCommandLine{};
	WSocketTuple           SocketTuple{};
	ESocketConnectionState ConnectionState{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, ProcessItemId, ApplicationItemId, ProcessId, ApplicationName, ApplicationPath,
			ApplicationCommandLine, SocketTuple, ConnectionState);
	}
};

struct WTrafficTreeTupleAddition
{
	WTrafficItemId ItemId{};
	WTrafficItemId SocketItemId{};
	WSocketTuple   SocketTuple{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, SocketItemId, SocketTuple);
	}
};

struct WTrafficTreeSocketStateChange
{
	WTrafficItemId              ItemId{};
	ESocketConnectionState      NewState{};
	uint8_t                     SocketType{};
	std::optional<WSocketTuple> SocketTuple{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, NewState, SocketType, SocketTuple);
	}
};

struct WTrafficTreeUpdates
{
	std::vector<WTrafficItemId>                MarkedForRemovalItems;
	std::vector<WTrafficItemId>                RemovedItems;
	std::vector<WTrafficTreeTrafficUpdate>     UpdatedItems;
	std::vector<WTrafficTreeSocketAddition>    AddedSockets;
	std::vector<WTrafficTreeTupleAddition>     AddedTuples;
	std::vector<WTrafficTreeSocketStateChange> SocketStateChange;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(RemovedItems, MarkedForRemovalItems, UpdatedItems, AddedSockets, AddedTuples, SocketStateChange);
	}

	void Reset()
	{
		MarkedForRemovalItems.clear();
		RemovedItems.clear();
		UpdatedItems.clear();
		AddedSockets.clear();
		AddedTuples.clear();
		SocketStateChange.clear();
	}
};