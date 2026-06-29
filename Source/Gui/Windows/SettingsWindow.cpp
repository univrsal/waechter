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
#include "Util/TrayIcon.hpp"
#include "Windows/SdlWindow.hpp"

void WSettingsWindow::Draw()
{
	if (!bVisible)
	{
		return;
	}
	ImGuiIO& Io = ImGui::GetIO();
	auto     DisplaySize = Io.DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(DisplaySize.x * 0.5f, DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(WSdlWindow::ScaleSize({ 580, 400 }));
	if (ImGui::Begin(TR("settings.title"), &bVisible,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		ImGui::SeparatorText(TR("settings.socket.header"));

		if (ImGui::BeginCombo(TR("settings.socket.history"), WSettings::GetInstance().SocketPath.c_str()))
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
		ImGui::InputText(TR("settings.socket.path"), &WSettings::GetInstance().SocketPath);

		ImGui::InputText(TR("settings.socket.ws_auth_token"), &WSettings::GetInstance().WebSocketAuthToken,
			ImGuiInputTextFlags_Password);
#ifndef __EMSCRIPTEN__
		ImGui::Checkbox(
			TR("settings.socket.ws_allow_self_sign"), &WSettings::GetInstance().bAllowSelfSignedCertificates);
		ImGui::Checkbox(
			TR("settings.socket.ws_skip_cert_check"), &WSettings::GetInstance().bSkipCertificateHostnameCheck);
#endif
		if (ImGui::Button(TR("settings.socket.connect")))
		{
			if (!WSettings::GetInstance().SocketPath.empty())
			{
				WSettings::GetInstance().AddToSocketPathHistory(WSettings::GetInstance().SocketPath);
			}
			WClient::GetInstance().Start();
		}

		ImGui::Spacing();

		ImGui::SeparatorText(TR("settings.ui.header"));
		if (ImGui::Checkbox(TR("settings.ui.use_dark_theme"), &WSettings::GetInstance().bUseDarkTheme))
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

		if (ImGui::Checkbox(
				TR("settings.ui.reduce_fps_inactive"), &WSettings::GetInstance().bReduceFrameRateWhenInactive))
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

#ifndef __EMSCRIPTEN__
		if (ImGui::Checkbox(TR("settings.ui.enable_tray_icon"), &WSettings::GetInstance().bEnableTrayIcon))
		{
			WSettings::GetInstance().Save();
			if (WSettings::GetInstance().bEnableTrayIcon)
			{
				WTrayIcon::GetInstance().Init();
			}
			else
			{
				WTrayIcon::GetInstance().Destroy();
			}
		}

		if (WSettings::GetInstance().bEnableTrayIcon)
		{
			ImGui::Indent();
			if (ImGui::Checkbox(
					TR("settings.ui.minimize_to_tray_on_close"), &WSettings::GetInstance().bMinimizeToTrayOnClose))
			{
				WSettings::GetInstance().Save();
			}
			ImGui::Unindent();
		}
#endif

		// drop down for language selection
		static std::vector<std::pair<std::string, std::string>> const Languages = { { "en_US", "English (US)" },
			{ "de_DE", "Deutsch (DE)" } };
		if (ImGui::BeginCombo(TR("settings.ui.language"), WSettings::GetInstance().SelectedLanguage.c_str()))
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

#if __EMSCRIPTEN__
		if (ImGui::Button(TR("generic.save")))
		{
			WSettings::GetInstance().Save();
		}
#endif
	}
	ImGui::End();
}