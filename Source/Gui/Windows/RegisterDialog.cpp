/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "RegisterDialog.hpp"

#define INCBIN_PREFIX G
#include <imgui.h>
#include <incbin.h>

#include "GlfwWindow.hpp"
#include "Util/Settings.hpp"
#include "Util/Keylogic/validate.h"

INCBIN(WatermarkImage, WATERMARK_IMAGE);

void WRegisterDialog::Init()
{
	memcpy(Username, WSettings::GetInstance().RegisteredUsername.c_str(), sizeof(Username) - 1);
	memcpy(SerialKey, WSettings::GetInstance().RegistrationSerialKey.c_str(), sizeof(SerialKey) - 1);
	Validate();

	Watermark = WImageUtils::LoadImageFromMemoryRGBA8(GWatermarkImageData, GWatermarkImageSize);
}

void WRegisterDialog::Draw()
{
	if (!bValid)
	{
		// draw watermark in bottom right corner
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y + 15),
			ImGuiCond_Always, ImVec2(1.0f, 1.0f));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		if (ImGui::Begin("Watermark", nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
					| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize
					| ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration))
		{
			ImGui::Image(Watermark.TextureId, ImVec2(Watermark.Width / 2.f, Watermark.Height / 2.f));
		}
		ImGui::PopStyleVar(2);
		ImGui::End();
	}
	if (!bVisible)
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();
	auto     display_size = io.DisplaySize; // Current window/swapchain size
	ImGui::SetNextWindowPos(ImVec2(display_size.x * 0.5f, display_size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize({ 300, 135 });

	if (ImGui::Begin("Register Wächter", &bVisible,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
	{
		int TextBoxFlags = ImGuiInputTextFlags_CharsNoBlank;
		ImGui::InputText("Username", Username, sizeof(Username), bValid ? ImGuiInputTextFlags_ReadOnly : 0);
		ImGui::InputText("Serial Key", SerialKey, sizeof(SerialKey),
			bValid ? (ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_Password) : TextBoxFlags);

		if (bValid)
		{
			if (ImGui::Button("Unregister"))
			{
				WSettings::GetInstance().RegisteredUsername.clear();
				WSettings::GetInstance().RegistrationSerialKey.clear();
				WSettings::GetInstance().Save();
				bValid = false;
				bFailedActivation = false;
				WGlfwWindow::GetInstance().SetTitle("Wächter (UNREGISTERED)");
			}
		}
		else
		{
			if (ImGui::Button("Register"))
			{
				Validate();
				if (!bValid)
				{
					bFailedActivation = true;
				}
			}
		}

		if (bValid)
		{
			ImGui::TextColored(ImVec4(0.1f, 0.7f, 0.1f, 1.0f), "Wächter is activated.");
		}
		else if (bFailedActivation)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid username or serial key.");
		}
	}
	ImGui::End();
}

void WRegisterDialog::Validate()
{
	if (bValid)
		return;
	if (is_valid_key(Username, SerialKey))
	{
		WSettings::GetInstance().RegisteredUsername = Username;
		WSettings::GetInstance().RegistrationSerialKey = SerialKey;
		WSettings::GetInstance().Save();
		bValid = true;
		WGlfwWindow::GetInstance().SetTitle("Wächter");
	}
	else
	{
		WGlfwWindow::GetInstance().SetTitle("Wächter (UNREGISTERED)");
	}
}
