//
// Created by usr on 12/10/2025.
//

#include "EbpfData.hpp"

WEbpfData::WEbpfData(WWaechterEbpf const& EbpfObj)
{
	SocketEvents = std::make_unique<TEbpfRingBuffer<WSocketEvent>>(EbpfObj.Skeleton->maps.socket_event_ring);
}