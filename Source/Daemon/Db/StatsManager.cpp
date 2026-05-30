/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "StatsManager.hpp"

#include <ranges>

#include "sqlpp11/sqlpp11.h"

#include "DbManager.hpp"
#include "Schema.hpp"
#include "IPAddress.hpp"
#include "Time.hpp"
#include "Data/SystemMap.hpp"

WMemoryStat WStatsManager::GetMemoryUsage()
{
	std::scoped_lock Lock(DataMutex);
	WMemoryStat      Stats{};
	Stats.Name = "WStatsManager";

	WMemoryStatEntry SnapshotMemory{ "TrafficSnapshot",
		sizeof(WTrafficSnapshot) + CALC_MAP_USAGE(CurrentSnapshot.Apps, WTrafficItemId, WAppStats) };
	for (auto const& AppTraffic : CurrentSnapshot.Apps | std::views::values)
	{
		SnapshotMemory.Usage += CALC_MAP_USAGE(AppTraffic.Traffic, WIPAddress, WTrafficStats);
	}
	Stats.ChildEntries.emplace_back(SnapshotMemory);
	return Stats;
}

void WStatsManager::UpdateAppStats(WTrafficItemId AppId, WIPAddress const& RemoteHost, WBytes In, WBytes Out)
{
	if (CurrentSnapshot.Apps.contains(AppId))
	{
		if (auto& [AppTraffic] = CurrentSnapshot.Apps[AppId]; AppTraffic.contains(RemoteHost))
		{
			AppTraffic[RemoteHost].BytesIn += In;
			AppTraffic[RemoteHost].BytesOut += Out;
		}
		else
		{
			AppTraffic[RemoteHost] = WTrafficStats{ In, Out };
		}
	}
	else
	{
		WAppStats NewAppStats{};
		NewAppStats.Traffic[RemoteHost] = WTrafficStats{ In, Out };
		CurrentSnapshot.Apps[AppId] = NewAppStats;
	}
}

void WStatsManager::MakeSnapshot()
{
	uint64_t SnapshotID{};
	WDbManager::GetInstance().Run([&](auto& DbConn) {
		constexpr Db::Schema::TrafficSnapshot Snapshot;
		SnapshotID = DbConn(sqlpp::insert_into(Snapshot).set(Snapshot.Start = WTime::GetEpochSeconds()));
	});

	std::vector<Db::Schema::TrafficEvent> Events;
	{
		std::scoped_lock Lock(DataMutex);

		for (auto const& [AppId, AppStats] : CurrentSnapshot.Apps)
		{
			auto const& TrafficItem = WSystemMap::GetInstance().GetTrafficItemById(AppId);
			if (!TrafficItem || TrafficItem->GetType() != TI_Application)
			{
				spdlog::warn("App ID {} not found in system map, skipping traffic event", AppId);
				break;
			}
			auto AppItem = std::dynamic_pointer_cast<WApplicationItem>(TrafficItem);
			for (auto const& [IP, Traffic] : AppStats.Traffic)
			{
				Db::Schema::TrafficEvent NewEvent{};
				WDbManager::GetInstance().Run([&](auto& DbConn) {
					constexpr Db::Schema::Application App;
					constexpr Db::Schema::Host        Host;

					auto AppResult =
						DbConn(sqlpp::select(App.ID).from(App).where(App.BinaryPath == AppItem->ApplicationPath));
					if (AppResult.empty())
						NewEvent.AppID = DbConn(sqlpp::insert_into(App).set(App.BinaryPath = AppItem->ApplicationPath));
					else
						NewEvent.AppID = AppResult.front().ID.value();

					auto IPStr = IP.ToString();
					auto HostResult = DbConn(sqlpp::select(Host.ID).from(Host).where(Host.IPAddress == IPStr));
					if (HostResult.empty())
						NewEvent.HostID = DbConn(sqlpp::insert_into(Host).set(Host.IPAddress = IPStr));
					else
						NewEvent.HostID = HostResult.front().ID.value();
				});
				NewEvent.SnapshotID = SnapshotID;
				NewEvent.BytesIn = Traffic.BytesIn;
				NewEvent.BytesOut = Traffic.BytesOut;
				Events.push_back(NewEvent);
			}
		}
	}

	WDbManager::GetInstance().Run([&](auto& DbConn) {
		constexpr Db::Schema::TrafficEvent Event;
		for (auto const& E : Events)
		{
			DbConn(sqlpp::insert_into(Event).set(Event.SnapshotID = E.SnapshotID, Event.AppID = E.AppID,
				Event.HostID = E.HostID, Event.BytesIn = E.BytesIn, Event.BytesOut = E.BytesOut));
		}
	});
	CurrentSnapshot.Apps.clear();
}
