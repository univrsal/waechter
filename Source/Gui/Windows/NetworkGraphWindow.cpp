//
// Created by usr on 01/11/2025.
//

#include "NetworkGraphWindow.hpp"

#include <imgui.h>
#include <implot.h>
#include <limits>

void WNetworkGraphWindow::Draw()
{
	// Draw empty Network activity window so it appears as a tab next to Log
	Time += static_cast<double>(ImGui::GetIO().DeltaTime);
	if (ImGui::Begin("Network activity", nullptr, ImGuiWindowFlags_None))
	{
		static ImPlotAxisFlags flags = 0;
		if (ImPlot::BeginPlot("##Scrolling", ImVec2(-1, 150)))
		{
			std::lock_guard Lock(Mutex);
			ImPlot::SetupAxes(nullptr, nullptr, flags, flags);
			ImPlot::SetupAxisLimits(ImAxis_X1, Time - History, Time, ImGuiCond_Always);
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
			// Prevent vertical panning/zooming below 0
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, std::numeric_limits<double>::infinity());

			ImPlot::PlotLine("Upload", &UploadBuffer.Data[0].x, &UploadBuffer.Data[0].y, UploadBuffer.Data.size(), 0, UploadBuffer.Offset, 2 * sizeof(float));
			ImPlot::PlotLine("Download", &DownloadBuffer.Data[0].x, &DownloadBuffer.Data[0].y, DownloadBuffer.Data.size(), 0, DownloadBuffer.Offset, 2 * sizeof(float));
			ImPlot::EndPlot();
		}
	}
	ImGui::End();
}