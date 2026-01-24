/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "MainWindow.hpp"

#include <string>

#include "imgui_internal.h"
#include "imgui.h"

#include "Client.hpp"
#include "Util/I18n.hpp"
#include "Windows/GlfwWindow.hpp"
#include "Util/IP2Asn.hpp"
#include "Util/LibCurl.hpp"
#include "Util/ProtocolDB.hpp"
#include "Util/Settings.hpp"

void WMainWindow::DrawConnectionIndicator()
{
	float const IndicatorSize = 12.0f;
	float const Radius = IndicatorSize * 0.5f;
	float const MarginRight = -2.0f;

	ImGui::SameLine(0.0f, 0.0f);

	// Compute target X from current position + remaining available width
	float CurX = ImGui::GetCursorPosX();
	float Avail = ImGui::GetContentRegionAvail().x;
	float X = CurX + Avail - IndicatorSize - MarginRight;
	if (X > CurX)
	{
		ImGui::SetCursorPosX(X);
	}

	// Use a full-frame-height invisible button and draw the dot centered inside it
	ImVec2 BtnSize(IndicatorSize, ImGui::GetFrameHeight());
	ImGui::InvisibleButton("conn_indicator", BtnSize);
	ImVec2 Min = ImGui::GetItemRectMin();
	ImVec2 Max = ImGui::GetItemRectMax();
	ImVec2 Center = ImVec2((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);

	ImDrawList* DL = ImGui::GetWindowDrawList();
	ImU32       Col = WClient::GetInstance().IsConnected() ? IM_COL32(0, 200, 0, 255) : IM_COL32(200, 0, 0, 255);
	DL->AddCircleFilled(Center, Radius, Col);

	if (ImGui::IsItemHovered())
	{
		std::string Tooltip;
		if (WClient::GetInstance().IsConnected())
		{
			Tooltip = fmt::format("{}\n"
								  "▼ {}\n"
								  "▲ {}",
				TR("__connected"), WTrafficFormat::AutoFormat(WClient::GetInstance().DaemonToClientTrafficRate.load()),
				WTrafficFormat::AutoFormat(WClient::GetInstance().ClientToDaemonTrafficRate.load()));
		}
		else
		{
			Tooltip = TR("__not_connected");
		}
		ImGui::SetTooltip("%s", Tooltip.c_str());
	}
}

void WMainWindow::Init(ImGuiID Main)
{
	LogWindow.AttachToSpdlog();
	// Build default dock layout only if the dockspace has no restored layout:
	// - node is null (no dockspace yet)
	// - or node is a leaf AND has no windows (no splits/restored docks)
	ImGuiDockNode* Node = ImGui::DockBuilderGetNode(Main);
	bool           bNeedDefaultLayout = (Node == nullptr)
		|| ((Node->ChildNodes[0] == nullptr && Node->ChildNodes[1] == nullptr) && Node->Windows.empty());

	if (bNeedDefaultLayout)
	{
		if (Node == nullptr)
		{
			ImGui::DockBuilderRemoveNode(Main);
			ImGui::DockBuilderAddNode(Main, ImGuiDockNodeFlags_DockSpace);
		}

		ImGuiID DockIdMain = Main;
		ImGuiID DockIdDown = ImGui::DockBuilderSplitNode(DockIdMain, ImGuiDir_Down, 0.25f, nullptr, &DockIdMain);
		ImGuiID DockIdRight = ImGui::DockBuilderSplitNode(DockIdMain, ImGuiDir_Right, 0.20f, nullptr, &DockIdMain);
		ImGui::DockBuilderDockWindow(TR("window.traffic_tree"), DockIdMain);
		ImGui::DockBuilderDockWindow("Connection History", DockIdMain);
		ImGui::DockBuilderDockWindow("Log", DockIdDown);
		ImGui::DockBuilderDockWindow("Network activity", DockIdDown);
		ImGui::DockBuilderDockWindow("Details", DockIdRight);
		ImGui::DockBuilderFinish(Main);
	}
	FlagAtlas.Load();
	WProtocolDB::GetInstance().Init();
	WIP2Asn::GetInstance().Init();

	spdlog::info("libcurl version: {}", WLibCurl::GetLoadedVersion());
	RegisterDialog.Init();
	bInit = true;
}

WMainWindow& WMainWindow::Get()
{
	return *WGlfwWindow::GetInstance().GetMainWindow();
}

void WMainWindow::Draw()
{
	ImGuiIO& Io = ImGui::GetIO();
	ImVec2   DisplaySize = Io.DisplaySize;

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(DisplaySize);

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu(TR("menu.view")))
		{
			if (ImGui::MenuItem("Show Uninitialized Sockets", "", &WSettings::GetInstance().bShowUninitalizedSockets))
			{
				WSettings::GetInstance().Save();
			}
			if (ImGui::MenuItem("Show Offline Processes", "", &WSettings::GetInstance().bShowOfflineProcesses))
			{
				WSettings::GetInstance().Save();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(TR("menu.tools")))
		{
			bool bEnabled = !WIP2Asn::GetInstance().IsUpdateInProgress();
			if (ImGui::MenuItem("Update IP2Asn DB", nullptr, false, bEnabled))
			{
				WIP2Asn::GetInstance().UpdateDatabase();
			}
			if (ImGui::MenuItem("Settings", nullptr, false))
			{
				SettingsWindow.Show();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("Register..."))
			{
				RegisterDialog.Show();
			}
			if (ImGui::MenuItem("About"))
			{
				AboutDialog.Show();
			}
			ImGui::EndMenu();
		}
		DrawConnectionIndicator();
		ImGui::EndMainMenuBar();
	}

	ImGuiID Main = ImGui::DockSpaceOverViewport();

	if (!bInit)
	{
		Init(Main);
	}

	LogWindow.Draw();
	NetworkGraphWindow.Draw();
	DetailsWindow.Draw();
	AboutDialog.Draw();
	RegisterDialog.Draw();
	ConnectionHistoryWindow.Draw();
	SettingsWindow.Draw();
	WIP2Asn::GetInstance().DrawDownloadProgressWindow();
	WClient::GetInstance().GetTrafficTree()->Draw(Main);
}