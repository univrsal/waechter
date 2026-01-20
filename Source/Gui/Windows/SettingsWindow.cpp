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
	ImGuiIO& Io = ImGui::GetIO();
	auto     DisplaySize = Io.DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(DisplaySize.x * 0.5f, DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize({ 500, 235 });
	if (ImGui::Begin("Settings", &bVisible,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		ImGui::InputText("Socket path", &WSettings::GetInstance().SocketPath);
		ImGui::InputText(
			"WebSocket auth token", &WSettings::GetInstance().WebSocketAuthToken, ImGuiInputTextFlags_Password);

		if (ImGui::Checkbox("Use dark theme", &WSettings::GetInstance().bUseDarkTheme))
		{
			if (WSettings::GetInstance().bUseDarkTheme)
			{
				ImGui::StyleColorsDark();
			}
			else
			{
				ImGui::StyleColorsLight();
			}
		}

		if (ImGui::Button("Connect"))
		{
			WClient::GetInstance().Start();
		}
	}
	ImGui::End();
}