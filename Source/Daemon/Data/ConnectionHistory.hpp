/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>
#include <deque>
#include <mutex>

#include "Singleton.hpp"
#include "IPAddress.hpp"
#include "Time.hpp"
#include "Types.hpp"
#include "Data/ConnectionHistoryUpdate.hpp"

#include <cassert>

class WAppCounter;
class WSocketItem;
class WSocketCounter;
class WTupleCounter;
class WTupleItem;

struct WConnectionHistoryEntry
{
	std::shared_ptr<WAppCounter> App{};
	std::shared_ptr<WSocketItem> Socket{};
	std::shared_ptr<WTupleItem>  Tuple{}; // set if this is a UDP connection
	WTrafficItemId               ConectionItemId{};

	WEndpoint RemoteEndpoint{};
	WSec      StartTime{ WTime::GetUnixNow() };
	WSec      EndTime{ 0 };
	WBytes    DataIn{ 0 };
	WBytes    DataOut{ 0 };

	bool Update(); // return true if anything changed

	WConnectionHistoryEntry(std::shared_ptr<WAppCounter> App_, std::shared_ptr<WSocketItem> Socket_,
		std::shared_ptr<WTupleItem> Tuple_, WEndpoint const& RemoteEndpoint_);
};

class WConnectionHistory : public TSingleton<WConnectionHistory>
{
	std::mutex Mutex;

	std::deque<WConnectionHistoryEntry> History;
	uint32_t                            NewItemCounter{ 0 };

	void OnSocketConnected(WSocketCounter const* SocketCounter);
	void OnUDPTupleCreated(std::shared_ptr<WTupleCounter> const& TupleCounter, WEndpoint const& Endpoint);
	void Push(std::shared_ptr<WAppCounter> const& App, std::shared_ptr<WSocketItem> const& Socket,
		std::shared_ptr<WTupleItem> const& Tuple, WEndpoint const& RemoteEndpoint)
	{
		WConnectionHistoryEntry Entry{ App, Socket, Tuple, RemoteEndpoint };
		// todo: (maybe) the rest should be pushed into the database
		std::scoped_lock Lock(Mutex);
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
};
