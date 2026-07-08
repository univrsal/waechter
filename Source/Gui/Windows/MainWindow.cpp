/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "MainWindow.hpp"

#include <string>

#include "imgui_internal.h"
#include "imgui.h"

#include "Client.hpp"
#include "Random.hpp"
#include "Util/I18n.hpp"
#include "Windows/SdlWindow.hpp"
#include "../../Daemon/Data/IP2Asn.hpp"
#include "../../Daemon/Data/LibCurl.hpp"
#include "Util/ProtocolDB.hpp"
#include "Util/Settings.hpp"

void WMainWindow::DrawConnectionIndicator()
{
	float const IndicatorSize = WSdlWindow::ScaleValue(12.0f);
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
			Tooltip = std::format("{}\n"
								  "▼ {}\n"
								  "▲ {}",
				TR("connected"), WTrafficFormat::AutoFormat(WClient::GetInstance().DaemonToClientTrafficRate.load()),
				WTrafficFormat::AutoFormat(WClient::GetInstance().ClientToDaemonTrafficRate.load()));
		}
		else
		{
			Tooltip = TR("not_connected");
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
		ImGui::DockBuilderDockWindow(TR("traffic_tree.title"), DockIdMain);
		ImGui::DockBuilderDockWindow(TR("connection_history.title"), DockIdMain);
		ImGui::DockBuilderDockWindow(TR("log.title"), DockIdDown);
		ImGui::DockBuilderDockWindow(TR("network_activity.title"), DockIdDown);
		ImGui::DockBuilderDockWindow(TR("details.title"), DockIdRight);
		ImGui::DockBuilderFinish(Main);
	}
	FlagAtlas.Load();
	WProtocolDB::GetInstance().Init();
	RegisterDialog.Init();
	bInit = true;
}

WMainWindow& WMainWindow::Get()
{
	return *WSdlWindow::GetInstance().GetMainWindow();
}

std::shared_ptr<WTrafficTree> const& WMainWindow::GetTrafficTree()
{
	return WSdlWindow::GetInstance().GetMainWindow()->GetDetailsWindow().GetTrafficTree();
}

void WMainWindow::Draw()
{
	ImGuiIO& Io = ImGui::GetIO();
	ImVec2   DisplaySize = Io.DisplaySize;

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(DisplaySize);

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu(TR("menu.file")))
		{
			if (ImGui::MenuItem(TR("menu.exit"), ""))
			{
				WSdlWindow::GetInstance().RequestClose();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(TR("menu.view")))
		{
			if (ImGui::MenuItem(
					TR("menu.show_uninitialized_sockets"), "", &WSettings::GetInstance().bShowUninitalizedSockets))
			{
				WSettings::GetInstance().Save();
			}
			if (ImGui::MenuItem(TR("menu.show_offline_processes"), "", &WSettings::GetInstance().bShowOfflineProcesses))
			{
				WSettings::GetInstance().Save();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(TR("menu.tools")))
		{
			bool bEnabled = !WIP2Asn::GetInstance().IsUpdateInProgress();

			if (ImGui::MenuItem(TR("menu.update_ip2asn"), nullptr, false, bEnabled))
			{
				// todo: send request to server
				// WIP2Asn::GetInstance().UpdateDatabase();
				WIP2AsnUpdateRequest const Request{};
				WClient::GetInstance().SendMessage(MT_UpdateIP2AsnDb, Request);
			}
			if (ImGui::MenuItem(TR("settings.title"), nullptr, false))
			{
				SettingsWindow.Show();
			}
			if (ImGui::MenuItem(TR("memory_usage.title"), nullptr, false))
			{
				MemoryUsageWindow.Show();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(TR("menu.help")))
		{
			if (ImGui::MenuItem(TR("register.title")))
			{
				RegisterDialog.Show();
			}
			if (ImGui::MenuItem(TR("about.title")))
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
	ConnectionHistoryWindow.Draw();
	SettingsWindow.Draw();
	MemoryUsageWindow.Draw();

	for (auto const& StatWindow : StatWindows)
	{
		StatWindow->Draw();
	}
	std::erase_if(StatWindows, [](auto const& StatWindow) { return !StatWindow->IsOpen(); });

	WClient::GetInstance().GetTrafficTree()->Draw(Main);
	// draw last for the watermark
	RegisterDialog.Draw();
}

void WMainWindow::OpenStatsWindow(WStatsRequest Request, WConnectionHistoryRequest HistoryRequest)
{
	Request.RequestId = WRandom::RandomInteger();
	HistoryRequest.RequestId = WRandom::RandomInteger();
	StatWindows.emplace_back(std::make_unique<WStatWindow>(Request, HistoryRequest));
}

void WMainWindow::HandleStatsResponse(WBuffer const& Buf) const
{
	WStatsResponse Response{};

	if (!DeserializeMessage(Buf, Response))
	{
		spdlog::error("Failed to deserialize stats response");
		return;
	}

	for (auto const& StatWindow : StatWindows)
	{
		if (StatWindow->GetRequestId() == Response.RequestId)
		{
			StatWindow->SetResponse(Response);
			return;
		}
	}
	spdlog::warn("Received stats response with unknown request ID: {}", Response.RequestId);
}

void WMainWindow::HandleHistoryResponse(WBuffer const& Buf) const
{
	WConnectionHistoryResponse Response{};

	if (!DeserializeMessage(Buf, Response))
	{
		spdlog::error("Failed to deserialize stats response");
		return;
	}
	for (auto const& StatWindow : StatWindows)
	{
		if (StatWindow->GetHistoryRequestId() == Response.RequestId)
		{
			StatWindow->SetHistoryResponse(Response);
			return;
		}
	}
	spdlog::warn("Received history response with unknown request ID: {}", Response.RequestId);
}