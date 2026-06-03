/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "StatWindow.hpp"

#include <ctime>

#include "Client.hpp"
#include "imgui.h"
#include "implot.h"

#include "Format.hpp"
#include "Windows/SdlWindow.hpp"

static constexpr WSec SecondsPerDay = 86400LL;
static constexpr WSec SecondsPerWeek = 7LL * SecondsPerDay;
static constexpr WSec SecondsPerYear = 365LL * SecondsPerDay;

static WSec ApproximateMonthSeconds()
{
	// Use current month length for a more accurate "last month" range
	std::time_t Now = std::time(nullptr);
	std::tm*    Tm = std::localtime(&Now);
	int         Month = Tm->tm_mon; // 0-11
	int         Year = Tm->tm_year + 1900;
	// Days in the previous calendar month
	int const  PrevMonth = (Month == 0) ? 12 : Month;
	int const  PrevYear = (Month == 0) ? Year - 1 : Year;
	int const  Days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	bool const Leap = (PrevYear % 4 == 0 && (PrevYear % 100 != 0 || PrevYear % 400 == 0));
	int const  DaysInMonth = (PrevMonth == 2 && Leap) ? 29 : Days[PrevMonth - 1];
	return static_cast<WSec>(DaysInMonth) * SecondsPerDay;
}

void WStatWindow::ApplyTimeFrame(ETimeFrame TimeFrame)
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

WStatWindow::WStatWindow(WStatsRequest const& InRequest) : Request(InRequest)
{
	UpdateTitle();
	WClient::GetInstance().SendMessage(MT_StatsRequest, Request);
}

static constexpr char const* TimeFrameLabels[] = {
	"Last 24 hours",
	"Last 7 days",
	"Last month",
	"Last year",
};

void WStatWindow::Draw()
{
	if (!bOpen)
	{
		return;
	}
	ImVec2 const DefaultWindowSize = WSdlWindow::ScaleSize(ImVec2(800, 450));
	ImGuiIO const& Io = ImGui::GetIO();
	ImGui::SetNextWindowSize(DefaultWindowSize, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(Io.DisplaySize.x * 0.5f, Io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
	if (ImGui::Begin(Title.c_str(), &bOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
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
			auto const N = Response.DataPoints.size();

			// Choose a compact timestamp format based on the request duration
			WSec const     Duration = Request.EndTime - Request.StartTime;
			constexpr WSec OneDay = 86400LL;
			char const*    TimeFmt = Duration <= OneDay ? "%H:%M" : (Duration <= 31LL * OneDay ? "%m-%d" : "%Y-%m");

			// Build per-group labels and value arrays
			std::vector<std::string> LabelStrings(N);
			std::vector<char const*> LabelPtrs(N);
			std::vector<double>      Positions(N);
			// PlotBarGroups layout: values[item * group_count + group]
			// item 0 = Download (In), item 1 = Upload (Out)
			std::vector<double> Values(2 * N);

			for (std::size_t i = 0; i < N; ++i)
			{
				LabelStrings[i] = WTimeFormat::FormatUnixTime(Response.DataPoints[i].Timestamp, TimeFmt);
				LabelPtrs[i] = LabelStrings[i].c_str();
				Positions[i] = static_cast<double>(i);
				Values[i] = static_cast<double>(Response.DataPoints[i].In);      // Download
				Values[N + i] = static_cast<double>(Response.DataPoints[i].Out); // Upload
			}

			static auto ByteFormatter = [](double Value, char* Buf, int Size, void*) -> int {
				auto Str = WStorageFormat::AutoFormat(static_cast<WBytes>(Value < 0.0 ? 0.0 : Value));
				return snprintf(Buf, static_cast<std::size_t>(Size), "%s", Str.c_str());
			};

			char const* SeriesLabels[] = { "Download", "Upload" };

			if (ImPlot::BeginPlot("##TrafficChart", ImVec2(-1, -1)))
			{
				ImPlot::SetupAxes("Time", "Traffic");
				ImPlot::SetupAxisTicks(ImAxis_X1, Positions.data(), N, LabelPtrs.data());
				ImPlot::SetupAxisFormat(ImAxis_Y1, ByteFormatter, nullptr);
				ImPlot::PlotBarGroups(SeriesLabels, Values.data(), 2, N, 0.67, 0.0);
				ImPlot::EndPlot();
			}
		}
	}
	ImGui::End();
}