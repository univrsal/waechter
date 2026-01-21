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
	ImGui::SetNextWindowSize({ 500, 310 });
	if (ImGui::Begin("Settings", &bVisible,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		// Socket Settings
		ImGui::SeparatorText("Socket");

		// Socket path combo with history
		if (ImGui::BeginCombo("History", WSettings::GetInstance().SocketPath.c_str()))
		{
			// Show current path if it's not in history
			bool bFoundInHistory = false;
			for (auto const& HistoryPath : WSettings::GetInstance().SocketPathHistory)
			{
				if (HistoryPath == WSettings::GetInstance().SocketPath)
				{
					bFoundInHistory = true;
					break;
				}
			}

			if (!bFoundInHistory && !WSettings::GetInstance().SocketPath.empty())
			{
				bool bSelected = true;
				if (ImGui::Selectable(WSettings::GetInstance().SocketPath.c_str(), bSelected))
				{
					// Already the current path
				}
			}

			// Show history
			for (auto const& HistoryPath : WSettings::GetInstance().SocketPathHistory)
			{
				bool bSelected = (HistoryPath == WSettings::GetInstance().SocketPath);
				if (ImGui::Selectable(HistoryPath.c_str(), bSelected))
				{
					WSettings::GetInstance().SocketPath = HistoryPath;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}
		// Allow editing the path directly
		ImGui::InputText("Socket path", &WSettings::GetInstance().SocketPath);

		ImGui::InputText(
			"WebSocket auth token", &WSettings::GetInstance().WebSocketAuthToken, ImGuiInputTextFlags_Password);

		ImGui::Checkbox("Allow self-signed certificates", &WSettings::GetInstance().bAllowSelfSignedCertificates);
		ImGui::Checkbox("Skip certificate hostname check", &WSettings::GetInstance().bSkipCertificateHostnameCheck);
		if (ImGui::Button("Connect"))
		{
			// Add to history before connecting
			if (!WSettings::GetInstance().SocketPath.empty())
			{
				WSettings::GetInstance().AddToSocketPathHistory(WSettings::GetInstance().SocketPath);
			}
			WClient::GetInstance().Start();
		}

		ImGui::Spacing();

		// UI Settings
		ImGui::SeparatorText("UI");
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
	}
	ImGui::End();
}