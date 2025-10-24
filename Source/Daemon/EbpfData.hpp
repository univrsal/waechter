//
// Created by usr on 12/10/2025.
//

#pragma once

#include "EbpfMap.hpp"
#include "WaechterEbpf.hpp"

#ifndef PACKET_HEADER_SIZE
#define PACKET_HEADER_SIZE 128
#endif

struct WPacketData
{
	uint8_t RawData[PACKET_HEADER_SIZE];
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
