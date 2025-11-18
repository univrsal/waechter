//
// Created by usr on 30/10/2025.
//
#pragma once
#include <memory>
#include <unordered_map>

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

struct WTupleItem : ITrafficItem
{
	WTupleItem() = default;
	// This is essentially just a traffic item

	[[nodiscard]] ETrafficItemType GetType() const override { return TI_Tuple; }

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, DownloadSpeed, UploadSpeed, TotalDownloadBytes, TotalUploadBytes);
	}
};

struct WSocketItem : ITrafficItem
{
	WSocketTuple SocketTuple{};
	std::unordered_map<WEndpoint, std::shared_ptr<WTupleItem>>
		UDPPerConnectionTraffic; // Only for UDP sockets, since they can send/receive no many addresses

	uint8_t                SocketType{};
	ESocketConnectionState ConnectionState{};

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, DownloadSpeed, UploadSpeed, TotalDownloadBytes, TotalUploadBytes, ConnectionState, SocketTuple,
			SocketType, UDPPerConnectionTraffic);
	}

	[[nodiscard]] ETrafficItemType GetType() const override { return TI_Socket; }
};
