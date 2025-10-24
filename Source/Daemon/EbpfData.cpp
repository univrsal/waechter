//
// Created by usr on 12/10/2025.
//

#include "EbpfData.hpp"

WEbpfData::WEbpfData(WWaechterEbpf const& EbpfObj)
{
	PacketData = std::make_unique<WEbpfRingBuffer<WPacketData>>(EbpfObj.FindMapFd("packet_ring"));
}