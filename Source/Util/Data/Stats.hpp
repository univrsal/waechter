/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Types.hpp"
#include <string>
#include <vector>

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