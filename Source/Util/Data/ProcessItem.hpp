//
// Created by usr on 30/10/2025.
//

#pragma once
#include <string>
#include <unordered_map>

#include "TrafficItem.hpp"
#include "SocketItem.hpp"
#include "Types.hpp"

class WProcessItem : public ITrafficItem
{
	WProcessId ProcessId{};

	std::unordered_map<WSocketCookie, std::shared_ptr<WSocketItem>> Sockets;

public:
	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, ProcessId);
	}
};
