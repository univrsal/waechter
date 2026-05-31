/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "StatWindow.hpp"

#include "Client.hpp"
#include "imgui.h"

#include "Format.hpp"

WStatWindow::WStatWindow(WStatsRequest const& InRequest) : Request(InRequest)
{
	Title = Request.Target + " (" + WTimeFormat::FormatUnixTime(Request.StartTime) + " - "
		+ WTimeFormat::FormatUnixTime(Request.EndTime) + ")";
	WClient::GetInstance().SendMessage(MT_StatsRequest, Request);
}

void WStatWindow::Draw()
{
	if (ImGui::Begin(Title.c_str(), &bOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
	{
		std::scoped_lock Lock(DataMutex);
		if (Response.DataPoints.empty())
		{
			ImGui::Text("Loading...");
		}
		else
		{
			// bar chart for the traffic data points
		}
	}
	ImGui::End();
}