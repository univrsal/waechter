//
// Created by usr on 26/10/2025.
//

#pragma once

#include <memory>

#include "EBPFCommon.h"
#include "IPAddress.hpp"
#include "TrafficCounter.hpp"
#include "Json.hpp"
#include "TrafficItem.hpp"

namespace ESocketState
{
	enum Type
	{
		Unknown,
		Created,
		Connecting,
		Connected,
		Closed
	};
} // namespace ESocketState

class WProcessMap;

class WSocketInfo : public ITrafficItem
{
public:
	ESocketState::Type SocketState{};
	WProcessMap*       ParentProcess{};
	WSocketTuple       SocketTuple{};

	void ProcessSocketEvent(WSocketEvent const& Event);

	void ToJson(WJson::object& Json);
};