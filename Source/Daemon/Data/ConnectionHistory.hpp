/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>
#include <deque>
#include <unordered_set>
#include <mutex>

#include "Singleton.hpp"
#include "IPAddress.hpp"
#include "Time.hpp"
#include "Types.hpp"
#include "MemoryStats.hpp"
#include "Data/ConnectionHistoryUpdate.hpp"


struct WAppCounter;
struct WSocketItem;
struct WSocketCounter;
struct WTupleCounter;
struct WTupleItem;

struct WConnectionSet
{
	std::unordered_set<std::shared_ptr<ITrafficItem>> Connections;

	// When a socket/tuple disconnects it gets removed from the set
	// however this also means it won't contribute to the data counts anymore
	// so we store the total data in/out of all disconnected items here
	// to keep the totals correct
	WBytes BaseDataIn{ 0 };
	WBytes BaseDataOut{ 0 };

	WConnectionSet() = default;
};

struct WConnectionHistoryEntry
{
	std::shared_ptr<WAppCounter> App{};
	std::shared_ptr<WConnectionSet> Set{};
	WTrafficItemId                  ConnectionId{};

	WEndpoint RemoteEndpoint{};
	WSec      StartTime{ WTime::GetUnixNow() };
	WSec      EndTime{ 0 };
	WBytes    DataIn{ 0 };
	WBytes    DataOut{ 0 };

	bool Update(); // return true if anything changed

	WConnectionHistoryEntry(
		std::shared_ptr<WAppCounter> App_, std::shared_ptr<WConnectionSet> Set_, WEndpoint const& RemoteEndpoint_);
};

struct WConnectionKeyHash
{
	size_t operator()(std::pair<std::string, WEndpoint> const& Key) const noexcept
	{
		size_t H1 = std::hash<std::string>{}(Key.first);
		size_t H2 = WEndpointHash{}(Key.second);
		return H1 ^ (H2 + 0x9e3779b97f4a7c15ULL + (H1 << 6) + (H1 >> 2));
	}
};

class WConnectionHistory : public TSingleton<WConnectionHistory>, public IMemoryTrackable
{
	std::mutex Mutex;

	uint32_t                            NewItemCounter{ 0 };
	std::deque<WConnectionHistoryEntry> History;

	std::unordered_map<std::pair<std::string, WEndpoint>, std::shared_ptr<WConnectionSet>, WConnectionKeyHash>
		ActiveConnections;

	void OnSocketConnected(WSocketCounter const* SocketCounter);
	void OnSocketRemoved(std::shared_ptr<WSocketCounter> const& SocketCounter);

	void OnUDPTupleCreated(std::shared_ptr<WTupleCounter> const& TupleCounter, WEndpoint const& Endpoint);

	void Push(std::shared_ptr<WAppCounter> const& App, std::shared_ptr<WConnectionSet> const& Set,
		WEndpoint const& RemoteEndpoint)
	{
		// no lock here, it's already acquired by the caller
		WConnectionHistoryEntry Entry{ App, Set, RemoteEndpoint };
		// todo: (maybe) the rest should be pushed into the database
		History.push_back(Entry);
		++NewItemCounter;
		if (History.size() > kMaxHistorySize)
		{
			History.pop_front();
		}
	}

public:
	void RegisterSignalHandlers();

	WConnectionHistoryUpdate Update();
	WConnectionHistoryUpdate Serialize();

	WMemoryStat GetMemoryUsage() override;
};
