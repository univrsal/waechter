/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <array>
#include <cassert>
#include <string>
#include <cstring>
#include <optional>
#include <functional>
#ifdef _WIN32
	#include <Ws2tcpip.h>
	#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
	#include <sys/types.h>
	#include <sys/socket.h>
#endif

namespace EIPFamily
{
	enum Type : uint8_t
	{
		Unknown = 0,
		IPv4 = 4, // don't set this to AF_INET(6) packet header parsing depends on the values being 4 and 6
		IPv6 = 6
	};
} // namespace EIPFamily

namespace EProtocol
{
	enum Type : uint8_t
	{
		Unknown = 0,
		TCP = 6,
		UDP = 17,
		ICMP = 1,
		ESP = 50,
		ICMPv6 = 58
	};

	inline std::string ToString(Type Protocol)
	{
		switch (Protocol)
		{
			case EProtocol::TCP:
				return "TCP";
			case EProtocol::UDP:
				return "UDP";
			case EProtocol::ICMP:
				return "ICMP";
			case EProtocol::ESP:
				return "ESP";
			case EProtocol::ICMPv6:
				return "ICMPv6";
			default:
				return "Unknown";
		}
	}
} // namespace EProtocol

struct WIPAddress
{
	// IPv4: first 4 bytes used; IPv6: all 16 bytes used.
	std::array<uint8_t, 16> Bytes{};
	EIPFamily::Type         Family{};

	bool operator<(WIPAddress const& Other) const { return Bytes < Other.Bytes; }
	bool operator<=(WIPAddress const& Other) const { return Bytes <= Other.Bytes; }
	bool operator>=(WIPAddress const& Other) const { return Bytes >= Other.Bytes; }

	[[nodiscard]] std::string ToString() const
	{
		if (Family == EIPFamily::IPv4)
		{
			in_addr Addr4{};
			Addr4.s_addr = htonl((Bytes[0] << 24) | (Bytes[1] << 16) | (Bytes[2] << 8) | Bytes[3]);
			char        Buffer[INET_ADDRSTRLEN];
			char const* Result = inet_ntop(AF_INET, &Addr4, Buffer, INET_ADDRSTRLEN);
			if (Result)
			{
				return { Buffer };
			}
			return {};
		}

		in6_addr Addr6{};
		for (unsigned long i = 0; i < 16; ++i)
		{
			Addr6.s6_addr[i] = Bytes[i];
		}
		char        Buffer[INET6_ADDRSTRLEN];
		char const* Result = inet_ntop(AF_INET6, &Addr6, Buffer, INET6_ADDRSTRLEN);
		if (Result)
		{
			return { Buffer };
		}
		return {};
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

	[[nodiscard]] bool IsZero() const
	{
		if (Family == EIPFamily::IPv4)
		{
			return (Bytes[0] == 0 && Bytes[1] == 0 && Bytes[2] == 0 && Bytes[3] == 0);
		}

		if (Family == EIPFamily::IPv6)
		{
			for (unsigned long i = 0; i < 16; ++i)
			{
				if (Bytes[i] != 0)
					return false;
			}
			return true;
		}

		return true;
	}

	[[nodiscard]] bool IsBroadcast() const
	{
		if (Family == EIPFamily::IPv4)
		{
			return (Bytes[0] == 255 && Bytes[1] == 255 && Bytes[2] == 255 && Bytes[3] == 255);
		}

		// IPv6 has no broadcast address
		return false;
	}

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(Bytes, Family);
	}

	uint32_t ToInt() const
	{
		assert(Family == EIPFamily::IPv4);
		return (static_cast<uint32_t>(Bytes[0]) << 24) | (static_cast<uint32_t>(Bytes[1]) << 16)
			| (static_cast<uint32_t>(Bytes[2]) << 8) | static_cast<uint32_t>(Bytes[3]);
	}

	void FromIPv4Uint32(uint32_t IPv4Addr_NetworkByteOrder)
	{
		auto IPv4Addr_HostByteOrder = ntohl(IPv4Addr_NetworkByteOrder);
		Bytes[0] = static_cast<uint8_t>((IPv4Addr_HostByteOrder >> 24) & 0xFF);
		Bytes[1] = static_cast<uint8_t>((IPv4Addr_HostByteOrder >> 16) & 0xFF);
		Bytes[2] = static_cast<uint8_t>((IPv4Addr_HostByteOrder >> 8) & 0xFF);
		Bytes[3] = static_cast<uint8_t>(IPv4Addr_HostByteOrder & 0xFF);
		Family = EIPFamily::IPv4;
	}

	void FromIPv6Array(unsigned int const* IPv6Addr_NetworkByteOrder)
	{
		for (unsigned long i = 0; i < 4; i++)
		{
			Bytes[i * 4 + 0] = static_cast<uint8_t>((IPv6Addr_NetworkByteOrder[i] >> 24) & 0xFF);
			Bytes[i * 4 + 1] = static_cast<uint8_t>((IPv6Addr_NetworkByteOrder[i] >> 16) & 0xFF);
			Bytes[i * 4 + 2] = static_cast<uint8_t>((IPv6Addr_NetworkByteOrder[i] >> 8) & 0xFF);
			Bytes[i * 4 + 3] = static_cast<uint8_t>(IPv6Addr_NetworkByteOrder[i] & 0xFF);
		}
		Family = EIPFamily::IPv6;
	}

	static std::optional<WIPAddress> FromStringV4(std::string const& IPStr)
	{
		WIPAddress Addr{};
		in_addr    IPV4Addr{};

		if (inet_pton(AF_INET, IPStr.c_str(), &IPV4Addr) != 1)
		{
			return std::nullopt;
		}

		auto const* Bytes = reinterpret_cast<uint8_t const*>(&IPV4Addr.s_addr);
		Addr.Bytes[0] = Bytes[0];
		Addr.Bytes[1] = Bytes[1];
		Addr.Bytes[2] = Bytes[2];
		Addr.Bytes[3] = Bytes[3];
		Addr.Family = EIPFamily::IPv4;

		return Addr;
	}

	static std::optional<WIPAddress> FromStringV6(std::string const& IPStr)
	{
		WIPAddress Addr{};
		in6_addr   IPV6Addr{};

		if (inet_pton(AF_INET6, IPStr.c_str(), &IPV6Addr) != 1)
		{
			return std::nullopt;
		}

		for (uint8_t i = 0; i < 16; ++i)
		{
			Addr.Bytes[i] = IPV6Addr.s6_addr[i];
		}

		Addr.Family = EIPFamily::IPv6;

		return Addr;
	}

	static std::optional<WIPAddress> FromString(std::string const& IPStr)
	{
		if (IPStr.find('.') != std::string::npos && IPStr.find(':') == std::string::npos)
		{
			return FromStringV4(IPStr);
		}
		return FromStringV6(IPStr);
	}
};

inline bool operator==(WIPAddress const& Lhs, WIPAddress const& Rhs)
{
	return Lhs.Bytes == Rhs.Bytes && Lhs.Family == Rhs.Family;
}

inline bool operator!=(WIPAddress const& Lhs, WIPAddress const& Rhs)
{
	return !(Lhs == Rhs);
}

struct WEndpoint
{
	WIPAddress Address{};
	uint16_t   Port{}; // host byte order

	[[nodiscard]] std::string ToString() const
	{
		if (Address.Family == EIPFamily::IPv6)
		{
			return "[" + Address.ToString() + "]:" + std::to_string(Port);
		}
		return Address.ToString() + ":" + std::to_string(Port);
	}

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(Address, Port);
	}
};

inline bool operator==(WEndpoint const& lhs, WEndpoint const& rhs)
{
	return lhs.Address == rhs.Address && lhs.Port == rhs.Port;
}

inline bool operator!=(WEndpoint const& lhs, WEndpoint const& rhs)
{
	return !(lhs == rhs);
}

struct WSocketTuple
{
	WEndpoint       LocalEndpoint{};
	WEndpoint       RemoteEndpoint{};
	EProtocol::Type Protocol{ EProtocol::TCP };

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(LocalEndpoint, RemoteEndpoint, Protocol);
	}

	[[nodiscard]] std::string ToString() const
	{
		return LocalEndpoint.ToString() + " -> " + RemoteEndpoint.ToString() + " (" + EProtocol::ToString(Protocol)
			+ ")";
	}
};

struct ByteArray16Hash
{
	size_t operator()(std::array<uint8_t, 16> const& A) const noexcept
	{
		uint64_t P1, P2;
		std::memcpy(&P1, A.data(), 8);
		std::memcpy(&P2, A.data() + 8, 8);
		uint64_t Hash = P1 ^ (P2 + 0x9e3779b97f4a7c15ULL + (P1 << 6) + (P1 >> 2));
		return Hash;
	}
};

struct WIPAddressHash
{
	size_t operator()(WIPAddress const& Ip) const noexcept
	{
		ByteArray16Hash ByteHash;
		size_t Hbytes = ByteHash(Ip.Bytes);
		size_t Hfamily = std::hash<uint8_t>{}(Ip.Family);
		return (Hbytes ^ (Hfamily + 0x9e3779b97f4a7c15ULL + (Hbytes << 6) + (Hbytes >> 2)));
	}
};

struct WEndpointHash
{
	size_t operator()(WEndpoint const& Endpoint) const noexcept
	{
		ByteArray16Hash ByteArrayHash;

		size_t H1 = ByteArrayHash(Endpoint.Address.Bytes);
		size_t H2 = std::hash<uint8_t>{}(Endpoint.Address.Family);
		size_t H3 = std::hash<uint16_t>{}(Endpoint.Port);
		size_t Combined = H1;
		Combined = Combined * 31 + H2;
		Combined = Combined * 31 + H3;
		return Combined;
	}
};

namespace std
{
	template <>
	struct hash<WIPAddress>
	{
		size_t operator()(WIPAddress const& Ip) const noexcept
		{
			WIPAddressHash Hash;
			return Hash(Ip);
		}
	};

	template <>
	struct hash<WEndpoint>
	{
		size_t operator()(WEndpoint const& ep) const noexcept
		{
			WEndpointHash h;
			return h(ep);
		}
	};
} // namespace std
