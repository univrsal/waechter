//
// Created by usr on 12/10/2025.
//

#pragma once

#include <memory>

#include "EbpfRingBuffer.hpp"
#include "WaechterEbpf.hpp"

#ifndef PACKET_HEADER_SIZE
	#define PACKET_HEADER_SIZE 128
#endif

struct WPacketData
{
	uint8_t  RawData[PACKET_HEADER_SIZE];
	uint64_t Cookie;
	uint64_t PidTgId;
	uint64_t CGroupId;
	uint64_t Bytes;
	uint64_t Timestamp;
	uint8_t  Direction;
};

class WEbpfData
{

public:
	std::unique_ptr<WEbpfRingBuffer<WPacketData>> PacketData;

	[[nodiscard]] bool IsValid() const
	{
		return PacketData->IsValid();
	}

	void UpdateData() const
	{
		PacketData->Poll(1);
	}

	explicit WEbpfData(WWaechterEbpf const& EbpfObj);
};
