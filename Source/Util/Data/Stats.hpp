/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>
#include <vector>
#include <memory>

// ReSharper disable CppUnusedIncludeDirective
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/memory.hpp"
// ReSharper enable CppUnusedIncludeDirective

#include "Types.hpp"
#include "IPAddress.hpp"

struct WStatsRequest
{
	uint32_t    RequestId;
	WSec        StartTime; // unix timestamps
	WSec        EndTime;
	std::string Target;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(StartTime, EndTime, Target, RequestId);
	}
};

struct WStatData
{
	WBytes In;
	WBytes Out;
	WSec   Timestamp;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(In, Out, Timestamp);
	}
};

struct WStatsResponse
{
	std::vector<WStatData> DataPoints;
	uint32_t               RequestId;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(DataPoints, RequestId);
	}
};

struct WConnectionHistoryRequest
{
	uint32_t                    RequestId{};
	std::string                 TargetName{}; // if we want the history of an app, ASN, country or organization
	std::shared_ptr<WIPAddress> HostTarget{}; // if we want the history for a specific ip
	uint64_t                    Offset{};
	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(RequestId, TargetName, HostTarget, Offset);
	}
};

// app, port, (protocol on the port), data in, data out, start, end

struct WConnectionHistoryResponseEntry
{
	std::string AppName;
	WEndpoint   Endpoint;
	WBytes      DataIn;
	WBytes      DataOut;
	WSec        StartTime;
	WSec        EndTime;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(AppName, Endpoint, DataIn, DataOut, StartTime, EndTime);
	}
};

struct WConnectionHistoryResponse
{
	uint32_t                                     RequestId;
	std::vector<WConnectionHistoryResponseEntry> Entries;
	uint64_t                                     NumTotalEntries;
	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(RequestId, Entries, NumTotalEntries);
	}
};