/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SettingsWindow.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "Client.hpp"
#include "Util/I18n.hpp"
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
	if (ImGui::Begin(TR("window.settings"), &bVisible,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		ImGui::SeparatorText(TR("socket.settings"));

		if (ImGui::BeginCombo(TR("socket.history"), WSettings::GetInstance().SocketPath.c_str()))
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
				ImGui::Selectable(WSettings::GetInstance().SocketPath.c_str(), true);
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
		ImGui::InputText(TR("socket.path"), &WSettings::GetInstance().SocketPath);

		ImGui::InputText(
			TR("ws.auth_token"), &WSettings::GetInstance().WebSocketAuthToken, ImGuiInputTextFlags_Password);

		ImGui::Checkbox(TR("ws.allow_self_sign"), &WSettings::GetInstance().bAllowSelfSignedCertificates);
		ImGui::Checkbox(TR("ws.skip_cert_check"), &WSettings::GetInstance().bSkipCertificateHostnameCheck);
		if (ImGui::Button(TR("button.connect")))
		{
			if (!WSettings::GetInstance().SocketPath.empty())
			{
				WSettings::GetInstance().AddToSocketPathHistory(WSettings::GetInstance().SocketPath);
			}
			WClient::GetInstance().Start();
		}

		ImGui::Spacing();

		ImGui::SeparatorText(TR("ui"));
		if (ImGui::Checkbox(TR("ui.use_dark_theme"), &WSettings::GetInstance().bUseDarkTheme))
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

		if (ImGui::Checkbox(TR("ui.reduce_fps_inactive"), &WSettings::GetInstance().bReduceFrameRateWhenInactive))
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

		// drop down for language selection
		static std::vector<std::pair<std::string, std::string>> const Languages = { { "en_US", "English (US)" },
			{ "de_DE", "Deutsch (DE)" } };
		if (ImGui::BeginCombo(TR("language"), WSettings::GetInstance().SelectedLanguage.c_str()))
		{
			for (auto const& [Code, Name] : Languages)
			{
				bool const bSelected = Code == WSettings::GetInstance().SelectedLanguage;
				if (ImGui::Selectable(Name.c_str(), bSelected))
				{
					WSettings::GetInstance().SelectedLanguage = Code;
					WI18n::GetInstance().LoadLanguage(Code);
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}
	ImGui::End();
}