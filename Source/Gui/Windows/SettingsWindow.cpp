/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SettingsWindow.hpp"

#include "Client.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include "Util/Settings.hpp"

void WSettingsWindow::Draw()
{
	if (!bVisible)
	{
		return;
	}
	ImGuiIO& io = ImGui::GetIO();
	auto     display_size = io.DisplaySize; // Current window/swapchain size
	ImGui::SetNextWindowPos(ImVec2(display_size.x * 0.5f, display_size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize({ 500, 235 });
	if (ImGui::Begin("Settings", &bVisible,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		ImGui::InputText("Socket path", &WSettings::GetInstance().SocketPath);
		ImGui::InputText(
			"WebSocket auth token", &WSettings::GetInstance().WebSocketAuthToken, ImGuiInputTextFlags_Password);

		if (ImGui::Button("Connect"))
		{
			WClient::GetInstance().Start();
		}
	}
	ImGui::End();
}