/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "NetworkGraphWindow.hpp"

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

void WNetworkGraphWindow::Draw()
{
	// Draw empty Network activity window so it appears as a tab next to Log
	Time += static_cast<double>(ImGui::GetIO().DeltaTime);
	if (ImGui::Begin("Network activity", nullptr, ImGuiWindowFlags_None))
	{
		if (ImPlot::BeginPlot("##Scrolling", ImVec2(-1, -1)))
		{
			std::lock_guard Lock(Mutex);

			// Setup axes: show chosen unit on Y axis
			ImPlot::SetupAxes(nullptr, TrafficFmt.Label, 0, 0);
			ImPlot::SetupAxisFormat(ImAxis_Y1, FormatBandwidth, &TrafficFmt);

			auto const PointCount = std::min(static_cast<int>(History) + 5, UploadBuffer.Data.size());
			auto       DataStart = std::max(0, UploadBuffer.Data.size() - static_cast<int>(History) - 5);

			ImPlot::SetupAxisLimits(ImAxis_X1, Time - History, Time - 1.1, ImGuiCond_Always);
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, CurrentMaxRateInGraph * 1.1, ImGuiCond_Always);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, std::numeric_limits<double>::infinity());
			ImPlot::SetNextLineStyle(ImVec4(0.9f, 0.3f, 0.05f, 1.0f), 2.f);
			ImPlot::PlotStairs("Upload", &UploadBuffer.Data[DataStart].x, &UploadBuffer.Data[DataStart].y, PointCount,
				0, UploadBuffer.Offset, 2 * sizeof(float));
			ImPlot::SetNextLineStyle(ImVec4(0.2f, 0.4f, 0.8f, 1.0f), 2.f);
			ImPlot::PlotStairs("Download", &DownloadBuffer.Data[DataStart].x, &DownloadBuffer.Data[DataStart].y,
				PointCount, 0, DownloadBuffer.Offset, 2 * sizeof(float));
			ImPlot::EndPlot();
		}
	}
	ImGui::End();
}