/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "PacketParser.hpp"

#include <tracy/Tracy.hpp>

namespace EIPv6ExtensionHeaders
{
	enum Type : uint8_t
	{
		HOP_BY_HOP = 0,
		ROUTING = 43,
		FRAGMENT = 44,
		ESP = 50,
		AH = 51,
		DESTINATION_OPTS = 60,
		MOBILITY = 135,
		HOST_IDENTITY = 139,
		SHIM6 = 140,
		NO_NEXT_HEADER = 59
	};
} // namespace EIPv6ExtensionHeaders

static uint16_t Read16(uint8_t const* Buffer)
{
	return static_cast<uint16_t>(static_cast<uint16_t>(Buffer[0]) << 8 | static_cast<uint16_t>(Buffer[1]));
}

static bool ParseIPv4(uint8_t const* Buffer, std::size_t Length, WPacketHeaderParser& Out, std::size_t& L4Offset)
{
	if (Length < 20)
	{
		return false;
	}

	uint8_t Vihl = Buffer[0];
	if (Vihl >> 4 != 4)
	{
		return false;
	}

	uint8_t Ihl = (Vihl & 0x0F) * 4;
	if (Ihl < 20 || Length < Ihl)
	{
		return false;
	}

	Out.Src.Address.Family = EIPFamily::IPv4;
	Out.Dst.Address.Family = EIPFamily::IPv4;
	Out.L4Proto = static_cast<EProtocol::Type>(Buffer[9]);

	// src/dst
	for (unsigned long i = 0; i < 4; ++i)
	{
		Out.Src.Address.Bytes[i] = Buffer[12 + i];
		Out.Dst.Address.Bytes[i] = Buffer[16 + i];
	}

	L4Offset = Ihl;
	// TCP header length fix-up when needed happens in caller
	return true;
}

static bool IsIPv6ExtensionHeader(uint8_t NextHeader)
{
	switch (static_cast<EIPv6ExtensionHeaders::Type>(NextHeader))
	{
		case EIPv6ExtensionHeaders::HOP_BY_HOP:
		case EIPv6ExtensionHeaders::ROUTING:
		case EIPv6ExtensionHeaders::FRAGMENT:
		case EIPv6ExtensionHeaders::ESP:
		case EIPv6ExtensionHeaders::AH:
		case EIPv6ExtensionHeaders::DESTINATION_OPTS:
		case EIPv6ExtensionHeaders::MOBILITY:
		case EIPv6ExtensionHeaders::HOST_IDENTITY:
		case EIPv6ExtensionHeaders::SHIM6:
		case EIPv6ExtensionHeaders::NO_NEXT_HEADER:
			return true;
		default:
			return false;
	}
}

static bool ParseIPv6(uint8_t const* Buffer, std::size_t Length, WPacketHeaderParser& Out, std::size_t& L4Offset)
{
	if (Length < 40)
	{
		return false;
	}

	if (Buffer[0] >> 4 != 6)
	{
		return false;
	}

	Out.Src.Address.Family = EIPFamily::IPv6;
	Out.Dst.Address.Family = EIPFamily::IPv6;

	// src/dst
	for (unsigned long i = 0; i < 16; ++i)
	{
		Out.Src.Address.Bytes[i] = Buffer[8 + i];
		Out.Dst.Address.Bytes[i] = Buffer[24 + i];
	}

	uint8_t     Next = Buffer[6];
	std::size_t Offset = 40;

	while (true)
	{
		if (!IsIPv6ExtensionHeader(Next))
			break;
		if (Next == EIPv6ExtensionHeaders::NO_NEXT_HEADER)
		{
			Out.L4Proto = EProtocol::Unknown;
			return false;
		}

		if (Next == EIPv6ExtensionHeaders::FRAGMENT)
		{
			if (Length < Offset + 8)
			{
				return false;
			}

			Next = Buffer[Offset]; // Next Header is first byte
			Offset += 8;
			continue;
		}

		if (Next == EIPv6ExtensionHeaders::AH)
		{
			// AH: length in 32-bit words, not counting first 2 words
			if (Length < Offset + 2)
			{
				return false;
			}
			uint8_t     HdrLenWords = Buffer[Offset + 1];
			std::size_t HdrLen = (static_cast<std::size_t>(HdrLenWords) + 2) * 4;

			if (Length < Offset + HdrLen)
			{
				return false;
			}
			Next = Buffer[Offset]; // Next Header
			Offset += HdrLen;
			continue;
		}

		if (Next == EIPv6ExtensionHeaders::ESP)
		{
			// ESP: can’t safely find ports beyond this
			Out.L4Proto = EProtocol::ESP;
			return false;
		}

		// Hop-by-Hop, Dest Options, Routing: hdr ext len in 8-byte units (not including first 8 bytes)
		if (Length < Offset + 2)
		{
			return false;
		}

		uint8_t     HdrExtLen = Buffer[Offset + 1];
		std::size_t HdrLen = (static_cast<std::size_t>(HdrExtLen) + 1) * 8;

		if (Length < Offset + HdrLen)
		{
			return false;
		}

		Next = Buffer[Offset]; // Next Header
		Offset += HdrLen;
	}

	Out.L4Proto = EProtocol::Unknown;
	L4Offset = Offset;
	return true;
}

static bool ParseL4(uint8_t const* Buffer, std::size_t Length, WPacketHeaderParser& Out, std::size_t L4Offset)
{
	if (Out.L4Proto == EProtocol::TCP)
	{
		if (Length < L4Offset + 20)
		{
			return false;
		}

		Out.Src.Port = Read16(Buffer + L4Offset);
		Out.Dst.Port = Read16(Buffer + L4Offset + 2);
		uint8_t DataOffset = (Buffer[L4Offset + 12] >> 4) * 4;

		if (DataOffset < 20 || Length < L4Offset + DataOffset)
		{
			return false;
		}

		return true;
	}

	if (Out.L4Proto == EProtocol::UDP)
	{
		if (Length < L4Offset + 8)
		{
			return false;
		}

		Out.Src.Port = Read16(Buffer + L4Offset);
		Out.Dst.Port = Read16(Buffer + L4Offset + 2);
		return true;
	}

	// ICMP/ICMPv6 or other: ports left as 0
	return true;
}

inline bool ParsePacketL3(uint8_t const* Data, std::size_t Length, WPacketHeaderParser& Out)
{
	Out = WPacketHeaderParser{};
	if (Length == 0)
	{
		return false;
	}

	std::size_t L4Offset = 0;
	if (Data[0] >> 4 == EIPFamily::IPv4)
	{
		if (!ParseIPv4(Data, Length, Out, L4Offset))
		{
			return false;
		}
	}
	else if (Data[0] >> 4 == EIPFamily::IPv6)
	{
		if (!ParseIPv6(Data, Length, Out, L4Offset))
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	if (!ParseL4(Data, Length, Out, L4Offset))
	{
		return false;
	}
	return true;
}

bool WPacketHeaderParser::ParsePacketHeader(uint8_t const* Data, std::size_t Length, WPacketHeaderParser& outPacketKey)
{
	return ParsePacketL3(Data, Length, outPacketKey);
}

bool WPacketHeaderParser::ParsePacket(uint8_t const* Data, std::size_t Length)
{
	ZoneScopedN("WPacketHeaderParser::ParsePacket");
	return ParsePacketL3(Data, Length, *this);
}