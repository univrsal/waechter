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

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, NewDownloadSpeed, NewUploadSpeed);
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
	WSocketTuple           SocketTuple{};
	ESocketConnectionState ConnectionState{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, ProcessItemId, ApplicationItemId, ProcessId, ApplicationName, ApplicationPath, SocketTuple, ConnectionState);
	}
};

struct WTrafficTreeUpdates
{
	std::vector<WTrafficItemId>             MarkedForRemovalItems;
	std::vector<WTrafficItemId>             RemovedItems;
	std::vector<WTrafficTreeTrafficUpdate>  UpdatedItems;
	std::vector<WTrafficTreeSocketAddition> AddedSockets;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(RemovedItems, MarkedForRemovalItems, UpdatedItems, AddedSockets);
	}
};