//
// Created by usr on 27/10/2025.
//

#include "AboutDialog.hpp"
#include <imgui.h>

void WAboutDialog::Draw()
{
	if (!bVisible)
	{
		return;
	}

	if (ImGui::Begin("About", &bVisible, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
	{
		ImGui::Text("Test");

		ImGui::End();
	}
}