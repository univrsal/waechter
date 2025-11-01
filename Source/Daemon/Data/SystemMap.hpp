//
// Created by usr on 26/10/2025.
//

#pragma once
#include <memory>
#include <unordered_map>
#include <mutex>

#include "EBPFCommon.h"
#include "Types.hpp"
#include "Singleton.hpp"
#include "TrafficCounter.hpp"
#include "Data/SystemItem.hpp"
#include "Data/TrafficTreeUpdate.hpp"

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
class WSystemMap : public TSingleton<WSystemMap>
{
public:
	struct WAppCounter : TTrafficCounter<WApplicationItem>
	{
		explicit WAppCounter(std::shared_ptr<WApplicationItem> const& Item)
			: TTrafficCounter(Item)
		{
		}
	};

	struct WProcessCounter : TTrafficCounter<WProcessItem>
	{
		explicit WProcessCounter(std::shared_ptr<WProcessItem> const& Item, std::shared_ptr<WAppCounter> const& ParentApp_)
			: TTrafficCounter(Item)
			, ParentApp(ParentApp_)
		{
		}
		std::shared_ptr<WAppCounter> ParentApp;
	};

	struct WSocketCounter : TTrafficCounter<WSocketItem>
	{
		explicit WSocketCounter(std::shared_ptr<WSocketItem> const& Item, std::shared_ptr<WProcessCounter> const& ParentProcess_)
			: TTrafficCounter(Item)
			, ParentProcess(ParentProcess_)
		{
		}

		void ProcessSocketEvent(WSocketEvent const& Event);

		std::shared_ptr<WProcessCounter> ParentProcess;
	};

private:
	WTrafficItemId               NextItemId{ 1 }; // 0 is the root item
	std::shared_ptr<WSystemItem> SystemItem = std::make_shared<WSystemItem>();
	TTrafficCounter<WSystemItem> TrafficCounter{ SystemItem };

	std::unordered_map<std::string, std::shared_ptr<WAppCounter>>      Applications{};
	std::unordered_map<WProcessId, std::shared_ptr<WProcessCounter>>   Processes{};
	std::unordered_map<WSocketCookie, std::shared_ptr<WSocketCounter>> Sockets{};

	std::shared_ptr<WSocketCounter>  FindOrMapSocket(WSocketCookie SocketCookie, std::shared_ptr<WProcessCounter> const& ParentProcess);
	std::shared_ptr<WProcessCounter> FindOrMapProcess(WProcessId PID, const std::shared_ptr<WAppCounter>& ParentApp);
	std::shared_ptr<WAppCounter>     FindOrMapApplication(std::string const& AppPath, std::string const& AppName);

	std::vector<WTrafficItemId> MarkedForRemovalItems{};
	std::vector<WTrafficItemId> RemovedItems{};

	std::vector<std::shared_ptr<WSocketCounter>> AddedSockets{};

	void Cleanup();

public:
	WSystemMap();
	~WSystemMap() override = default;

	std::mutex DataMutex;

	std::shared_ptr<WSocketCounter> MapSocket(WSocketCookie SocketCookie, WProcessId PID, bool bSilentFail = false);

	void RefreshAllTrafficCounters();

	void PushIncomingTraffic(WBytes Bytes, WSocketCookie SocketCookie);

	void PushOutgoingTraffic(WBytes Bytes, WSocketCookie SocketCookie);

	void MarkSocketForRemoval(WSocketCookie SocketCookie)
	{
		if (const auto It = Sockets.find(SocketCookie); It != Sockets.end())
		{
			It->second->MarkForRemoval();
			MarkedForRemovalItems.emplace_back(It->second->TrafficItem->ItemId);
		}
	}

	double GetDownloadSpeed() const
	{
		return SystemItem->DownloadSpeed;
	}

	double GetUploadSpeed() const
	{
		return SystemItem->UploadSpeed;
	}

	bool HasNewData() const
	{
		return TrafficCounter.GetState() == CS_Active || !AddedSockets.empty() || !RemovedItems.empty() || !MarkedForRemovalItems.empty();
	}

	std::shared_ptr<WSystemItem> GetSystemItem()
	{
		return SystemItem;
	}

	WTrafficTreeUpdates GetUpdates();
};
