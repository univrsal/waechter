//
// Created by usr on 26/10/2025.
//

#include "MainWindow.hpp"

#include "imgui_internal.h"

#include <imgui.h>

void WMainWindow::Draw()
{
	ImGuiIO& io = ImGui::GetIO();
	ImVec2   display_size = io.DisplaySize; // Current window/swapchain size

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(display_size);

	// Optional: remove window decorations and background if you want a full overlay
	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoDecoration;

	ImGui::Begin("MainWindow", nullptr, window_flags);

	ImGui::Text("Hello, fullscreen window!");

	ImGui::End();
}