/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Resolver.hpp"

#include "Format.hpp"

#include <netdb.h>

#include "tracy/Tracy.hpp"
#include "spdlog/spdlog.h"

void WResolver::ResolveAddress(WQueuedRequest const& Request)
{
	int  Result = 0;
	char Host[NI_MAXHOST]{};

	auto const& Address = Request.AddressToResolve;

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
	CurrentCacheRamUsage += sizeof(WIPAddress) + strlen(Host) + 1 + sizeof(std::string);
	spdlog::debug("Current resolver cache RAM usage: {}", WStorageFormat::AutoFormat(CurrentCacheRamUsage));

	if (CurrentCacheRamUsage > MaxCacheRamUsage)
	{
		spdlog::info("Resolver cache exceeded max RAM of usage of {}, dropping some entries...",
			WStorageFormat::AutoFormat(MaxCacheRamUsage));
		int DroppedEntries = 0;
		while (CurrentCacheRamUsage > MaxCacheRamUsage / 2 && !ResolvedAddresses.empty())
		{
			auto const It = ResolvedAddresses.begin();
			CurrentCacheRamUsage -= sizeof(WIPAddress) + It->second.size() + 1 + sizeof(std::string);
			ResolvedAddresses.erase(It);
			++DroppedEntries;
		}
		if (DroppedEntries > 0)
		{
			spdlog::info("Dropped {} entries from resolver cache. New RAM usage {}", DroppedEntries,
				WStorageFormat::AutoFormat(CurrentCacheRamUsage));
		}
	}

	if (Host == Address.ToString())
	{
		ResolvedAddresses[Address] = {};
		Request.Promise.Finish({});
		return;
	}

	spdlog::debug("Resolved {} to {}", Address.ToString(), Host);
	ResolvedAddresses[Address] = Host;
	Request.Promise.Finish(Host);
}

void WResolver::ResolverThreadFunc()
{
	static std::string Empty = {};
	tracy::SetThreadName("ResolverThread");
	while (bRunning)
	{
		std::unique_lock Lock(QueueMutex);
		QueueCondition.wait(Lock, [this] { return !PendingAddresses.empty() || !bRunning; });

		if (!bRunning)
		{
			break;
		}

		if (!PendingAddresses.empty())
		{
			auto const& Request = PendingAddresses.front();
			PendingAddresses.pop();

			if (Request.AddressToResolve.IsZero())
			{
				Request.Promise.Finish(Empty);
			}
			else if (auto It = ResolvedAddresses.find(Request.AddressToResolve); It != ResolvedAddresses.end())
			{
				Request.Promise.Finish(It->second);
			}
			else
			{
				Lock.unlock();
				ResolveAddress(Request);
			}
		}
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
	QueueCondition.notify_one();
	if (ResolverThread.joinable())
	{
		ResolverThread.join();
	}
}

TPromise<std::string const&> WResolver::Resolve(WIPAddress const& Address)
{
	TPromise<std::string const&> Promise{};

	{
		WQueuedRequest  Request{ Address, Promise };
		std::lock_guard Lock(QueueMutex);
		PendingAddresses.push(Request);
	}

	return Promise;
}

WMemoryStat WResolver::GetMemoryUsage()
{
	std::scoped_lock Lock(QueueMutex);
	WMemoryStat      Stats;
	Stats.Name = "WResolver";
	Stats.ChildEntries.emplace_back(WMemoryStatEntry{ .Name = "Cache", .Usage = CurrentCacheRamUsage });
	return Stats;
}
