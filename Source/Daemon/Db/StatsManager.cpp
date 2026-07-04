/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "StatsManager.hpp"

#include <array>
#include <ranges>
#include <map>

#include "sqlpp11/sqlpp11.h"

#include "DbManager.hpp"
#include "Schema.hpp"
#include "IPAddress.hpp"
#include "Buffer.hpp"
#include "Messages.hpp"
#include "Time.hpp"
#include "Data/IP2Asn.hpp"
#include "Data/SystemMap.hpp"
#include "Data/Stats.hpp"

#include <tracy/Tracy.hpp>

WMemoryStat WStatsManager::GetMemoryUsage()
{
	std::scoped_lock Lock(DataMutex);
	WMemoryStat      Stats{};
	Stats.Name = "WStatsManager";

	WMemoryStatEntry SnapshotMemory{ "TrafficSnapshot",
		CALC_MAP_USAGE(CurrentSnapshot.Apps, WTrafficItemId, WItemStats)
			+ CALC_MAP_USAGE(CurrentSnapshot.Filters, WTrafficItemId, WTrafficStats) };
	for (auto const& [ItemName, Traffic] : CurrentSnapshot.Apps | std::views::values)
	{
		SnapshotMemory.Usage += CALC_MAP_USAGE(Traffic, WIPAddress, WTrafficStats);
	}
	Stats.ChildEntries.emplace_back(SnapshotMemory);
	return Stats;
}

void WStatsManager::UpdateFilterStats(std::string const& FilterName, WBytes In, WBytes Out)
{
	if (In == 0 && Out == 0)
	{
		return;
	}

	if (CurrentSnapshot.Filters.contains(FilterName))
	{
		auto& [BytesIn, BytesOut] = CurrentSnapshot.Filters[FilterName];
		BytesIn += In;
		BytesOut += Out;
	}
	else
	{
		WTrafficStats Stats{};
		Stats.BytesIn = In;
		Stats.BytesOut = Out;
		CurrentSnapshot.Filters[FilterName] = Stats;
	}
}

void WStatsManager::UpdateAppStats(
	WTrafficItemId AppId, std::string const& ApplicationPath, WIPAddress const& RemoteHost, WBytes In, WBytes Out)
{
	if (In == 0 && Out == 0)
	{
		// We sometimes push empty traffic to update the average to 0, but that is irrelevant for
		// the stats
		spdlog::trace("WStatsManager::UpdateAppStats(): Empty", AppId);
		return;
	}

	if (CurrentSnapshot.Apps.contains(AppId))
	{
		auto& [ItemName, Traffic] = CurrentSnapshot.Apps[AppId];
		if (ItemName.empty())
		{
			ItemName = ApplicationPath;
		}

		if (auto& AppTraffic = Traffic; AppTraffic.contains(RemoteHost))
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
		WItemStats NewAppStats{};
		NewAppStats.ApplicationPath = ApplicationPath;
		NewAppStats.Traffic[RemoteHost] = WTrafficStats{ In, Out };
		CurrentSnapshot.Apps[AppId] = NewAppStats;
	}
	spdlog::trace("WStatsManager::UpdateAppStats(): AppId={}, ApplicationPath={}, RemoteHost={}, In={}, Out={}", AppId,
		ApplicationPath, RemoteHost.ToString(), In, Out);
}

void WStatsManager::MakeSnapshot()
{
	spdlog::debug("Making traffic snapshot");
	// Grab current snapshot data and clear it under the lock
	WSnapshot LocalSnapshot{};
	{
		ZoneScopedN("MakeSnapshot - Swap snapshot");
		std::scoped_lock Lock(DataMutex);
		LocalSnapshot = std::move(CurrentSnapshot);
		CurrentSnapshot = {};
		spdlog::debug("Making snapshot of local snapshot: {} apps", LocalSnapshot.Apps.size());
	}

	WDbManager::GetInstance().Run([&](auto& DbConn) {
		ZoneScopedN("MakeSnapshot - DB write");
		constexpr Db::Schema::Asn             Asn;
		constexpr Db::Schema::TrafficSnapshot TS;
		constexpr Db::Schema::TrafficEvent    TE;
		constexpr Db::Schema::TrafficItem     TrafficItem;
		constexpr Db::Schema::Host            Host;

		int64_t const SnapshotID =
			static_cast<int64_t>(DbConn(sqlpp::insert_into(TS).set(TS.Start = WTime::GetEpochSeconds())));

		for (auto const& [AppId, AppStats] : LocalSnapshot.Apps)
		{
			if (AppStats.ApplicationPath.empty())
			{
				spdlog::debug("App ID {} has no application path in snapshot, skipping traffic event", AppId);
				continue;
			}

			auto AppResult = DbConn(
				sqlpp::select(TrafficItem.ID).from(TrafficItem).where(TrafficItem.Name == AppStats.ApplicationPath));
			int64_t const AppID = AppResult.empty()
				? static_cast<int64_t>(
					  DbConn(sqlpp::insert_into(TrafficItem).set(TrafficItem.Name = AppStats.ApplicationPath)))
				: AppResult.front().ID.value();

			for (auto const& [IP, Traffic] : AppStats.Traffic)
			{
				spdlog::debug("Recording traffic event: App '{}', Remote '{}', In {}, Out {}", AppStats.ApplicationPath,
					IP.ToString(), Traffic.BytesIn, Traffic.BytesOut);
				auto const AsnResult = WIP2Asn::GetInstance().LookupSync(IP);
				auto const HostResult =
					DbConn(sqlpp::select(Host.ID).from(Host).where(Host.IPAddress == IP.GetBytesVector()));
				int64_t AsnID = 0;
				if (AsnResult)
				{
					auto AsnIdResult = DbConn(sqlpp::select(Asn.ID).from(Asn).where(Asn.Number == AsnResult->ASN
						&& Asn.Country == AsnResult->Country && Asn.Organization == AsnResult->Organization));
					if (AsnIdResult.empty())
					{
						AsnID = static_cast<int64_t>(DbConn(sqlpp::insert_into(Asn).set(Asn.Number = AsnResult->ASN,
							Asn.Country = AsnResult->Country, Asn.Organization = AsnResult->Organization)));
						spdlog::debug("Inserted new ASN {} into database with ID {}", AsnResult->ASN, AsnID);
					}
					else
					{
						AsnID = AsnIdResult.front().ID.value();
					}
				}
				int64_t HostID = 0;
				// the table has a check and expects 4 or 6, but sometimes we get 0 for unknown family, so we default to
				// 4
				auto const Family = IP.Family == 0 ? 4 : static_cast<int>(IP.Family);

				if (AsnID > 0)
				{
					HostID = HostResult.empty()
						? static_cast<int64_t>(DbConn(sqlpp::insert_into(Host).set(
							  Host.IPAddress = IP.GetBytesVector(), Host.Family = Family, Host.AsnID = AsnID)))
						: HostResult.front().ID.value();
				}
				else
				{
					HostID = HostResult.empty() ? static_cast<int64_t>(DbConn(sqlpp::insert_into(Host).set(
													  Host.IPAddress = IP.GetBytesVector(), Host.Family = Family)))
												: HostResult.front().ID.value();
				}

				DbConn(sqlpp::insert_into(TE).set(TE.SnapshotID = SnapshotID, TE.ItemID = AppID, TE.HostID = HostID,
					TE.BytesIn = static_cast<int64_t>(Traffic.BytesIn),
					TE.BytesOut = static_cast<int64_t>(Traffic.BytesOut)));
			}
		}

		for (auto const& [FilterName, Traffic] : LocalSnapshot.Filters)
		{
			auto FilterResult =
				DbConn(sqlpp::select(TrafficItem.ID).from(TrafficItem).where(TrafficItem.Name == FilterName));
			int64_t const FilterID = FilterResult.empty()
				? static_cast<int64_t>(DbConn(sqlpp::insert_into(TrafficItem).set(TrafficItem.Name = FilterName)))
				: FilterResult.front().ID.value();
			DbConn(sqlpp::insert_into(TE).set(TE.SnapshotID = SnapshotID, TE.ItemID = FilterID,
				TE.BytesIn = static_cast<int64_t>(Traffic.BytesIn),
				TE.BytesOut = static_cast<int64_t>(Traffic.BytesOut)));
		}
	});
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

TPromise<std::string> WStatsManager::RequestStats(WBuffer const& Request, EMessageType const Type)
{
	WRequestData Data;
	Data.Request = Request;
	Data.Type = Type;
	std::scoped_lock Lock(QueueMutex);
	PendingRequests.push(Data);
	QueueCondition.notify_one();
	return Data.Response;
}

void WStatsManager::ProcessHistoryRequest(
	WConnectionHistoryRequest const& Request, TPromise<std::string> const& Promise)
{
	ZoneScopedN("WStatsManager::ProcessHistoryRequest");
	WConnectionHistoryResponse Response{};
	Response.RequestId = Request.RequestId;
	spdlog::info("Received stats request for {}", Request.AppTarget);

	WDbManager::GetInstance().Run([&](auto& DbConn) {
		constexpr Db::Schema::ConnectionHistoryEntry CE;
		if (Request.HostTarget)
		{
			constexpr Db::Schema::TrafficItem TrafficItem;
			constexpr Db::Schema::Host        Host;
			uint64_t                          HostID{};
			auto                              Result =
				DbConn(sqlpp::select(Host.ID).from(Host).where(Host.IPAddress == Request.HostTarget->GetBytesVector()));
			spdlog::info("History request for host {}", Request.HostTarget->ToString());
			if (Result.empty())
			{
				spdlog::info("No connection history data for {}", Request.HostTarget->ToString());
				Promise.Finish("");
				return;
			}
			HostID = static_cast<uint64_t>(Result.front().ID.value());
			auto Count = DbConn(sqlpp::select(sqlpp::count(CE.ID))
					.from(CE.join(TrafficItem).on(CE.ItemID == TrafficItem.ID))
					.where(CE.RemoteHostID == HostID));
			Response.NumTotalEntries = static_cast<uint64_t>(Count.front().count.value());
			spdlog::info("Found {} entries for {}", Response.NumTotalEntries, Request.HostTarget->ToString());
			auto Rows = DbConn(
				sqlpp::select(CE.ItemID, TrafficItem.Name, CE.Port, CE.StartTime, CE.EndTime, CE.DataIn, CE.DataOut)
					.from(CE.join(TrafficItem).on(CE.ItemID == TrafficItem.ID))
					.where(CE.RemoteHostID == HostID)
					.limit(100u)
					.offset(Request.Offset));
			for (auto const& Row : Rows)
			{
				Response.Entries.emplace_back(WConnectionHistoryResponseEntry{
					.AppName = Row.Name.value(),
					.Endpoint = {}, // endpoint request, so not filling this
					.DataIn = static_cast<WBytes>(Row.DataIn.value()),
					.DataOut = static_cast<WBytes>(Row.DataOut.value()),
					.StartTime = Row.StartTime.value(),
					.EndTime = Row.EndTime.value(),
				});
			}
		}
		else
		{
			constexpr Db::Schema::TrafficItem TrafficItem;
			constexpr Db::Schema::Host        Host;
			uint64_t                          ItemID{};
			auto                              Result =
				DbConn(sqlpp::select(TrafficItem.ID).from(TrafficItem).where(TrafficItem.Name == Request.AppTarget));
			if (Result.empty())
			{
				spdlog::info("No connection history data for {}", Request.AppTarget);
				Promise.Finish("");
				return;
			}
			ItemID = static_cast<uint64_t>(Result.front().ID.value());
			auto Count = DbConn(sqlpp::select(sqlpp::count(CE.ID))
					.from(CE.join(Host).on(CE.RemoteHostID == Host.ID))
					.where(CE.ItemID == ItemID));
			Response.NumTotalEntries = static_cast<uint64_t>(Count.front().count.value());
			auto Rows = DbConn(sqlpp::select(
				CE.ItemID, Host.IPAddress, Host.Family, CE.Port, CE.StartTime, CE.EndTime, CE.DataIn, CE.DataOut)
					.from(CE.join(Host).on(CE.RemoteHostID == Host.ID))
					.where(CE.ItemID == ItemID)
					.limit(100u)
					.offset(Request.Offset));
			spdlog::info("Found {} entries for {}", Response.NumTotalEntries, Request.AppTarget);
			for (auto const& Row : Rows)
			{
				WEndpoint  P{};
				auto const AddressBytes = Row.IPAddress.value();
				auto const Family = static_cast<EIPFamily::Type>(Row.Family.value());

				P.Port = Row.Port;
				P.Address.Family = Family;

				if (Family == EIPFamily::IPv4)
				{
					if (AddressBytes.size() == 4)
					{
						std::copy(AddressBytes.begin(), AddressBytes.end(), P.Address.Bytes.begin());
					}
					// otherwise it's ::/zero ip which defaults to v4
				}
				else if (Family == EIPFamily::IPv6)
				{
					assert(AddressBytes.size() == 16);
					std::copy(AddressBytes.begin(), AddressBytes.end(), P.Address.Bytes.begin());
				}
				else
				{
					spdlog::warn("Invalid IP family in history entry row: {}", Row.Family.value());
					continue;
				}

				Response.Entries.emplace_back(WConnectionHistoryResponseEntry{
					.AppName = "", // this is a request for one specific app so no point in filling this
					.Endpoint = P,
					.DataIn = static_cast<WBytes>(Row.DataIn.value()),
					.DataOut = static_cast<WBytes>(Row.DataOut.value()),
					.StartTime = Row.StartTime.value(),
					.EndTime = Row.EndTime.value(),
				});
			}
		}
		Promise.Finish(SerializeMessage(MT_HistoryResponse, Response));
	});
}

void WStatsManager::ProcessStatsRequest(WStatsRequest const& Request, TPromise<std::string> const& Promise)
{
	ZoneScopedN("WStatsManager::ProcessStatsRequest");
	WStatsResponse Response{};
	Response.RequestId = Request.RequestId;

	constexpr WSec OneHour = 3600LL;
	constexpr WSec OneDay = 86400LL;

	bool       bFailed = false;
	WSec const Duration = Request.EndTime - Request.StartTime;
	bool const BucketByMonth = Duration > 31LL * OneDay;
	WSec const BucketSecs = BucketByMonth ? 0LL : Duration <= 24LL * OneHour ? OneHour : OneDay;

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
		return T / BucketSecs * BucketSecs;
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

	// Snapshots are written when an interval ends, so their timestamp marks the end of the
	// sampled range rather than the beginning. Shift by one second before flooring so a
	// snapshot written exactly on a bucket boundary is attributed to the bucket that just ended.
	auto Accumulate = [&](WSec const SnapshotStart, WBytes const BytesIn, WBytes const BytesOut) {
		WSec const Key = GetBucketStart(std::max<WSec>(0, SnapshotStart - 1));
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
		Binary_Or_Filter,
		Host
	};

	ETargetType TargetType;
	if (Request.Target == "system")
	{
		TargetType = ETargetType::System;
	}
	else if (!Request.Target.empty() && Request.Target.front() == '/')
	{
		TargetType = ETargetType::Binary_Or_Filter;
	}
	else if (!Request.Target.empty()
		&& (Request.Target == "LAN" || Request.Target == "Internet" || Request.Target == "Localhost"))
	{
		TargetType = ETargetType::Binary_Or_Filter;
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
					.where(TS.Start > Request.StartTime && TS.Start <= Request.EndTime));
			for (auto const& Row : Rows)
			{
				Accumulate(Row.Start.value(), static_cast<uint64_t>(Row.BytesIn), static_cast<uint64_t>(Row.BytesOut));
			}
		}
		else if (TargetType == ETargetType::Binary_Or_Filter)
		{
			constexpr Db::Schema::TrafficItem TrafficItem;
			auto                              Rows = DbConn(sqlpp::select(TS.Start, TE.BytesIn, TE.BytesOut)
												 .from(TE.join(TS).on(TE.SnapshotID == TS.ID).join(TrafficItem).on(TE.ItemID == TrafficItem.ID))
												 .where(TS.Start > Request.StartTime && TS.Start <= Request.EndTime
													 && TrafficItem.Name == Request.Target));
			for (auto const& Row : Rows)
			{
				Accumulate(Row.Start.value(), static_cast<uint64_t>(Row.BytesIn), static_cast<uint64_t>(Row.BytesOut));
			}
		}
		else
		{
			auto const ParsedIP = WIPAddress::FromString(Request.Target);
			if (!ParsedIP)
			{
				spdlog::error("Invalid IP address in stats request: {}", Request.Target);
				bFailed = true;
				return;
			}
			constexpr Db::Schema::Host Host;
			auto                       Rows = DbConn(sqlpp::select(TS.Start, TE.BytesIn, TE.BytesOut)
										  .from(TE.join(TS).on(TE.SnapshotID == TS.ID).join(Host).on(TE.HostID == Host.ID))
									  .where(TS.Start > Request.StartTime && TS.Start <= Request.EndTime
											  && Host.IPAddress == ParsedIP->GetBytesVector()));
			for (auto const& Row : Rows)
			{
				Accumulate(Row.Start.value(), static_cast<uint64_t>(Row.BytesIn), static_cast<uint64_t>(Row.BytesOut));
			}
		}
	});

	if (bFailed)
	{
		Promise.Finish("");
		return;
	}

	// Flatten the ordered bucket map into the response vector
	Response.DataPoints.reserve(Buckets.size());
	for (auto const& Data : Buckets | std::views::values)
	{
		Response.DataPoints.push_back(Data);
	}

	Promise.Finish(SerializeMessage(MT_StatsResponse, Response));
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
		spdlog::debug("Processing stats request, queue size: {}", PendingRequests.size());
		auto RequestData = std::move(PendingRequests.front());
		PendingRequests.pop();
		Lock.unlock();

		switch (RequestData.Type)
		{
			case MT_HistoryRequest:
			{
				WConnectionHistoryRequest Request;
				if (!DeserializeMessage(RequestData.Request, Request))
				{
					spdlog::error("Failed to deserialize stats request");
					return;
				}
				ProcessHistoryRequest(Request, RequestData.Response);
			}
			break;
			case MT_StatsRequest:
			{
				WStatsRequest Request;
				if (!DeserializeMessage(RequestData.Request, Request))
				{
					spdlog::error("Failed to deserialize stats request");
					return;
				}
				ProcessStatsRequest(Request, RequestData.Response);
			}
			break;
			default:
				assert(false && "Invalid stats request");
		}
	}
}
