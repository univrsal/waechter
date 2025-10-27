//
// Created by usr on 26/10/2025.
//

#pragma once

#include <memory>

#include "EBPFCommon.h"
#include "IPAddress.hpp"
#include "TrafficCounter.hpp"
#include "Json.hpp"

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

struct WSocketInfo
{
	ESocketState::Type SocketState{};
	WProcessMap*       ParentProcess{};
	WSocketTuple       SocketTuple{};
	WTrafficCounter    TrafficCounter{};

	void ProcessSocketEvent(WSocketEvent const& Event);

	void ToJson(WJson::object& Json);
};