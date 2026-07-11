/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SetupWindow.hpp"

#include "imgui.h"

#include "Data/Protocol.hpp"
#include "Util/I18n.hpp"
#include "SdlWindow.hpp"
#include "Util/Settings.hpp"

void WSetupWindow::DrawDaemonSettings()
{

	if (ImGui::BeginCombo(TR("setup.main_interface"), DaemonConfig.MainInterface.c_str()))
	{
		for (auto const& Interface : DaemonConfig.NetworkInterfaces)
		{
			bool const bSelected = Interface == DaemonConfig.MainInterface;
			if (ImGui::Selectable(Interface.c_str(), bSelected))
			{
				DaemonConfig.MainInterface = Interface;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::BeginCombo(TR("setup.vpn_interface"), DaemonConfig.VpnInterface.c_str()))
	{
		for (auto const& Interface : DaemonConfig.NetworkInterfaces)
		{
			bool const bSelected = Interface == DaemonConfig.VpnInterface;
			if (ImGui::Selectable(Interface.c_str(), bSelected))
			{
				DaemonConfig.VpnInterface = Interface;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::InputText(TR("setup.socket_path"), DaemonSocketPath, sizeof(DaemonSocketPath)))
	{
		DaemonConfig.SocketPath = DaemonSocketPath;
	}
	if (ImGui::InputText(TR("setup.daemon_user"), DaemonUser, sizeof(DaemonUser)))
	{
		DaemonConfig.DaemonUser = DaemonUser;
	}
	if (ImGui::InputText(TR("setup.daemon_group"), DaemonGroup, sizeof(DaemonGroup)))
	{
		DaemonConfig.DaemonGroup = DaemonGroup;
	}
	if (ImGui::InputText(TR("setup.socket_mode"), DaemonMode, sizeof(DaemonMode)))
	{
		try
		{
			DaemonConfig.SocketMode = std::stoi(DaemonMode);
		}
		catch (std::exception const&)
		{
		}
	}
	ImGui::TextWrapped("%s", TR("setup.restart_note"));
	if (ImGui::Button(TR("setup.save")))
	{
		WClient::GetInstance().SendMessage(MT_DaemonConfig, DaemonConfig);
		bVisible = false;
	}
}

void WSetupWindow::Draw()
{

	if (!bVisible)
	{
		return;
	}
	ImGuiIO const& Io = ImGui::GetIO();
	auto const     DisplaySize = Io.DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(DisplaySize.x * 0.5f, DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(WSdlWindow::ScaleSize({ 580, 400 }));
	if (ImGui::Begin(TR("setup.title"), &bVisible,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		ImGui::TextWrapped("%s", TR("setup.text"));

		ImGui::TextLinkOpenURL("https://waechter.st/docs", "https://waechter.st/docs");

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

		DrawDaemonSettings();
	}
	ImGui::End();
}

void WSetupWindow::HandleDaemonConfig(WDaemonConfigMessage const& Cfg)
{
	if (!Cfg.bFirstTimeSetupRan)
	{
		bVisible = true;
	}
	DaemonConfig = Cfg;
	std::strncpy(DaemonUser, Cfg.DaemonUser.c_str(), sizeof(DaemonUser) - 1);
	DaemonUser[sizeof(DaemonUser) - 1] = '\0';
	std::strncpy(DaemonGroup, Cfg.DaemonGroup.c_str(), sizeof(DaemonGroup) - 1);
	DaemonGroup[sizeof(DaemonGroup) - 1] = '\0';
	std::snprintf(DaemonMode, sizeof(DaemonMode), "%i", Cfg.SocketMode);
	std::strncpy(DaemonSocketPath, Cfg.SocketPath.c_str(), sizeof(DaemonSocketPath) - 1);
	DaemonSocketPath[sizeof(DaemonSocketPath) - 1] = '\0';
}