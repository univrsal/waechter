/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "NetworkGraphWindow.hpp"

#include <cmath>
#include <cstdio>
#include <limits>

#include "imgui.h"
#include "implot.h"
#include "Util/I18n.hpp"
#include "tracy/Tracy.hpp"

#include "Util/Settings.hpp"

double CalculateNiceTickInterval(double Range, int TargetTicks = 5)
{
	if (Range <= 0.0)
		return 1.0;
	double const RoughInterval = Range / static_cast<double>(TargetTicks);
	double const Magnitude = std::pow(10.0, std::floor(std::log10(RoughInterval)));
	double const Normalized = RoughInterval / Magnitude;

	double NiceNormalized;
	if (Normalized <= 1.0)
		NiceNormalized = 1.0;
	else if (Normalized <= 2.0)
		NiceNormalized = 2.0;
	else if (Normalized <= 5.0)
		NiceNormalized = 5.0;
	else
		NiceNormalized = 10.0;

	return NiceNormalized * Magnitude;
}

int FormatBandwidth(double Value, char* Buf, int Size, void* UserData)
{
	auto const*  U = static_cast<WNetworkGraphWindow::WUnitFmt const*>(UserData);
	double const V = Value / (U ? U->Factor : 1.0);
	return std::snprintf(Buf, static_cast<size_t>(Size), "%.2f", V);
}

int FormatTimeAgo(double Value, char* Buf, int Size, void*)
{
	// Value represents seconds in the past (positive = past)
	double const Seconds = Value;
	if (Seconds < 60.0)
		return std::snprintf(Buf, static_cast<size_t>(Size), "%.0f s", Seconds);

	double const Minutes = Seconds / 60.0;
	if (Minutes < 60.0)
	{
		// Show decimal for minutes to avoid duplicate labels
		if (Minutes < 10.0)
			return std::snprintf(Buf, static_cast<size_t>(Size), "%.1f min", Minutes);
		else
			return std::snprintf(Buf, static_cast<size_t>(Size), "%.0f min", Minutes);
	}

	double const Hours = Minutes / 60.0;
	return std::snprintf(Buf, static_cast<size_t>(Size), "%.1f h", Hours);
}

WNetworkGraphWindow::WNetworkGraphWindow()
{
	UploadBuffer.AddPoint(0, 0);
	DownloadBuffer.AddPoint(0, 0);
	auto Val = std::clamp(WSettings::GetInstance().NetworkGraphHistorySetting, 0, NGH_Count - 1);
	History = HistoryDuration[Val];
}

void WNetworkGraphWindow::AddData(WBytesPerSecond Upload, WBytesPerSecond Download)
{
	std::lock_guard Lock(Mutex);
	UploadBuffer.AddPoint(static_cast<float>(Time), static_cast<float>(Upload));
	DownloadBuffer.AddPoint(static_cast<float>(Time), static_cast<float>(Download));

	CurrentMaxUploadRate = 0.0;
	CurrentMaxDownloadRate = 0.0;
	double const T0 = Time - HistoryDuration[WSettings::GetInstance().NetworkGraphHistorySetting];

	auto CalcMaxRate = [&](auto const& Buffer, WBytesPerSecond& MaxRate) {
		for (auto const& Point : Buffer.Data)
		{
			if (std::isfinite(static_cast<double>(Point.y)) && static_cast<double>(Point.x) >= T0)
			{
				if (static_cast<double>(Point.y) > MaxRate)
				{
					MaxRate = static_cast<double>(Point.y);
				}
			}
		}
	};

	CalcMaxRate(UploadBuffer, CurrentMaxUploadRate);
	CalcMaxRate(DownloadBuffer, CurrentMaxDownloadRate);

	auto ChooseUnit = [](WBytesPerSecond Rate) -> WUnitFmt {
		if (Rate >= 1 WGiB)
		{
			return { "GiB/s", 1 WGiB };
		}
		if (Rate >= 1 WMiB)
		{
			return { "MiB/s", 1 WMiB };
		}
		if (Rate >= 1 WKiB)
		{
			return { "KiB/s", 1 WKiB };
		}
		return { "B/s", 1.0 };
	};

	UploadFmt = ChooseUnit(CurrentMaxUploadRate);
	DownloadFmt = ChooseUnit(CurrentMaxDownloadRate);
}

void WNetworkGraphWindow::Draw()
{
	ZoneScopedN("WNetworkGraphWindow::Draw");
	Time += static_cast<double>(ImGui::GetIO().DeltaTime);
	if (ImGui::Begin(TR("window.network_activity"), nullptr, ImGuiWindowFlags_None))
	{
		// History duration dropdown
		static char const*  HistoryOptions[] = { "1 min", "5 min", "15 min" };

		ImGui::SetNextItemWidth(100);
		if (ImGui::Combo(TR("duration"), &WSettings::GetInstance().NetworkGraphHistorySetting, HistoryOptions,
				IM_ARRAYSIZE(HistoryOptions)))
		{
			History = HistoryDuration[WSettings::GetInstance().NetworkGraphHistorySetting];
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100);
		ImGui::SliderFloat(TR("line_width"), &WSettings::GetInstance().NetworkGraphLineWidth, 0.5f, 5.0f, "%.1f");

		if (ImPlot::BeginPlot("##Scrolling", ImVec2(-1, -1)))
		{
			std::lock_guard Lock(Mutex);

			// Setup axes: show chosen unit on Y axis, time ago on X axis
			ImPlot::SetupAxes("Time", DownloadFmt.Label, 0, 0);
			ImPlot::SetupAxisFormat(ImAxis_Y1, FormatBandwidth, &DownloadFmt);
			ImPlot::SetupAxisFormat(ImAxis_X1, FormatTimeAgo, nullptr);

			// X-axis: offset by 1 second to avoid gap (show 1s ago to History+1s ago)
			// This ensures we only show the range of data we actually have
			ImPlot::SetupAxisLimits(ImAxis_X1, 1.0, History + 1.0, ImGuiCond_Always);

			// Calculate aligned Y-axis limits to prevent grid interference
			// Use the same number of ticks for both axes
			constexpr int TargetTicks = 5;
			double const  DownloadMax = CurrentMaxDownloadRate * 1.1;
			double const  UploadMax = CurrentMaxUploadRate * 1.1;

			// Calculate tick intervals in the original units (bytes)
			double const DownloadInterval = CalculateNiceTickInterval(DownloadMax, TargetTicks);
			double const UploadInterval = CalculateNiceTickInterval(UploadMax, TargetTicks);

			// Round up the max values to be multiples of the intervals
			double const DownloadLimit = std::ceil(DownloadMax / DownloadInterval) * DownloadInterval;
			double const UploadLimit = std::ceil(UploadMax / UploadInterval) * UploadInterval;

			// Y1 (left) for Download, Y2 (right) for Upload
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, DownloadLimit, ImGuiCond_Always);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, std::numeric_limits<double>::infinity());
			ImPlot::SetupAxisTicks(ImAxis_Y1, 0.0, DownloadLimit, TargetTicks + 1);

			ImPlot::SetupAxis(ImAxis_Y2, UploadFmt.Label, ImPlotAxisFlags_Opposite | ImPlotAxisFlags_NoGridLines);
			ImPlot::SetupAxisFormat(ImAxis_Y2, FormatBandwidth, &UploadFmt);
			ImPlot::SetupAxisLimits(ImAxis_Y2, 0.0, UploadLimit, ImGuiCond_Always);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y2, 0.0, std::numeric_limits<double>::infinity());
			ImPlot::SetupAxisTicks(ImAxis_Y2, 0.0, UploadLimit, TargetTicks + 1);

			// Transform data: convert time values to "time ago" (Time - x)
			// We need to plot with inverted X values
			// Also filter out points outside the visible range to prevent wrapping artifacts

			// Create temporary buffers with inverted time
			static ImVector<ImVec2> UploadInverted;
			static ImVector<ImVec2> DownloadInverted;
			UploadInverted.clear();
			DownloadInverted.clear();

			double const T0 = Time - History - 2.0; // Minimum visible time (with 1s buffer)

			for (auto const& P : UploadBuffer.Data)
			{
				float const OriginalTime = P.x;
				// Only include points within the visible time range
				if (OriginalTime >= static_cast<float>(T0))
				{
					float const TimeAgo = static_cast<float>(Time) - OriginalTime;
					UploadInverted.push_back(ImVec2(TimeAgo, P.y));
				}
			}

			for (auto const& P : DownloadBuffer.Data)
			{
				float const OriginalTime = P.x;
				// Only include points within the visible time range
				if (OriginalTime >= static_cast<float>(T0))
				{
					float const TimeAgo = static_cast<float>(Time) - OriginalTime;
					DownloadInverted.push_back(ImVec2(TimeAgo, P.y));
				}
			}

			// Plot Upload on Y2 (right axis)
			ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
			ImPlot::SetNextLineStyle(ImVec4(0.9f, 0.3f, 0.05f, 1.0f), WSettings::GetInstance().NetworkGraphLineWidth);
			ImPlot::PlotStairs(TR("upload"), &UploadInverted[0].x, &UploadInverted[0].y, UploadInverted.size(), 0, 0,
				2 * sizeof(float));

			// Plot Download on Y1 (left axis)
			ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
			ImPlot::SetNextLineStyle(ImVec4(0.2f, 0.4f, 0.8f, 1.0f), WSettings::GetInstance().NetworkGraphLineWidth);
			ImPlot::PlotStairs(TR("download"), &DownloadInverted[0].x, &DownloadInverted[0].y, DownloadInverted.size(),
				0, 0, 2 * sizeof(float));
			ImPlot::EndPlot();
		}
	}
	ImGui::End();
}