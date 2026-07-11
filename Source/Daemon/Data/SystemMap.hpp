/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>
#include <unordered_map>
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
#include "Data/SocketStateParser.hpp"

static constexpr WSocketCookie kSyntheticCookieBase = static_cast<WSocketCookie>(1) << 63;
/**
 * Both the client and the daemon need a tree of applications, processes, and sockets,
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
	WSec               LastCleanupMessageTime{};

	std::atomic<WTrafficItemId>  NextItemId{ 1 }; // 0 is the root item
	std::shared_ptr<WSystemItem> SystemItem = std::make_shared<WSystemItem>();
	TTrafficCounter<WSystemItem> TrafficCounter{ SystemItem };

	std::vector<std::unique_ptr<WFilterCounter>> FilterCounters{};

	std::unordered_map<std::string, std::shared_ptr<WAppCounter>>      Applications{};
	std::unordered_map<WProcessId, std::shared_ptr<WProcessCounter>>   Processes{};
	std::unordered_map<WSocketCookie, std::shared_ptr<WSocketCounter>> Sockets{};
	std::unordered_map<WTrafficItemId, std::shared_ptr<ITrafficItem>>  TrafficItems{};
	std::unordered_map<WEndpoint, std::shared_ptr<WSocketCounter>>     OrphanedSockets{};

	std::shared_ptr<WSocketCounter> FindOrMapSocket(
		WSocketCookie SocketCookie, std::shared_ptr<WProcessCounter> const& ParentProcess);
	std::shared_ptr<WProcessCounter> FindOrMapProcess(WProcessId PID, std::shared_ptr<WAppCounter> const& ParentApp);
	std::shared_ptr<WAppCounter>     FindOrMapApplication(
			std::string const& ExePath, std::string const& CommandLine, std::string const& AppName);
	void MarkSocketForRemoval(std::shared_ptr<WSocketCounter> const& Socket);

	void Cleanup();

	void DoPacketParsing(WSocketEvent const& Event, std::shared_ptr<WSocketCounter> const& SockCounter);

	std::shared_ptr<WSocketCounter> MapSocketFromTrafficEvent(WSocketEvent const& Event);

	void PushTrafficForSocket(WSocketEvent const& Event, std::shared_ptr<WSocketCounter> const& Socket) const;

	std::shared_ptr<WTupleCounter> GetOrCreateUDPTupleCounter(
		std::shared_ptr<WSocketCounter> const& SockCounter, WEndpoint const& Endpoint);

	void RegisterDefaultFilters();

public:
	WSystemMap();
	~WSystemMap() override = default;

	std::mutex DataMutex;

	WTrafficItemId GetNextItemId() { return NextItemId.fetch_add(1); }

	std::shared_ptr<WSocketCounter> MapSocket(WSocketEvent const& Event, WProcessId PID, bool bSilentFail = false);

	void AddExistingSockets();
	void ProcessInitialApps();

	void ReparentOrphanedSocket(WEndpoint const& Endpoint, WProcessId NewParentProcess);

	void MergeSyntheticSocket(std::shared_ptr<WSocketCounter> const& Socket);

	// After an accept event, re-check /proc to find the worker that actually owns the
	// accepted socket's fd, and move the socket to that process if it differs from the
	// master PID that was stored in port_to_pid at bind() time.
	void ReparentAcceptedSocket(std::shared_ptr<WSocketCounter> const& Socket);

	void RefreshAllTrafficCounters();

	void PushIncomingTraffic(WSocketEvent const& Event);

	void PushOutgoingTraffic(WSocketEvent const& Event);

	void MarkSocketForRemoval(WSocketEvent const& Event)
	{
		std::scoped_lock Lock(DataMutex);
		if (auto const It = Sockets.find(Event.Cookie); It != Sockets.end())
		{
			if (Event.Data.SocketCloseEventData.LocalPort > 0)
			{
				// todo: iterating over all sockets is a bit dumb since this really
				//       only affects listen sockets that were opened before the daemon started
				//       but it's probably not too much of an issue
				for (auto const& Sock : Sockets | std::views::values)
				{
					if (Sock->TrafficItem->SocketTuple.LocalEndpoint.Port == Event.Data.SocketCloseEventData.LocalPort)
					{
						MarkSocketForRemoval(Sock);
						break;
					}
				}
			}
			MarkSocketForRemoval(It->second);
		}
	}

	double GetDownloadSpeed() const { return SystemItem->DownloadSpeed; }

	double GetUploadSpeed() const { return SystemItem->UploadSpeed; }

	bool HasNewData() const { return TrafficCounter.GetState() == CS_Active || MapUpdate.HasUpdates(); }

	std::shared_ptr<WSystemItem> GetSystemItem() { return SystemItem; }

	std::vector<std::string> GetActiveApplicationPaths();

	WTrafficTreeUpdates const& GetUpdates() { return MapUpdate.GetUpdates(); }

	WMapUpdate& GetMapUpdate() { return MapUpdate; }

	std::shared_ptr<ITrafficItem> GetTrafficItemById(WTrafficItemId const ItemId)
	{
		auto const It = TrafficItems.find(ItemId);
		if (It != TrafficItems.end())
		{
			return It->second;
		}
		return {};
	}

	WMemoryStat GetMemoryUsage() override;
};
