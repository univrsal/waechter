/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SettingsWindow.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "Client.hpp"
#include "GlfwWindow.hpp"
#include "Util/I18n.hpp"
#include "Util/Settings.hpp"

void WSettingsWindow::Draw()
{
	if (!bVisible)
	{
		return;
	}
	ImGuiIO& Io = ImGui::GetIO();
	auto&    S = WSettings::GetInstance();
	auto     DisplaySize = Io.DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(DisplaySize.x * 0.5f, DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize({ 550, 510 });
	if (ImGui::Begin(TR("window.settings"), &bVisible,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		ImGui::SeparatorText(TR("socket.settings"));

		if (ImGui::BeginCombo(TR("socket.history"), S.SocketPath.c_str()))
		{
			// Show current path if it's not in history
			bool bFoundInHistory = false;
			for (auto const& HistoryPath : S.SocketPathHistory)
			{
				if (HistoryPath == S.SocketPath)
				{
					bFoundInHistory = true;
					break;
				}
			}

			if (!bFoundInHistory && !S.SocketPath.empty())
			{
				ImGui::Selectable(S.SocketPath.c_str(), true);
			}

			// Show history
			for (auto const& HistoryPath : S.SocketPathHistory)
			{
				bool bSelected = (HistoryPath == S.SocketPath);
				if (ImGui::Selectable(HistoryPath.c_str(), bSelected))
				{
					S.SocketPath = HistoryPath;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}
		ImGui::InputText(TR("socket.path"), &S.SocketPath);

		ImGui::InputText(TR("ws.auth_token"), &S.WebSocketAuthToken, ImGuiInputTextFlags_Password);

		ImGui::Checkbox(TR("ws.allow_self_sign"), &S.bAllowSelfSignedCertificates);
		ImGui::Checkbox(TR("ws.skip_cert_check"), &S.bSkipCertificateHostnameCheck);
		if (ImGui::Button(TR("button.connect")))
		{
			if (!S.SocketPath.empty())
			{
				S.AddToSocketPathHistory(S.SocketPath);
			}
			WClient::GetInstance().Start();
		}

		ImGui::Spacing();

		ImGui::SeparatorText(TR("ui"));
		if (ImGui::Checkbox(TR("ui.use_dark_theme"), &S.bUseDarkTheme))
		{
			if (S.bUseDarkTheme)
			{
				ImGui::StyleColorsDark();
			}
			else
			{
				ImGui::StyleColorsLight();
			}
		}

		ImGui::SameLine();
		ImGui::Checkbox(TR("ui.reduce_fps_inactive"), &S.bReduceFrameRateWhenInactive);

		if (ImGui::Checkbox(TR("ui.tray_icon"), &S.bUseTrayIcon))
		{
			if (S.bUseTrayIcon)
			{
				WGlfwWindow::GetInstance().GetTrayIcon().Init();
			}
			else
			{
				WGlfwWindow::GetInstance().GetTrayIcon().DeInit();
			}
		}
		if (S.bUseTrayIcon)
		{
			ImGui::SameLine();
			ImGui::Checkbox(TR("ui.minimize_to_tray"), &S.bMinimizeToTray);
			ImGui::Checkbox(TR("ui.close_to_tray"), &S.bCloseToTray);
			ImGui::SameLine();
			if (ImGui::Checkbox(TR("ui.use_custom_tray_color"), &S.bUseCustomTrayIconColor))
			{
				WTrayIcon::SetColor(S.TrayIconBackgroundColor.Color);
			}

			if (S.bUseCustomTrayIconColor)
			{
				ImGui::PushItemWidth(180.0f);
				if (ImGui::ColorPicker3(TR("ui.tray_icon_background_color"),
						reinterpret_cast<float*>(&S.TrayIconBackgroundColor.Color),
						ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoOptions))
				{
					WTrayIcon::SetColor(S.TrayIconBackgroundColor.Color);
				}
			}
			else
			{
				if (S.bUseDarkTheme)
				{
					WTrayIcon::UseDarkColor();
				}
				else
				{
					WTrayIcon::UseLightColor();
				}
			}
		}

		// drop down for language selection
		static std::vector<std::pair<std::string, std::string>> const Languages = { { "en_US", "English (US)" },
			{ "de_DE", "Deutsch (DE)" } };
		if (ImGui::BeginCombo(TR("language"), S.SelectedLanguage.c_str()))
		{
			for (auto const& [Code, Name] : Languages)
			{
				bool const bSelected = Code == S.SelectedLanguage;
				if (ImGui::Selectable(Name.c_str(), bSelected))
				{
					S.SelectedLanguage = Code;
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