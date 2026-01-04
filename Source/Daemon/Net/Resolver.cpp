/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Resolver.hpp"

#include <netdb.h>
#include <tracy/Tracy.hpp>
#include <spdlog/spdlog.h>

void WResolver::ResolveAddress(WIPAddress const& Address)
{
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
	}

	if (Result != 0)
	{
		// getnameinfo returns GAI error codes, not errno
		if (Result == EAI_AGAIN)
		{
			spdlog::debug("Resolver failed for {}: {} ({})", Address.ToString(), gai_strerror(Result), Result);
		}
		else
		{
			spdlog::warn("Resolver failed for {}: {} ({})", Address.ToString(), gai_strerror(Result), Result);
		}
	}

	std::lock_guard Lock(ResolvedAddressesMutex);
	if (Host == Address.ToString())
	{
		ResolvedAddresses[Address] = {};
		return;
	}

	spdlog::debug("Resolved {} to {}", Address.ToString(), Host);
	ResolvedAddresses[Address] = Host;
}

void WResolver::ResolverThreadFunc()
{
	tracy::SetThreadName("ResolverThread");
	while (bRunning)
	{
		std::unique_lock Lock(QueueMutex);
		if (!PendingAddresses.empty())
		{
			WIPAddress Address = PendingAddresses.front();
			PendingAddresses.pop();
			Lock.unlock();

			ResolveAddress(Address);
			continue;
		}
		Lock.unlock();

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void WResolver::Start()
{
	bRunning = true;
	ResolverThread = std::thread(&WResolver::ResolverThreadFunc, this);
}

void WResolver::Stop()
{
	bRunning = false;
	if (ResolverThread.joinable())
	{
		ResolverThread.join();
	}
}

std::string const& WResolver::Resolve(WIPAddress const& Address)
{
	static std::string Empty = {};
	if (Address.IsZero())
	{
		return Empty;
	}
	{
		std::lock_guard Lock(ResolvedAddressesMutex);

		if (auto It = ResolvedAddresses.find(Address); It != ResolvedAddresses.end())
		{
			return It->second;
		}
		ResolvedAddresses[Address] = Empty;
	}

	std::lock_guard QueueLock(QueueMutex);
	PendingAddresses.push(Address);

	return Empty;
}
