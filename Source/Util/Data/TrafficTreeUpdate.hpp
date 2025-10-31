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
	WProcessId             ProcessId{};
	std::string            ApplicationPath{};
	std::string            ApplicationName{};
	WSocketTuple           SocketTuple{};
	ESocketConnectionState ConnectionState{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, ProcessId, ApplicationName, ApplicationPath, SocketTuple, ConnectionState);
	}
};

struct WTrafficTreeUpdates
{
	std::vector<WTrafficItemId>             RemovedItems;
	std::vector<WTrafficTreeTrafficUpdate>  UpdatedItems;
	std::vector<WTrafficTreeSocketAddition> AddedSockets;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(RemovedItems, UpdatedItems, AddedSockets);
	}
};