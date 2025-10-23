//
// Created by usr on 12/10/2025.
//

#include "EbpfData.hpp"

WEbpfData::WEbpfData(WWaechterEbpf const& EbpfObj)
{
	PacketStatsMap = WEbpfMap<WPacketData>(EbpfObj.FindMapFd("packet_stats"));
}