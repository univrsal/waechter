/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "AboutDialog.hpp"

#include "imgui.h"

#include "SdlWindow.hpp"
#include "Util/I18n.hpp"
#include "Util/Settings.hpp"
#include "Assets.hpp"

WAboutDialog::WAboutDialog()
{
	Logo = WImageUtils::LoadImageFromMemoryRGBA8(GIconData, GIconSize);
	auto MakeString = [](unsigned char const Data[], size_t Size) {
		if (Data == nullptr || Size == 0)
		{
			return std::string("");
		}
		return std::string(reinterpret_cast<char const*>(Data), Size);
	};
	ThirdPartyLibraries.emplace_back(
		"glad", "https://github.com/Dav1dde/glad", MakeString(GGladLicenseData, GGladLicenseSize));
	ThirdPartyLibraries.emplace_back("SDL2", "https://www.libsdl.org", MakeString(GSdl2LicenseData, GSdl2LicenseSize));
	ThirdPartyLibraries.emplace_back(
		"Dear ImGui", "https://github.com/ocornut/imgui", MakeString(GImguiLicenseData, GImguiLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"inih", "https://github.com/benhoyt/inih", MakeString(GInihLicenseData, GInihLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"json11", "https://github.com/dropbox/json11", MakeString(GJson11LicenseData, GJson11LicenseSize));
	ThirdPartyLibraries.emplace_back(
		"spdlog", "https://github.com/gabime/spdlog", MakeString(GSpdlogLicenseData, GSpdlogLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"stb", "https://github.com/nothings/stb", MakeString(GStbLicenseData, GStbLicenseSize));
	ThirdPartyLibraries.emplace_back("JetBrains Mono", "https://www.jetbrains.com/lp/mono/",
		MakeString(GJetbrainsMonoLicenseData, GJetbrainsMonoLicenseSize));
	ThirdPartyLibraries.emplace_back("AdwaitaSans Regular", "https://github.com/GNOME/adwaita-fonts",
		MakeString(GAdwaitaSansLicenseData, GAdwaitaSansLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"cereal", "https://github.com/USCiLab/cereal", MakeString(GCerealLicenseData, GCerealLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"implot", "https://github.com/epezent/implot", MakeString(GImPlotLicenseData, GImPlotLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"flag-icons", "https://github.com/lipis/flag-icons", MakeString(GFlagsLicenseData, GFlagsLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"sigslot", "https://github.com/palacaze/sigslot", MakeString(GSigSlotLicenseData, GSigSlotLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"traycon", "https://github.com/univrsal/traycon", MakeString(GTrayconLicenseData, GTrayconLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"date", "https://github.com/HowardHinnant/date", MakeString(GDateLicenseData, GDateLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"sqlpp11", "https://github.com/rbock/sqlpp11", MakeString(GSqlpp11LicenseData, GSqlpp11LicenseSize));

	VersionString = std::format(
		"Version {}, commit {}@{}\nCompiled at {}", WAECHTER_VERSION, GIT_COMMIT_HASH, GIT_BRANCH, BUILD_TIME);

	WaechterLicense = std::string(reinterpret_cast<char const*>(GWaechterLicenseData), GWaechterLicenseSize);
}

void WAboutDialog::Draw()
{
	if (!bVisible)
	{
		return;
	}

	ImGuiIO const& Io = ImGui::GetIO();
	auto const     DisplaySize = Io.DisplaySize; // Current window/swapchain size
	ImGui::SetNextWindowPos(ImVec2(DisplaySize.x * 0.5f, DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize({ WSdlWindow::ScaleValue(700), DisplaySize.y - WSdlWindow::ScaleValue(100) });

	if (ImGui::Begin(TR("about.title"), &bVisible,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		auto const Style = ImGui::GetStyle();
		ImGui::PushFont(nullptr, Style.FontSizeBase * 1.4f);
		ImGui::Image(Logo.TextureId, ImVec2(WSdlWindow::ScaleValue(96), WSdlWindow::ScaleValue(96)));
		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::Text("Wächter");
		ImGui::PopFont();

		ImGui::Spacing();
		ImGui::Text("%s", VersionString.c_str());
		ImGui::TextLinkOpenURL("https://waechter.st", "https://waechter.st");
		ImGui::SameLine();
		ImGui::TextLinkOpenURL("https://github.com/univrsal/waechter", "https://github.com/univrsal/waechter");
		if (WSdlWindow::GetInstance().GetMainWindow()->IsRegistered())
		{
			ImGui::Text("Registered to %s", WSettings::GetInstance().RegisteredUsername.c_str());
		}
		else
		{
			ImGui::Text("UNREGISTERED VERSION");
		}
		ImGui::EndGroup();
		if (ImGui::BeginTabBar("##Statswindow"))
		{
			if (ImGui::BeginTabItem(TR("about.license"), nullptr, ImGuiTabItemFlags_NoReorder))
			{
				if (ImGui::BeginChild(
						"##LicenseScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_None))
				{
					ImGui::TextWrapped("%s", WaechterLicense.c_str());
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(TR("about.contributors"), nullptr, ImGuiTabItemFlags_NoReorder))
			{
				if (ImGui::BeginChild(
						"##ContributorsScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_None))
				{
					ImGui::TextWrapped("%.*s", static_cast<int>(GAuthorsSize), GAuthorsData);
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(TR("about.thirdparty_libraries"), nullptr, ImGuiTabItemFlags_NoReorder))
			{
				if (ImGui::BeginChild(
						"##ThirdPartyScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_None))
				{
					for (auto const& Library : ThirdPartyLibraries)
					{
						ImGui::PushFont(nullptr, Style.FontSizeBase * 1.2f);
						if (ImGui::TreeNodeEx(Library.Name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth))
						{
							ImGui::PopFont();
							ImGui::TextLinkOpenURL(Library.Url.c_str(), Library.Url.c_str());
							ImGui::TextWrapped("%s", Library.LicenseText.c_str());
							ImGui::TreePop();
						}
						else
						{
							ImGui::PopFont();
						}
					}
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
}