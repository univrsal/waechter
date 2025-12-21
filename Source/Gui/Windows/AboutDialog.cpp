/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define INCBIN_PREFIX G

#include "AboutDialog.hpp"

#include "GlfwWindow.hpp"
#include "RegisterDialog.hpp"
#include "Util/Settings.hpp"

#include <imgui.h>
#include <incbin.h>
#include <spdlog/fmt/fmt.h>

INCBIN(GladLicense, THIRD_PARTY_GLAD_LICENSE_PATH);
INCBIN(GlfwLicense, THIRD_PARTY_GLFW_LICENSE_PATH);
INCBIN(ImguiLicense, THIRD_PARTY_IMGUI_LICENSE_PATH);
INCBIN(IncbinLicense, THIRD_PARTY_INCBIN_LICENSE_PATH);
INCBIN(InihLicense, THIRD_PARTY_INIH_LICENSE_PATH);
INCBIN(Json11License, THIRD_PARTY_JSON11_LICENSE_PATH);
INCBIN(SpdlogLicense, THIRD_PARTY_SPDLOG_LICENSE_PATH);
INCBIN(StbLicense, THIRD_PARTY_STB_LICENSE_PATH);
INCBIN(JetbrainsMonoLicense, THIRD_PARTY_JETBRAINS_MONO_LICENSE_PATH);
INCBIN(CerealLicense, THIRD_PARTY_CEREAL_LICENSE_PATH);
INCBIN(ImPlotLicense, THIRD_PARTY_IMPLOT_LICENSE_PATH);
INCBIN(FlagsLicense, THIRD_PARTY_FLAGS_LICENSE_PATH);
INCBIN(SigSlotLicense, THIRD_PARTY_SIGSLOT_LICENSE_PATH);

WAboutDialog::WAboutDialog()
{
	auto MakeString = [](unsigned char const Data[], size_t Size) {
		if (Data == nullptr || Size == 0)
		{
			return std::string("");
		}
		return std::string(reinterpret_cast<char const*>(Data), Size);
	};
	ThirdPartyLibraries.emplace_back(
		"glad", "https://github.com/Dav1dde/glad", MakeString(GGladLicenseData, GGladLicenseSize));
	ThirdPartyLibraries.emplace_back("glfw", "https://www.glfw.org", MakeString(GGlfwLicenseData, GGlfwLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"Dear ImGui", "https://github.com/ocornut/imgui", MakeString(GImguiLicenseData, GImguiLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"incbin", "https://github.com/graphitemaster/incbin", MakeString(GIncbinLicenseData, GIncbinLicenseSize));
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
	ThirdPartyLibraries.emplace_back(
		"cereal", "https://github.com/USCiLab/cereal", MakeString(GCerealLicenseData, GCerealLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"implot", "https://github.com/epezent/implot", MakeString(GImPlotLicenseData, GImPlotLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"flag-icons", "https://github.com/lipis/flag-icons", MakeString(GFlagsLicenseData, GFlagsLicenseSize));
	ThirdPartyLibraries.emplace_back(
		"sigslot", "https://github.com/palacaze/sigslot", MakeString(GSigSlotLicenseData, GSigSlotLicenseSize));

	VersionString = fmt::format(
		"Version {}, commit {}@{}\nCompiled at {}", WAECHTER_VERSION, GIT_COMMIT_HASH, GIT_BRANCH, BUILD_TIME);
}

void WAboutDialog::Draw()
{
	if (!bVisible)
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();
	auto     display_size = io.DisplaySize; // Current window/swapchain size
	ImGui::SetNextWindowPos(ImVec2(display_size.x * 0.5f, display_size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize({ 700, display_size.y - 100 });

	if (ImGui::Begin(
			"About", &bVisible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		auto Style = ImGui::GetStyle();
		ImGui::PushFont(nullptr, Style.FontSizeBase * 1.4f);
		ImGui::Text("Wächter");
		ImGui::PopFont();

		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
		ImGui::Text("%s", VersionString.c_str());
		if (WGlfwWindow::GetInstance().GetMainWindow()->IsRegistered())
		{
			ImGui::Text("Registered to %s", WSettings::GetInstance().RegisteredUsername.c_str());
		}
		else
		{
			ImGui::Text("UNREGISTERED VERSION");
		}
		ImGui::PopStyleColor();

		ImGui::Spacing();

		ImGui::Text("Thirdparty libraries:");

		for (auto const& Library : ThirdPartyLibraries)
		{
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::PushFont(nullptr, Style.FontSizeBase * 1.2f);
			ImGui::Text("%s", Library.Name.c_str());
			ImGui::PopFont();
			ImGui::Spacing();
			ImGui::TextLinkOpenURL(Library.Url.c_str(), Library.Url.c_str());
			ImGui::Spacing();
			ImGui::TextWrapped("%s", Library.LicenseText.c_str());
		}
	}
	ImGui::End();
}