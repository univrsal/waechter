/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>

#include "EBPFCommon.h"
#include "Types.hpp"
#include "Singleton.hpp"
#include "TrafficCounter.hpp"
#include "MemoryStats.hpp"
#include "Data/SystemItem.hpp"
#include "Data/TrafficTreeUpdate.hpp"
#include "Data/Counters.hpp"
#include "Data/MapUpdate.hpp"
#include "Net/SocketStateParser.hpp"

/**
 * Both the client and the daemon need a tree of applications, processes, and sockets
 * but the daemon also needs to maintain global traffic counters and mappings.
 * We split this into two trees. SystemItem is the root for the tree
 * that gets serialized and sent to the client. The structure of this tree is
 * shared between the client and daemon.
 *
 * The Other tree is built using the maps defined in this class each node containing
 * a traffic counter and a shared pointer to the corresponding node in the SystemItem tree.
 */
class WSystemMap : public TSingleton<WSystemMap>, public IMemoryTrackable
{
	friend class WMapUpdate;
	WSocketStateParser SocketStateParser{};
	WMapUpdate         MapUpdate{};

	std::atomic<WTrafficItemId>  NextItemId{ 1 }; // 0 is the root item
	std::shared_ptr<WSystemItem> SystemItem = std::make_shared<WSystemItem>();
	TTrafficCounter<WSystemItem> TrafficCounter{ SystemItem };

	std::unordered_map<std::string, std::shared_ptr<WAppCounter>>      Applications{};
	std::unordered_map<WProcessId, std::shared_ptr<WProcessCounter>>   Processes{};
	std::unordered_map<WSocketCookie, std::shared_ptr<WSocketCounter>> Sockets{};
	std::unordered_map<WTrafficItemId, std::shared_ptr<ITrafficItem>>  TrafficItems{};
	std::unordered_set<WSocketCookie>                                  ClosedSocketCookies{};

	std::shared_ptr<WSocketCounter> FindOrMapSocket(
		WSocketCookie SocketCookie, std::shared_ptr<WProcessCounter> const& ParentProcess);
	std::shared_ptr<WProcessCounter> FindOrMapProcess(WProcessId PID, std::shared_ptr<WAppCounter> const& ParentApp);
	std::shared_ptr<WAppCounter>     FindOrMapApplication(
			std::string const& ExePath, std::string const& CommandLine, std::string const& AppName);

	void Cleanup();

	void DoPacketParsing(WSocketEvent const& Event, std::shared_ptr<WSocketCounter> const& SockCounter);

	std::shared_ptr<WTupleCounter> GetOrCreateUDPTupleCounter(
		std::shared_ptr<WSocketCounter> const& SockCounter, WEndpoint const& Endpoint);

	WSec LastCleanupMessageTime{};

public:
	WSystemMap();
	~WSystemMap() override = default;

	std::mutex DataMutex;

	WTrafficItemId GetNextItemId() { return NextItemId.fetch_add(1); }

	std::shared_ptr<WSocketCounter> MapSocket(WSocketEvent const& Event, WProcessId PID, bool bSilentFail = false);

	void RefreshAllTrafficCounters();

	void PushIncomingTraffic(WSocketEvent const& Event);

	void PushOutgoingTraffic(WSocketEvent const& Event);

	void MarkSocketForRemoval(WSocketCookie SocketCookie)
	{
		// Track this cookie as closed, even if the socket doesn't exist yet
		ClosedSocketCookies.insert(SocketCookie);

		if (auto const It = Sockets.find(SocketCookie); It != Sockets.end())
		{
			It->second->MarkForRemoval();
			It->second->TrafficItem->ConnectionState = ESocketConnectionState::Closed;
			MapUpdate.MarkItemForRemoval(It->second->TrafficItem->ItemId);
			It->second->ParentProcess->PushIncomingTraffic(0); // Force state update
			It->second->ParentProcess->ParentApp->PushOutgoingTraffic(0);
			TrafficCounter.PushIncomingTraffic(0);
		}
	}

	double GetDownloadSpeed() const { return SystemItem->DownloadSpeed; }

	double GetUploadSpeed() const { return SystemItem->UploadSpeed; }

	bool HasNewData() const { return TrafficCounter.GetState() == CS_Active || MapUpdate.HasUpdates(); }

	std::shared_ptr<WSystemItem> GetSystemItem() { return SystemItem; }

	std::vector<std::string> GetActiveApplicationPaths();

	WTrafficTreeUpdates const& GetUpdates() { return MapUpdate.GetUpdates(); }

	WMapUpdate& GetMapUpdate() { return MapUpdate; }

	std::shared_ptr<ITrafficItem> GetTrafficItemById(WTrafficItemId ItemId)
	{
		auto It = TrafficItems.find(ItemId);
		if (It != TrafficItems.end())
		{
			return It->second;
		}
		return {};
	}

	WMemoryStat GetMemoryUsage() override;
};
