//
// Created by usr on 26/10/2025.
//

#pragma once

#include <memory>

#include "EBPFCommon.h"
#include "IPAddress.hpp"
#include "Json.hpp"
#include "TrafficItem.hpp"

enum class ESocketConnectionState
{
	Unknown,
	Created,
	Connecting,
	Connected,
	Closed
};

class WProcessMap;

class WSocketInfo : public ITrafficItem
{
public:
	ESocketConnectionState SocketState{};
	WProcessMap*           ParentProcess{};
	WSocketTuple           SocketTuple{};

	void ProcessSocketEvent(WSocketEvent const& Event);

	void ToJson(WJson::object& Json);
};