/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SocketStateParser.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>

// TCP states from Linux kernel
constexpr uint32_t TCP_ESTABLISHED = 0x01;
constexpr uint32_t TCP_LISTEN = 0x0A;

constexpr uint16_t EPHEMERAL_PORT_START = 32768;

WSocketStateParser::WSocketStateParser()
{
	ParseTcpFile("/proc/net/tcp");
	ParseTcpFile("/proc/net/tcp6");
	ParseUdpFile("/proc/net/udp");
	ParseUdpFile("/proc/net/udp6");
}

ESocketType::Type WSocketStateParser::DetermineSocketType(
	WEndpoint const& LocalEndpoint, EProtocol::Type Protocol) const
{
	if (Protocol == EProtocol::TCP)
	{
		std::ifstream File("/proc/net/tcp");
		if (File.is_open())
		{
			std::string Line;
			std::getline(File, Line); // Skip header

			while (std::getline(File, Line))
			{
				auto Result = ParseTcpLine(Line, LocalEndpoint);
				if (Result.has_value())
				{
					return Result.value();
				}
			}
		}

		File = std::ifstream("/proc/net/tcp6");
		if (File.is_open())
		{
			std::string Line;
			std::getline(File, Line);

			while (std::getline(File, Line))
			{
				auto Result = ParseTcpLine(Line, LocalEndpoint);
				if (Result.has_value())
				{
					return Result.value();
				}
			}
		}
	}
	else if (Protocol == EProtocol::UDP)
	{
		std::ifstream File("/proc/net/udp");
		if (File.is_open())
		{
			std::string Line;
			std::getline(File, Line);

			while (std::getline(File, Line))
			{
				auto Result = ParseUdpLine(Line, LocalEndpoint);
				if (Result.has_value())
				{
					return Result.value();
				}
			}
		}

		File = std::ifstream("/proc/net/udp6");
		if (File.is_open())
		{
			std::string Line;
			std::getline(File, Line);

			while (std::getline(File, Line))
			{
				auto Result = ParseUdpLine(Line, LocalEndpoint);
				if (Result.has_value())
				{
					return Result.value();
				}
			}
		}
	}

	return ESocketType::Unknown;
}

void WSocketStateParser::ParseTcpFile(std::string const& FilePath) const
{
	std::ifstream File(FilePath);
	if (!File.is_open())
	{
		return;
	}

	std::string Line;
	std::getline(File, Line); // Skip header

	while (std::getline(File, Line))
	{
		std::istringstream Iss(Line);
		std::string        Slot, LocalAddr, RemAddr, StStr;
		uint32_t           State;

		Iss >> Slot >> LocalAddr >> RemAddr >> std::hex >> State;

		if (State == TCP_LISTEN)
		{
			// Extract port from local address (format: ADDR:PORT)
			size_t ColonPos = LocalAddr.find(':');
			if (ColonPos != std::string::npos)
			{
				std::string PortStr = LocalAddr.substr(ColonPos + 1);
				uint16_t    Port = static_cast<uint16_t>(std::stoul(PortStr, nullptr, 16));
				KnownListeningPorts.insert(Port);
			}
		}
	}
}

void WSocketStateParser::ParseUdpFile(std::string const& FilePath) const
{
	std::ifstream File(FilePath);
	if (!File.is_open())
	{
		return;
	}

	std::string Line;
	std::getline(File, Line);

	while (std::getline(File, Line))
	{
		std::istringstream Iss(Line);
		std::string        Slot, LocalAddr, RemAddr;

		Iss >> Slot >> LocalAddr >> RemAddr;

		// UDP listening sockets have remote address 00000000:0000
		if (RemAddr.find("00000000:0000") == 0 || RemAddr.find("00000000000000000000000000000000:0000") == 0)
		{
			size_t ColonPos = LocalAddr.find(':');
			if (ColonPos != std::string::npos)
			{
				std::string PortStr = LocalAddr.substr(ColonPos + 1);
				uint16_t    Port = static_cast<uint16_t>(std::stoul(PortStr, nullptr, 16));
				if (Port != 0) // Ignore unbound sockets
				{
					KnownListeningPorts.insert(Port);
				}
			}
		}
	}
}

std::optional<ESocketType::Type> WSocketStateParser::ParseTcpLine(
	std::string const& Line, WEndpoint const& TargetEndpoint) const
{
	std::istringstream Iss(Line);
	std::string        Slot, LocalAddrStr, RemAddrStr;
	uint32_t           State;

	Iss >> Slot >> LocalAddrStr >> RemAddrStr >> std::hex >> State;

	WIPAddress LocalAddr;
	uint16_t   LocalPort;
	bool       bIsIPv6 = (LocalAddrStr.find(':') != LocalAddrStr.rfind(':'));

	if (!ParseAddressPort(LocalAddrStr, LocalAddr, LocalPort, bIsIPv6))
		return std::nullopt;

	if (LocalPort != TargetEndpoint.Port || TargetEndpoint.Address != LocalAddr)
	{
		return std::nullopt;
	}

	if (State == TCP_LISTEN)
	{
		return ESocketType::Listen;
	}

	if (State == TCP_ESTABLISHED)
	{
		// Heuristic: if local port is in known listening ports or well-known range, likely Accept
		if (KnownListeningPorts.contains(LocalPort) || LocalPort < 1024)
		{
			return ESocketType::Accept;
		}

		if (LocalPort >= EPHEMERAL_PORT_START)
		{
			return ESocketType::Connect;
		}
		// Ambiguous - could be either, default to Connect
		return ESocketType::Connect;
	}

	return std::nullopt;
}

std::optional<ESocketType::Type> WSocketStateParser::ParseUdpLine(
	std::string const& Line, WEndpoint const& TargetEndpoint) const
{
	std::istringstream Iss(Line);
	std::string        Slot, LocalAddrStr, RemAddrStr;

	Iss >> Slot >> LocalAddrStr >> RemAddrStr;

	WIPAddress LocalAddr;
	uint16_t   LocalPort;
	bool       bIsIPv6 = LocalAddrStr.find(':') != LocalAddrStr.rfind(':');

	if (!ParseAddressPort(LocalAddrStr, LocalAddr, LocalPort, bIsIPv6))
		return std::nullopt;

	if (LocalPort != TargetEndpoint.Port || TargetEndpoint.Address != LocalAddr)
	{
		return std::nullopt;
	}

	WIPAddress RemAddr;
	uint16_t   RemPort;

	if (!ParseAddressPort(RemAddrStr, RemAddr, RemPort, bIsIPv6))
		return std::nullopt;

	if (RemAddr.IsZero() && RemPort == 0)
	{
		// Unconnected UDP socket, likely listening
		return ESocketType::Listen;
	}

	// Connected UDP socket - use same heuristic as TCP
	if (KnownListeningPorts.contains(LocalPort) || LocalPort < 1024)
	{
		return ESocketType::Accept;
	}
	return ESocketType::Connect;
}

bool WSocketStateParser::ParseAddressPort(
	std::string const& AddrPortStr, WIPAddress& OutAddr, uint16_t& OutPort, bool bIsIPv6)
{
	size_t const ColonPos = AddrPortStr.find(':');
	if (ColonPos == std::string::npos)
		return false;

	std::string const AddrStr = AddrPortStr.substr(0, ColonPos);
	std::string const PortStr = AddrPortStr.substr(ColonPos + 1);

	// Parse port (always in hex)
	OutPort = static_cast<uint16_t>(std::stoul(PortStr, nullptr, 16));

	if (bIsIPv6)
	{
		// IPv6 address is 32 hex chars (128 bits)
		if (AddrStr.length() != 32)
		{
			return false;
		}

		// Parse as 4 uint32_t values in little-endian byte order
		uint32_t Addr6[4];
		for (size_t i = 0; i < 4; ++i)
		{
			std::string QuadStr = AddrStr.substr(i * 8, 8);
			Addr6[i] = static_cast<uint32_t>(std::stoul(QuadStr, nullptr, 16));
		}

		// Convert from little-endian to bytes
		for (size_t i = 0; i < 4; ++i)
		{
			OutAddr.Bytes[i * 4 + 0] = static_cast<uint8_t>(Addr6[i] & 0xFF);
			OutAddr.Bytes[i * 4 + 1] = static_cast<uint8_t>(Addr6[i] >> 8 & 0xFF);
			OutAddr.Bytes[i * 4 + 2] = static_cast<uint8_t>(Addr6[i] >> 16 & 0xFF);
			OutAddr.Bytes[i * 4 + 3] = static_cast<uint8_t>(Addr6[i] >> 24 & 0xFF);
		}

		OutAddr.Family = EIPFamily::IPv6;
	}
	else
	{
		// IPv4 address is 8 hex chars (32 bits) in little-endian
		if (AddrStr.length() != 8)
		{
			return false;
		}

		uint32_t const Addr4 = static_cast<uint32_t>(std::stoul(AddrStr, nullptr, 16));

		// Convert from little-endian to bytes
		OutAddr.Bytes[0] = static_cast<uint8_t>(Addr4 & 0xFF);
		OutAddr.Bytes[1] = static_cast<uint8_t>(Addr4 >> 8 & 0xFF);
		OutAddr.Bytes[2] = static_cast<uint8_t>(Addr4 >> 16 & 0xFF);
		OutAddr.Bytes[3] = static_cast<uint8_t>(Addr4 >> 24 & 0xFF);

		OutAddr.Family = EIPFamily::IPv4;
	}

	return true;
}
