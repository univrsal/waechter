//
// Created by usr on 30/10/2025.
//
#pragma once

#pragma once
#include "TrafficItem.hpp"
#include "IPAddress.hpp"

enum class ESocketConnectionState
{
	Unknown,
	Created,
	Connecting,
	Connected,
	Closed
};

enum class ESocketType
{
	Unknown,
	Connect, /* Outgoing tcp/udp */
	Listen,  /* i.e. tcp server socket */
	Accept   /* Incoming udp (and tcp?) */
};

struct WSocketItem : ITrafficItem
{
	WSocketTuple           SocketTuple{};
	ESocketType            SocketType{};
	ESocketConnectionState ConnectionState{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, DownloadSpeed, UploadSpeed, TotalDownloadBytes, TotalUploadBytes, ConnectionState, SocketTuple);
	}

	[[nodiscard]] ETrafficItemType GetType() const override
	{
		return TI_Socket;
	}
};
