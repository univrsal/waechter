//
// Created by usr on 12/10/2025.
//

#pragma once
#include <memory>

#include "EbpfRingBuffer.hpp"
#include "WaechterEbpf.hpp"
#include "EBPFCommon.h"
#include "EbpfMap.hpp"

class WEbpfData
{

public:
	std::unique_ptr<TEbpfRingBuffer<WSocketEvent>>         SocketEvents;
	std::unique_ptr<TEbpfMap<WSocketCookie, WNetworkItemRules>> SocketRules;

	[[nodiscard]] bool IsValid() const { return SocketEvents && SocketEvents->IsValid(); }

	void UpdateData() const
	{
		if (SocketEvents && SocketEvents->IsValid())
		{
			SocketEvents->Poll(1);
		}
	}

	explicit WEbpfData(WWaechterEbpf const& EbpfObj);
};
