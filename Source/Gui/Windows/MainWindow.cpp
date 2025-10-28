//
// Created by usr on 26/10/2025.
//

#include "MainWindow.hpp"

#include "imgui_internal.h"

#include <imgui.h>

void WMainWindow::DrawConnectionIndicator()
{
	const float IndicatorSize = 12.0f;
	const float Radius = IndicatorSize * 0.5f;
	const float MarginRight = -2.0f;

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
	ImU32       Col = Client.IsConnected() ? IM_COL32(0, 200, 0, 255) : IM_COL32(200, 0, 0, 255);
	DL->AddCircleFilled(Center, Radius, Col);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Client.IsConnected() ? "Connected to daemon" : "Not connected to daemon");
	}
}

void WMainWindow::Draw()
{
	ImGuiIO& io = ImGui::GetIO();
	ImVec2   display_size = io.DisplaySize; // Current window/swapchain size

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(display_size);

	// Optional: remove window decorations and background if you want a full overlay
	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoDecoration;

	if (ImGui::Begin("MainWindow", nullptr, window_flags))
	{

		if (ImGui::BeginMenuBar())
		{
			// First draw menu items normally (left side)
			if (ImGui::BeginMenu("Help"))
			{
				if (ImGui::MenuItem("About"))
				{
					AboutDialog.Show();
				}
				ImGui::EndMenu();
			}
			DrawConnectionIndicator();
			ImGui::EndMenuBar();
		}
		ImGui::Text("Hello, fullscreen window!");

		ImGui::End();
	}

	AboutDialog.Draw();
}