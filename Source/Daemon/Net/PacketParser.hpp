//
// Created by usr on 23/10/2025.

#pragma once
#include <cstdint>

#include "IPAddress.hpp"

struct WPacketHeaderParser
{
	EProtocol::Type L4Proto{ 0 };

	WEndpoint Src;
	WEndpoint Dst;

	static bool ParsePacketHeader(uint8_t const* Data, std::size_t Length, WPacketHeaderParser& OutHeader);

	bool ParsePacket(uint8_t const* Data, std::size_t Length);
};
