/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "NetworkGraphWindow.hpp"

#include "tracy/Tracy.hpp"

#include <cmath>
#include <imgui.h>
#include <implot.h>
#include <cstdio>
#include <limits>

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
		return std::snprintf(Buf, static_cast<size_t>(Size), "%.0f min", Minutes);

	double const Hours = Minutes / 60.0;
	return std::snprintf(Buf, static_cast<size_t>(Size), "%.1f h", Hours);
}

void WNetworkGraphWindow::Draw()
{
	ZoneScopedN("WNetworkGraphWindow::Draw");
	Time += static_cast<double>(ImGui::GetIO().DeltaTime);
	if (ImGui::Begin("Network activity", nullptr, ImGuiWindowFlags_None))
	{
		// History duration dropdown
		static char const*  HistoryOptions[] = { "1 min", "5 min", "15 min" };
		static double const HistoryValues[] = { 60.0, 300.0, 900.0 };
		ImGui::SetNextItemWidth(100);
		if (ImGui::Combo("Duration", &HistoryIndex, HistoryOptions, IM_ARRAYSIZE(HistoryOptions)))
		{
			History = HistoryValues[HistoryIndex];
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100);
		ImGui::SliderFloat("Line width", &LineWidth, 0.5f, 5.0f, "%.1f");

		if (ImPlot::BeginPlot("##Scrolling", ImVec2(-1, -1)))
		{
			std::lock_guard Lock(Mutex);

			// Setup axes: show chosen unit on Y axis, time ago on X axis
			ImPlot::SetupAxes("Time", DownloadFmt.Label, 0, 0);
			ImPlot::SetupAxisFormat(ImAxis_Y1, FormatBandwidth, &DownloadFmt);
			ImPlot::SetupAxisFormat(ImAxis_X1, FormatTimeAgo, nullptr);

			// X axis: offset by 1 second to avoid gap (show 1s ago to History+1s ago)
			// This ensures we only show the range of data we actually have
			ImPlot::SetupAxisLimits(ImAxis_X1, 1.0, History + 1.0, ImGuiCond_Always);
			// Y1 (left) for Download, Y2 (right) for Upload
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, CurrentMaxDownloadRate * 1.1, ImGuiCond_Always);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, std::numeric_limits<double>::infinity());

			ImPlot::SetupAxis(ImAxis_Y2, UploadFmt.Label, ImPlotAxisFlags_Opposite);
			ImPlot::SetupAxisFormat(ImAxis_Y2, FormatBandwidth, &UploadFmt);
			ImPlot::SetupAxisLimits(ImAxis_Y2, 0.0, CurrentMaxUploadRate * 1.1, ImGuiCond_Always);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y2, 0.0, std::numeric_limits<double>::infinity());

			// Transform data: convert time values to "time ago" (Time - x)
			// We need to plot with inverted X values

			// Create temporary buffers with inverted time
			static ImVector<ImVec2> UploadInverted;
			static ImVector<ImVec2> DownloadInverted;
			UploadInverted.resize(UploadBuffer.Data.size());
			DownloadInverted.resize(DownloadBuffer.Data.size());
			for (int i = 0; i < UploadBuffer.Data.size(); ++i)
			{
				UploadInverted[i].x = static_cast<float>(Time) - UploadBuffer.Data[i].x; // Convert to "seconds ago"
				UploadInverted[i].y = UploadBuffer.Data[i].y;
			}

			for (int i = 0; i < DownloadBuffer.Data.size(); ++i)
			{
				DownloadInverted[i].x = static_cast<float>(Time) - DownloadBuffer.Data[i].x; // Convert to "seconds ago"
				DownloadInverted[i].y = DownloadBuffer.Data[i].y;
			}

			// Plot Upload on Y2 (right axis)
			ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
			ImPlot::SetNextLineStyle(ImVec4(0.9f, 0.3f, 0.05f, 1.0f), LineWidth);
			ImPlot::PlotStairs(
				"Upload", &UploadInverted[0].x, &UploadInverted[0].y, UploadInverted.size(), 0, 0, 2 * sizeof(float));

			// Plot Download on Y1 (left axis)
			ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
			ImPlot::SetNextLineStyle(ImVec4(0.2f, 0.4f, 0.8f, 1.0f), LineWidth);
			ImPlot::PlotStairs("Download", &DownloadInverted[0].x, &DownloadInverted[0].y, DownloadInverted.size(), 0,
				0, 2 * sizeof(float));
			ImPlot::EndPlot();
		}
	}
	ImGui::End();
}