/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <mutex>

#include "Data/Stats.hpp"

struct ImGuiTableSortSpecs;

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
	std::vector<uint64_t>    StartTimeNumeric;
	std::vector<std::string> EndTime;
	std::vector<uint64_t>    EndTimeNumeric;
	std::vector<std::string> BytesIn;
	std::vector<uint64_t>    BytesInNumeric;
	std::vector<std::string> BytesOut;
	std::vector<uint64_t>    BytesOutNumeric;

	void Clear()
	{
		AppOrRemoteEndpoint.clear();
		Protocol.clear();
		StartTime.clear();
		StartTimeNumeric.clear();
		EndTime.clear();
		EndTimeNumeric.clear();
		BytesIn.clear();
		BytesInNumeric.clear();
		BytesOut.clear();
		BytesOutNumeric.clear();
	}

	void Reserve(size_t Size, bool bWithProtocol = false)
	{
		AppOrRemoteEndpoint.reserve(Size);
		if (bWithProtocol)
			Protocol.reserve(Size);
		StartTime.reserve(Size);
		StartTimeNumeric.reserve(Size);
		EndTime.reserve(Size);
		EndTimeNumeric.reserve(Size);
		BytesIn.reserve(Size);
		BytesInNumeric.reserve(Size);
		BytesOut.reserve(Size);
		BytesOutNumeric.reserve(Size);
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
	enum EHistoryState
	{
		State_RequestPending,
		State_DataReceived
	} HistoryState{};

	uint           HistoryMaxPages{ 1 };
	uint           HistoryPage{ 1 };
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
	bool                     bRequireSorting{};

	void ApplyTimeFrame(ETimeFrame TimeFrame);
	void UpdateTitle();

	void BuildGraphData();

	void DrawGraphTab();
	void DrawHistoryData();

	void BuildHistoryDataCache();

	void SortTable(ImGuiTableSortSpecs const* Specs, bool bIsAppTable);

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
		bRequireSorting = true;
		HistoryState = State_DataReceived;
		HistoryMaxPages = std::max<uint>(static_cast<uint>(InResponse.NumTotalEntries / 100), 1);
	}

	[[nodiscard]] uint32_t GetRequestId() const { return Request.RequestId; }
	[[nodiscard]] uint32_t GetHistoryRequestId() const { return HistoryRequest.RequestId; }

	[[nodiscard]] bool IsOpen() const { return bOpen; }
};
