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

class WSocketItem : public ITrafficItem
{
	WSocketTuple           SocketTuple{};
	ESocketConnectionState ConnectionState{};

public:
	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, SocketTuple, ConnectionState);
	}
};
