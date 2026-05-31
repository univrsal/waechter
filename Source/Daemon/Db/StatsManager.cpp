/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "StatsManager.hpp"

#include <array>
#include <ctime>
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
	QueueCondition.notify_one();
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
		spdlog::info("Processing stats request, queue size: {}", PendingRequests.size());
		auto [Request, Promise] = PendingRequests.front();
		PendingRequests.pop();
		Lock.unlock();

		WStatsResponse Response{};
		ProcessStatsRequest(Request, Response);
		Promise.Finish(Response);
	}
}

#if WDEBUG
void WStatsManager::PushDebugSnapshots()
{
	spdlog::info("WStatsManager::PushDebugSnapshots - inserting debug traffic data for the past 7 days");

	// Fake applications and remote hosts to use for debug traffic
	static constexpr std::array<std::string_view, 5> DebugApps = {
		"/usr/bin/firefox",
		"/usr/bin/curl",
		"/usr/bin/wget",
		"/usr/lib/chromium/chromium",
		"/usr/bin/spotify",
	};
	static constexpr std::array<std::string_view, 6> DebugHosts = {
		"93.184.216.34",   // example.com
		"142.250.80.46",   // google.com
		"151.101.1.140",   // fastly CDN
		"104.21.55.201",   // cloudflare
		"52.94.236.248",   // amazonaws
		"185.199.108.153", // github
	};

	// Traffic scale per app (bytes per hour at peak)
	static constexpr std::array<uint64_t, 5> AppPeakIn = { 8'000'000, 500'000, 300'000, 6'000'000, 2'000'000 };
	static constexpr std::array<uint64_t, 5> AppPeakOut = { 1'000'000, 200'000, 100'000, 800'000, 200'000 };

	// Hour-of-day activity curve: higher during waking hours
	auto HourWeight = [](int Hour) -> double {
		if (Hour < 6)
			return 0.05;
		if (Hour < 9)
			return 0.4;
		if (Hour < 12)
			return 0.85;
		if (Hour < 14)
			return 1.0;
		if (Hour < 18)
			return 0.9;
		if (Hour < 22)
			return 0.75;
		return 0.2;
	};

	// Simple deterministic pseudo-random variation in range [0.5, 1.5]
	auto Jitter = [](int Day, int Hour, int AppIdx, int HostIdx) -> double {
		uint32_t Hash = static_cast<uint32_t>(Day * 1000 + Hour * 50 + AppIdx * 7 + HostIdx * 3);
		Hash ^= Hash >> 13;
		Hash *= 0x9e3779b9u;
		Hash ^= Hash >> 11;
		return 0.5 + (Hash & 0xFFFFu) / 65535.0;
	};

	WSec const Now = WTime::GetEpochSeconds();
	WSec const NowHour = (Now / 3600LL) * 3600LL; // floor to current hour
	WSec const WeekStart = NowHour - 7LL * 24LL * 3600LL;

	WDbManager::GetInstance().Run([&](auto& DbConn) {
		constexpr Db::Schema::TrafficSnapshot TS;
		constexpr Db::Schema::TrafficEvent    TE;

		// Get-or-insert helper for Application
		auto GetOrInsertApp = [&](std::string_view Path) -> int64_t {
			constexpr Db::Schema::Application App;
			auto Result = DbConn(sqlpp::select(App.ID).from(App).where(App.BinaryPath == Path));
			if (!Result.empty())
				return Result.front().ID.value();
			return static_cast<int64_t>(DbConn(sqlpp::insert_into(App).set(App.BinaryPath = Path)));
		};

		// Get-or-insert helper for Host
		auto GetOrInsertHost = [&](std::string_view IP) -> int64_t {
			constexpr Db::Schema::Host Host;
			auto                       Result = DbConn(sqlpp::select(Host.ID).from(Host).where(Host.IPAddress == IP));
			if (!Result.empty())
				return Result.front().ID.value();
			return static_cast<int64_t>(DbConn(sqlpp::insert_into(Host).set(Host.IPAddress = IP)));
		};

		// Cache app and host DB ids
		std::array<int64_t, DebugApps.size()>  AppIDs{};
		std::array<int64_t, DebugHosts.size()> HostIDs{};
		for (std::size_t i = 0; i < DebugApps.size(); ++i)
			AppIDs[i] = GetOrInsertApp(DebugApps[i]);
		for (std::size_t i = 0; i < DebugHosts.size(); ++i)
			HostIDs[i] = GetOrInsertHost(DebugHosts[i]);

		// One snapshot per hour for 7 days (168 snapshots total)
		for (WSec HourStart = WeekStart; HourStart < NowHour; HourStart += 3600LL)
		{
			std::time_t const TimeT = static_cast<std::time_t>(HourStart);
			std::tm           Tm{};
			gmtime_r(&TimeT, &Tm);
			int const    HourOfDay = Tm.tm_hour;
			int const    DayIdx = static_cast<int>((HourStart - WeekStart) / 86400LL);
			double const HW = HourWeight(HourOfDay);

			int64_t const SnapshotID = static_cast<int64_t>(DbConn(sqlpp::insert_into(TS).set(TS.Start = HourStart)));

			// Each app talks to a deterministic subset of hosts (1-3 per hour)
			for (std::size_t AppIdx = 0; AppIdx < DebugApps.size(); ++AppIdx)
			{
				std::size_t const HostCount = 1 + (AppIdx % DebugHosts.size() % 3);
				for (std::size_t h = 0; h < HostCount; ++h)
				{
					std::size_t const HostIdx = (AppIdx + h) % DebugHosts.size();
					double const J = Jitter(DayIdx, HourOfDay, static_cast<int>(AppIdx), static_cast<int>(HostIdx));

					auto const BytesIn = static_cast<int64_t>(AppPeakIn[AppIdx] * HW * J);
					auto const BytesOut = static_cast<int64_t>(AppPeakOut[AppIdx] * HW * J);

					DbConn(sqlpp::insert_into(TE).set(TE.SnapshotID = SnapshotID, TE.AppID = AppIDs[AppIdx],
						TE.HostID = HostIDs[HostIdx], TE.BytesIn = BytesIn, TE.BytesOut = BytesOut));
				}
			}
		}
	});

	spdlog::info("WStatsManager::PushDebugSnapshots - done");
}
#endif