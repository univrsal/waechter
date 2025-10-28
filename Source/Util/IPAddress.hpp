//
// Created by usr on 23/10/2025.
//

#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace EIPFamily
{
	enum Type : uint8_t
	{
		Unknown = 0,
		IPv4 = 4,
		IPv6 = 6
	};
} // namespace EIPFamily

namespace EProtocol
{
	enum Type : uint8_t
	{
		TCP = 6,
		UDP = 17,
		ICMP = 1,
		ICMPv6 = 58
	};
} // namespace EProtocol

struct WIPAddress
{
	// IPv4: first 4 bytes used; IPv6: all 16 bytes used.
	std::array<uint8_t, 16> Bytes;
	EIPFamily::Type         Family;

	[[nodiscard]] std::string to_string() const
	{
		if (Family == EIPFamily::IPv4)
		{
			return std::to_string(Bytes[0]) + "." + std::to_string(Bytes[1]) + "." + std::to_string(Bytes[2]) + "." + std::to_string(Bytes[3]);
		}

		char buffer[40];
		snprintf(buffer, sizeof(buffer),
			"%x:%x:%x:%x:%x:%x:%x:%x",
			(Bytes[0] << 8) | Bytes[1],
			(Bytes[2] << 8) | Bytes[3],
			(Bytes[4] << 8) | Bytes[5],
			(Bytes[6] << 8) | Bytes[7],
			(Bytes[8] << 8) | Bytes[9],
			(Bytes[10] << 8) | Bytes[11],
			(Bytes[12] << 8) | Bytes[13],
			(Bytes[14] << 8) | Bytes[15]);
		return { buffer };
	}

	[[nodiscard]] bool IsLocalhost() const
	{
		if (Family == EIPFamily::IPv4)
		{
			return (Bytes[0] == 127);
		}

		if (Family == EIPFamily::IPv6)
		{
			// ::1
			for (unsigned long i = 0; i < 15; ++i)
			{
				if (Bytes[i] != 0)
					return false;
			}
			return Bytes[15] == 1;
		}

		return false;
	}
};

struct WEndpoint
{
	WIPAddress Address{};
	uint16_t   Port{}; // host byte order

	[[nodiscard]] std::string to_string() const
	{
		if (Address.Family == EIPFamily::IPv6)
		{
			return "[" + Address.to_string() + "]:" + std::to_string(Port);
		}
		return Address.to_string() + ":" + std::to_string(Port);
	}
};

struct WSocketTuple
{
	WEndpoint       LocalEndpoint{};
	WEndpoint       RemoteEndpoint{};
	EProtocol::Type Protocol{ EProtocol::TCP };
};