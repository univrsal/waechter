/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "EbpfData.hpp"

WEbpfData::WEbpfData(WWaechterEbpf const& EbpfObj)
{
	SocketEvents = std::make_unique<TEbpfRingBuffer<WSocketEvent>>(EbpfObj.Skeleton->maps.socket_event_ring);
	SocketRules = std::make_unique<TEbpfMap<WSocketCookie, WTrafficItemRulesBase>>(EbpfObj.Skeleton->maps.socket_rules);
	SocketMarks = std::make_unique<TEbpfMap<uint16_t, uint16_t>>(EbpfObj.Skeleton->maps.ingress_port_marks);
}