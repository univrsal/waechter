//
// Created by usr on 31/10/2025.
//

#pragma once
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

struct WTrafficTreeSocketStateChange
{
	WTrafficItemId         ItemId{};
	ESocketConnectionState NewState{};
	ESocketType            SocketType{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, NewState, SocketType);
	}
};

struct WTrafficTreeUpdates
{
	std::vector<WTrafficItemId>                MarkedForRemovalItems;
	std::vector<WTrafficItemId>                RemovedItems;
	std::vector<WTrafficTreeTrafficUpdate>     UpdatedItems;
	std::vector<WTrafficTreeSocketAddition>    AddedSockets;
	std::vector<WTrafficTreeSocketStateChange> SocketStateChange;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(RemovedItems, MarkedForRemovalItems, UpdatedItems, AddedSockets, SocketStateChange);
	}
};