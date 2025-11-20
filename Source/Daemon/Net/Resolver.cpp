//
// Created by usr on 17/11/2025.
//

#include "Resolver.hpp"

#include "ErrnoUtil.hpp"

#include <netdb.h>
#include <spdlog/spdlog.h>

std::string const& WResolver::Resolve(WIPAddress const& Address)
{
	static std::string Empty = {};
	if (Address.IsZero())
	{
		return Empty;
	}
	std::lock_guard Lock(ResolvedAddressesMutex);

	if (auto It = ResolvedAddresses.find(Address); It != ResolvedAddresses.end())
	{
		return It->second;
	}

	int  Result = 0;
	char Host[NI_MAXHOST]{};

	if (Address.Family == EIPFamily::IPv6)
	{
		in6_addr Addr6{};
		std::copy(std::begin(Address.Bytes), std::end(Address.Bytes), Addr6.s6_addr);
		sockaddr_in6 SockAddr6{};
		SockAddr6.sin6_family = AF_INET6;
		SockAddr6.sin6_addr = Addr6;
		SockAddr6.sin6_port = 0;

		Result =
			getnameinfo(reinterpret_cast<sockaddr*>(&SockAddr6), sizeof(SockAddr6), Host, sizeof(Host), nullptr, 0, 0);
	}
	else if (Address.Family == EIPFamily::IPv4)
	{
		in_addr Addr4{};
		// IPv4 addresses are stored in network byte order in the first 4 bytes
		Addr4.s_addr =
			htonl((Address.Bytes[0] << 24) | (Address.Bytes[1] << 16) | (Address.Bytes[2] << 8) | Address.Bytes[3]);
		sockaddr_in SockAddr4{};
		SockAddr4.sin_family = AF_INET;
		SockAddr4.sin_addr = Addr4;
		SockAddr4.sin_port = 0;

		Result =
			getnameinfo(reinterpret_cast<sockaddr*>(&SockAddr4), sizeof(SockAddr4), Host, sizeof(Host), nullptr, 0, 0);
	}
	else
	{
		auto V4 = Address;
		auto V6 = Address;
		V4.Family = EIPFamily::IPv4;
		V6.Family = EIPFamily::IPv6;
		spdlog::info(
			"Unknown address family {} ({}/{})", static_cast<int>(Address.Family), V4.ToString(), V6.ToString());
		return Empty;
	}

	if (Result != 0)
	{
		// getnameinfo returns GAI error codes, not errno
		spdlog::warn("Resolver failed for {}: {} ({})", Address.ToString(), gai_strerror(Result), Result);
		return Empty;
	}

	if (Host == Address.ToString())
	{
		ResolvedAddresses[Address] = {};
		return Empty;
	}

	spdlog::debug("Resolved {} to {}", Address.ToString(), Host);
	ResolvedAddresses[Address] = Host;
	return ResolvedAddresses[Address];
}
