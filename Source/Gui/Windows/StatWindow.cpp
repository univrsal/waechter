/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "StatWindow.hpp"

#include <limits>
#include <ctime>
#include <numeric>
#include <utility>

#include "imgui.h"
#include "implot.h"

#include "Client.hpp"
#include "Format.hpp"
#include "Util/I18n.hpp"
#include "Util/ProtocolDB.hpp"
#include "Windows/SdlWindow.hpp"

static constexpr WSec SecondsPerDay = 86400LL;
static constexpr WSec SecondsPerWeek = 7LL * SecondsPerDay;
static constexpr WSec SecondsPerYear = 365LL * SecondsPerDay;

static WSec ApproximateMonthSeconds()
{
	// Use current month length for a more accurate "last month" range
	std::time_t const Now = std::time(nullptr);
	std::tm const*    Tm = std::localtime(&Now);
	int const         Month = Tm->tm_mon; // 0-11
	int const         Year = Tm->tm_year + 1900;
	// Days in the previous calendar month
	int const  PrevMonth = Month == 0 ? 12 : Month;
	int const  PrevYear = Month == 0 ? Year - 1 : Year;
	int const  Days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	bool const Leap = PrevYear % 4 == 0 && (PrevYear % 100 != 0 || PrevYear % 400 == 0);
	int const  DaysInMonth = PrevMonth == 2 && Leap ? 29 : Days[PrevMonth - 1];
	return static_cast<WSec>(DaysInMonth) * SecondsPerDay;
}

void WStatWindow::ApplyTimeFrame(ETimeFrame const TimeFrame)
{
	CurrentTimeFrame = TimeFrame;
	WSec const Now = WTime::GetEpochHours();
	WSec       Elapsed = SecondsPerDay;
	switch (TimeFrame)
	{
		case ETimeFrame::Last24Hours:
			Elapsed = SecondsPerDay;
			break;
		case ETimeFrame::Last7Days:
			Elapsed = SecondsPerWeek;
			break;
		case ETimeFrame::LastMonth:
			Elapsed = ApproximateMonthSeconds();
			break;
		case ETimeFrame::LastYear:
			Elapsed = SecondsPerYear;
			break;
	}
	Request.StartTime = Now - Elapsed;
	Request.EndTime = Now;
	Response.DataPoints.clear();
	UpdateTitle();
	WClient::GetInstance().SendMessage(MT_StatsRequest, Request);
}

void WStatWindow::UpdateTitle()
{
	Title = Request.Target + " (" + WTimeFormat::FormatUnixTime(Request.StartTime) + " - "
		+ WTimeFormat::FormatUnixTime(Request.EndTime) + ")###StatWindow_" + std::to_string(Request.RequestId);
}

void WStatWindow::BuildGraphData()
{
	auto const N = Response.DataPoints.size();
	double     MaxTraffic = 0.0;

	// Choose a compact timestamp format based on the request duration
	WSec const     Duration = Request.EndTime - Request.StartTime;
	constexpr WSec OneDay = 86400LL;
	char const*    TimeFmt = Duration <= OneDay ? "%H:%M" : Duration <= 31LL * OneDay ? "%m-%d" : "%Y-%m";

	// Build per-group labels and value arrays
	LabelStrings.resize(N);
	LabelPtrs.resize(N);
	Positions.resize(N);
	// PlotBarGroups layout: values[item * group_count + group]
	// item 0 = Download (In), item 1 = Upload (Out)
	Values.resize(2 * N);

	for (std::size_t i = 0; i < N; ++i)
	{
		LabelStrings[i] = WTimeFormat::FormatUnixTime(Response.DataPoints[i].Timestamp, TimeFmt);
		LabelPtrs[i] = LabelStrings[i].c_str();
		Positions[i] = static_cast<double>(i);
		Values[i] = static_cast<double>(Response.DataPoints[i].In);      // Download
		Values[N + i] = static_cast<double>(Response.DataPoints[i].Out); // Upload
		MaxTraffic = std::max(MaxTraffic, std::max(Values[i], Values[N + i]));
	}

	YAxisMax = MaxTraffic > 0.0 ? MaxTraffic * 1.10 : 1.0;
}

static constexpr char const* TimeFrameLabels[] = {
	"Last 24 hours",
	"Last 7 days",
	"Last month",
	"Last year",
};

void WStatWindow::DrawGraphTab()
{
	// Time frame selector
	int Selection = static_cast<int>(CurrentTimeFrame);
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::Combo("##TimeFrame", &Selection, TimeFrameLabels, 4))
	{
		std::scoped_lock Lock(DataMutex);
		ApplyTimeFrame(static_cast<ETimeFrame>(Selection));
	}
	ImGui::Separator();

	std::scoped_lock Lock(DataMutex);
	if (Response.DataPoints.empty())
	{
		ImGui::Text("Loading...");
	}
	else
	{

		static auto ByteFormatter = [](double const Value, char* Buf, int const Size, void*) -> int {
			auto const Str = WStorageFormat::AutoFormat(static_cast<WBytes>(Value < 0.0 ? 0.0 : Value));
			return snprintf(Buf, static_cast<std::size_t>(Size), "%s", Str.c_str());
		};

		char const* SeriesLabels[] = { TR("download"), TR("upload") };
		auto const  N = Response.DataPoints.size();

		if (ImPlot::BeginPlot("##TrafficChart", ImVec2(-1, -1)))
		{
			// Thin tick labels so they don't overlap on the X axis.
			WSec const        Duration = Request.EndTime - Request.StartTime;
			constexpr WSec    OneDay = 86400LL;
			float const       PlotWidth = ImGui::GetContentRegionAvail().x;
			float const       EstLabelWidth = Duration <= OneDay ? 55.0f : 90.0f; // "HH:MM" vs "MM-DD" / "YYYY-MM"
			std::size_t const SkipN =
				std::max<std::size_t>(1, std::ceil<std::size_t>(EstLabelWidth * static_cast<float>(N) / PlotWidth));

			FilteredPositions.clear();
			FilteredLabelPtrs.clear();
			for (std::size_t i = 0; i < N; ++i)
			{
				if (i % SkipN == 0)
				{
					FilteredPositions.push_back(Positions[i]);
					FilteredLabelPtrs.push_back(LabelPtrs[i]);
				}
			}

			ImPlot::SetupAxes(TR("time"), TR("traffic"));
			ImPlot::SetupAxisTicks(ImAxis_X1, FilteredPositions.data(), static_cast<int>(FilteredPositions.size()),
				FilteredLabelPtrs.data());
			ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, -0.5, static_cast<double>(N) - 0.5);
			ImPlot::SetupAxisFormat(ImAxis_Y1, ByteFormatter, nullptr);
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, YAxisMax, ImGuiCond_Always);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, std::numeric_limits<double>::infinity());
			ImPlot::PlotBarGroups(SeriesLabels, Values.data(), 2, N, 0.7);
			ImPlot::EndPlot();
		}
	}
}

void WStatWindow::DrawHistoryData()
{
	std::scoped_lock Lock(DataMutex);

	// Reserve space for pagination bar at the bottom
	float const PaginationHeight = ImGui::GetFrameHeightWithSpacing();
	float const TableHeight = ImGui::GetContentRegionAvail().y - PaginationHeight;

	if (ImGui::BeginChild(
			"##HistoryTableScroll", ImVec2(0, TableHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoMove))
	{
		if (HistoryDataCache.AppOrRemoteEndpoint.empty())
		{
			if (HistoryState == State_RequestPending)
			{
				ImGui::Text("Loading...");
			}
			else
			{
				ImGui::Text("No data.");
			}
		}
		else
		{
			if (ImGui::BeginTable("##ConnectionHistoryTable", 7,
					ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY
						| ImGuiTableFlags_Sortable))
			{
				auto const bIsAppTable = !HistoryDataCache.Protocol.empty();
				if (bIsAppTable)
				{
					ImGui::TableSetupColumn(TR("ip"), ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn(TR("protocol"), ImGuiTableColumnFlags_WidthFixed, 80.0f);
				}
				else
				{
					ImGui::TableSetupColumn(TR("app_name"), ImGuiTableColumnFlags_WidthStretch);
				}

				ImGui::TableSetupColumn(TR("data_in"), ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn(TR("data_out"), ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn(TR("start_time"), ImGuiTableColumnFlags_WidthFixed, 180.0f);
				ImGui::TableSetupColumn(TR("end_time"), ImGuiTableColumnFlags_WidthFixed, 180.0f);
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableHeadersRow();

				if (auto* Sort = ImGui::TableGetSortSpecs(); Sort && !ImGui::GetIO().KeyCtrl)
				{
					if (Sort->SpecsDirty)
					{
						SortTable(Sort, bIsAppTable);
						Sort->SpecsDirty = false;
					}
				}

				for (size_t i = 0; i < HistoryDataCache.AppOrRemoteEndpoint.size(); ++i)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%s", HistoryDataCache.AppOrRemoteEndpoint[i].c_str());
					if (bIsAppTable)
					{
						ImGui::TableNextColumn();
						ImGui::Text("%s", HistoryDataCache.Protocol[i].c_str());
					}

					ImGui::TableNextColumn();
					ImGui::Text("%s", HistoryDataCache.BytesIn[i].c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%s", HistoryDataCache.BytesOut[i].c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%s", HistoryDataCache.StartTime[i].c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%s", HistoryDataCache.EndTime[i].c_str());
				}

				ImGui::EndTable();
			}
		}
	}
	ImGui::EndChild();

	if (ImGui::Button(TR("<")))
	{
		HistoryPage = HistoryPage > 1 ? HistoryPage - 1 : 1;
		HistoryRequest.Offset = std::max<uint64_t>(0, HistoryPage * 100);
		WClient::GetInstance().SendMessage(MT_HistoryRequest, HistoryRequest);
		HistoryState = State_RequestPending;
	}
	ImGui::SameLine();
	ImGui::Text("Page %i of %i", HistoryPage, HistoryMaxPages);
	ImGui::SameLine();
	if (ImGui::Button(">"))
	{
		HistoryPage = std::min<uint64_t>(HistoryMaxPages, HistoryPage + 1);
		HistoryRequest.Offset = std::min<uint64_t>(HistoryPage * 100, HistoryResponse.NumTotalEntries - 100);
		WClient::GetInstance().SendMessage(MT_HistoryRequest, HistoryRequest);
		HistoryState = State_RequestPending;
	}
}
void WStatWindow::BuildHistoryDataCache()
{
	HistoryDataCache.Clear();

	if (HistoryResponse.Entries.empty())
	{
		return;
	}

	if (HistoryResponse.Entries[0].AppName != "") // this is the history of an endpoint
	{
		HistoryDataCache.Reserve(HistoryResponse.Entries.size());
		for (auto const& Entry : HistoryResponse.Entries)
		{
			HistoryDataCache.AppOrRemoteEndpoint.push_back(Entry.AppName);
			HistoryDataCache.BytesIn.push_back(WStorageFormat::AutoFormat(Entry.DataIn));
			HistoryDataCache.BytesOut.push_back(WStorageFormat::AutoFormat(Entry.DataOut));
			HistoryDataCache.StartTime.push_back(WTimeFormat::FormatUnixTime(Entry.StartTime));
			HistoryDataCache.EndTime.push_back(WTimeFormat::FormatUnixTime(Entry.EndTime));
			HistoryDataCache.BytesInNumeric.push_back(Entry.DataIn);
			HistoryDataCache.BytesOutNumeric.push_back(Entry.DataOut);
			HistoryDataCache.StartTimeNumeric.push_back(static_cast<uint64_t>(Entry.StartTime));
			HistoryDataCache.EndTimeNumeric.push_back(static_cast<uint64_t>(Entry.EndTime));
		}
	}
	else
	{
		HistoryDataCache.Reserve(HistoryResponse.Entries.size(), /* bWithProtocol */ true);
		for (auto const& Entry : HistoryResponse.Entries)
		{
			HistoryDataCache.AppOrRemoteEndpoint.push_back(Entry.Endpoint.ToString());
			auto TCPProto = WProtocolDB::GetInstance().GetServiceName(EProtocol::UDP, Entry.Endpoint.Port);
			auto UDPProto = WProtocolDB::GetInstance().GetServiceName(EProtocol::UDP, Entry.Endpoint.Port);
			if (UDPProto != "" && TCPProto != "")
				HistoryDataCache.Protocol.push_back(TCPProto);
			else if (UDPProto != "")
				HistoryDataCache.Protocol.push_back(UDPProto);
			else if (TCPProto != "")
				HistoryDataCache.Protocol.push_back(TCPProto);
			else
				HistoryDataCache.Protocol.push_back("n/a");
			HistoryDataCache.BytesIn.push_back(WStorageFormat::AutoFormat(Entry.DataIn));
			HistoryDataCache.BytesOut.push_back(WStorageFormat::AutoFormat(Entry.DataOut));
			HistoryDataCache.StartTime.push_back(WTimeFormat::FormatUnixTime(Entry.StartTime));
			HistoryDataCache.EndTime.push_back(WTimeFormat::FormatUnixTime(Entry.EndTime));
			HistoryDataCache.BytesInNumeric.push_back(Entry.DataIn);
			HistoryDataCache.BytesOutNumeric.push_back(Entry.DataOut);
			HistoryDataCache.StartTimeNumeric.push_back(static_cast<uint64_t>(Entry.StartTime));
			HistoryDataCache.EndTimeNumeric.push_back(static_cast<uint64_t>(Entry.EndTime));
		}
	}
}

void WStatWindow::SortTable(ImGuiTableSortSpecs const* Specs, bool const bIsAppTable)
{
	if (Specs->SpecsCount == 0)
	{
		return;
	}

	auto const& ColSpec = Specs->Specs[0];
	auto const  N = HistoryDataCache.AppOrRemoteEndpoint.size();
	if (N == 0)
	{
		return;
	}

	int        NumCols;
	bool const Ascending = ColSpec.SortDirection == ImGuiSortDirection_Ascending;

	// Build LUT: column index -> numeric sort vector (nullptr for string columns)
	std::vector<uint64_t>* NumLut[6] = {};
	if (bIsAppTable)
	{
		// [0]=AppName(S), [1]=Protocol(S), [2]=BytesIn, [3]=BytesOut, [4]=StartTime, [5]=EndTime
		NumLut[2] = &HistoryDataCache.BytesInNumeric;
		NumLut[3] = &HistoryDataCache.BytesOutNumeric;
		NumLut[4] = &HistoryDataCache.StartTimeNumeric;
		NumLut[5] = &HistoryDataCache.EndTimeNumeric;
		NumCols = 6;
	}
	else
	{
		// [0]=IP(S), [1]=BytesIn, [2]=BytesOut, [3]=StartTime, [4]=EndTime
		NumLut[1] = &HistoryDataCache.BytesInNumeric;
		NumLut[2] = &HistoryDataCache.BytesOutNumeric;
		NumLut[3] = &HistoryDataCache.StartTimeNumeric;
		NumLut[4] = &HistoryDataCache.EndTimeNumeric;
		NumCols = 5;
	}

	if (ColSpec.ColumnIndex >= NumCols)
	{
		return;
	}

	// Create index array and sort by the target column
	std::vector<size_t> Indices(N);
	std::iota(Indices.begin(), Indices.end(), 0);

	if (auto const* NumVec = NumLut[ColSpec.ColumnIndex])
	{
		// Numeric column — compare uint64_t directly
		auto const& SortCol = *NumVec;
		std::ranges::sort(Indices, [&](size_t const A, size_t const B) {
			return Ascending ? SortCol[A] < SortCol[B] : SortCol[A] > SortCol[B];
		});
	}
	else
	{
		// String column (AppOrRemoteEndpoint or Protocol)
		std::vector<std::string> const& SortCol =
			ColSpec.ColumnIndex == 0 ? HistoryDataCache.AppOrRemoteEndpoint : HistoryDataCache.Protocol;
		std::ranges::sort(Indices, [&](size_t const A, size_t const B) {
			int const Cmp = SortCol[A].compare(SortCol[B]);
			return Ascending ? Cmp < 0 : Cmp > 0;
		});
	}

	// Reorder all columns according to sorted indices
	auto Reorder = [&]<typename T>(std::vector<T>& Vec) {
		auto Sorted = Vec;
		for (size_t i = 0; i < N; ++i)
		{
			Vec[i] = std::move(Sorted[Indices[i]]);
		}
	};

	Reorder(HistoryDataCache.AppOrRemoteEndpoint);
	if (bIsAppTable)
	{
		Reorder(HistoryDataCache.Protocol);
	}
	Reorder(HistoryDataCache.BytesIn);
	Reorder(HistoryDataCache.BytesOut);
	Reorder(HistoryDataCache.StartTime);
	Reorder(HistoryDataCache.EndTime);
	Reorder(HistoryDataCache.BytesInNumeric);
	Reorder(HistoryDataCache.BytesOutNumeric);
	Reorder(HistoryDataCache.StartTimeNumeric);
	Reorder(HistoryDataCache.EndTimeNumeric);
}

WStatWindow::WStatWindow(WStatsRequest InRequest, WConnectionHistoryRequest InHistoryRequest)
	: Request(std::move(InRequest)), HistoryRequest(std::move(InHistoryRequest))
{
	UpdateTitle();
	WClient::GetInstance().SendMessage(MT_StatsRequest, Request);
	WClient::GetInstance().SendMessage(MT_HistoryRequest, HistoryRequest);
	HistoryState = State_RequestPending;
}

void WStatWindow::Draw()
{
	if (!bOpen)
	{
		return;
	}
	ImVec2 const   DefaultWindowSize = WSdlWindow::ScaleSize(ImVec2(800, 450));
	ImGuiIO const& Io = ImGui::GetIO();
	ImGui::SetNextWindowSize(DefaultWindowSize, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(
		ImVec2(Io.DisplaySize.x * 0.5f, Io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
	if (ImGui::Begin(Title.c_str(), &bOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
	{
		if (ImGui::BeginTabBar("##Statswindow"))
		{
			if (ImGui::BeginTabItem(TR("graph")))
			{
				DrawGraphTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(TR("window.connection_history")))
			{
				DrawHistoryData();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
}