//
// Created by usr on 12/10/2025.
//

#pragma once

#include <memory>

#include "EbpfRingBuffer.hpp"
#include "WaechterEbpf.hpp"

#include "EBPFCommon.h"

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

struct WSocketEvent
{
	uint8_t  EventType; // enum ENetEventType
	uint64_t Cookie;
	uint64_t PidTgId;
	uint64_t CgroupId;
};

class WEbpfData
{

public:
	std::unique_ptr<WEbpfRingBuffer<WPacketData>>  PacketData;
	std::unique_ptr<WEbpfRingBuffer<WSocketEvent>> SocketEvents;

	[[nodiscard]] bool IsValid() const
	{
		return PacketData && PacketData->IsValid() && SocketEvents && SocketEvents->IsValid();
	}

	void UpdateData() const
	{
		// Poll both ring buffers so callbacks deliver data into the in-memory queues.
		if (PacketData && PacketData->IsValid())
		{
			PacketData->Poll(1);
		}
		if (SocketEvents && SocketEvents->IsValid())
		{
			SocketEvents->Poll(1);
		}
	}

	explicit WEbpfData(WWaechterEbpf const& EbpfObj);
};
