/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <mutex>

#include "Data/Stats.hpp"

enum class ETimeFrame : int
{
	Last24Hours = 0,
	Last7Days   = 1,
	LastMonth   = 2,
	LastYear    = 3,
};

struct WHistoryDataCache
{
	std::vector<std::string> AppOrRemoteEndpoint;
	std::vector<std::string> Protocol;
	std::vector<std::string> StartTime;
	std::vector<std::string> EndTime;
	std::vector<std::string> BytesIn;
	std::vector<std::string> BytesOut;

	void Clear()
	{
		AppOrRemoteEndpoint.clear();
		Protocol.clear();
		StartTime.clear();
		EndTime.clear();
		BytesIn.clear();
		BytesOut.clear();
	}

	void Reserve(size_t Size, bool bWithProtocol = false)
	{
		AppOrRemoteEndpoint.reserve(Size);
		if (bWithProtocol)
			Protocol.reserve(Size);
		StartTime.reserve(Size);
		EndTime.reserve(Size);
		BytesIn.reserve(Size);
		BytesOut.reserve(Size);
	}
};

class WStatWindow
{
	std::mutex                 DataMutex;
	WHistoryDataCache          HistoryDataCache;
	WStatsRequest              Request{};
	WStatsResponse             Response{};
	WConnectionHistoryResponse HistoryResponse{};
	WConnectionHistoryRequest  HistoryRequest{};
	std::string    Title;
	bool           bOpen{ true };
	ETimeFrame     CurrentTimeFrame{ ETimeFrame::Last24Hours };

	std::vector<std::string> LabelStrings;
	std::vector<char const*> LabelPtrs;
	std::vector<double>      Positions;
	std::vector<double>      Values;
	std::vector<double>      FilteredPositions;
	std::vector<char const*> FilteredLabelPtrs;
	double                   YAxisMax{};

	void ApplyTimeFrame(ETimeFrame TimeFrame);
	void UpdateTitle();

	void BuildGraphData();

	void DrawGraphTab();
	void DrawHistoryData();

	void BuildHistoryDataCache();

public:
	WStatWindow(WStatsRequest InRequest, WConnectionHistoryRequest InHistoryRequest);
	void Draw();

	void SetResponse(WStatsResponse const& InResponse)
	{
		std::scoped_lock Lock(DataMutex);
		Response = InResponse;
		BuildGraphData();
	}

	void SetHistoryResponse(WConnectionHistoryResponse const& InResponse)
	{
		std::scoped_lock Lock(DataMutex);
		HistoryResponse = InResponse;
		BuildHistoryDataCache();
	}

	[[nodiscard]] uint32_t GetRequestId() const { return Request.RequestId; }
	[[nodiscard]] uint32_t GetHistoryRequestId() const { return HistoryRequest.RequestId; }

	[[nodiscard]] bool IsOpen() const { return bOpen; }
};
