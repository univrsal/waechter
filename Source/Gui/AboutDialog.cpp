//
// Created by usr on 27/10/2025.
//

#include "AboutDialog.hpp"
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

WAboutDialog::WAboutDialog()
{
	ThirdPartyLibraries.clear();

	ThirdPartyLibraries.emplace_back("glad", "https://github.com/Dav1dde/glad", gGladLicenseData, gGladLicenseSize);
	ThirdPartyLibraries.emplace_back("glfw", "https://www.glfw.org", gGlfwLicenseData, gGlfwLicenseSize);
	ThirdPartyLibraries.emplace_back("Dear ImGui", "https://github.com/ocornut/imgui", gImguiLicenseData, gImguiLicenseSize);
	ThirdPartyLibraries.emplace_back("incbin", "https://github.com/graphitemaster/incbin", gIncbinLicenseData, gIncbinLicenseSize);
	ThirdPartyLibraries.emplace_back("inih", "https://github.com/benhoyt/inih", gInihLicenseData, gInihLicenseSize);
	ThirdPartyLibraries.emplace_back("json11", "https://github.com/dropbox/json11", gJson11LicenseData, gJson11LicenseSize);
	ThirdPartyLibraries.emplace_back("spdlog", "https://github.com/gabime/spdlog", gSpdlogLicenseData, gSpdlogLicenseSize);
	ThirdPartyLibraries.emplace_back("stb", "https://github.com/nothings/stb", gStbLicenseData, gStbLicenseSize);

	VersionString = fmt::format("Version {}, commit {}@{}\nCompiled at {}", WAECHTER_VERSION, GIT_COMMIT_HASH, GIT_BRANCH, BUILD_TIME);
}

void WAboutDialog::Draw()
{
	if (!bVisible)
	{
		return;
	}

	if (ImGui::Begin("About", &bVisible, ImGuiWindowFlags_NoCollapse))
	{
		auto Style = ImGui::GetStyle();
		ImGui::PushFont(nullptr, Style.FontSizeBase * 1.4);
		ImGui::Text("Wächter");
		ImGui::PopFont();

		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2, 0.2, 0.2, 1));
		ImGui::Text("%s", VersionString.c_str());
		ImGui::PopStyleColor();

		ImGui::Spacing();

		ImGui::Text("Thirdparty libraries:");

		for (const auto& Library : ThirdPartyLibraries)
		{
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::PushFont(nullptr, Style.FontSizeBase * 1.2);
			ImGui::Text("%s", Library.Name.c_str());
			ImGui::PopFont();
			ImGui::Spacing();
			ImGui::TextLinkOpenURL(Library.Url.c_str(), Library.Url.c_str());
			ImGui::Spacing();
			ImGui::TextWrapped("%s", Library.LicenseText.c_str());
		}

		ImGui::End();
	}
}