/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

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
	std::string            ResolvedAddress{};
	WSocketTuple           SocketTuple{};
	ESocketConnectionState ConnectionState{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, ProcessItemId, ApplicationItemId, ProcessId, ApplicationName, ApplicationPath,
			ApplicationCommandLine, SocketTuple, ConnectionState, ResolvedAddress);
	}
};

struct WTrafficTreeTupleAddition
{
	WTrafficItemId ItemId{};
	WTrafficItemId SocketItemId{};
	WEndpoint      Endpoint{};
	std::string    ResolvedAddress{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, SocketItemId, Endpoint, ResolvedAddress);
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

		MarkedForRemovalItems.shrink_to_fit();
		RemovedItems.shrink_to_fit();
		UpdatedItems.shrink_to_fit();
		AddedSockets.shrink_to_fit();
		AddedTuples.shrink_to_fit();
		SocketStateChange.shrink_to_fit();
	}
};