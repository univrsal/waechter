/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "StatsManager.hpp"

#include <ranges>
#include <map>

#include "sqlpp11/sqlpp11.h"

#include "DbManager.hpp"
#include "Schema.hpp"
#include "IPAddress.hpp"
#include "Time.hpp"
#include "Data/SystemMap.hpp"
#include "Data/Stats.hpp"

#include <tracy/Tracy.hpp>

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
		ZoneScopedN("MakeSnapshot - Prepare events");
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
					{
						NewEvent.AppID = DbConn(sqlpp::insert_into(App).set(App.BinaryPath = AppItem->ApplicationPath));
					}
					else
					{
						NewEvent.AppID = AppResult.front().ID.value();
					}

					auto const IPStr = IP.ToString();
					auto const HostResult = DbConn(sqlpp::select(Host.ID).from(Host).where(Host.IPAddress == IPStr));

					if (HostResult.empty())
					{
						NewEvent.HostID = DbConn(sqlpp::insert_into(Host).set(Host.IPAddress = IPStr));
					}
					else
					{
						NewEvent.HostID = HostResult.front().ID.value();
					}
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

void WStatsManager::StartRequestProcessThread()
{
	if (bRunning)
	{
		return;
	}
	bRunning = true;
	RequestThread = std::thread(&WStatsManager::RequestThreadFunction, this);
}

void WStatsManager::StopRequestProcessThread()
{
	{
		std::scoped_lock Lock(DataMutex);
		bRunning = false;
	}
	QueueCondition.notify_one();
	if (RequestThread.joinable())
	{
		RequestThread.join();
	}
}

TPromise<WStatsResponse const&> WStatsManager::RequestStats(WStatsRequest const& Request)
{
	WQueuedRequest                  NewRequest{ Request, {} };
	TPromise<WStatsResponse const&> Promise = NewRequest.Promise;
	{
		std::scoped_lock Lock(QueueMutex);
		PendingRequests.push(std::move(NewRequest));
	}
	return Promise;
}

void WStatsManager::ProcessStatsRequest(WStatsRequest const& Request, WStatsResponse& Response)
{
	ZoneScopedN("WStatsManager::ProcessStatsRequest");

	Response.RequestId = Request.RequestId;

	constexpr WSec OneHour = 3600LL;
	constexpr WSec OneDay = 86400LL;

	WSec const Duration = Request.EndTime - Request.StartTime;
	bool const BucketByMonth = Duration > 31LL * OneDay;
	WSec const BucketSecs = BucketByMonth ? 0LL : (Duration <= 24LL * OneHour ? OneHour : OneDay);

	// Floor a timestamp to the start of its bucket
	auto GetBucketStart = [&](WSec const T) -> WSec {
		if (BucketByMonth)
		{
			std::time_t const TimeT = T;
			std::tm           Tm{};
			gmtime_r(&TimeT, &Tm);
			Tm.tm_mday = 1;
			Tm.tm_hour = Tm.tm_min = Tm.tm_sec = 0;
			Tm.tm_isdst = -1;
			return timegm(&Tm);
		}
		return (T / BucketSecs) * BucketSecs;
	};

	// Advance to the next bucket boundary
	auto NextBucket = [&](WSec const T) -> WSec {
		if (BucketByMonth)
		{
			std::time_t const TimeT = T;
			std::tm           Tm{};
			gmtime_r(&TimeT, &Tm);
			if (++Tm.tm_mon > 11)
			{
				Tm.tm_mon = 0;
				++Tm.tm_year;
			}
			Tm.tm_isdst = -1;
			return timegm(&Tm);
		}
		return T + BucketSecs;
	};

	// Pre-populate ordered buckets so empty ranges still emit zeroed entries
	std::map<WSec, WStatData> Buckets;
	for (WSec BucketTime = GetBucketStart(Request.StartTime); BucketTime < Request.EndTime;
		BucketTime = NextBucket(BucketTime))
	{
		Buckets[BucketTime] = WStatData{ 0, 0, BucketTime };
	}

	// Accumulate one database row into the appropriate bucket
	auto Accumulate = [&](WSec const SnapshotStart, WBytes const BytesIn, WBytes const BytesOut) {
		WSec const Key = GetBucketStart(SnapshotStart);
		if (auto const It = Buckets.find(Key); It != Buckets.end())
		{
			It->second.In += BytesIn;
			It->second.Out += BytesOut;
		}
	};

	// Determine target type: "system" / binary path (starts with '/') / IP address
	enum class ETargetType
	{
		System,
		Binary,
		Host
	};

	ETargetType TargetType;
	if (Request.Target == "system")
	{
		TargetType = ETargetType::System;
	}
	else if (!Request.Target.empty() && Request.Target.front() == '/')
	{
		TargetType = ETargetType::Binary;
	}
	else
	{
		TargetType = ETargetType::Host;
	}

	WDbManager::GetInstance().Run([&](auto& DbConn) {
		constexpr Db::Schema::TrafficEvent    TE;
		constexpr Db::Schema::TrafficSnapshot TS;

		if (TargetType == ETargetType::System)
		{
			auto Rows = DbConn(sqlpp::select(TS.Start, TE.BytesIn, TE.BytesOut)
					.from(TE.join(TS).on(TE.SnapshotID == TS.ID))
					.where(TS.Start >= Request.StartTime && TS.Start < Request.EndTime));
			for (auto const& Row : Rows)
			{
				Accumulate(Row.Start.value(), static_cast<uint64_t>(Row.BytesIn), static_cast<uint64_t>(Row.BytesOut));
			}
		}
		else if (TargetType == ETargetType::Binary)
		{
			constexpr Db::Schema::Application App;
			auto                              Rows = DbConn(sqlpp::select(TS.Start, TE.BytesIn, TE.BytesOut)
												 .from(TE.join(TS).on(TE.SnapshotID == TS.ID).join(App).on(TE.AppID == App.ID))
												 .where(TS.Start >= Request.StartTime && TS.Start < Request.EndTime
													 && App.BinaryPath == Request.Target));
			for (auto const& Row : Rows)
			{
				Accumulate(Row.Start.value(), static_cast<uint64_t>(Row.BytesIn), static_cast<uint64_t>(Row.BytesOut));
			}
		}
		else
		{
			constexpr Db::Schema::Host Host;
			auto                       Rows = DbConn(sqlpp::select(TS.Start, TE.BytesIn, TE.BytesOut)
										  .from(TE.join(TS).on(TE.SnapshotID == TS.ID).join(Host).on(TE.HostID == Host.ID))
										  .where(TS.Start >= Request.StartTime && TS.Start < Request.EndTime
											  && Host.IPAddress == Request.Target));
			for (auto const& Row : Rows)
			{
				Accumulate(Row.Start.value(), static_cast<uint64_t>(Row.BytesIn), static_cast<uint64_t>(Row.BytesOut));
			}
		}
	});

	// Flatten the ordered bucket map into the response vector
	Response.DataPoints.reserve(Buckets.size());
	for (auto const& Data : Buckets | std::views::values)
	{
		Response.DataPoints.push_back(Data);
	}
}

void WStatsManager::RequestThreadFunction()
{
	tracy::SetThreadName("stats-request");
	pthread_setname_np(pthread_self(), "stat-reques");

	while (bRunning)
	{
		std::unique_lock Lock(QueueMutex);
		QueueCondition.wait(Lock, [this] { return !PendingRequests.empty() || !bRunning; });

		if (!bRunning && PendingRequests.empty())
		{
			break;
		}

		auto [Request, Promise] = PendingRequests.front();
		PendingRequests.pop();
		Lock.unlock();

		WStatsResponse Response{};
		ProcessStatsRequest(Request, Response);
		Promise.Finish(Response);
	}
}
