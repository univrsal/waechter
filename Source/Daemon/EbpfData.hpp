//
// Created by usr on 12/10/2025.
//

#pragma once

#include "EbpfMap.hpp"
#include "WaechterEbpf.hpp"


struct WPacketData
{
	uint8_t RawData[256];
};

class WEbpfData
{

public:
	WEbpfMap<WPacketData> PacketStatsMap {-1};

	bool IsValid() const
	{
		return PacketStatsMap.IsValid();
	}

	void UpdateData()
	{
		PacketStatsMap.Update();
	}

	explicit WEbpfData(WWaechterEbpf const& EbpfObj);
};
