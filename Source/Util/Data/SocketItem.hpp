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

namespace ESocketType
{
	// UDP sockets can both be listening and connected at the same time
	// so we use bitflags here
	enum Type : uint8_t
	{
		Unknown = 0,
		Connect = 1 << 0, /* Outgoing tcp/udp */
		Listen = 1 << 1,  /* i.e. tcp server socket */
		Accept = 1 << 2   /* Incoming udp (and tcp?) */
	};
} // namespace ESocketType

struct WSocketItem : ITrafficItem
{
	WSocketTuple           SocketTuple{};
	uint8_t                SocketType{};
	ESocketConnectionState ConnectionState{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, DownloadSpeed, UploadSpeed, TotalDownloadBytes, TotalUploadBytes, ConnectionState, SocketTuple,
			SocketType);
	}

	[[nodiscard]] ETrafficItemType GetType() const override { return TI_Socket; }
};
