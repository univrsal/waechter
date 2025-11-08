//
// Created by usr on 01/11/2025.
//

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
			ImPlot::SetupAxes(nullptr, TrafficFmt.Label, 0, ImPlotAxisFlags_AutoFit);
			ImPlot::SetupAxisFormat(ImAxis_Y1, FormatBandwidth, &TrafficFmt);

			ImPlot::SetupAxisLimits(ImAxis_X1, Time - History, Time, ImGuiCond_Always);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, std::numeric_limits<double>::infinity());
			ImPlot::PlotLine("Upload", &UploadBuffer.Data[0].x, &UploadBuffer.Data[0].y, UploadBuffer.Data.size(), 0,
				UploadBuffer.Offset, 2 * sizeof(float));
			ImPlot::PlotLine("Download", &DownloadBuffer.Data[0].x, &DownloadBuffer.Data[0].y,
				DownloadBuffer.Data.size(), 0, DownloadBuffer.Offset, 2 * sizeof(float));
			ImPlot::EndPlot();
		}
	}
	ImGui::End();
}