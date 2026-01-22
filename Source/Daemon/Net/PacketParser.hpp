/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "IPAddress.hpp"

struct WPacketHeaderParser
{
	EProtocol::Type L4Proto{ 0 };

	WEndpoint Src;
	WEndpoint Dst;

	static bool ParsePacketHeader(uint8_t const* Data, std::size_t Length, WPacketHeaderParser& OutHeader);

	bool ParsePacket(uint8_t const* Data, std::size_t Length);
};
