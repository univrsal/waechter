/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>

#include "EbpfRingBuffer.hpp"
#include "WaechterEbpf.hpp"
#include "EBPFCommon.h"
#include "EbpfMap.hpp"

class WEbpfData
{

public:
	std::unique_ptr<TEbpfRingBuffer<WSocketEvent>>                  SocketEvents;
	std::unique_ptr<TEbpfMap<WSocketCookie, WTrafficItemRulesBase>> SocketRules;
	std::unique_ptr<TEbpfMap<uint16_t, uint16_t>>                   SocketMarks;
	std::unique_ptr<TEbpfMap<uint32_t, uint32_t>>                   PidDownloadMarks;

	[[nodiscard]] bool IsValid() const { return SocketEvents && SocketEvents->IsValid(); }

	void UpdateData() const
	{
		if (SocketEvents && SocketEvents->IsValid())
		{
			SocketEvents->Poll(500);
		}
	}

	explicit WEbpfData(WWaechterEbpf const& EbpfObj);
};
