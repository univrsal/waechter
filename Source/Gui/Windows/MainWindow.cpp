//
// Created by usr on 26/10/2025.
//

#include "MainWindow.hpp"

#include <imgui_internal.h>
#include <imgui.h>
#include <string>

#include "Client.hpp"
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
			Tooltip = fmt::format("Connected to daemon\n"
								  "Download rate: {}\n"
								  "Upload rate: {}",
				WTrafficFormat::AutoFormat(WClient::GetInstance().DaemonToClientTrafficRate.load()),
				WTrafficFormat::AutoFormat(WClient::GetInstance().ClientToDaemonTrafficRate.load()));
		}
		else
		{
			Tooltip = "Not connected to daemon";
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
		ImGui::DockBuilderDockWindow("Traffic Tree", DockIdMain);
		ImGui::DockBuilderDockWindow("Log", DockIdDown);
		ImGui::DockBuilderDockWindow("Network activity", DockIdDown);
		ImGui::DockBuilderDockWindow("Details", DockIdRight);
		ImGui::DockBuilderFinish(Main);
	}
	FlagAtlas.Load();
	LibCurl.Load();
	WProtocolDB::GetInstance().Init();
	RegisterDialog.Init();
	bInit = true;
}

void WMainWindow::Draw()
{
	ImGuiIO& io = ImGui::GetIO();
	ImVec2   display_size = io.DisplaySize; // Current window/swapchain size

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(display_size);

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Show Uninitialized Sockets", "", &WSettings::GetInstance().bShowUninitalizedSockets))
			{
				WSettings::GetInstance().Save();
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

	// Create windows so they can be saved/restored by ImGui ini
	WClient::GetInstance().GetTrafficTree()->Draw(Main);
	LogWindow.Draw();
	NetworkGraphWindow.Draw();
	DetailsWindow.Draw();
	AboutDialog.Draw();
	RegisterDialog.Draw();
}