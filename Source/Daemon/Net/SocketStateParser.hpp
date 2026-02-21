/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <optional>
#include <unordered_set>
#include <mutex>

#include "IPAddress.hpp"
#include "Data/SocketItem.hpp"

// When the daemon first starts up there are often already a bunch of existing sockets
// the type of each socket is determined by the EBPF program by monitoring which syscalls (bind(), connect(), accept())
// are called in relation to it, this does not work for existing sockets so instead on startup we look at the
// filesystem `/proc/net/{tcp,udp,tcp6,udp6}` to find out which sockets already exist
// when we encounter traffic on a socket that hasn't been discovered by the EBPF program we map it by parsing the
// source/destination IP from the packet header, then we can use the local ip/port to figure out its state
// from the file system. This doesn't work for all socket types so we make some assumptions to guess the socket type
// generally the daemon should run before the network is established and therefore catches all sockets being created
// but if that is not the case we make best-effort guesses to build a full map of all existing sockets
class WSocketStateParser
{
public:
	WSocketStateParser();

	[[nodiscard]] ESocketType::Type DetermineSocketType(WEndpoint const& LocalEndpoint, EProtocol::Type Protocol) const;

	void ParseData();

	bool IsUsedPort(uint16_t Port)
	{
		if (Port == 0)
		{
			// Can't determine
			return true;
		}
		std::scoped_lock Lock(Mutex);
		return KnownUsedPorts.contains(Port);
	}
	mutable std::mutex Mutex;

	std::unordered_set<uint16_t> const& GetUsedPorts() const { return KnownUsedPorts; }

private:
	void ParseTcpFile(std::string const& FilePath) const;
	void ParseUdpFile(std::string const& FilePath) const;

	// Parse line from /proc/net/tcp or /proc/net/tcp6
	std::optional<ESocketType::Type> ParseTcpLine(std::string const& Line, WEndpoint const& TargetEndpoint) const;

	// Parse line from /proc/net/udp or /proc/net/udp6
	std::optional<ESocketType::Type> ParseUdpLine(std::string const& Line, WEndpoint const& TargetEndpoint) const;

	// parse hex address:port format
	static bool ParseAddressPort(std::string const& AddrPortStr, WIPAddress& OutAddr, uint16_t& OutPort, bool bIsIPv6);

	mutable std::unordered_set<uint16_t> KnownUsedPorts;

	// Store known listening ports for heuristics
	mutable std::unordered_set<uint16_t> KnownListeningPorts;
};
